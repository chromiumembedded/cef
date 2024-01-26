// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/renderer/browser_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "libcef/common/app_manager.h"
#include "libcef/renderer/blink_glue.h"
#include "libcef/renderer/render_frame_util.h"
#include "libcef/renderer/render_manager.h"
#include "libcef/renderer/thread_util.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/renderer/render_frame.h"
#include "content/renderer/document_state.h"
#include "content/renderer/navigation_state.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_frame_content_dumper.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_security_policy.h"
#include "third_party/blink/public/web/web_view.h"

// CefBrowserImpl static methods.
// -----------------------------------------------------------------------------

// static
CefRefPtr<CefBrowserImpl> CefBrowserImpl::GetBrowserForView(
    blink::WebView* view) {
  return CefRenderManager::Get()->GetBrowserForView(view);
}

// static
CefRefPtr<CefBrowserImpl> CefBrowserImpl::GetBrowserForMainFrame(
    blink::WebFrame* frame) {
  return CefRenderManager::Get()->GetBrowserForMainFrame(frame);
}

// CefBrowser methods.
// -----------------------------------------------------------------------------

bool CefBrowserImpl::IsValid() {
  CEF_REQUIRE_RT_RETURN(false);
  return !!GetWebView();
}

CefRefPtr<CefBrowserHost> CefBrowserImpl::GetHost() {
  DCHECK(false) << "GetHost cannot be called from the render process";
  return nullptr;
}

bool CefBrowserImpl::CanGoBack() {
  CEF_REQUIRE_RT_RETURN(false);

  return blink_glue::CanGoBack(GetWebView());
}

void CefBrowserImpl::GoBack() {
  CEF_REQUIRE_RT_RETURN_VOID();

  blink_glue::GoBack(GetWebView());
}

bool CefBrowserImpl::CanGoForward() {
  CEF_REQUIRE_RT_RETURN(false);

  return blink_glue::CanGoForward(GetWebView());
}

void CefBrowserImpl::GoForward() {
  CEF_REQUIRE_RT_RETURN_VOID();

  blink_glue::GoForward(GetWebView());
}

bool CefBrowserImpl::IsLoading() {
  CEF_REQUIRE_RT_RETURN(false);

  if (GetWebView()) {
    blink::WebFrame* main_frame = GetWebView()->MainFrame();
    if (main_frame) {
      return main_frame->ToWebLocalFrame()->IsLoading();
    }
  }
  return false;
}

void CefBrowserImpl::Reload() {
  CEF_REQUIRE_RT_RETURN_VOID();

  if (GetWebView()) {
    blink::WebFrame* main_frame = GetWebView()->MainFrame();
    if (main_frame && main_frame->IsWebLocalFrame()) {
      main_frame->ToWebLocalFrame()->StartReload(
          blink::WebFrameLoadType::kReload);
    }
  }
}

void CefBrowserImpl::ReloadIgnoreCache() {
  CEF_REQUIRE_RT_RETURN_VOID();

  if (GetWebView()) {
    blink::WebFrame* main_frame = GetWebView()->MainFrame();
    if (main_frame && main_frame->IsWebLocalFrame()) {
      main_frame->ToWebLocalFrame()->StartReload(
          blink::WebFrameLoadType::kReloadBypassingCache);
    }
  }
}

void CefBrowserImpl::StopLoad() {
  CEF_REQUIRE_RT_RETURN_VOID();

  if (GetWebView()) {
    blink::WebFrame* main_frame = GetWebView()->MainFrame();
    if (main_frame && main_frame->IsWebLocalFrame()) {
      main_frame->ToWebLocalFrame()->DeprecatedStopLoading();
    }
  }
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

  if (GetWebView()) {
    blink::WebFrame* main_frame = GetWebView()->MainFrame();
    if (main_frame && main_frame->IsWebLocalFrame()) {
      return !main_frame->ToWebLocalFrame()->GetDocument().IsNull();
    }
  }
  return false;
}

