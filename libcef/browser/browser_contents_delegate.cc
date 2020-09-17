// Copyright 2020 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/browser_contents_delegate.h"

#include "libcef/browser/browser_host_base.h"

#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_view_host.h"

CefBrowserContentsDelegate::CefBrowserContentsDelegate(
    scoped_refptr<CefBrowserInfo> browser_info)
    : browser_info_(browser_info) {
  DCHECK(browser_info_->browser());
}

void CefBrowserContentsDelegate::ObserveWebContents(
    content::WebContents* new_contents) {
  Observe(new_contents);

  if (new_contents) {
    // Create the frame representation before OnAfterCreated is called for a new
    // browser. Additionally, RenderFrameCreated is otherwise not called at all
    // for new popup browsers.
    RenderFrameCreated(new_contents->GetMainFrame());
  }
}

void CefBrowserContentsDelegate::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void CefBrowserContentsDelegate::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void CefBrowserContentsDelegate::LoadingStateChanged(
    content::WebContents* source,
    bool to_different_document) {
  const int current_index =
      source->GetController().GetLastCommittedEntryIndex();
  const int max_index = source->GetController().GetEntryCount() - 1;

  const bool is_loading = source->IsLoading();
  const bool can_go_back = (current_index > 0);
  const bool can_go_forward = (current_index < max_index);

  // This method may be called multiple times in a row with |is_loading|
  // true as a result of https://crrev.com/5e750ad0. Ignore the 2nd+ times.
  if (is_loading_ == is_loading && can_go_back_ == can_go_back &&
      can_go_forward_ == can_go_forward) {
    return;
  }

  is_loading_ = is_loading;
  can_go_back_ = can_go_back;
  can_go_forward_ = can_go_forward;
  OnStateChanged(State::kNavigation);

  if (auto c = client()) {
    if (auto handler = c->GetLoadHandler()) {
      handler->OnLoadingStateChange(browser(), is_loading, can_go_back,
                                    can_go_forward);
    }
  }
}

void CefBrowserContentsDelegate::UpdateTargetURL(content::WebContents* source,
                                                 const GURL& url) {
  if (auto c = client()) {
    if (auto handler = c->GetDisplayHandler()) {
      handler->OnStatusMessage(browser(), url.spec());
    }
  }
}

bool CefBrowserContentsDelegate::DidAddMessageToConsole(
    content::WebContents* source,
    blink::mojom::ConsoleMessageLevel log_level,
    const base::string16& message,
    int32_t line_no,
    const base::string16& source_id) {
  if (auto c = client()) {
    if (auto handler = c->GetDisplayHandler()) {
      // Use LOGSEVERITY_DEBUG for unrecognized |level| values.
      cef_log_severity_t cef_level = LOGSEVERITY_DEBUG;
      switch (log_level) {
        case blink::mojom::ConsoleMessageLevel::kVerbose:
          cef_level = LOGSEVERITY_DEBUG;
          break;
        case blink::mojom::ConsoleMessageLevel::kInfo:
          cef_level = LOGSEVERITY_INFO;
          break;
        case blink::mojom::ConsoleMessageLevel::kWarning:
          cef_level = LOGSEVERITY_WARNING;
          break;
        case blink::mojom::ConsoleMessageLevel::kError:
          cef_level = LOGSEVERITY_ERROR;
          break;
      }

      return handler->OnConsoleMessage(browser(), cef_level, message, source_id,
                                       line_no);
    }
  }

  return false;
}

void CefBrowserContentsDelegate::DidNavigateMainFramePostCommit(
    content::WebContents* web_contents) {
  has_document_ = false;
  OnStateChanged(State::kDocument);
}

void CefBrowserContentsDelegate::EnterFullscreenModeForTab(
    content::RenderFrameHost* requesting_frame,
    const blink::mojom::FullscreenOptions& options) {
  OnFullscreenModeChange(/*fullscreen=*/true);
}

void CefBrowserContentsDelegate::ExitFullscreenModeForTab(
    content::WebContents* web_contents) {
  OnFullscreenModeChange(/*fullscreen=*/false);
}

void CefBrowserContentsDelegate::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  browser_info_->MaybeCreateFrame(render_frame_host, false /* is_guest_view */);
}

void CefBrowserContentsDelegate::RenderFrameHostChanged(
    content::RenderFrameHost* old_host,
    content::RenderFrameHost* new_host) {
  // Just in case RenderFrameCreated wasn't called for some reason.
  RenderFrameCreated(new_host);
}

void CefBrowserContentsDelegate::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  const auto frame_id = CefFrameHostImpl::MakeFrameId(render_frame_host);
  browser_info_->RemoveFrame(render_frame_host);

  if (focused_frame_ && focused_frame_->GetIdentifier() == frame_id) {
    focused_frame_ = nullptr;
    OnStateChanged(State::kFocusedFrame);
  }
}

void CefBrowserContentsDelegate::RenderViewReady() {
  if (auto c = client()) {
    if (auto handler = c->GetRequestHandler()) {
      handler->OnRenderViewReady(browser());
    }
  }
}

