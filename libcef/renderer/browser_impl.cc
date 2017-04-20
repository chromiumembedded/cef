// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/renderer/browser_impl.h"

#include <string>
#include <vector>

#include "libcef/common/cef_messages.h"
#include "libcef/common/content_client.h"
#include "libcef/common/process_message_impl.h"
#include "libcef/common/request_impl.h"
#include "libcef/common/response_manager.h"
#include "libcef/renderer/content_renderer_client.h"
#include "libcef/renderer/dom_document_impl.h"
#include "libcef/renderer/thread_util.h"
#include "libcef/renderer/webkit_glue.h"

#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/navigation_state.h"
#include "content/public/renderer/render_view.h"
#include "content/renderer/navigation_state_impl.h"
#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLResponse.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebFrameContentDumper.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"
#include "third_party/WebKit/public/web/WebSecurityPolicy.h"
#include "third_party/WebKit/public/web/WebView.h"

using blink::WebFrame;
using blink::WebScriptSource;
using blink::WebString;
using blink::WebURL;
using blink::WebView;

// CefBrowserImpl static methods.
// -----------------------------------------------------------------------------

// static
CefRefPtr<CefBrowserImpl> CefBrowserImpl::GetBrowserForView(
    content::RenderView* view) {
  return CefContentRendererClient::Get()->GetBrowserForView(view);
}

// static
CefRefPtr<CefBrowserImpl> CefBrowserImpl::GetBrowserForMainFrame(
    blink::WebFrame* frame) {
  return CefContentRendererClient::Get()->GetBrowserForMainFrame(frame);
}


// CefBrowser methods.
// -----------------------------------------------------------------------------

CefRefPtr<CefBrowserHost> CefBrowserImpl::GetHost() {
  NOTREACHED() << "GetHost cannot be called from the render process";
  return nullptr;
}

bool CefBrowserImpl::CanGoBack() {
  CEF_REQUIRE_RT_RETURN(false);

  return webkit_glue::CanGoBack(render_view()->GetWebView());
}

void CefBrowserImpl::GoBack() {
  CEF_REQUIRE_RT_RETURN_VOID();

  webkit_glue::GoBack(render_view()->GetWebView());
}

bool CefBrowserImpl::CanGoForward() {
  CEF_REQUIRE_RT_RETURN(false);

  return webkit_glue::CanGoForward(render_view()->GetWebView());
}

void CefBrowserImpl::GoForward() {
  CEF_REQUIRE_RT_RETURN_VOID();

  webkit_glue::GoForward(render_view()->GetWebView());
}

bool CefBrowserImpl::IsLoading() {
  CEF_REQUIRE_RT_RETURN(false);

  if (render_view()->GetWebView()) {
    blink::WebFrame* main_frame = render_view()->GetWebView()->MainFrame();
    if (main_frame)
      return main_frame->ToWebLocalFrame()->IsLoading();
  }
  return false;
}

void CefBrowserImpl::Reload() {
  CEF_REQUIRE_RT_RETURN_VOID();

  if (render_view()->GetWebView() && render_view()->GetWebView()->MainFrame()) {
    render_view()->GetWebView()->MainFrame()->Reload(
        blink::WebFrameLoadType::kReload);
  }
}

void CefBrowserImpl::ReloadIgnoreCache() {
  CEF_REQUIRE_RT_RETURN_VOID();

  if (render_view()->GetWebView() && render_view()->GetWebView()->MainFrame()) {
    render_view()->GetWebView()->MainFrame()->Reload(
        blink::WebFrameLoadType::kReloadBypassingCache);
  }
}

void CefBrowserImpl::StopLoad() {
  CEF_REQUIRE_RT_RETURN_VOID();

  if (render_view()->GetWebView() && render_view()->GetWebView()->MainFrame())
    render_view()->GetWebView()->MainFrame()->StopLoading();
}

int CefBrowserImpl::GetIdentifier() {
  CEF_REQUIRE_RT_RETURN(0);

  return browser_id();
}

