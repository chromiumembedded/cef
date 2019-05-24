// Copyright (c) 2017 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/net/net_util.h"

#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/browser_platform_delegate.h"
#include "libcef/browser/resource_context.h"
#include "libcef/common/net/scheme_registration.h"

#include "base/optional.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/resource_request_info.h"
#include "net/url_request/url_request.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace net_util {

namespace {

CefString SerializeRequestInitiator(
    base::Optional<url::Origin> request_initiator) {
  if (request_initiator.has_value())
    return request_initiator->Serialize();
  return "null";
}

CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(
    CefResourceContext* resource_context,
    int render_process_id,
    int render_frame_id,
    int frame_tree_node_id,
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefRequest> request,
    bool is_navigation,
    bool is_download,
    base::Optional<url::Origin> request_initiator) {
  CEF_REQUIRE_IOT();
  DCHECK(resource_context);
  DCHECK(request);

  CefRefPtr<CefResourceRequestHandler> resource_request_handler;

  const CefString& request_initiator_str =
      SerializeRequestInitiator(request_initiator);

  const bool is_custom_scheme =
      !GURL(request->GetURL().ToString()).SchemeIsHTTPOrHTTPS();

  // Not supported by the old network implementation, but keep the value
  // consistent with the NetworkService implementation.
  bool disable_default_handling = is_custom_scheme;

  // Give the browser handler a chance first.
  if (browser) {
    DCHECK(frame);

    CefRefPtr<CefClient> client = browser->GetHost()->GetClient();
    if (client) {
      CefRefPtr<CefRequestHandler> request_handler =
          client->GetRequestHandler();
      if (request_handler) {
        resource_request_handler = request_handler->GetResourceRequestHandler(
            browser, frame, request, is_navigation, is_download,
            request_initiator_str, disable_default_handling);
      }
    }
  }

  // Give the request context handler a chance.
  if (!resource_request_handler) {
    CefRefPtr<CefRequestContextHandler> request_context_handler =
        resource_context->GetHandler(render_process_id, render_frame_id,
                                     frame_tree_node_id, false);
    if (request_context_handler) {
      resource_request_handler =
          request_context_handler->GetResourceRequestHandler(
              browser, frame, request, is_navigation, is_download,
              request_initiator_str, disable_default_handling);
    }
  }

  return resource_request_handler;
}

void HandleExternalProtocolOnIOThread(CefResourceContext* resource_context,
                                      int render_process_id,
                                      CefRefPtr<CefBrowserHostImpl> browser,
                                      CefRefPtr<CefFrame> frame,
                                      CefRefPtr<CefRequestImpl> request) {
  CEF_REQUIRE_IOT();

  CefRefPtr<CefResourceRequestHandler> request_handler =
      GetResourceRequestHandler(
          resource_context, render_process_id, -1, -1, browser.get(), frame,
          request.get(), true /* is_navigation */, false /* is_download */,
          base::Optional<url::Origin>());
  if (!request_handler)
    return;

  bool allow_os_execution = false;
  request_handler->OnProtocolExecution(browser, frame, request.get(),
                                       allow_os_execution);
  if (allow_os_execution) {
    const GURL& url = GURL(request->GetURL().ToString());
    CefBrowserPlatformDelegate::HandleExternalProtocol(url);
  }
}

}  // namespace

bool IsInternalRequest(const net::URLRequest* request) {
  // With PlzNavigate we now receive blob URLs. Ignore these URLs.
  // See https://crbug.com/776884 for details.
  if (request->url().SchemeIs(url::kBlobScheme)) {
    return true;
  }

  return false;
}

CefRefPtr<CefBrowserHostImpl> GetBrowserForRequest(
    const net::URLRequest* request) {
  DCHECK(request);
  CEF_REQUIRE_IOT();

  // When navigating the main frame a new (pre-commit) URLRequest will be
  // created before the RenderFrameHost. Consequently we can't rely on
  // ResourceRequestInfo::GetRenderFrameForRequest returning a valid frame
  // ID. See https://crbug.com/776884 for background.
  int render_process_id = -1;
  int render_frame_id = MSG_ROUTING_NONE;
  if (content::ResourceRequestInfo::GetRenderFrameForRequest(
          request, &render_process_id, &render_frame_id) &&
      render_process_id >= 0 && render_frame_id >= 0) {
    return CefBrowserHostImpl::GetBrowserForFrameRoute(render_process_id,
                                                       render_frame_id);
  }

  content::ResourceRequestInfo* request_info =
      content::ResourceRequestInfo::ForRequest(request);
  if (request_info) {
    return CefBrowserHostImpl::GetBrowserForFrameTreeNode(
        request_info->GetFrameTreeNodeId());
  }

  return nullptr;
}

