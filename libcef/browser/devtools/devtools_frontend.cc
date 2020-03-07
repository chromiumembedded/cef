// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/devtools/devtools_frontend.h"

#include <stddef.h>

#include <utility>

#include "libcef/browser/browser_context.h"
#include "libcef/browser/devtools/devtools_manager_delegate.h"
#include "libcef/browser/net/devtools_scheme_handler.h"

#include "base/base64.h"
#include "base/guid.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/file_url_loader.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/shared_cors_origin_access_list.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "ipc/ipc_channel.h"
#include "net/base/completion_once_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/simple_url_loader_stream_consumer.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {

static std::string GetFrontendURL() {
  return base::StringPrintf("%s://%s/devtools_app.html",
                            content::kChromeDevToolsScheme,
                            scheme::kChromeDevToolsHost);
}

std::unique_ptr<base::DictionaryValue> BuildObjectForResponse(
    const net::HttpResponseHeaders* rh,
    bool success,
    int net_error) {
  auto response = std::make_unique<base::DictionaryValue>();
  int responseCode = 200;
  if (rh) {
    responseCode = rh->response_code();
  } else if (!success) {
    // In case of no headers, assume file:// URL and failed to load
    responseCode = 404;
  }
  response->SetInteger("statusCode", responseCode);
  response->SetInteger("netError", net_error);
  response->SetString("netErrorName", net::ErrorToString(net_error));

  auto headers = std::make_unique<base::DictionaryValue>();
  size_t iterator = 0;
  std::string name;
  std::string value;
  // TODO(caseq): this probably needs to handle duplicate header names
  // correctly by folding them.
  while (rh && rh->EnumerateHeaderLines(&iterator, &name, &value))
    headers->SetString(name, value);

  response->Set("headers", std::move(headers));
  return response;
}

}  // namespace

class CefDevToolsFrontend::NetworkResourceLoader
    : public network::SimpleURLLoaderStreamConsumer {
 public:
  NetworkResourceLoader(int stream_id,
                        CefDevToolsFrontend* bindings,
                        std::unique_ptr<network::SimpleURLLoader> loader,
                        network::mojom::URLLoaderFactory* url_loader_factory,
                        int request_id)
      : stream_id_(stream_id),
        bindings_(bindings),
        loader_(std::move(loader)),
        request_id_(request_id) {
    loader_->SetOnResponseStartedCallback(base::BindOnce(
        &NetworkResourceLoader::OnResponseStarted, base::Unretained(this)));
    loader_->DownloadAsStream(url_loader_factory, this);
  }

 private:
  void OnResponseStarted(const GURL& final_url,
                         const network::mojom::URLResponseHead& response_head) {
    response_headers_ = response_head.headers;
  }

  void OnDataReceived(base::StringPiece chunk,
                      base::OnceClosure resume) override {
    base::Value chunkValue;

    bool encoded = !base::IsStringUTF8(chunk);
    if (encoded) {
      std::string encoded_string;
      base::Base64Encode(chunk, &encoded_string);
      chunkValue = base::Value(std::move(encoded_string));
    } else {
      chunkValue = base::Value(chunk);
    }
    base::Value id(stream_id_);
    base::Value encodedValue(encoded);

    bindings_->CallClientFunction("DevToolsAPI.streamWrite", &id, &chunkValue,
                                  &encodedValue);
    std::move(resume).Run();
  }

  void OnComplete(bool success) override {
    auto response = BuildObjectForResponse(response_headers_.get(), success,
                                           loader_->NetError());
    bindings_->SendMessageAck(request_id_, response.get());

    bindings_->loaders_.erase(bindings_->loaders_.find(this));
  }

  void OnRetry(base::OnceClosure start_retry) override { NOTREACHED(); }

  const int stream_id_;
  CefDevToolsFrontend* const bindings_;
  std::unique_ptr<network::SimpleURLLoader> loader_;
  int request_id_;
  scoped_refptr<net::HttpResponseHeaders> response_headers_;

  DISALLOW_COPY_AND_ASSIGN(NetworkResourceLoader);
};

// This constant should be in sync with
// the constant at devtools_ui_bindings.cc.
const size_t kMaxMessageChunkSize = IPC::Channel::kMaximumMessageSize / 4;