CefRefPtr<CefFrame> CefBrowserImpl::GetMainFrame() {
  CEF_REQUIRE_RT_RETURN(nullptr);

  if (GetWebView()) {
    blink::WebFrame* main_frame = GetWebView()->MainFrame();
    if (main_frame && main_frame->IsWebLocalFrame()) {
      return GetWebFrameImpl(main_frame->ToWebLocalFrame()).get();
    }
  }
  return nullptr;
}

CefRefPtr<CefFrame> CefBrowserImpl::GetFocusedFrame() {
  CEF_REQUIRE_RT_RETURN(nullptr);

  if (GetWebView() && GetWebView()->FocusedFrame()) {
    return GetWebFrameImpl(GetWebView()->FocusedFrame()).get();
  }
  return nullptr;
}

CefRefPtr<CefFrame> CefBrowserImpl::GetFrameByIdentifier(
    const CefString& identifier) {
  CEF_REQUIRE_RT_RETURN(nullptr);

  return GetWebFrameImpl(identifier).get();
}

CefRefPtr<CefFrame> CefBrowserImpl::GetFrameByName(const CefString& name) {
  CEF_REQUIRE_RT_RETURN(nullptr);

  blink::WebView* web_view = GetWebView();
  if (web_view) {
    const blink::WebString& frame_name =
        blink::WebString::FromUTF16(name.ToString16());
    // Search by assigned frame name (Frame::name).
    blink::WebFrame* frame = web_view->MainFrame();
    if (frame && frame->IsWebLocalFrame()) {
      frame = frame->ToWebLocalFrame()->FindFrameByName(frame_name);
    }
    if (!frame) {
      // Search by unique frame name (Frame::uniqueName).
      const std::string& searchname = name;
      for (blink::WebFrame* cur_frame = web_view->MainFrame(); cur_frame;
           cur_frame = cur_frame->TraverseNext()) {
        if (cur_frame->IsWebLocalFrame() &&
            render_frame_util::GetName(cur_frame->ToWebLocalFrame()) ==
                searchname) {
          frame = cur_frame;
          break;
        }
      }
    }
    if (frame && frame->IsWebLocalFrame()) {
      return GetWebFrameImpl(frame->ToWebLocalFrame()).get();
    }
  }

  return nullptr;
}

size_t CefBrowserImpl::GetFrameCount() {
  CEF_REQUIRE_RT_RETURN(0);

  int count = 0;

  if (GetWebView()) {
    for (blink::WebFrame* frame = GetWebView()->MainFrame(); frame;
         frame = frame->TraverseNext()) {
      count++;
    }
  }

  return count;
}

void CefBrowserImpl::GetFrameIdentifiers(std::vector<CefString>& identifiers) {
  CEF_REQUIRE_RT_RETURN_VOID();

  if (identifiers.size() > 0) {
    identifiers.clear();
  }

  if (GetWebView()) {
    for (blink::WebFrame* frame = GetWebView()->MainFrame(); frame;
         frame = frame->TraverseNext()) {
      if (frame->IsWebLocalFrame()) {
        identifiers.push_back(
            render_frame_util::GetIdentifier(frame->ToWebLocalFrame()));
      }
    }
  }
}

void CefBrowserImpl::GetFrameNames(std::vector<CefString>& names) {
  CEF_REQUIRE_RT_RETURN_VOID();

  if (names.size() > 0) {
    names.clear();
  }

  if (GetWebView()) {
    for (blink::WebFrame* frame = GetWebView()->MainFrame(); frame;
         frame = frame->TraverseNext()) {
      if (frame->IsWebLocalFrame()) {
        names.push_back(render_frame_util::GetName(frame->ToWebLocalFrame()));
      }
    }
  }
}

// CefBrowserImpl public methods.
// -----------------------------------------------------------------------------

CefBrowserImpl::CefBrowserImpl(blink::WebView* web_view,
                               int browser_id,
                               bool is_popup,
                               bool is_windowless)
    : blink::WebViewObserver(web_view),
      browser_id_(browser_id),
      is_popup_(is_popup),
      is_windowless_(is_windowless) {}

