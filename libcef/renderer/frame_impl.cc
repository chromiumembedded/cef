// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/renderer/frame_impl.h"

#include "base/compiler_specific.h"

// Enable deprecation warnings on Windows. See http://crbug.com/585142.
#if defined(OS_WIN)
#if defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wdeprecated-declarations"
#else
#pragma warning(push)
#pragma warning(default : 4996)
#endif
#endif

#include "libcef/common/cef_messages.h"
#include "libcef/common/content_client.h"
#include "libcef/common/net/http_header_utils.h"
#include "libcef/common/process_message_impl.h"
#include "libcef/common/request_impl.h"
#include "libcef/common/response_manager.h"
#include "libcef/renderer/blink_glue.h"
#include "libcef/renderer/browser_impl.h"
#include "libcef/renderer/dom_document_impl.h"
#include "libcef/renderer/render_frame_util.h"
#include "libcef/renderer/render_urlrequest_impl.h"
#include "libcef/renderer/thread_util.h"
#include "libcef/renderer/v8_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "content/public/renderer/render_view.h"
#include "content/renderer/render_frame_impl.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_document_loader.h"
#include "third_party/blink/public/web/web_frame_content_dumper.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_navigation_control.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/public/web/web_view.h"

CefFrameImpl::CefFrameImpl(CefBrowserImpl* browser,
                           blink::WebLocalFrame* frame,
                           int64_t frame_id)
    : browser_(browser),
      frame_(frame),
      frame_id_(frame_id),
      response_manager_(new CefResponseManager) {}

CefFrameImpl::~CefFrameImpl() {}

bool CefFrameImpl::IsValid() {
  CEF_REQUIRE_RT_RETURN(false);

  return (frame_ != NULL);
}

void CefFrameImpl::Undo() {
  ExecuteCommand("Undo");
}

void CefFrameImpl::Redo() {
  ExecuteCommand("Redo");
}

void CefFrameImpl::Cut() {
  ExecuteCommand("Cut");
}

void CefFrameImpl::Copy() {
  ExecuteCommand("Copy");
}

void CefFrameImpl::Paste() {
  ExecuteCommand("Paste");
}

void CefFrameImpl::Delete() {
  ExecuteCommand("Delete");
}

void CefFrameImpl::SelectAll() {
  ExecuteCommand("SelectAll");
}

void CefFrameImpl::ViewSource() {
  NOTREACHED() << "ViewSource cannot be called from the renderer process";
}

void CefFrameImpl::GetSource(CefRefPtr<CefStringVisitor> visitor) {
  CEF_REQUIRE_RT_RETURN_VOID();
  if (frame_) {
    const CefString& content =
        std::string(blink::WebFrameContentDumper::DumpAsMarkup(frame_).Utf8());
    visitor->Visit(content);
  }
}

void CefFrameImpl::GetText(CefRefPtr<CefStringVisitor> visitor) {
  CEF_REQUIRE_RT_RETURN_VOID();
  if (frame_) {
    const CefString& content = blink_glue::DumpDocumentText(frame_);
    visitor->Visit(content);
  }
}

void CefFrameImpl::LoadRequest(CefRefPtr<CefRequest> request) {
  CEF_REQUIRE_RT_RETURN_VOID();

  if (!frame_)
    return;

  CefMsg_LoadRequest_Params params;
  params.url = GURL(std::string(request->GetURL()));
  params.method = request->GetMethod();
  params.site_for_cookies =
      GURL(std::string(request->GetFirstPartyForCookies()));

  CefRequest::HeaderMap headerMap;
  request->GetHeaderMap(headerMap);
  if (!headerMap.empty())
    params.headers = HttpHeaderUtils::GenerateHeaders(headerMap);

  CefRefPtr<CefPostData> postData = request->GetPostData();
  if (postData.get()) {
    CefPostDataImpl* impl = static_cast<CefPostDataImpl*>(postData.get());
    params.upload_data = new net::UploadData();
    impl->Get(*params.upload_data.get());
  }

  params.load_flags = request->GetFlags();

  OnLoadRequest(params);
}

void CefFrameImpl::LoadURL(const CefString& url) {
  CEF_REQUIRE_RT_RETURN_VOID();

  if (!frame_)
    return;

  CefMsg_LoadRequest_Params params;
  params.url = GURL(url.ToString());
  params.method = "GET";

  OnLoadRequest(params);
}

