// Copyright 2013 the Chromium Embedded Framework Authors. Portions Copyright
// 2012 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#include "libcef/browser/devtools_frontend.h"

#include "libcef/browser/content_browser_client.h"
#include "libcef/browser/devtools_delegate.h"
#include "libcef/browser/request_context_impl.h"
#include "libcef/browser/thread_util.h"

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "ipc/ipc_channel.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_response_writer.h"

namespace {


// ResponseWriter -------------------------------------------------------------

class ResponseWriter : public net::URLFetcherResponseWriter {
 public:
  ResponseWriter(base::WeakPtr<CefDevToolsFrontend> devtools_,
                 int stream_id);
  ~ResponseWriter() override;

  // URLFetcherResponseWriter overrides:
  int Initialize(const net::CompletionCallback& callback) override;
  int Write(net::IOBuffer* buffer,
            int num_bytes,
            const net::CompletionCallback& callback) override;
  int Finish(const net::CompletionCallback& callback) override;

 private:
  base::WeakPtr<CefDevToolsFrontend> devtools_;
  int stream_id_;

  DISALLOW_COPY_AND_ASSIGN(ResponseWriter);
};

ResponseWriter::ResponseWriter(
    base::WeakPtr<CefDevToolsFrontend> devtools,
    int stream_id)
    : devtools_(devtools),
      stream_id_(stream_id) {
}

ResponseWriter::~ResponseWriter() {
}

int ResponseWriter::Initialize(const net::CompletionCallback& callback) {
  return net::OK;
}

int ResponseWriter::Write(net::IOBuffer* buffer,
                          int num_bytes,
                          const net::CompletionCallback& callback) {
  std::string chunk = std::string(buffer->data(), num_bytes);
  if (!base::IsStringUTF8(chunk))
    return num_bytes;

  base::FundamentalValue* id = new base::FundamentalValue(stream_id_);
  base::StringValue* chunkValue = new base::StringValue(chunk);

  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&CefDevToolsFrontend::CallClientFunction,
                 devtools_, "DevToolsAPI.streamWrite",
                 base::Owned(id), base::Owned(chunkValue), nullptr));
  return num_bytes;
}

int ResponseWriter::Finish(const net::CompletionCallback& callback) {
  return net::OK;
}

// This constant should be in sync with
// the constant at devtools_ui_bindings.cc.
const size_t kMaxMessageChunkSize = IPC::Channel::kMaximumMessageSize / 4;

}  // namespace

// static
CefDevToolsFrontend* CefDevToolsFrontend::Show(
    CefRefPtr<CefBrowserHostImpl> inspected_browser,
    const CefWindowInfo& windowInfo,
    CefRefPtr<CefClient> client,
    const CefBrowserSettings& settings,
    const CefPoint& inspect_element_at) {
  CefBrowserSettings new_settings = settings;
  if (CefColorGetA(new_settings.background_color) == 0) {
    // Use white as the default background color for DevTools instead of the
    // CefSettings.background_color value.
    new_settings.background_color = SK_ColorWHITE;
  }

  CefBrowserHostImpl::CreateParams create_params;
  if (!inspected_browser->IsViewsHosted())
    create_params.window_info.reset(new CefWindowInfo(windowInfo));
  create_params.client = client;
  create_params.settings = new_settings;
  create_params.devtools_opener = inspected_browser;
  create_params.request_context = inspected_browser->GetRequestContext();

  CefRefPtr<CefBrowserHostImpl> frontend_browser =
      CefBrowserHostImpl::Create(create_params);

  content::WebContents* inspected_contents = inspected_browser->web_contents();

  // CefDevToolsFrontend will delete itself when the frontend WebContents is
  // destroyed.
  CefDevToolsFrontend* devtools_frontend = new CefDevToolsFrontend(
      static_cast<CefBrowserHostImpl*>(frontend_browser.get()),
      inspected_contents, inspect_element_at);

  // Need to load the URL after creating the DevTools objects.
  CefDevToolsDelegate* delegate =
      CefContentBrowserClient::Get()->devtools_delegate();
  frontend_browser->GetMainFrame()->LoadURL(delegate->GetChromeDevToolsURL());

  return devtools_frontend;
}

void CefDevToolsFrontend::Focus() {
  frontend_browser_->SetFocus(true);
}