CefBrowserImpl::~CefBrowserImpl() = default;

CefRefPtr<CefFrameImpl> CefBrowserImpl::GetWebFrameImpl(
    blink::WebLocalFrame* frame) {
  DCHECK(frame);
  const auto& frame_token = frame->GetLocalFrameToken();

  // Frames are re-used between page loads. Only add the frame to the map once.
  FrameMap::const_iterator it = frames_.find(frame_token);
  if (it != frames_.end()) {
    return it->second;
  }

  CefRefPtr<CefFrameImpl> framePtr(new CefFrameImpl(this, frame));
  frames_.insert(std::make_pair(frame_token, framePtr));

  return framePtr;
}

CefRefPtr<CefFrameImpl> CefBrowserImpl::GetWebFrameImpl(
    const std::string& identifier) {
  const auto& frame_token =
      render_frame_util::ParseFrameTokenFromIdentifier(identifier);
  if (!frame_token) {
    return nullptr;
  }

  // Check if we already know about the frame.
  FrameMap::const_iterator it = frames_.find(*frame_token);
  if (it != frames_.end()) {
    return it->second;
  }

  if (GetWebView()) {
    // Check if the frame exists but we don't know about it yet.
    if (auto* local_frame =
            blink::WebLocalFrame::FromFrameToken(*frame_token)) {
      return GetWebFrameImpl(local_frame);
    }
  }

  return nullptr;
}

// RenderViewObserver methods.
// -----------------------------------------------------------------------------

void CefBrowserImpl::OnDestruct() {
  // Notify that the browser window has been destroyed.
  CefRefPtr<CefApp> app = CefAppManager::Get()->GetApplication();
  if (app.get()) {
    CefRefPtr<CefRenderProcessHandler> handler = app->GetRenderProcessHandler();
    if (handler.get()) {
      handler->OnBrowserDestroyed(this);
    }
  }

  CefRenderManager::Get()->OnBrowserDestroyed(this);
}

void CefBrowserImpl::FrameDetached(blink::WebLocalFrame* frame) {
  frames_.erase(frame->GetLocalFrameToken());
}

void CefBrowserImpl::OnLoadingStateChange(bool isLoading) {
  CefRefPtr<CefApp> app = CefAppManager::Get()->GetApplication();
  if (app.get()) {
    CefRefPtr<CefRenderProcessHandler> handler = app->GetRenderProcessHandler();
    if (handler.get()) {
      CefRefPtr<CefLoadHandler> load_handler = handler->GetLoadHandler();
      if (load_handler.get()) {
        blink::WebView* web_view = GetWebView();
        const bool canGoBack = blink_glue::CanGoBack(web_view);
        const bool canGoForward = blink_glue::CanGoForward(web_view);

        // Don't call OnLoadingStateChange multiple times with the same status.
        // This can occur in cases where there are multiple highest-level
        // LocalFrames in-process for the same browser.
        if (last_loading_state_ &&
            last_loading_state_->IsMatch(isLoading, canGoBack, canGoForward)) {
          return;
        }

        if (was_in_bfcache_) {
          // Send the expected callbacks when exiting the BFCache.
          DCHECK(!isLoading);
          load_handler->OnLoadingStateChange(this, /*isLoading=*/true,
                                             canGoBack, canGoForward);

          auto main_frame = GetMainFrame();
          load_handler->OnLoadStart(this, main_frame, TT_EXPLICIT);
          load_handler->OnLoadEnd(this, main_frame, 0);

          was_in_bfcache_ = false;
        }

        load_handler->OnLoadingStateChange(this, isLoading, canGoBack,
                                           canGoForward);
        last_loading_state_ =
            std::make_unique<LoadingState>(isLoading, canGoBack, canGoForward);
      }
    }
  }
}

void CefBrowserImpl::OnEnterBFCache() {
  // Reset loading state so that notifications will be resent if/when exiting
  // BFCache.
  was_in_bfcache_ = true;
  last_loading_state_.reset();
}