bool CefBrowserImpl::IsSame(CefRefPtr<CefBrowser> that) {
  CEF_REQUIRE_RT_RETURN(false);

  CefBrowserImpl* impl = static_cast<CefBrowserImpl*>(that.get());
  return (impl == this);
}

bool CefBrowserImpl::IsPopup() {
  CEF_REQUIRE_RT_RETURN(false);

  return is_popup();
}

bool CefBrowserImpl::HasDocument() {
  CEF_REQUIRE_RT_RETURN(false);

  if (render_view()->GetWebView() && render_view()->GetWebView()->MainFrame())
    return !render_view()->GetWebView()->MainFrame()->GetDocument().IsNull();
  return false;
}

CefRefPtr<CefFrame> CefBrowserImpl::GetMainFrame() {
  CEF_REQUIRE_RT_RETURN(nullptr);

  if (render_view()->GetWebView() && render_view()->GetWebView()->MainFrame())
    return GetWebFrameImpl(render_view()->GetWebView()->MainFrame()).get();
  return nullptr;
}

CefRefPtr<CefFrame> CefBrowserImpl::GetFocusedFrame() {
  CEF_REQUIRE_RT_RETURN(nullptr);

  if (render_view()->GetWebView() &&
      render_view()->GetWebView()->FocusedFrame()) {
    return GetWebFrameImpl(render_view()->GetWebView()->FocusedFrame()).get();
  }
  return nullptr;
}

CefRefPtr<CefFrame> CefBrowserImpl::GetFrame(int64 identifier) {
  CEF_REQUIRE_RT_RETURN(nullptr);

  return GetWebFrameImpl(identifier).get();
}

CefRefPtr<CefFrame> CefBrowserImpl::GetFrame(const CefString& name) {
  CEF_REQUIRE_RT_RETURN(nullptr);

  blink::WebView* web_view = render_view()->GetWebView();
  if (web_view) {
    const blink::WebString& frame_name = blink::WebString::FromUTF16(name);
    // Search by assigned frame name (Frame::name).
    WebFrame* frame = web_view->FindFrameByName(frame_name,
                                                web_view->MainFrame());
    if (!frame) {
      // Search by unique frame name (Frame::uniqueName).
      const std::string& searchname = name;
      for (WebFrame* cur_frame = web_view->MainFrame(); cur_frame;
           cur_frame = cur_frame->TraverseNext()) {
        if (webkit_glue::GetUniqueName(cur_frame) == searchname) {
          frame = cur_frame;
          break;
        }
      }
    }
    if (frame)
      return GetWebFrameImpl(frame).get();
  }

  return nullptr;
}

size_t CefBrowserImpl::GetFrameCount() {
  CEF_REQUIRE_RT_RETURN(0);

  int count = 0;

  if (render_view()->GetWebView()) {
    for (WebFrame* frame = render_view()->GetWebView()->MainFrame(); frame;
         frame = frame->TraverseNext()) {
      count++;
    }
  }

  return count;
}

void CefBrowserImpl::GetFrameIdentifiers(std::vector<int64>& identifiers) {
  CEF_REQUIRE_RT_RETURN_VOID();

  if (identifiers.size() > 0)
    identifiers.clear();

  if (render_view()->GetWebView()) {
    for (WebFrame* frame = render_view()->GetWebView()->MainFrame(); frame;
         frame = frame->TraverseNext()) {
      identifiers.push_back(webkit_glue::GetIdentifier(frame));
    }
  }
}

void CefBrowserImpl::GetFrameNames(std::vector<CefString>& names) {
  CEF_REQUIRE_RT_RETURN_VOID();

  if (names.size() > 0)
    names.clear();

  if (render_view()->GetWebView()) {
    for (WebFrame* frame = render_view()->GetWebView()->MainFrame(); frame;
         frame = frame->TraverseNext()) {
      names.push_back(webkit_glue::GetUniqueName(frame));
    }
  }
}

bool CefBrowserImpl::SendProcessMessage(CefProcessId target_process,
                                        CefRefPtr<CefProcessMessage> message) {
  Cef_Request_Params params;
  CefProcessMessageImpl* impl =
      static_cast<CefProcessMessageImpl*>(message.get());
  if (impl->CopyTo(params)) {
    return SendProcessMessage(target_process, params.name, &params.arguments,
                              true);
  }

  return false;
}