// static
CefDevToolsFrontend* CefDevToolsFrontend::Show(
    CefRefPtr<CefBrowserHostImpl> inspected_browser,
    const CefWindowInfo& windowInfo,
    CefRefPtr<CefClient> client,
    const CefBrowserSettings& settings,
    const CefPoint& inspect_element_at) {
  CefBrowserSettings new_settings = settings;
  if (!windowInfo.windowless_rendering_enabled &&
      CefColorGetA(new_settings.background_color) != SK_AlphaOPAQUE) {
    // Use white as the default background color for windowed DevTools instead
    // of the CefSettings.background_color value.
    new_settings.background_color = SK_ColorWHITE;
  }

  CefBrowserHostImpl::CreateParams create_params;
  if (!inspected_browser->IsViewsHosted())
    create_params.window_info.reset(new CefWindowInfo(windowInfo));
  create_params.client = client;
  create_params.settings = new_settings;
  create_params.devtools_opener = inspected_browser;
  create_params.request_context = inspected_browser->GetRequestContext();
  create_params.extra_info = inspected_browser->browser_info()->extra_info();

  CefRefPtr<CefBrowserHostImpl> frontend_browser =
      CefBrowserHostImpl::Create(create_params);

  content::WebContents* inspected_contents = inspected_browser->web_contents();

  // CefDevToolsFrontend will delete itself when the frontend WebContents is
  // destroyed.
  CefDevToolsFrontend* devtools_frontend = new CefDevToolsFrontend(
      static_cast<CefBrowserHostImpl*>(frontend_browser.get()),
      inspected_contents, inspect_element_at);

  // Need to load the URL after creating the DevTools objects.
  frontend_browser->GetMainFrame()->LoadURL(GetFrontendURL());

  return devtools_frontend;
}

void CefDevToolsFrontend::Activate() {
  frontend_browser_->ActivateContents(web_contents());
}

void CefDevToolsFrontend::Focus() {
  frontend_browser_->SetFocus(true);
}

void CefDevToolsFrontend::InspectElementAt(int x, int y) {
  if (inspect_element_at_.x != x || inspect_element_at_.y != y)
    inspect_element_at_.Set(x, y);
  if (agent_host_)
    agent_host_->InspectElement(inspected_contents_->GetFocusedFrame(), x, y);
}

void CefDevToolsFrontend::Close() {
  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::Bind(&CefBrowserHostImpl::CloseBrowser,
                            frontend_browser_.get(), true));
}

void CefDevToolsFrontend::DisconnectFromTarget() {
  if (!agent_host_)
    return;
  agent_host_->DetachClient(this);
  agent_host_ = nullptr;
}

CefDevToolsFrontend::CefDevToolsFrontend(
    CefRefPtr<CefBrowserHostImpl> frontend_browser,
    content::WebContents* inspected_contents,
    const CefPoint& inspect_element_at)
    : content::WebContentsObserver(frontend_browser->web_contents()),
      frontend_browser_(frontend_browser),
      inspected_contents_(inspected_contents),
      inspect_element_at_(inspect_element_at),
      file_manager_(frontend_browser.get(), GetPrefs()),
      weak_factory_(this) {}

CefDevToolsFrontend::~CefDevToolsFrontend() {}

void CefDevToolsFrontend::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  content::RenderFrameHost* frame = navigation_handle->GetRenderFrameHost();
  if (navigation_handle->IsInMainFrame()) {
    frontend_host_ = content::DevToolsFrontendHost::Create(
        frame,
        base::Bind(&CefDevToolsFrontend::HandleMessageFromDevToolsFrontend,
                   base::Unretained(this)));
    return;
  }

  std::string origin = navigation_handle->GetURL().GetOrigin().spec();
  auto it = extensions_api_.find(origin);
  if (it == extensions_api_.end())
    return;
  std::string script = base::StringPrintf("%s(\"%s\")", it->second.c_str(),
                                          base::GenerateGUID().c_str());
  content::DevToolsFrontendHost::SetupExtensionsAPI(frame, script);
}

void CefDevToolsFrontend::DocumentAvailableInMainFrame() {
  // Don't call AttachClient multiple times for the same DevToolsAgentHost.
  // Otherwise it will call AgentHostClosed which closes the DevTools window.
  // This may happen in cases where the DevTools content fails to load.
  scoped_refptr<content::DevToolsAgentHost> agent_host =
      content::DevToolsAgentHost::GetOrCreateFor(inspected_contents_);
  if (agent_host != agent_host_) {
    if (agent_host_)
      agent_host_->DetachClient(this);
    agent_host_ = agent_host;
    agent_host_->AttachClient(this);
    if (!inspect_element_at_.IsEmpty()) {
      agent_host_->InspectElement(inspected_contents_->GetFocusedFrame(),
                                  inspect_element_at_.x, inspect_element_at_.y);
    }
  }
}

void CefDevToolsFrontend::WebContentsDestroyed() {
  if (agent_host_) {
    agent_host_->DetachClient(this);
    agent_host_ = nullptr;
  }
  delete this;
}