void CefFrameImpl::LoadString(const CefString& string, const CefString& url) {
  CEF_REQUIRE_RT_RETURN_VOID();

  if (frame_) {
    GURL gurl = GURL(url.ToString());
    content::RenderFrame::FromWebFrame(frame_)->LoadHTMLString(
        string.ToString(), gurl, "UTF-8", GURL(),
        false /* replace_current_item */);
  }
}

void CefFrameImpl::ExecuteJavaScript(const CefString& jsCode,
                                     const CefString& scriptUrl,
                                     int startLine) {
  CEF_REQUIRE_RT_RETURN_VOID();

  if (jsCode.empty())
    return;
  if (startLine < 1)
    startLine = 1;

  if (frame_) {
    GURL gurl = GURL(scriptUrl.ToString());
    frame_->ExecuteScript(blink::WebScriptSource(
        blink::WebString::FromUTF16(jsCode.ToString16()), gurl, startLine));
  }
}

bool CefFrameImpl::IsMain() {
  CEF_REQUIRE_RT_RETURN(false);

  if (frame_)
    return (frame_->Parent() == NULL);
  return false;
}

bool CefFrameImpl::IsFocused() {
  CEF_REQUIRE_RT_RETURN(false);

  if (frame_ && frame_->View())
    return (frame_->View()->FocusedFrame() == frame_);
  return false;
}

CefString CefFrameImpl::GetName() {
  CefString name;
  CEF_REQUIRE_RT_RETURN(name);

  if (frame_)
    name = render_frame_util::GetName(frame_);
  return name;
}

int64 CefFrameImpl::GetIdentifier() {
  CEF_REQUIRE_RT_RETURN(0);

  return frame_id_;
}

CefRefPtr<CefFrame> CefFrameImpl::GetParent() {
  CEF_REQUIRE_RT_RETURN(NULL);

  if (frame_) {
    blink::WebFrame* parent = frame_->Parent();
    if (parent && parent->IsWebLocalFrame())
      return browser_->GetWebFrameImpl(parent->ToWebLocalFrame()).get();
  }

  return NULL;
}

CefString CefFrameImpl::GetURL() {
  CefString url;
  CEF_REQUIRE_RT_RETURN(url);

  if (frame_) {
    GURL gurl = frame_->GetDocument().Url();
    if (gurl.is_empty()) {
      // For popups the main document URL will be empty during loading. Return
      // the provisional document URL instead.
      blink::WebDocumentLoader* loader = frame_->GetProvisionalDocumentLoader();
      if (loader)
        gurl = loader->GetUrl();
    }
    url = gurl.spec();
  }
  return url;
}

CefRefPtr<CefBrowser> CefFrameImpl::GetBrowser() {
  CEF_REQUIRE_RT_RETURN(NULL);

  return browser_;
}

CefRefPtr<CefV8Context> CefFrameImpl::GetV8Context() {
  CEF_REQUIRE_RT_RETURN(NULL);

  if (frame_) {
    v8::Isolate* isolate = blink::MainThreadIsolate();
    v8::HandleScope handle_scope(isolate);
    return new CefV8ContextImpl(isolate, frame_->MainWorldScriptContext());
  } else {
    return NULL;
  }
}

void CefFrameImpl::VisitDOM(CefRefPtr<CefDOMVisitor> visitor) {
  CEF_REQUIRE_RT_RETURN_VOID();

  if (!frame_)
    return;

  // Create a CefDOMDocumentImpl object that is valid only for the scope of this
  // method.
  CefRefPtr<CefDOMDocumentImpl> documentImpl;
  const blink::WebDocument& document = frame_->GetDocument();
  if (!document.IsNull())
    documentImpl = new CefDOMDocumentImpl(browser_, frame_);

  visitor->Visit(documentImpl.get());

  if (documentImpl.get())
    documentImpl->Detach();
}

CefRefPtr<CefURLRequest> CefFrameImpl::CreateURLRequest(
    CefRefPtr<CefRequest> request,
    CefRefPtr<CefURLRequestClient> client) {
  CEF_REQUIRE_RT_RETURN(NULL);

  if (!request || !client || !frame_)
    return NULL;

  CefRefPtr<CefRenderURLRequest> impl =
      new CefRenderURLRequest(this, request, client);
  if (impl->Start())
    return impl.get();
  return NULL;
}

void CefFrameImpl::SendProcessMessage(CefProcessId target_process,
                                      CefRefPtr<CefProcessMessage> message) {
  Cef_Request_Params params;
  CefProcessMessageImpl* impl =
      static_cast<CefProcessMessageImpl*>(message.get());
  if (impl->CopyTo(params)) {
    SendProcessMessage(target_process, params.name, &params.arguments, true);
  }
}