void CefBrowserContentsDelegate::RenderProcessGone(
    base::TerminationStatus status) {
  cef_termination_status_t ts = TS_ABNORMAL_TERMINATION;
  if (status == base::TERMINATION_STATUS_PROCESS_WAS_KILLED)
    ts = TS_PROCESS_WAS_KILLED;
  else if (status == base::TERMINATION_STATUS_PROCESS_CRASHED)
    ts = TS_PROCESS_CRASHED;
  else if (status == base::TERMINATION_STATUS_OOM)
    ts = TS_PROCESS_OOM;
  else if (status != base::TERMINATION_STATUS_ABNORMAL_TERMINATION)
    return;

  if (auto c = client()) {
    if (auto handler = c->GetRequestHandler()) {
      auto navigation_lock = browser_info_->CreateNavigationLock();
      handler->OnRenderProcessTerminated(browser(), ts);
    }
  }
}

void CefBrowserContentsDelegate::OnFrameFocused(
    content::RenderFrameHost* render_frame_host) {
  CefRefPtr<CefFrameHostImpl> frame = static_cast<CefFrameHostImpl*>(
      browser_info_->GetFrameForHost(render_frame_host).get());
  if (!frame || frame->IsFocused())
    return;

  CefRefPtr<CefFrameHostImpl> previous_frame = focused_frame_;
  if (frame->IsMain())
    focused_frame_ = nullptr;
  else
    focused_frame_ = frame;

  if (!previous_frame) {
    // The main frame is focused by default.
    previous_frame = browser_info_->GetMainFrame();
  }

  if (previous_frame->GetIdentifier() != frame->GetIdentifier()) {
    previous_frame->SetFocused(false);
    frame->SetFocused(true);
  }

  OnStateChanged(State::kFocusedFrame);
}

void CefBrowserContentsDelegate::DocumentAvailableInMainFrame() {
  has_document_ = true;
  OnStateChanged(State::kDocument);

  if (auto c = client()) {
    if (auto handler = c->GetRequestHandler()) {
      handler->OnDocumentAvailableInMainFrame(browser());
    }
  }
}

void CefBrowserContentsDelegate::LoadProgressChanged(double progress) {
  if (auto c = client()) {
    if (auto handler = c->GetDisplayHandler()) {
      handler->OnLoadingProgressChange(browser(), progress);
    }
  }
}

void CefBrowserContentsDelegate::DidStopLoading() {
  // Notify all renderers that loading has stopped. We used to use
  // RenderFrameObserver::DidStopLoading in the renderer process but that was
  // removed in https://crrev.com/3e37dd0ead. However, that callback wasn't
  // necessarily accurate because it wasn't called in all of the cases where
  // RenderFrameImpl sends the FrameHostMsg_DidStopLoading message. This adds
  // an additional round trip but should provide the same or improved
  // functionality.
  for (const auto& frame : browser_info_->GetAllFrames()) {
    frame->MaybeSendDidStopLoading();
  }
}

void CefBrowserContentsDelegate::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  const net::Error error_code = navigation_handle->GetNetErrorCode();

  // Skip calls where the navigation has not yet committed and there is no
  // error code. For example, when creating a browser without loading a URL.
  if (!navigation_handle->HasCommitted() && error_code == net::OK)
    return;

  const bool is_main_frame = navigation_handle->IsInMainFrame();
  const GURL& url =
      (error_code == net::OK ? navigation_handle->GetURL() : GURL());

  auto browser_info = browser_info_;

  // May return NULL when starting a new navigation if the previous navigation
  // caused the renderer process to crash during load.
  CefRefPtr<CefFrameHostImpl> frame = browser_info->GetFrameForFrameTreeNode(
      navigation_handle->GetFrameTreeNodeId());
  if (!frame) {
    if (is_main_frame) {
      frame = browser_info->GetMainFrame();
    } else {
      frame =
          browser_info->CreateTempSubFrame(CefFrameHostImpl::kInvalidFrameId);
    }
  }
  frame->RefreshAttributes();

  if (error_code == net::OK) {
    // The navigation has been committed and there is no error.
    DCHECK(navigation_handle->HasCommitted());

    // Don't call OnLoadStart for same page navigations (fragments,
    // history state).
    if (!navigation_handle->IsSameDocument()) {
      OnLoadStart(frame.get(), navigation_handle->GetPageTransition());
    }

    if (is_main_frame) {
      OnAddressChange(url);
    }
  } else {
    // The navigation failed with an error. This may happen before commit
    // (e.g. network error) or after commit (e.g. response filter error).
    // If the error happened before commit then this call will originate from
    // RenderFrameHostImpl::OnDidFailProvisionalLoadWithError.
    // OnLoadStart/OnLoadEnd will not be called.
    OnLoadError(frame.get(), navigation_handle->GetURL(), error_code);
  }
}

void CefBrowserContentsDelegate::DidFailLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code) {
  // The navigation failed after commit. OnLoadStart was called so we also
  // call OnLoadEnd.
  auto frame = browser_info_->GetFrameForHost(render_frame_host);
  frame->RefreshAttributes();
  OnLoadError(frame, validated_url, error_code);
  OnLoadEnd(frame, validated_url, error_code);
}

