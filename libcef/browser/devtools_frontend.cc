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
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/devtools_http_handler.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "net/base/net_util.h"
#include "third_party/skia/include/core/SkColor.h"

namespace {

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

  CefRefPtr<CefBrowserHostImpl> frontend_browser =
      CefBrowserHostImpl::Create(windowInfo, client, CefString(),
                                 new_settings,
                                 inspected_browser->GetWindowHandle(), true,
                                 inspected_browser->GetRequestContext());

  scoped_refptr<content::DevToolsAgentHost> agent_host =
      content::DevToolsAgentHost::GetOrCreateFor(
          inspected_browser->GetWebContents());
  if (!inspect_element_at.IsEmpty())
    agent_host->InspectElement(inspect_element_at.x, inspect_element_at.y);

  // CefDevToolsFrontend will delete itself when the frontend WebContents is
  // destroyed.
  CefDevToolsFrontend* devtools_frontend = new CefDevToolsFrontend(
      static_cast<CefBrowserHostImpl*>(frontend_browser.get()),
      agent_host.get());

  // Need to load the URL after creating the DevTools objects.
  CefDevToolsDelegate* delegate =
      CefContentBrowserClient::Get()->devtools_delegate();
  frontend_browser->GetMainFrame()->LoadURL(delegate->GetChromeDevToolsURL());

  devtools_frontend->Activate();
  devtools_frontend->Focus();

  return devtools_frontend;
}

void CefDevToolsFrontend::Activate() {
  frontend_browser_->ActivateContents(web_contents());
}

void CefDevToolsFrontend::Focus() {
  web_contents()->Focus();
}

void CefDevToolsFrontend::Close() {
  CEF_POST_TASK(CEF_UIT,
      base::Bind(&CefBrowserHostImpl::CloseBrowser, frontend_browser_.get(),
                 true));
}

CefDevToolsFrontend::CefDevToolsFrontend(
    CefRefPtr<CefBrowserHostImpl> frontend_browser,
    content::DevToolsAgentHost* agent_host)
    : WebContentsObserver(frontend_browser->GetWebContents()),
      frontend_browser_(frontend_browser),
      agent_host_(agent_host) {
}

CefDevToolsFrontend::~CefDevToolsFrontend() {
}

void CefDevToolsFrontend::RenderViewCreated(
    content::RenderViewHost* render_view_host) {
  if (!frontend_host_) {
    frontend_host_.reset(
        content::DevToolsFrontendHost::Create(
            web_contents()->GetMainFrame(), this));
    agent_host_->AttachClient(this);
  }
}

void CefDevToolsFrontend::WebContentsDestroyed() {
  agent_host_->DetachClient();
  delete this;
}

void CefDevToolsFrontend::HandleMessageFromDevToolsFrontend(
    const std::string& message) {
  std::string method;
  int id = 0;
  base::ListValue* params = NULL;
  base::DictionaryValue* dict = NULL;
  scoped_ptr<base::Value> parsed_message(base::JSONReader::Read(message));
  if (!parsed_message ||
      !parsed_message->GetAsDictionary(&dict) ||
      !dict->GetString("method", &method)) {
    return;
  }
  dict->GetList("params", &params);

  std::string browser_message;
  if (method == "sendMessageToBrowser" && params &&
      params->GetSize() == 1 && params->GetString(0, &browser_message)) {
    agent_host_->DispatchProtocolMessage(browser_message);
  } else if (method == "loadCompleted") {
    web_contents()->GetMainFrame()->ExecuteJavaScript(
        base::ASCIIToUTF16("DevToolsAPI.setUseSoftMenu(true);"));
  } else {
    return;
  }

  dict->GetInteger("id", &id);
  if (id) {
    std::string code = "DevToolsAPI.embedderMessageAck(" +
        base::IntToString(id) + ",\"\");";
    base::string16 javascript = base::UTF8ToUTF16(code);
    web_contents()->GetMainFrame()->ExecuteJavaScript(javascript);
  }
}

void CefDevToolsFrontend::HandleMessageFromDevToolsFrontendToBackend(
    const std::string& message) {
  agent_host_->DispatchProtocolMessage(message);
}

void CefDevToolsFrontend::DispatchProtocolMessage(
    content::DevToolsAgentHost* agent_host,
    const std::string& message) {
  if (message.length() < kMaxMessageChunkSize) {
    base::string16 javascript = base::UTF8ToUTF16(
        "DevToolsAPI.dispatchMessage(" + message + ");");
    web_contents()->GetMainFrame()->ExecuteJavaScript(javascript);
    return;
  }

  base::FundamentalValue total_size(static_cast<int>(message.length()));
  for (size_t pos = 0; pos < message.length(); pos += kMaxMessageChunkSize) {
    base::StringValue message_value(message.substr(pos, kMaxMessageChunkSize));
    std::string param;
    base::JSONWriter::Write(&message_value, &param);
    std::string code = "DevToolsAPI.dispatchMessageChunk(" + param + ");";
    base::string16 javascript = base::UTF8ToUTF16(code);
    web_contents()->GetMainFrame()->ExecuteJavaScript(javascript);
  }
}

void CefDevToolsFrontend::AgentHostClosed(
    content::DevToolsAgentHost* agent_host,
    bool replaced) {
  DCHECK(agent_host == agent_host_.get());
  Close();
}