void CefDevToolsFrontend::HandleMessageFromDevToolsFrontend(
    const std::string& message) {
  std::string method;
  base::ListValue* params = nullptr;
  base::DictionaryValue* dict = nullptr;
  base::Optional<base::Value> parsed_message = base::JSONReader::Read(message);
  if (!parsed_message || !parsed_message->GetAsDictionary(&dict) ||
      !dict->GetString("method", &method)) {
    return;
  }
  int request_id = 0;
  dict->GetInteger("id", &request_id);
  dict->GetList("params", &params);

  if (method == "dispatchProtocolMessage" && params && params->GetSize() == 1) {
    std::string protocol_message;
    if (!agent_host_ || !params->GetString(0, &protocol_message))
      return;
    agent_host_->DispatchProtocolMessage(
        this, base::as_bytes(base::make_span(protocol_message)));
  } else if (method == "loadCompleted") {
    web_contents()->GetMainFrame()->ExecuteJavaScriptForTests(
        base::ASCIIToUTF16("DevToolsAPI.setUseSoftMenu(true);"),
        base::NullCallback());
  } else if (method == "loadNetworkResource" && params->GetSize() == 3) {
    // TODO(pfeldman): handle some of the embedder messages in content.
    std::string url;
    std::string headers;
    int stream_id;
    if (!params->GetString(0, &url) || !params->GetString(1, &headers) ||
        !params->GetInteger(2, &stream_id)) {
      return;
    }

    GURL gurl(url);
    if (!gurl.is_valid()) {
      base::DictionaryValue response;
      response.SetInteger("statusCode", 404);
      response.SetBoolean("urlValid", false);
      SendMessageAck(request_id, &response);
      return;
    }

    net::NetworkTrafficAnnotationTag traffic_annotation =
        net::DefineNetworkTrafficAnnotation(
            "devtools_handle_front_end_messages", R"(
            semantics {
              sender: "Developer Tools"
              description:
                "When user opens Developer Tools, the browser may fetch "
                "additional resources from the network to enrich the debugging "
                "experience (e.g. source map resources)."
              trigger: "User opens Developer Tools to debug a web page."
              data: "Any resources requested by Developer Tools."
              destination: OTHER
            }
            policy {
              cookies_allowed: YES
              cookies_store: "user"
              setting:
                "It's not possible to disable this feature from settings."
              chrome_policy {
                DeveloperToolsAvailability {
                  policy_options {mode: MANDATORY}
                  DeveloperToolsAvailability: 2
                }
              }
            })");

    // Based on DevToolsUIBindings::LoadNetworkResource.
    auto resource_request = std::make_unique<network::ResourceRequest>();
    resource_request->url = gurl;
    // TODO(caseq): this preserves behavior of URLFetcher-based
    // implementation. We really need to pass proper first party origin from
    // the front-end.
    resource_request->site_for_cookies = net::SiteForCookies::FromUrl(gurl);
    resource_request->headers.AddHeadersFromString(headers);

    std::unique_ptr<network::mojom::URLLoaderFactory> file_url_loader_factory;
    scoped_refptr<network::SharedURLLoaderFactory> network_url_loader_factory;
    std::unique_ptr<network::mojom::URLLoaderFactory> webui_url_loader_factory;
    network::mojom::URLLoaderFactory* url_loader_factory;
    if (gurl.SchemeIsFile()) {
      file_url_loader_factory = content::CreateFileURLLoaderFactory(
          base::FilePath() /* profile_path */,
          nullptr /* shared_cors_origin_access_list */);
      url_loader_factory = file_url_loader_factory.get();
    } else if (content::HasWebUIScheme(gurl)) {
      base::DictionaryValue response;
      response.SetInteger("statusCode", 403);
      SendMessageAck(request_id, &response);
      return;
    } else {
      auto* partition = content::BrowserContext::GetStoragePartitionForSite(
          web_contents()->GetBrowserContext(), gurl);
      network_url_loader_factory =
          partition->GetURLLoaderFactoryForBrowserProcess();
      url_loader_factory = network_url_loader_factory.get();
    }

    auto simple_url_loader = network::SimpleURLLoader::Create(
        std::move(resource_request), traffic_annotation);
    auto resource_loader = std::make_unique<NetworkResourceLoader>(
        stream_id, this, std::move(simple_url_loader), url_loader_factory,
        request_id);
    loaders_.insert(std::move(resource_loader));
    return;
  } else if (method == "getPreferences") {
    SendMessageAck(request_id,
                   GetPrefs()->GetDictionary(prefs::kDevToolsPreferences));
    return;
  } else if (method == "setPreference") {
    std::string name;
    std::string value;
    if (!params->GetString(0, &name) || !params->GetString(1, &value)) {
      return;
    }
    DictionaryPrefUpdate update(GetPrefs(), prefs::kDevToolsPreferences);
    update.Get()->SetKey(name, base::Value(value));
  } else if (method == "removePreference") {
    std::string name;
    if (!params->GetString(0, &name))
      return;
    DictionaryPrefUpdate update(GetPrefs(), prefs::kDevToolsPreferences);
    update.Get()->RemoveWithoutPathExpansion(name, nullptr);
  } else if (method == "requestFileSystems") {
    web_contents()->GetMainFrame()->ExecuteJavaScriptForTests(
        base::ASCIIToUTF16("DevToolsAPI.fileSystemsLoaded([]);"),
        base::NullCallback());
  } else if (method == "reattach") {
    if (!agent_host_)
      return;
    agent_host_->DetachClient(this);
    agent_host_->AttachClient(this);
  } else if (method == "registerExtensionsAPI") {
    std::string origin;
    std::string script;
    if (!params->GetString(0, &origin) || !params->GetString(1, &script))
      return;
    extensions_api_[origin + "/"] = script;
  } else if (method == "save" && params->GetSize() == 3) {
    std::string url;
    std::string content;
    bool save_as;
    if (!params->GetString(0, &url) || !params->GetString(1, &content) ||
        !params->GetBoolean(2, &save_as)) {
      return;
    }
    file_manager_.SaveToFile(url, content, save_as);
  } else if (method == "append" && params->GetSize() == 2) {
    std::string url;
    std::string content;
    if (!params->GetString(0, &url) || !params->GetString(1, &content)) {
      return;
    }
    file_manager_.AppendToFile(url, content);
  } else {
    return;
  }

  if (request_id)
    SendMessageAck(request_id, nullptr);
}

