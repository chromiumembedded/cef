// Copyright 2020 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/browser_contents_delegate.h"

#include "libcef/browser/browser_host_base.h"
#include "libcef/browser/browser_platform_delegate.h"
#include "libcef/browser/browser_util.h"
#include "libcef/browser/native/cursor_util.h"
#include "libcef/common/frame_util.h"

#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/focused_node_details.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_observer.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/input/native_web_keyboard_event.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-blink.h"
#include "third_party/blink/public/mojom/widget/platform_widget.mojom-test-utils.h"

using content::KeyboardEventProcessingResult;

namespace {

class CefWidgetHostInterceptor
    : public blink::mojom::WidgetHostInterceptorForTesting,
      public content::RenderWidgetHostObserver {
 public:
  CefWidgetHostInterceptor(CefRefPtr<CefBrowser> browser,
                           content::RenderWidgetHost* render_widget_host)
      : browser_(browser),
        render_widget_host_(render_widget_host),
        impl_(static_cast<content::RenderWidgetHostImpl*>(render_widget_host)
                  ->widget_host_receiver_for_testing()
                  .SwapImplForTesting(this)) {
    render_widget_host_->AddObserver(this);
  }

  CefWidgetHostInterceptor(const CefWidgetHostInterceptor&) = delete;
  CefWidgetHostInterceptor& operator=(const CefWidgetHostInterceptor&) = delete;

  blink::mojom::WidgetHost* GetForwardingInterface() override { return impl_; }

  // WidgetHostInterceptorForTesting method:
  void SetCursor(const ui::Cursor& cursor) override {
    if (cursor_util::OnCursorChange(browser_, cursor)) {
      // Don't change the cursor.
      return;
    }

    GetForwardingInterface()->SetCursor(cursor);
  }

  // RenderWidgetHostObserver method:
  void RenderWidgetHostDestroyed(
      content::RenderWidgetHost* widget_host) override {
    widget_host->RemoveObserver(this);
    delete this;
  }

 private:
  CefRefPtr<CefBrowser> const browser_;
  content::RenderWidgetHost* const render_widget_host_;
  blink::mojom::WidgetHost* const impl_;
};

}  // namespace

CefBrowserContentsDelegate::CefBrowserContentsDelegate(
    scoped_refptr<CefBrowserInfo> browser_info)
    : browser_info_(browser_info) {
  DCHECK(browser_info_->browser());
}

void CefBrowserContentsDelegate::ObserveWebContents(
    content::WebContents* new_contents) {
  WebContentsObserver::Observe(new_contents);

  if (new_contents) {
    // Make sure MaybeCreateFrame is called at least one time.
    // Create the frame representation before OnAfterCreated is called for a new
    // browser.
    browser_info_->MaybeCreateFrame(new_contents->GetPrimaryMainFrame(),
                                    false /* is_guest_view */);

    // Make sure RenderWidgetCreated is called at least one time. This Observer
    // is registered too late to catch the initial creation.
    RenderWidgetCreated(new_contents->GetRenderViewHost()->GetWidget());
  }
}

void CefBrowserContentsDelegate::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void CefBrowserContentsDelegate::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

// |source| may be NULL for navigations in the current tab, or if the
// navigation originates from a guest view via MaybeAllowNavigation.
content::WebContents* CefBrowserContentsDelegate::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  bool cancel = false;

  if (auto c = client()) {
    if (auto handler = c->GetRequestHandler()) {
      // May return nullptr for omnibox navigations.
      auto frame = browser_info_->browser()->GetFrameForHost(
          content::RenderFrameHost::FromID(params.source_render_process_id,
                                           params.source_render_frame_id));
      if (!frame) {
        frame = browser()->GetMainFrame();
      }
      cancel = handler->OnOpenURLFromTab(
          browser(), frame, params.url.spec(),
          static_cast<cef_window_open_disposition_t>(params.disposition),
          params.user_gesture);
    }
  }

  // Returning nullptr will cancel the navigation.
  return cancel ? nullptr : web_contents();
}