blink::WebURLLoaderFactory* CefFrameImpl::GetURLLoaderFactory() {
  CEF_REQUIRE_RT();
  if (!url_loader_factory_ && frame_) {
    auto render_frame = content::RenderFrameImpl::FromWebFrame(frame_);
    if (render_frame) {
      url_loader_factory_ = render_frame->CreateURLLoaderFactory();
    }
  }
  return url_loader_factory_.get();
}

void CefFrameImpl::OnAttached() {
  Send(new CefHostMsg_FrameAttached(MSG_ROUTING_NONE));
}

bool CefFrameImpl::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CefFrameImpl, message)
    IPC_MESSAGE_HANDLER(CefMsg_Request, OnRequest)
    IPC_MESSAGE_HANDLER(CefMsg_Response, OnResponse)
    IPC_MESSAGE_HANDLER(CefMsg_ResponseAck, OnResponseAck)
    IPC_MESSAGE_HANDLER(CefMsg_LoadRequest, OnLoadRequest)
    IPC_MESSAGE_HANDLER(CefMsg_DidStopLoading, OnDidStopLoading)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void CefFrameImpl::OnDidFinishLoad() {
  blink::WebDocumentLoader* dl = frame_->GetDocumentLoader();
  const int http_status_code = dl->GetResponse().HttpStatusCode();
  Send(new CefHostMsg_DidFinishLoad(MSG_ROUTING_NONE, dl->GetUrl(),
                                    http_status_code));

  CefRefPtr<CefApp> app = CefContentClient::Get()->application();
  if (app) {
    CefRefPtr<CefRenderProcessHandler> handler = app->GetRenderProcessHandler();
    if (handler) {
      CefRefPtr<CefLoadHandler> load_handler = handler->GetLoadHandler();
      if (load_handler) {
        load_handler->OnLoadEnd(browser_, this, http_status_code);
      }
    }
  }
}

void CefFrameImpl::OnFocused() {
  Send(new CefHostMsg_FrameFocused(MSG_ROUTING_NONE));
}

void CefFrameImpl::OnDraggableRegionsChanged() {
  blink::WebVector<blink::WebDraggableRegion> webregions =
      frame_->GetDocument().DraggableRegions();
  std::vector<Cef_DraggableRegion_Params> regions;
  for (size_t i = 0; i < webregions.size(); ++i) {
    Cef_DraggableRegion_Params region;
    browser_->render_view()->ConvertViewportToWindowViaWidget(
        &webregions[i].bounds);
    region.bounds = webregions[i].bounds;
    region.draggable = webregions[i].draggable;
    regions.push_back(region);
  }
  Send(new CefHostMsg_UpdateDraggableRegions(MSG_ROUTING_NONE, regions));
}

void CefFrameImpl::OnDetached() {
  // The browser may hold the last reference to |this|. Take a reference here to
  // keep |this| alive until after this method returns.
  CefRefPtr<CefFrameImpl> self = this;

  browser_->FrameDetached(frame_id_);

  browser_ = nullptr;
  frame_ = nullptr;
  url_loader_factory_.reset();
  response_manager_.reset();
}

void CefFrameImpl::ExecuteCommand(const std::string& command) {
  CEF_REQUIRE_RT_RETURN_VOID();
  if (frame_)
    frame_->ExecuteCommand(blink::WebString::FromUTF8(command));
}

void CefFrameImpl::SendProcessMessage(CefProcessId target_process,
                                      const std::string& name,
                                      base::ListValue* arguments,
                                      bool user_initiated) {
  DCHECK_EQ(PID_BROWSER, target_process);
  DCHECK(!name.empty());

  if (!frame_)
    return;

  Cef_Request_Params params;
  params.name = name;
  if (arguments)
    params.arguments.Swap(arguments);
  params.user_initiated = user_initiated;
  params.request_id = -1;
  params.expect_response = false;

  Send(new CefHostMsg_Request(MSG_ROUTING_NONE, params));
}

void CefFrameImpl::Send(IPC::Message* message) {
  if (!frame_) {
    delete message;
    return;
  }

  auto render_frame = content::RenderFrame::FromWebFrame(frame_);
  message->set_routing_id(render_frame->GetRoutingID());
  render_frame->Send(message);
}