bool CefBrowserContentsDelegate::OnMessageReceived(
    const IPC::Message& message,
    content::RenderFrameHost* render_frame_host) {
  // Messages may arrive after a frame is detached. Ignore those messages.
  auto frame = browser_info_->GetFrameForHost(render_frame_host);
  if (frame) {
    return static_cast<CefFrameHostImpl*>(frame.get())
        ->OnMessageReceived(message);
  }
  return false;
}

void CefBrowserContentsDelegate::TitleWasSet(content::NavigationEntry* entry) {
  // |entry| may be NULL if a popup is created via window.open and never
  // navigated.
  if (entry)
    OnTitleChange(entry->GetTitle());
  else if (web_contents())
    OnTitleChange(web_contents()->GetTitle());
}

void CefBrowserContentsDelegate::PluginCrashed(
    const base::FilePath& plugin_path,
    base::ProcessId plugin_pid) {
  if (auto c = client()) {
    if (auto handler = c->GetRequestHandler()) {
      handler->OnPluginCrashed(browser(), plugin_path.value());
    }
  }
}

void CefBrowserContentsDelegate::DidUpdateFaviconURL(
    content::RenderFrameHost* render_frame_host,
    const std::vector<blink::mojom::FaviconURLPtr>& candidates) {
  if (auto c = client()) {
    if (auto handler = c->GetDisplayHandler()) {
      std::vector<CefString> icon_urls;
      for (const auto& icon : candidates) {
        if (icon->icon_type == blink::mojom::FaviconIconType::kFavicon) {
          icon_urls.push_back(icon->icon_url.spec());
        }
      }
      if (!icon_urls.empty()) {
        handler->OnFaviconURLChange(browser(), icon_urls);
      }
    }
  }
}

void CefBrowserContentsDelegate::OnWebContentsFocused(
    content::RenderWidgetHost* render_widget_host) {
  if (auto c = client()) {
    if (auto handler = c->GetFocusHandler()) {
      handler->OnGotFocus(browser());
    }
  }
}

void CefBrowserContentsDelegate::WebContentsDestroyed() {
  ObserveWebContents(nullptr);
  for (auto& observer : observers_) {
    observer.OnWebContentsDestroyed();
  }
}

void CefBrowserContentsDelegate::OnAddressChange(const GURL& url) {
  if (auto c = client()) {
    if (auto handler = c->GetDisplayHandler()) {
      // On the handler of an address change.
      handler->OnAddressChange(browser(), browser_info_->GetMainFrame(),
                               url.spec());
    }
  }
}

void CefBrowserContentsDelegate::OnLoadStart(
    CefRefPtr<CefFrame> frame,
    ui::PageTransition transition_type) {
  if (auto c = client()) {
    if (auto handler = c->GetLoadHandler()) {
      // On the handler that loading has started.
      handler->OnLoadStart(browser(), frame,
                           static_cast<cef_transition_type_t>(transition_type));
    }
  }
}

void CefBrowserContentsDelegate::OnLoadError(CefRefPtr<CefFrame> frame,
                                             const GURL& url,
                                             int error_code) {
  if (auto c = client()) {
    if (auto handler = c->GetLoadHandler()) {
      auto navigation_lock = browser_info_->CreateNavigationLock();
      // On the handler that loading has failed.
      handler->OnLoadError(browser(), frame,
                           static_cast<cef_errorcode_t>(error_code),
                           net::ErrorToShortString(error_code), url.spec());
    }
  }
}

void CefBrowserContentsDelegate::OnLoadEnd(CefRefPtr<CefFrame> frame,
                                           const GURL& url,
                                           int http_status_code) {
  if (auto c = client()) {
    if (auto handler = c->GetLoadHandler()) {
      handler->OnLoadEnd(browser(), frame, http_status_code);
    }
  }
}

void CefBrowserContentsDelegate::OnTitleChange(const base::string16& title) {
  if (auto c = client()) {
    if (auto handler = c->GetDisplayHandler()) {
      handler->OnTitleChange(browser(), title);
    }
  }
}

CefRefPtr<CefClient> CefBrowserContentsDelegate::client() const {
  if (auto b = browser()) {
    return b->GetHost()->GetClient();
  }
  return nullptr;
}

CefRefPtr<CefBrowser> CefBrowserContentsDelegate::browser() const {
  return browser_info_->browser();
}

void CefBrowserContentsDelegate::OnFullscreenModeChange(bool fullscreen) {
  if (fullscreen == is_fullscreen_)
    return;

  is_fullscreen_ = fullscreen;
  OnStateChanged(State::kFullscreen);

  if (auto c = client()) {
    if (auto handler = c->GetDisplayHandler()) {
      handler->OnFullscreenModeChange(browser(), fullscreen);
    }
  }
}

void CefBrowserContentsDelegate::OnStateChanged(State state_changed) {
  for (auto& observer : observers_) {
    observer.OnStateChanged(state_changed);
  }
}