// CefBrowserImpl public methods.
// -----------------------------------------------------------------------------

CefBrowserImpl::CefBrowserImpl(content::RenderView* render_view,
                               int browser_id,
                               bool is_popup,
                               bool is_windowless)
    : content::RenderViewObserver(render_view),
      browser_id_(browser_id),
      is_popup_(is_popup),
      is_windowless_(is_windowless) {
  response_manager_.reset(new CefResponseManager);
}

CefBrowserImpl::~CefBrowserImpl() {
}

void CefBrowserImpl::LoadRequest(const CefMsg_LoadRequest_Params& params) {
  CefRefPtr<CefFrameImpl> framePtr = GetWebFrameImpl(params.frame_id);
  if (!framePtr.get())
    return;

  WebFrame* web_frame = framePtr->web_frame();

  blink::WebURLRequest request;
  CefRequestImpl::Get(params, request);

  web_frame->LoadRequest(request);
}

bool CefBrowserImpl::SendProcessMessage(CefProcessId target_process,
                                        const std::string& name,
                                        base::ListValue* arguments,
                                        bool user_initiated) {
  DCHECK_EQ(PID_BROWSER, target_process);
  DCHECK(!name.empty());

  Cef_Request_Params params;
  params.name = name;
  if (arguments)
    params.arguments.Swap(arguments);
  params.frame_id = -1;
  params.user_initiated = user_initiated;
  params.request_id = -1;
  params.expect_response = false;

  return Send(new CefHostMsg_Request(routing_id(), params));
}

CefRefPtr<CefFrameImpl> CefBrowserImpl::GetWebFrameImpl(
    blink::WebFrame* frame) {
  DCHECK(frame);
  int64_t frame_id = webkit_glue::GetIdentifier(frame);

  // Frames are re-used between page loads. Only add the frame to the map once.
  FrameMap::const_iterator it = frames_.find(frame_id);
  if (it != frames_.end())
    return it->second;

  CefRefPtr<CefFrameImpl> framePtr(new CefFrameImpl(this, frame));
  frames_.insert(std::make_pair(frame_id, framePtr));

  const int64_t parent_id = frame->Parent() == NULL ?
      webkit_glue::kInvalidFrameId :
      webkit_glue::GetIdentifier(frame->Parent());
  const base::string16& name =
      base::UTF8ToUTF16(webkit_glue::GetUniqueName(frame));

  // Notify the browser that the frame has been identified.
  Send(new CefHostMsg_FrameIdentified(routing_id(), frame_id, parent_id, name));

  return framePtr;
}

CefRefPtr<CefFrameImpl> CefBrowserImpl::GetWebFrameImpl(int64_t frame_id) {
  if (frame_id == webkit_glue::kInvalidFrameId) {
    if (render_view()->GetWebView() && render_view()->GetWebView()->MainFrame())
      return GetWebFrameImpl(render_view()->GetWebView()->MainFrame());
    return nullptr;
  }

  // Check if we already know about the frame.
  FrameMap::const_iterator it = frames_.find(frame_id);
  if (it != frames_.end())
    return it->second;

  if (render_view()->GetWebView()) {
    // Check if the frame exists but we don't know about it yet.
    for (WebFrame* frame = render_view()->GetWebView()->MainFrame(); frame;
         frame = frame->TraverseNext()) {
      if (webkit_glue::GetIdentifier(frame) == frame_id)
        return GetWebFrameImpl(frame);
    }
  }

  return nullptr;
}

void CefBrowserImpl::AddFrameObject(int64_t frame_id,
                                    CefTrackNode* tracked_object) {
  CefRefPtr<CefTrackManager> manager;

  if (!frame_objects_.empty()) {
    FrameObjectMap::const_iterator it = frame_objects_.find(frame_id);
    if (it != frame_objects_.end())
      manager = it->second;
  }

  if (!manager.get()) {
    manager = new CefTrackManager();
    frame_objects_.insert(std::make_pair(frame_id, manager));
  }

  manager->Add(tracked_object);
}