void CefBrowserContentsDelegate::LoadingStateChanged(
    content::WebContents* source,
    bool should_show_loading_ui) {
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
      auto navigation_lock = browser_info_->CreateNavigationLock();
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
    const std::u16string& message,
    int32_t line_no,
    const std::u16string& source_id) {
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

void CefBrowserContentsDelegate::EnterFullscreenModeForTab(
    content::RenderFrameHost* requesting_frame,
    const blink::mojom::FullscreenOptions& options) {
  OnFullscreenModeChange(/*fullscreen=*/true);
}

void CefBrowserContentsDelegate::ExitFullscreenModeForTab(
    content::WebContents* web_contents) {
  OnFullscreenModeChange(/*fullscreen=*/false);
}

void CefBrowserContentsDelegate::CanDownload(
    const GURL& url,
    const std::string& request_method,
    base::OnceCallback<void(bool)> callback) {
  bool allow = true;

  if (auto delegate = platform_delegate()) {
    if (auto c = client()) {
      if (auto handler = c->GetDownloadHandler()) {
        allow = handler->CanDownload(browser(), url.spec(), request_method);
      }
    }
  }

  std::move(callback).Run(allow);
}

KeyboardEventProcessingResult
CefBrowserContentsDelegate::PreHandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  if (auto delegate = platform_delegate()) {
    if (auto c = client()) {
      if (auto handler = c->GetKeyboardHandler()) {
        CefKeyEvent cef_event;
        if (browser_util::GetCefKeyEvent(event, cef_event)) {
          cef_event.focus_on_editable_field = focus_on_editable_field_;

          auto event_handle = delegate->GetEventHandle(event);
          bool is_keyboard_shortcut = false;
          bool result = handler->OnPreKeyEvent(
              browser(), cef_event, event_handle, &is_keyboard_shortcut);
          if (result) {
            return KeyboardEventProcessingResult::HANDLED;
          } else if (is_keyboard_shortcut) {
            return KeyboardEventProcessingResult::NOT_HANDLED_IS_SHORTCUT;
          }
        }
      }
    }
  }

  return KeyboardEventProcessingResult::NOT_HANDLED;
}

bool CefBrowserContentsDelegate::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  // Check to see if event should be ignored.
  if (event.skip_if_unhandled) {
    return false;
  }

  if (auto delegate = platform_delegate()) {
    if (auto c = client()) {
      if (auto handler = c->GetKeyboardHandler()) {
        CefKeyEvent cef_event;
        if (browser_util::GetCefKeyEvent(event, cef_event)) {
          cef_event.focus_on_editable_field = focus_on_editable_field_;

          auto event_handle = delegate->GetEventHandle(event);
          if (handler->OnKeyEvent(browser(), cef_event, event_handle)) {
            return true;
          }
        }
      }
    }
  }

  return false;
}

void CefBrowserContentsDelegate::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  browser_info_->MaybeCreateFrame(render_frame_host, false /* is_guest_view */);
  if (render_frame_host->GetParent() == nullptr) {
    auto render_view_host = render_frame_host->GetRenderViewHost();
    auto base_background_color = platform_delegate()->GetBackgroundColor();
    if (browser_info_ && browser_info_->is_popup()) {
      // force reset page base background color because popup window won't get
      // the page base background from web_contents at the creation time
      web_contents()->SetPageBaseBackgroundColor(SkColor());
      web_contents()->SetPageBaseBackgroundColor(base_background_color);
    }
    if (render_view_host->GetWidget() &&
        render_view_host->GetWidget()->GetView()) {
      render_view_host->GetWidget()->GetView()->SetBackgroundColor(
          base_background_color);
    }

    platform_delegate()->RenderViewCreated(render_view_host);
  }
}

void CefBrowserContentsDelegate::RenderFrameHostChanged(
    content::RenderFrameHost* old_host,
    content::RenderFrameHost* new_host) {
  // Just in case RenderFrameCreated wasn't called for some reason.
  RenderFrameCreated(new_host);
}

void CefBrowserContentsDelegate::RenderFrameHostStateChanged(
    content::RenderFrameHost* host,
    content::RenderFrameHost::LifecycleState old_state,
    content::RenderFrameHost::LifecycleState new_state) {
  browser_info_->FrameHostStateChanged(host, old_state, new_state);
}

void CefBrowserContentsDelegate::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  browser_info_->RemoveFrame(render_frame_host);

  if (focused_frame_ && focused_frame_->IsSameFrame(render_frame_host)) {
    focused_frame_ = nullptr;
    OnStateChanged(State::kFocusedFrame);
  }
}

void CefBrowserContentsDelegate::RenderWidgetCreated(
    content::RenderWidgetHost* render_widget_host) {
  new CefWidgetHostInterceptor(browser(), render_widget_host);
}

void CefBrowserContentsDelegate::RenderViewReady() {
  platform_delegate()->RenderViewReady();

  if (auto c = client()) {
    if (auto handler = c->GetRequestHandler()) {
      handler->OnRenderViewReady(browser());
    }
  }
}

void CefBrowserContentsDelegate::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  cef_termination_status_t ts = TS_ABNORMAL_TERMINATION;
  if (status == base::TERMINATION_STATUS_PROCESS_WAS_KILLED) {
    ts = TS_PROCESS_WAS_KILLED;
  } else if (status == base::TERMINATION_STATUS_PROCESS_CRASHED) {
    ts = TS_PROCESS_CRASHED;
  } else if (status == base::TERMINATION_STATUS_OOM) {
    ts = TS_PROCESS_OOM;
  } else if (status != base::TERMINATION_STATUS_ABNORMAL_TERMINATION) {
    return;
  }

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
  if (!frame || frame->IsFocused()) {
    return;
  }

  CefRefPtr<CefFrameHostImpl> previous_frame = focused_frame_;
  if (frame->IsMain()) {
    focused_frame_ = nullptr;
  } else {
    focused_frame_ = frame;
  }

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