void CefFrameImpl::OnRequest(const Cef_Request_Params& params) {
  DCHECK(browser_);
  DCHECK(frame_);

  bool success = false;
  std::string response;
  bool expect_response_ack = false;

  TRACE_EVENT2("cef", "CefBrowserImpl::OnRequest", "request_id",
               params.request_id, "expect_response",
               params.expect_response ? 1 : 0);

  if (params.user_initiated) {
    // Give the user a chance to handle the request.
    CefRefPtr<CefApp> app = CefContentClient::Get()->application();
    if (app.get()) {
      CefRefPtr<CefRenderProcessHandler> handler =
          app->GetRenderProcessHandler();
      if (handler.get()) {
        CefRefPtr<CefProcessMessageImpl> message(new CefProcessMessageImpl(
            const_cast<Cef_Request_Params*>(&params), false, true));
        success = handler->OnProcessMessageReceived(browser_, this, PID_BROWSER,
                                                    message.get());
        message->Detach(nullptr);
      }
    }
  } else if (params.name == "execute-code") {
    // Execute code.
    DCHECK_EQ(params.arguments.GetSize(), (size_t)4);

    bool is_javascript = false;
    std::string code, script_url;
    int script_start_line = 0;

    params.arguments.GetBoolean(0, &is_javascript);
    params.arguments.GetString(1, &code);
    DCHECK(!code.empty());
    params.arguments.GetString(2, &script_url);
    params.arguments.GetInteger(3, &script_start_line);
    DCHECK_GE(script_start_line, 0);

    if (is_javascript) {
      frame_->ExecuteScript(
          blink::WebScriptSource(blink::WebString::FromUTF8(code),
                                 GURL(script_url), script_start_line));
      success = true;
    } else {
      // TODO(cef): implement support for CSS code.
      NOTIMPLEMENTED();
    }
  } else if (params.name == "execute-command") {
    // Execute command.
    DCHECK_EQ(params.arguments.GetSize(), (size_t)1);

    std::string command;

    params.arguments.GetString(0, &command);
    DCHECK(!command.empty());

    if (base::LowerCaseEqualsASCII(command, "getsource")) {
      response = blink::WebFrameContentDumper::DumpAsMarkup(frame_).Utf8();
      success = true;
    } else if (base::LowerCaseEqualsASCII(command, "gettext")) {
      response = blink_glue::DumpDocumentText(frame_);
      success = true;
    } else if (frame_->ExecuteCommand(blink::WebString::FromUTF8(command))) {
      success = true;
    }
  } else if (params.name == "load-string") {
    // Load a string.
    DCHECK_EQ(params.arguments.GetSize(), (size_t)2);

    std::string string, url;

    params.arguments.GetString(0, &string);
    params.arguments.GetString(1, &url);

    content::RenderFrame::FromWebFrame(frame_)->LoadHTMLString(
        string, GURL(url), "UTF-8", GURL(), false /* replace_current_item */);
  } else {
    // Invalid request.
    NOTREACHED();
  }

  if (params.expect_response) {
    DCHECK_GE(params.request_id, 0);

    // Send a response to the browser.
    Cef_Response_Params response_params;
    response_params.request_id = params.request_id;
    response_params.success = success;
    response_params.response = response;
    response_params.expect_response_ack = expect_response_ack;
    Send(new CefHostMsg_Response(MSG_ROUTING_NONE, response_params));
  }
}

void CefFrameImpl::OnResponse(const Cef_Response_Params& params) {
  response_manager_->RunHandler(params);
  if (params.expect_response_ack)
    Send(new CefHostMsg_ResponseAck(MSG_ROUTING_NONE, params.request_id));
}

void CefFrameImpl::OnResponseAck(int request_id) {
  response_manager_->RunAckHandler(request_id);
}

void CefFrameImpl::OnDidStopLoading() {
  // We should only receive this notification for the highest-level LocalFrame
  // in this frame's in-process subtree. If there are multiple of these for the
  // same browser then the other occurrences will be discarded in
  // OnLoadingStateChange.
  DCHECK(frame_->IsLocalRoot());
  browser_->OnLoadingStateChange(false);
}

void CefFrameImpl::OnLoadRequest(const CefMsg_LoadRequest_Params& params) {
  DCHECK(frame_);

  blink::WebURLRequest request;
  CefRequestImpl::Get(params, request);

  frame_->StartNavigation(request);
}

// Enable deprecation warnings on Windows. See http://crbug.com/585142.
#if defined(OS_WIN)
#if defined(__clang__)
#pragma GCC diagnostic pop
#else
#pragma warning(pop)
#endif
#endif