bool CefBrowserImpl::is_swapped_out() const {
  content::RenderViewImpl* render_view_impl =
      static_cast<content::RenderViewImpl*>(render_view());
  return (!render_view_impl || render_view_impl->is_swapped_out());
}


// RenderViewObserver methods.
// -----------------------------------------------------------------------------

void CefBrowserImpl::OnDestruct() {
  // Notify that the browser window has been destroyed.
  CefRefPtr<CefApp> app = CefContentClient::Get()->application();
  if (app.get()) {
    CefRefPtr<CefRenderProcessHandler> handler =
        app->GetRenderProcessHandler();
    if (handler.get())
      handler->OnBrowserDestroyed(this);
  }

  response_manager_.reset();

  CefContentRendererClient::Get()->OnBrowserDestroyed(this);
}

void CefBrowserImpl::DidStartLoading() {
  OnLoadingStateChange(true);
}

void CefBrowserImpl::DidStopLoading() {
  OnLoadingStateChange(false);
}

void CefBrowserImpl::DidFailLoad(
    blink::WebLocalFrame* frame,
    const blink::WebURLError& error) {
  OnLoadError(frame, error);
  OnLoadEnd(frame);
}

void CefBrowserImpl::DidFinishLoad(blink::WebLocalFrame* frame) {
  blink::WebDataSource* ds = frame->DataSource();
  Send(new CefHostMsg_DidFinishLoad(routing_id(),
                                    webkit_glue::GetIdentifier(frame),
                                    ds->GetRequest().Url(),
                                    !frame->Parent(),
                                    ds->GetResponse().HttpStatusCode()));
  OnLoadEnd(frame);
}

void CefBrowserImpl::DidStartProvisionalLoad(blink::WebLocalFrame* frame) {
  // Send the frame creation notification if necessary.
  GetWebFrameImpl(frame);
}

void CefBrowserImpl::DidFailProvisionalLoad(
    blink::WebLocalFrame* frame,
    const blink::WebURLError& error) {
  OnLoadError(frame, error);
}

void CefBrowserImpl::DidCommitProvisionalLoad(blink::WebLocalFrame* frame,
                                              bool is_new_navigation) {
  OnLoadStart(frame);
}

void CefBrowserImpl::FrameDetached(WebFrame* frame) {
  int64_t frame_id = webkit_glue::GetIdentifier(frame);

  if (!frames_.empty()) {
    // Remove the frame from the map.
    FrameMap::iterator it = frames_.find(frame_id);
    if (it != frames_.end()) {
      it->second->Detach();
      frames_.erase(it);
    }
  }

  if (!frame_objects_.empty()) {
    // Remove any tracked objects associated with the frame.
    FrameObjectMap::iterator it = frame_objects_.find(frame_id);
    if (it != frame_objects_.end())
      frame_objects_.erase(it);
  }
}

void CefBrowserImpl::FocusedNodeChanged(const blink::WebNode& node) {
  // Notify the handler.
  CefRefPtr<CefApp> app = CefContentClient::Get()->application();
  if (app.get()) {
    CefRefPtr<CefRenderProcessHandler> handler =
        app->GetRenderProcessHandler();
    if (handler.get()) {
      if (node.IsNull()) {
        handler->OnFocusedNodeChanged(this, GetFocusedFrame(), NULL);
      } else {
        const blink::WebDocument& document = node.GetDocument();
        if (!document.IsNull()) {
          blink::WebFrame* frame = document.GetFrame();
          CefRefPtr<CefDOMDocumentImpl> documentImpl =
              new CefDOMDocumentImpl(this, frame);
          handler->OnFocusedNodeChanged(this,
              GetWebFrameImpl(frame).get(),
              documentImpl->GetOrCreateNode(node));
          documentImpl->Detach();
        }
      }
    }
  }
}