void CefDevToolsFrontend::InspectElementAt(int x, int y) {
  if (inspect_element_at_.x != x || inspect_element_at_.y != y)
    inspect_element_at_.Set(x, y);
  if (agent_host_)
    agent_host_->InspectElement(this, x, y);
}

void CefDevToolsFrontend::Close() {
  CEF_POST_TASK(CEF_UIT,
      base::Bind(&CefBrowserHostImpl::CloseBrowser, frontend_browser_.get(),
                 true));
}

void CefDevToolsFrontend::DisconnectFromTarget() {
  if (!agent_host_)
    return;
  agent_host_->DetachClient(this);
  agent_host_ = NULL;
}

CefDevToolsFrontend::CefDevToolsFrontend(
    CefRefPtr<CefBrowserHostImpl> frontend_browser,
    content::WebContents* inspected_contents,
    const CefPoint& inspect_element_at)
    : WebContentsObserver(frontend_browser->web_contents()),
      frontend_browser_(frontend_browser),
      inspected_contents_(inspected_contents),
      inspect_element_at_(inspect_element_at),
      weak_factory_(this) {
}

CefDevToolsFrontend::~CefDevToolsFrontend() {
  for (const auto& pair : pending_requests_)
    delete pair.first;
}

void CefDevToolsFrontend::RenderViewCreated(
    content::RenderViewHost* render_view_host) {
  if (!frontend_host_) {
    frontend_host_.reset(content::DevToolsFrontendHost::Create(
        web_contents()->GetMainFrame(),
        base::Bind(&CefDevToolsFrontend::HandleMessageFromDevToolsFrontend,
                   base::Unretained(this))));

  }
}

void CefDevToolsFrontend::DocumentAvailableInMainFrame() {
  // Don't call AttachClient multiple times for the same DevToolsAgentHost.
  // Otherwise it will call AgentHostClosed which closes the DevTools window.
  // This may happen in cases where the DevTools content fails to load.
  scoped_refptr<content::DevToolsAgentHost> agent_host =
      content::DevToolsAgentHost::GetOrCreateFor(inspected_contents_);
  if (agent_host != agent_host_) {
    agent_host_ = agent_host;
    agent_host_->AttachClient(this);

    if (!inspect_element_at_.IsEmpty())
      InspectElementAt(inspect_element_at_.x, inspect_element_at_.y);
  }
}

void CefDevToolsFrontend::WebContentsDestroyed() {
  if (agent_host_)
    agent_host_->DetachClient(this);
  delete this;
}

void CefDevToolsFrontend::HandleMessageFromDevToolsFrontend(
    const std::string& message) {
  if (!agent_host_)
    return;
  std::string method;
  base::ListValue* params = NULL;
  base::DictionaryValue* dict = NULL;
  std::unique_ptr<base::Value> parsed_message = base::JSONReader::Read(message);
  if (!parsed_message ||
      !parsed_message->GetAsDictionary(&dict) ||
      !dict->GetString("method", &method)) {
    return;
  }
  int request_id = 0;
  dict->GetInteger("id", &request_id);
  dict->GetList("params", &params);

  if (method == "dispatchProtocolMessage" && params && params->GetSize() == 1) {
    if (!agent_host_ || !agent_host_->IsAttached())
      return;
    std::string protocol_message;
    if (!params->GetString(0, &protocol_message))
      return;
    agent_host_->DispatchProtocolMessage(this, protocol_message);
  } else if (method == "loadCompleted") {
    web_contents()->GetMainFrame()->ExecuteJavaScriptForTests(
        base::ASCIIToUTF16("DevToolsAPI.setUseSoftMenu(true);"));
  } else if (method == "loadNetworkResource" && params->GetSize() == 3) {
    // TODO(pfeldman): handle some of the embedder messages in content.
    std::string url;
    std::string headers;
    int stream_id;
    if (!params->GetString(0, &url) ||
        !params->GetString(1, &headers) ||
        !params->GetInteger(2, &stream_id)) {
      return;
    }

    GURL gurl(url);
    if (!gurl.is_valid()) {
      base::DictionaryValue response;
      response.SetInteger("statusCode", 404);
      SendMessageAck(request_id, &response);
      return;
    }

    net::URLFetcher* fetcher =
        net::URLFetcher::Create(gurl, net::URLFetcher::GET, this).release();
    pending_requests_[fetcher] = request_id;
    fetcher->SetRequestContext(
        content::BrowserContext::GetDefaultStoragePartition(
            web_contents()->GetBrowserContext())->
                GetURLRequestContext());
    fetcher->SetExtraRequestHeaders(headers);
    fetcher->SaveResponseWithWriter(base::WrapUnique(
        new ResponseWriter(weak_factory_.GetWeakPtr(), stream_id)));
    fetcher->Start();
    return;
  } else if (method == "getPreferences") {
    SendMessageAck(request_id, &preferences_);
    return;
  } else if (method == "setPreference") {
    std::string name;
    std::string value;
    if (!params->GetString(0, &name) ||
        !params->GetString(1, &value)) {
      return;
    }
    preferences_.SetStringWithoutPathExpansion(name, value);
  } else if (method == "removePreference") {
    std::string name;
    if (!params->GetString(0, &name))
      return;
    preferences_.RemoveWithoutPathExpansion(name, nullptr);
  } else if (method == "requestFileSystems") {
    web_contents()->GetMainFrame()->ExecuteJavaScriptForTests(
        base::ASCIIToUTF16("DevToolsAPI.fileSystemsLoaded([]);"));
  } else {
    return;
  }

  if (request_id)
    SendMessageAck(request_id, nullptr);
}