void CefBrowserContentsDelegate::PrimaryMainDocumentElementAvailable() {
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

  OnTitleChange(web_contents()->GetTitle());
}

void CefBrowserContentsDelegate::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  const net::Error error_code = navigation_handle->GetNetErrorCode();

  // Skip calls where the navigation has not yet committed and there is no
  // error code. For example, when creating a browser without loading a URL.
  if (!navigation_handle->HasCommitted() && error_code == net::OK) {
    return;
  }

  if (navigation_handle->IsInPrimaryMainFrame() &&
      navigation_handle->HasCommitted()) {
    // A primary main frame navigation has occured.
    has_document_ = false;
    OnStateChanged(State::kDocument);
  }

  const bool is_main_frame = navigation_handle->IsInMainFrame();
  const auto global_id = frame_util::GetGlobalId(navigation_handle);
  const GURL& url =
      (error_code == net::OK ? navigation_handle->GetURL() : GURL());

  auto browser_info = browser_info_;
  if (!browser_info->browser()) {
    // Ignore notifications when the browser is closing.
    return;
  }

  // May return NULL when starting a new navigation if the previous navigation
  // caused the renderer process to crash during load.
  CefRefPtr<CefFrameHostImpl> frame =
      browser_info->GetFrameForGlobalId(global_id);
  if (!frame) {
    if (is_main_frame) {
      frame = browser_info->GetMainFrame();
    } else {
      frame = browser_info->CreateTempSubFrame(frame_util::InvalidGlobalId());
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
      if (navigation_handle->IsServedFromBackForwardCache()) {
        // We won't get an OnLoadEnd notification from anywhere else.
        OnLoadEnd(frame.get(), navigation_handle->GetURL(), 0);
      }
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

void CefBrowserContentsDelegate::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  auto frame = browser_info_->GetFrameForHost(render_frame_host);
  frame->RefreshAttributes();

  int http_status_code = 0;
  if (auto response_head = render_frame_host->GetLastResponseHead()) {
    if (response_head->headers) {
      http_status_code = response_head->headers->response_code();
    }
  }

  OnLoadEnd(frame, validated_url, http_status_code);
}

void CefBrowserContentsDelegate::TitleWasSet(content::NavigationEntry* entry) {
  // |entry| may be NULL if a popup is created via window.open and never
  // navigated.
  if (entry) {
    OnTitleChange(entry->GetTitle());
  } else if (web_contents()) {
    OnTitleChange(web_contents()->GetTitle());
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

void CefBrowserContentsDelegate::OnFocusChangedInPage(
    content::FocusedNodeDetails* details) {
  focus_on_editable_field_ =
      details->focus_type != blink::mojom::blink::FocusType::kNone &&
      details->is_editable_node;
}

void CefBrowserContentsDelegate::WebContentsDestroyed() {
  auto wc = web_contents();
  ObserveWebContents(nullptr);
  for (auto& observer : observers_) {
    observer.OnWebContentsDestroyed(wc);
  }
}

bool CefBrowserContentsDelegate::OnSetFocus(cef_focus_source_t source) {
  // SetFocus() might be called while inside the OnSetFocus() callback. If
  // so, don't re-enter the callback.
  if (is_in_onsetfocus_) {
    return true;
  }

  if (auto c = client()) {
    if (auto handler = c->GetFocusHandler()) {
      is_in_onsetfocus_ = true;
      bool handled = handler->OnSetFocus(browser(), source);
      is_in_onsetfocus_ = false;

      return handled;
    }
  }

  return false;
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

CefBrowserPlatformDelegate* CefBrowserContentsDelegate::platform_delegate()
    const {
  auto browser = browser_info_->browser();
  if (browser) {
    return browser->platform_delegate();
  }
  return nullptr;
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
      auto navigation_lock = browser_info_->CreateNavigationLock();
      // On the handler that loading has started.
      handler->OnLoadStart(browser(), frame,
                           static_cast<cef_transition_type_t>(transition_type));
    }
  }
}

void CefBrowserContentsDelegate::OnLoadEnd(CefRefPtr<CefFrame> frame,
                                           const GURL& url,
                                           int http_status_code) {
  if (auto c = client()) {
    if (auto handler = c->GetLoadHandler()) {
      auto navigation_lock = browser_info_->CreateNavigationLock();
      handler->OnLoadEnd(browser(), frame, http_status_code);
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

void CefBrowserContentsDelegate::OnTitleChange(const std::u16string& title) {
  if (auto c = client()) {
    if (auto handler = c->GetDisplayHandler()) {
      handler->OnTitleChange(browser(), title);
    }
  }
}

void CefBrowserContentsDelegate::OnFullscreenModeChange(bool fullscreen) {
  if (fullscreen == is_fullscreen_) {
    return;
  }

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