// Based on ExtensionHelper::DraggableRegionsChanged.
void CefBrowserImpl::DraggableRegionsChanged(blink::WebFrame* frame) {
  blink::WebVector<blink::WebDraggableRegion> webregions =
      frame->GetDocument().DraggableRegions();
  std::vector<Cef_DraggableRegion_Params> regions;
  for (size_t i = 0; i < webregions.size(); ++i) {
    Cef_DraggableRegion_Params region;
    render_view()->ConvertViewportToWindowViaWidget(&webregions[i].bounds);
    region.bounds = webregions[i].bounds;
    region.draggable = webregions[i].draggable;
    regions.push_back(region);
  }
  Send(new CefHostMsg_UpdateDraggableRegions(routing_id(), regions));
}

bool CefBrowserImpl::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CefBrowserImpl, message)
    IPC_MESSAGE_HANDLER(CefMsg_Request, OnRequest)
    IPC_MESSAGE_HANDLER(CefMsg_Response, OnResponse)
    IPC_MESSAGE_HANDLER(CefMsg_ResponseAck, OnResponseAck)
    IPC_MESSAGE_HANDLER(CefMsg_LoadRequest, LoadRequest)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}


// RenderViewObserver::OnMessageReceived message handlers.
// -----------------------------------------------------------------------------

void CefBrowserImpl::OnRequest(const Cef_Request_Params& params) {
  bool success = false;
  std::string response;
  bool expect_response_ack = false;

  TRACE_EVENT2("libcef", "CefBrowserImpl::OnRequest",
               "request_id", params.request_id,
               "expect_response", params.expect_response ? 1 : 0);

  if (params.user_initiated) {
    // Give the user a chance to handle the request.
    CefRefPtr<CefApp> app = CefContentClient::Get()->application();
    if (app.get()) {
      CefRefPtr<CefRenderProcessHandler> handler =
          app->GetRenderProcessHandler();
      if (handler.get()) {
        CefRefPtr<CefProcessMessageImpl> message(
            new CefProcessMessageImpl(const_cast<Cef_Request_Params*>(&params),
                                      false, true));
        success = handler->OnProcessMessageReceived(this, PID_BROWSER,
                                                    message.get());
        message->Detach(nullptr);
      }
    }
  } else if (params.name == "execute-code") {
    // Execute code.
    CefRefPtr<CefFrameImpl> framePtr = GetWebFrameImpl(params.frame_id);
    if (framePtr.get()) {
      WebFrame* web_frame = framePtr->web_frame();
      if (web_frame) {
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
          web_frame->ExecuteScript(
              WebScriptSource(blink::WebString::FromUTF8(code),
                              GURL(script_url),
                              script_start_line));
          success = true;
        } else {
          // TODO(cef): implement support for CSS code.
          NOTIMPLEMENTED();
        }
      }
    }
  } else if (params.name == "execute-command") {
    // Execute command.
    CefRefPtr<CefFrameImpl> framePtr = GetWebFrameImpl(params.frame_id);
    if (framePtr.get()) {
      WebFrame* web_frame = framePtr->web_frame();
      if (web_frame) {
        DCHECK_EQ(params.arguments.GetSize(), (size_t)1);

        std::string command;

        params.arguments.GetString(0, &command);
        DCHECK(!command.empty());

        if (base::LowerCaseEqualsASCII(command, "getsource")) {
          if (web_frame->IsWebLocalFrame()) {
            response = blink::WebFrameContentDumper::DumpAsMarkup(
                web_frame->ToWebLocalFrame()).Utf8();
            success = true;
          }
        } else if (base::LowerCaseEqualsASCII(command, "gettext")) {
          response = webkit_glue::DumpDocumentText(web_frame);
          success = true;
        } else if (web_frame->IsWebLocalFrame() &&
                   web_frame->ToWebLocalFrame()->ExecuteCommand(
                      blink::WebString::FromUTF8(command))) {
          success = true;
        }
      }
    }
  } else if (params.name == "load-string") {
    // Load a string.
    CefRefPtr<CefFrameImpl> framePtr = GetWebFrameImpl(params.frame_id);
    if (framePtr.get()) {
      WebFrame* web_frame = framePtr->web_frame();
      if (web_frame) {
        DCHECK_EQ(params.arguments.GetSize(), (size_t)2);

        std::string string, url;

        params.arguments.GetString(0, &string);
        params.arguments.GetString(1, &url);

        web_frame->LoadHTMLString(string, GURL(url));
      }
    }
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
    Send(new CefHostMsg_Response(routing_id(), response_params));
  }
}