void CefDevToolsFrontend::DispatchProtocolMessage(
    content::DevToolsAgentHost* agent_host,
    const std::string& message) {
  if (message.length() < kMaxMessageChunkSize) {
    std::string param;
    base::EscapeJSONString(message, true, &param);
    std::string code = "DevToolsAPI.dispatchMessage(" + param + ");";
    base::string16 javascript = base::UTF8ToUTF16(code);
    web_contents()->GetMainFrame()->ExecuteJavaScriptForTests(javascript);
    return;
  }

  size_t total_size = message.length();
  for (size_t pos = 0; pos < message.length(); pos += kMaxMessageChunkSize) {
    std::string param;
    base::EscapeJSONString(message.substr(pos, kMaxMessageChunkSize), true,
                           &param);
    std::string code = "DevToolsAPI.dispatchMessageChunk(" + param + "," +
                       std::to_string(pos ? 0 : total_size) + ");";
    base::string16 javascript = base::UTF8ToUTF16(code);
    web_contents()->GetMainFrame()->ExecuteJavaScriptForTests(javascript);
  }
}

void CefDevToolsFrontend::SetPreferences(const std::string& json) {
  preferences_.Clear();
  if (json.empty())
    return;
  base::DictionaryValue* dict = nullptr;
  std::unique_ptr<base::Value> parsed = base::JSONReader::Read(json);
  if (!parsed || !parsed->GetAsDictionary(&dict))
    return;
  for (base::DictionaryValue::Iterator it(*dict); !it.IsAtEnd(); it.Advance()) {
    if (!it.value().IsType(base::Value::TYPE_STRING))
      continue;
    preferences_.SetWithoutPathExpansion(it.key(), it.value().CreateDeepCopy());
  }
}

void CefDevToolsFrontend::OnURLFetchComplete(const net::URLFetcher* source) {
  // TODO(pfeldman): this is a copy of chrome's devtools_ui_bindings.cc.
  // We should handle some of the commands including this one in content.
  DCHECK(source);
  PendingRequestsMap::iterator it = pending_requests_.find(source);
  DCHECK(it != pending_requests_.end());

  base::DictionaryValue response;
  base::DictionaryValue* headers = new base::DictionaryValue();
  net::HttpResponseHeaders* rh = source->GetResponseHeaders();
  response.SetInteger("statusCode", rh ? rh->response_code() : 200);
  response.Set("headers", headers);

  size_t iterator = 0;
  std::string name;
  std::string value;
  while (rh && rh->EnumerateHeaderLines(&iterator, &name, &value))
    headers->SetString(name, value);

  SendMessageAck(it->second, &response);
  pending_requests_.erase(it);
  delete source;
}

void CefDevToolsFrontend::CallClientFunction(
    const std::string& function_name,
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
      base::UTF8ToUTF16(javascript));
}

void CefDevToolsFrontend::SendMessageAck(int request_id,
                                           const base::Value* arg) {
  base::FundamentalValue id_value(request_id);
  CallClientFunction("DevToolsAPI.embedderMessageAck",
                     &id_value, arg, nullptr);
}

void CefDevToolsFrontend::AgentHostClosed(
    content::DevToolsAgentHost* agent_host,
    bool replaced) {
  DCHECK(agent_host == agent_host_.get());
  Close();
}