CefRefPtr<CefFrame> GetFrameForRequest(
    scoped_refptr<CefBrowserInfo> browser_info,
    const net::URLRequest* request) {
  CEF_REQUIRE_IOT();
  content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request);
  if (!info)
    return nullptr;

  // Try to locate the most reasonable match by ID.
  auto frame =
      browser_info->GetFrameForFrameTreeNode(info->GetFrameTreeNodeId());
  if (!frame) {
    frame = browser_info->GetFrameForRoute(info->GetRouteID(),
                                           info->GetRenderFrameID());
  }
  if (frame)
    return frame;

  // The IsMainFrame() flag isn't completely reliable, so do this after
  // searching by ID.
  if (info->IsMainFrame())
    return browser_info->GetMainFrame();

  // Create a temporary frame object for requests referencing sub-frames that
  // don't yet exist. Use the main frame as the parent because we don't know
  // the real parent.
  return browser_info->CreateTempSubFrame(CefFrameHostImpl::kInvalidFrameId);
}

CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(
    const net::URLRequest* request,
    CefRefPtr<CefRequestImpl>& cef_request,
    CefRefPtr<CefBrowser>& cef_browser,
    CefRefPtr<CefFrame>& cef_frame) {
  CEF_REQUIRE_IOT();
  content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request);
  if (!info)
    return nullptr;

  // Initiator will be non-null for subresource loads.
  const bool is_navigation =
      ui::PageTransitionIsNewNavigation(info->GetPageTransition()) &&
      !request->initiator().has_value();
  const bool is_download = info->IsDownload();
  const CefString& request_initiator =
      SerializeRequestInitiator(request->initiator());

  const bool is_custom_scheme =
      !scheme::IsInternalHandledScheme(request->url().scheme());

  // Not supported by the old network implementation, but keep the value
  // consistent with the NetworkService implementation.
  bool disable_default_handling = is_custom_scheme;

  CefRefPtr<CefResourceRequestHandler> resource_request_handler;

  CefRefPtr<CefBrowserHostImpl> browser = GetBrowserForRequest(request);
  CefRefPtr<CefFrame> frame;
  CefRefPtr<CefRequestImpl> requestPtr;

  // Give the browser handler a chance first.
  if (browser) {
    // A frame should always exist, or be created.
    frame = GetFrameForRequest(browser->browser_info(), request);
    DCHECK(frame);

    CefRefPtr<CefClient> client = browser->GetClient();
    if (client) {
      CefRefPtr<CefRequestHandler> request_handler =
          client->GetRequestHandler();
      if (request_handler) {
        requestPtr = new CefRequestImpl();
        requestPtr->Set(request);
        requestPtr->SetReadOnly(true);

        resource_request_handler = request_handler->GetResourceRequestHandler(
            browser.get(), frame, requestPtr.get(), is_navigation, is_download,
            request_initiator, disable_default_handling);
      }
    }
  }

  // Give the request context handler a chance.
  if (!resource_request_handler) {
    CefResourceContext* resource_context =
        static_cast<CefResourceContext*>(info->GetContext());
    if (!resource_context)
      return nullptr;

    const int render_process_id = info->GetChildID();
    const int render_frame_id = info->GetRenderFrameID();
    const int frame_tree_node_id = info->GetFrameTreeNodeId();

    CefRefPtr<CefRequestContextHandler> request_context_handler =
        resource_context->GetHandler(render_process_id, render_frame_id,
                                     frame_tree_node_id, false);
    if (request_context_handler) {
      if (!requestPtr) {
        requestPtr = new CefRequestImpl();
        requestPtr->Set(request);
        requestPtr->SetReadOnly(true);
      }

      resource_request_handler =
          request_context_handler->GetResourceRequestHandler(
              browser.get(), frame, requestPtr.get(), is_navigation,
              is_download, request_initiator, disable_default_handling);
    }
  }

  if (resource_request_handler) {
    // Success! Return all the objects that were discovered/created.
    cef_request = requestPtr;
    cef_browser = browser.get();
    cef_frame = frame;
  }
  return resource_request_handler;
}

void HandleExternalProtocol(
    CefRefPtr<CefRequestImpl> request,
    const content::ResourceRequestInfo::WebContentsGetter&
        web_contents_getter) {
  DCHECK(request);
  DCHECK(request->IsReadOnly());

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(HandleExternalProtocol, request,
                                          web_contents_getter));
    return;
  }

  content::WebContents* web_contents = web_contents_getter.Run();
  if (!web_contents)
    return;

  CefRefPtr<CefBrowserHostImpl> browser =
      CefBrowserHostImpl::GetBrowserForContents(web_contents);
  if (!browser)
    return;

  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  DCHECK(browser_context);

  CefResourceContext* resource_context =
      static_cast<CefResourceContext*>(browser_context->GetResourceContext());
  DCHECK(resource_context);

  const int render_process_id =
      web_contents->GetRenderViewHost()->GetProcess()->GetID();

  CEF_POST_TASK(
      CEF_IOT, base::Bind(HandleExternalProtocolOnIOThread,
                          base::Unretained(resource_context), render_process_id,
                          browser, browser->GetMainFrame(), request));
}

}  // namespace net_util