void CefBrowserImpl::OnResponse(const Cef_Response_Params& params) {
  response_manager_->RunHandler(params);
  if (params.expect_response_ack)
    Send(new CefHostMsg_ResponseAck(routing_id(), params.request_id));
}

void CefBrowserImpl::OnResponseAck(int request_id) {
  response_manager_->RunAckHandler(request_id);
}

void CefBrowserImpl::OnLoadingStateChange(bool isLoading) {
  if (is_swapped_out())
    return;

  CefRefPtr<CefApp> app = CefContentClient::Get()->application();
  if (app.get()) {
    CefRefPtr<CefRenderProcessHandler> handler =
        app->GetRenderProcessHandler();
    if (handler.get()) {
      CefRefPtr<CefLoadHandler> load_handler = handler->GetLoadHandler();
      if (load_handler.get()) {
        WebView* web_view = render_view()->GetWebView();
        const bool canGoBack = webkit_glue::CanGoBack(web_view);
        const bool canGoForward = webkit_glue::CanGoForward(web_view);

        load_handler->OnLoadingStateChange(this, isLoading, canGoBack,
                                           canGoForward);
      }
    }
  }
}

void CefBrowserImpl::OnLoadStart(blink::WebLocalFrame* frame) {
  if (is_swapped_out())
    return;

  CefRefPtr<CefApp> app = CefContentClient::Get()->application();
  if (app.get()) {
    CefRefPtr<CefRenderProcessHandler> handler =
        app->GetRenderProcessHandler();
    if (handler.get()) {
      CefRefPtr<CefLoadHandler> load_handler = handler->GetLoadHandler();
      if (load_handler.get()) {
        CefRefPtr<CefFrameImpl> cef_frame = GetWebFrameImpl(frame);
        load_handler->OnLoadStart(this, cef_frame.get(), TT_EXPLICIT);
      }
    }
  }
}

void CefBrowserImpl::OnLoadEnd(blink::WebLocalFrame* frame) {
  if (is_swapped_out())
    return;

  CefRefPtr<CefApp> app = CefContentClient::Get()->application();
  if (app.get()) {
    CefRefPtr<CefRenderProcessHandler> handler =
        app->GetRenderProcessHandler();
    if (handler.get()) {
      CefRefPtr<CefLoadHandler> load_handler = handler->GetLoadHandler();
      if (load_handler.get()) {
        CefRefPtr<CefFrameImpl> cef_frame = GetWebFrameImpl(frame);
        int httpStatusCode =
            frame->DataSource()->GetResponse().HttpStatusCode();
        load_handler->OnLoadEnd(this, cef_frame.get(), httpStatusCode);
      }
    }
  }
}

void CefBrowserImpl::OnLoadError(blink::WebLocalFrame* frame,
                                 const blink::WebURLError& error) {
  if (is_swapped_out())
    return;

  CefRefPtr<CefApp> app = CefContentClient::Get()->application();
  if (app.get()) {
    CefRefPtr<CefRenderProcessHandler> handler =
        app->GetRenderProcessHandler();
    if (handler.get()) {
      CefRefPtr<CefLoadHandler> load_handler = handler->GetLoadHandler();
      if (load_handler.get()) {
        CefRefPtr<CefFrameImpl> cef_frame = GetWebFrameImpl(frame);
        const cef_errorcode_t errorCode =
            static_cast<cef_errorcode_t>(error.reason);
        const std::string& errorText = error.localized_description.Utf8();
        const GURL& failedUrl = error.unreachable_url;
        load_handler->OnLoadError(this, cef_frame.get(), errorCode, errorText,
                                  failedUrl.spec());
      }
    }
  }
}