void CefDevToolsFrontend::DispatchProtocolMessage(
    content::DevToolsAgentHost* agent_host,
    base::span<const uint8_t> message) {
  base::StringPiece str_message(reinterpret_cast<const char*>(message.data()),
                                message.size());
  if (str_message.length() < kMaxMessageChunkSize) {
    std::string param;
    base::EscapeJSONString(str_message, true, &param);
    std::string code = "DevToolsAPI.dispatchMessage(" + param + ");";
    base::string16 javascript = base::UTF8ToUTF16(code);
    web_contents()->GetMainFrame()->ExecuteJavaScriptForTests(
        javascript, base::NullCallback());
    return;
  }

  size_t total_size = str_message.length();
  for (size_t pos = 0; pos < str_message.length();
       pos += kMaxMessageChunkSize) {
    std::string param;
    base::EscapeJSONString(str_message.substr(pos, kMaxMessageChunkSize), true,
                           &param);
    std::string code = "DevToolsAPI.dispatchMessageChunk(" + param + "," +
                       std::to_string(pos ? 0 : total_size) + ");";
    base::string16 javascript = base::UTF8ToUTF16(code);
    web_contents()->GetMainFrame()->ExecuteJavaScriptForTests(
        javascript, base::NullCallback());
  }
}

void CefDevToolsFrontend::CallClientFunction(const std::string& function_name,
                                             const base::Value* arg1,
                                             const base::Value* arg2,
                                             const base::Value* arg3) {
  std::string javascript = function_name + "(";
  if (arg1) {
    std::string json;
    base::JSONWriter::Write(*arg1, &json);
    javascript.append(json);
    if (arg2) {
      base::JSONWriter::Write(*arg2, &json);
      javascript.append(", ").append(json);
      if (arg3) {
        base::JSONWriter::Write(*arg3, &json);
        javascript.append(", ").append(json);
      }
    }
  }
  javascript.append(");");
  web_contents()->GetMainFrame()->ExecuteJavaScriptForTests(
      base::UTF8ToUTF16(javascript), base::NullCallback());
}

void CefDevToolsFrontend::SendMessageAck(int request_id,
                                         const base::Value* arg) {
  base::Value id_value(request_id);
  CallClientFunction("DevToolsAPI.embedderMessageAck", &id_value, arg, nullptr);
}

void CefDevToolsFrontend::AgentHostClosed(
    content::DevToolsAgentHost* agent_host) {
  DCHECK(agent_host == agent_host_.get());
  agent_host_ = nullptr;
  Close();
}

PrefService* CefDevToolsFrontend::GetPrefs() const {
  return static_cast<CefBrowserContext*>(
             frontend_browser_->web_contents()->GetBrowserContext())
      ->GetPrefs();
}
