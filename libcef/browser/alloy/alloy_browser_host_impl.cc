// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/browser/alloy/alloy_browser_host_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "cef/libcef/browser/alloy/browser_platform_delegate_alloy.h"
#include "cef/libcef/browser/audio_capturer.h"
#include "cef/libcef/browser/browser_context.h"
#include "cef/libcef/browser/browser_guest_util.h"
#include "cef/libcef/browser/browser_info.h"
#include "cef/libcef/browser/browser_info_manager.h"
#include "cef/libcef/browser/browser_platform_delegate.h"
#include "cef/libcef/browser/context.h"
#include "cef/libcef/browser/hang_monitor.h"
#include "cef/libcef/browser/media_access_query.h"
#include "cef/libcef/browser/osr/osr_util.h"
#include "cef/libcef/browser/request_context_impl.h"
#include "cef/libcef/browser/thread_util.h"
#include "cef/libcef/common/cef_switches.h"
#include "cef/libcef/common/drag_data_impl.h"
#include "cef/libcef/common/frame_util.h"
#include "cef/libcef/common/net/url_util.h"
#include "cef/libcef/common/request_impl.h"
#include "cef/libcef/common/values_impl.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/common/webui_url_constants.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/printing/browser/print_composite_client.h"
#include "components/zoom/page_zoom.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"
#include "net/base/net_errors.h"
#include "ui/events/base_event_utils.h"

using content::KeyboardEventProcessingResult;

namespace {

static constexpr base::TimeDelta kRecentlyAudibleTimeout = base::Seconds(2);

// List of WebUI hosts that have been tested to work in Alloy-style browsers.
// Do not add new hosts to this list without also manually testing all related
// functionality in CEF.
const char* kAllowedWebUIHosts[] = {
    chrome::kChromeUIAccessibilityHost,
    content::kChromeUIBlobInternalsHost,
    chrome::kChromeUIChromeURLsHost,
    chrome::kChromeUICreditsHost,
    content::kChromeUIGpuHost,
    content::kChromeUIHistogramHost,
    content::kChromeUIIndexedDBInternalsHost,
    chrome::kChromeUILicenseHost,
    content::kChromeUIMediaInternalsHost,
    chrome::kChromeUINetExportHost,
    chrome::kChromeUINetInternalsHost,
    content::kChromeUINetworkErrorHost,
    content::kChromeUINetworkErrorsListingHost,
    chrome::kChromeUIPrintHost,
    content::kChromeUIProcessInternalsHost,
    content::kChromeUIResourcesHost,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
    chrome::kChromeUISandboxHost,
#endif
    content::kChromeUIServiceWorkerInternalsHost,
    chrome::kChromeUISystemInfoHost,
    chrome::kChromeUITermsHost,
    chrome::kChromeUIThemeHost,
    content::kChromeUITracingHost,
    chrome::kChromeUIVersionHost,
    content::kChromeUIWebRTCInternalsHost,
};

bool IsAllowedWebUIHost(const std::string_view& host) {
  for (auto& allowed_host : kAllowedWebUIHosts) {
    if (base::EqualsCaseInsensitiveASCII(allowed_host, host)) {
      return true;
    }
  }
  return false;
}

}  // namespace

// AlloyBrowserHostImpl static methods.
// -----------------------------------------------------------------------------

// static
CefRefPtr<AlloyBrowserHostImpl> AlloyBrowserHostImpl::Create(
    CefBrowserCreateParams& create_params) {
  std::unique_ptr<CefBrowserPlatformDelegate> platform_delegate =
      CefBrowserPlatformDelegate::Create(create_params);
  CHECK(platform_delegate);

  // Expect runtime style to match.
  CHECK(platform_delegate->IsAlloyStyle());

  scoped_refptr<CefBrowserInfo> info =
      CefBrowserInfoManager::GetInstance()->CreateBrowserInfo(
          /*is_devtools_popup=*/false, platform_delegate->IsWindowless(),
          platform_delegate->IsPrintPreviewSupported(),
          create_params.extra_info);

  bool own_web_contents = false;

  // This call may modify |create_params|.
  auto web_contents =
      platform_delegate->CreateWebContents(create_params, own_web_contents);

  auto request_context_impl =
      static_cast<CefRequestContextImpl*>(create_params.request_context.get());

  CefRefPtr<AlloyBrowserHostImpl> browser =
      CreateInternal(create_params.settings, create_params.client, web_contents,
                     own_web_contents, info,
                     /*opener=*/nullptr, /*is_devtools_popup=*/false,
                     request_context_impl, std::move(platform_delegate));
  if (!browser) {
    return nullptr;
  }

  GURL url = url_util::MakeGURL(create_params.url, /*fixup=*/true);

  if (!url.is_empty()) {
    content::OpenURLParams params(url, content::Referrer(),
                                  WindowOpenDisposition::CURRENT_TAB,
                                  CefFrameHostImpl::kPageTransitionExplicit,
                                  /*is_renderer_initiated=*/false);
    browser->LoadMainFrameURL(params);
  }

  return browser.get();
}

// static
CefRefPtr<AlloyBrowserHostImpl> AlloyBrowserHostImpl::CreateInternal(
    const CefBrowserSettings& settings,
    CefRefPtr<CefClient> client,
    content::WebContents* web_contents,
    bool own_web_contents,
    scoped_refptr<CefBrowserInfo> browser_info,
    CefRefPtr<AlloyBrowserHostImpl> opener,
    bool is_devtools_popup,
    CefRefPtr<CefRequestContextImpl> request_context,
    std::unique_ptr<CefBrowserPlatformDelegate> platform_delegate) {
  CEF_REQUIRE_UIT();
  DCHECK(web_contents);
  DCHECK(browser_info);
  DCHECK(request_context);
  DCHECK(platform_delegate);

  // If |opener| is non-NULL it must be a popup window.
  DCHECK(!opener.get() || browser_info->is_popup());

  if (opener) {
    if (!opener->platform_delegate_) {
      // The opener window is being destroyed. Cancel the popup.
      if (own_web_contents) {
        delete web_contents;
      }
      return nullptr;
    }

    // Give the opener browser's platform delegate an opportunity to modify the
    // new browser's platform delegate.
    opener->platform_delegate_->PopupWebContentsCreated(
        settings, client, web_contents, platform_delegate.get(),
        is_devtools_popup);
  }

  // Take ownership of |web_contents| if |own_web_contents| is true.
  platform_delegate->WebContentsCreated(web_contents, own_web_contents);

  CefRefPtr<AlloyBrowserHostImpl> browser = new AlloyBrowserHostImpl(
      settings, client, web_contents, browser_info, opener, request_context,
      std::move(platform_delegate));
  browser->InitializeBrowser();

  if (!browser->CreateHostWindow()) {
    return nullptr;
  }

  // Notify that the browser has been created. These must be delivered in the
  // expected order.

  if (opener && opener->platform_delegate_) {
    // 1. Notify the opener browser's platform delegate. With Views this will
    // result in a call to CefBrowserViewDelegate::OnPopupBrowserViewCreated().
    // Do this first for consistency with Chrome style.
    opener->platform_delegate_->PopupBrowserCreated(
        browser->platform_delegate(), browser.get(), is_devtools_popup);
  }

  // 2. Notify the browser's LifeSpanHandler. This must always be the first
  // notification for the browser. Block navigation to avoid issues with focus
  // changes being sent to an unbound interface.
  {
    auto navigation_lock = browser_info->CreateNavigationLock();
    browser->OnAfterCreated();
  }

  // 3. Notify the platform delegate. With Views this will result in a call to
  // CefBrowserViewDelegate::OnBrowserCreated().
  browser->platform_delegate_->NotifyBrowserCreated();

  return browser;
}

// static
CefRefPtr<AlloyBrowserHostImpl> AlloyBrowserHostImpl::FromBaseChecked(
    CefRefPtr<CefBrowserHostBase> host_base) {
  if (!host_base) {
    return nullptr;
  }
  CHECK(host_base->IsAlloyStyle());
  return static_cast<AlloyBrowserHostImpl*>(host_base.get());
}

// static
CefRefPtr<AlloyBrowserHostImpl> AlloyBrowserHostImpl::GetBrowserForHost(
    const content::RenderViewHost* host) {
  return FromBaseChecked(CefBrowserHostBase::GetBrowserForHost(host));
}

// static
CefRefPtr<AlloyBrowserHostImpl> AlloyBrowserHostImpl::GetBrowserForHost(
    const content::RenderFrameHost* host) {
  return FromBaseChecked(CefBrowserHostBase::GetBrowserForHost(host));
}

// static
CefRefPtr<AlloyBrowserHostImpl> AlloyBrowserHostImpl::GetBrowserForContents(
    const content::WebContents* contents) {
  return FromBaseChecked(CefBrowserHostBase::GetBrowserForContents(contents));
}

// static
CefRefPtr<AlloyBrowserHostImpl> AlloyBrowserHostImpl::GetBrowserForGlobalId(
    const content::GlobalRenderFrameHostId& global_id) {
  return FromBaseChecked(CefBrowserHostBase::GetBrowserForGlobalId(global_id));
}

// AlloyBrowserHostImpl methods.
// -----------------------------------------------------------------------------

AlloyBrowserHostImpl::~AlloyBrowserHostImpl() = default;

void AlloyBrowserHostImpl::CloseBrowser(bool force_close) {
  if (CEF_CURRENTLY_ON_UIT()) {
    // Exit early if a close attempt is already pending and this method is
    // called again from somewhere other than WindowDestroyed().
    if (destruction_state_ >= DESTRUCTION_STATE_PENDING &&
        (IsWindowless() || !window_destroyed_)) {
      if (force_close && destruction_state_ == DESTRUCTION_STATE_PENDING) {
        // Upgrade the destruction state.
        destruction_state_ = DESTRUCTION_STATE_ACCEPTED;
      }
      return;
    }

    if (destruction_state_ < DESTRUCTION_STATE_ACCEPTED) {
      destruction_state_ = (force_close ? DESTRUCTION_STATE_ACCEPTED
                                        : DESTRUCTION_STATE_PENDING);
    }

    content::WebContents* contents = web_contents();
    if (contents && contents->NeedToFireBeforeUnloadOrUnloadEvents()) {
      // Will result in a call to BeforeUnloadFired() and, if the close isn't
      // canceled, CloseContents().
      contents->DispatchBeforeUnload(false /* auto_cancel */);
    } else {
      CloseContents(contents);
    }
  } else {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&AlloyBrowserHostImpl::CloseBrowser,
                                          this, force_close));
  }
}

bool AlloyBrowserHostImpl::TryCloseBrowser() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    DCHECK(false) << "called on invalid thread";
    return false;
  }

  // Protect against multiple requests to close while the close is pending.
  if (destruction_state_ <= DESTRUCTION_STATE_PENDING) {
    if (destruction_state_ == DESTRUCTION_STATE_NONE) {
      // Request that the browser close.
      CloseBrowser(false);
    }

    // Cancel the close.
    return false;
  }

  // Allow the close.
  return true;
}

CefWindowHandle AlloyBrowserHostImpl::GetWindowHandle() {
  if (is_views_hosted_ && CEF_CURRENTLY_ON_UIT()) {
    // Always return the most up-to-date window handle for a views-hosted
    // browser since it may change if the view is re-parented.
    if (platform_delegate_) {
      return platform_delegate_->GetHostWindowHandle();
    }
  }
  return host_window_handle_;
}

CefWindowHandle AlloyBrowserHostImpl::GetOpenerWindowHandle() {
  return opener_window_handle_;
}

void AlloyBrowserHostImpl::Find(const CefString& searchText,
                                bool forward,
                                bool matchCase,
                                bool findNext) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&AlloyBrowserHostImpl::Find, this, searchText,
                                 forward, matchCase, findNext));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->Find(searchText, forward, matchCase, findNext);
  }
}

void AlloyBrowserHostImpl::StopFinding(bool clearSelection) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&AlloyBrowserHostImpl::StopFinding,
                                          this, clearSelection));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->StopFinding(clearSelection);
  }
}

void AlloyBrowserHostImpl::SetAutoResizeEnabled(bool enabled,
                                                const CefSize& min_size,
                                                const CefSize& max_size) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&AlloyBrowserHostImpl::SetAutoResizeEnabled,
                                 this, enabled, min_size, max_size));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->SetAutoResizeEnabled(enabled, min_size, max_size);
  }
}

bool AlloyBrowserHostImpl::CanExecuteChromeCommand(int command_id) {
  return false;
}

void AlloyBrowserHostImpl::ExecuteChromeCommand(
    int command_id,
    cef_window_open_disposition_t disposition) {
  NOTIMPLEMENTED();
}

bool AlloyBrowserHostImpl::IsWindowRenderingDisabled() {
  return IsWindowless();
}

void AlloyBrowserHostImpl::WasResized() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&AlloyBrowserHostImpl::WasResized, this));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->WasResized();
  }
}

void AlloyBrowserHostImpl::WasHidden(bool hidden) {
  if (!IsWindowless()) {
    DCHECK(false) << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHost::WasHidden, this, hidden));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->WasHidden(hidden);
  }
}

void AlloyBrowserHostImpl::NotifyScreenInfoChanged() {
  if (!IsWindowless()) {
    DCHECK(false) << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&AlloyBrowserHostImpl::NotifyScreenInfoChanged, this));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->NotifyScreenInfoChanged();
  }
}

void AlloyBrowserHostImpl::Invalidate(PaintElementType type) {
  if (!IsWindowless()) {
    DCHECK(false) << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT, base::BindOnce(&AlloyBrowserHostImpl::Invalidate, this, type));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->Invalidate(type);
  }
}

void AlloyBrowserHostImpl::SendExternalBeginFrame() {
  if (!IsWindowless()) {
    DCHECK(false) << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&AlloyBrowserHostImpl::SendExternalBeginFrame, this));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->SendExternalBeginFrame();
  }
}

void AlloyBrowserHostImpl::SendTouchEvent(const CefTouchEvent& event) {
  if (!IsWindowless()) {
    DCHECK(false) << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&AlloyBrowserHostImpl::SendTouchEvent,
                                          this, event));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->SendTouchEvent(event);
  }
}

void AlloyBrowserHostImpl::SendCaptureLostEvent() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&AlloyBrowserHostImpl::SendCaptureLostEvent, this));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->SendCaptureLostEvent();
  }
}

int AlloyBrowserHostImpl::GetWindowlessFrameRate() {
  // Verify that this method is being called on the UI thread.
  if (!CEF_CURRENTLY_ON_UIT()) {
    DCHECK(false) << "called on invalid thread";
    return 0;
  }

  return osr_util::ClampFrameRate(settings_.windowless_frame_rate);
}

void AlloyBrowserHostImpl::SetWindowlessFrameRate(int frame_rate) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&AlloyBrowserHostImpl::SetWindowlessFrameRate,
                                 this, frame_rate));
    return;
  }

  settings_.windowless_frame_rate = frame_rate;

  if (platform_delegate_) {
    platform_delegate_->SetWindowlessFrameRate(frame_rate);
  }
}

// AlloyBrowserHostImpl public methods.
// -----------------------------------------------------------------------------

bool AlloyBrowserHostImpl::IsWindowless() const {
  return is_windowless_;
}

bool AlloyBrowserHostImpl::IsVisible() const {
  CEF_REQUIRE_UIT();
  if (IsWindowless() && platform_delegate_) {
    return !platform_delegate_->IsHidden();
  }
  return CefBrowserHostBase::IsVisible();
}

bool AlloyBrowserHostImpl::IsPictureInPictureSupported() const {
  // Not currently supported with OSR.
  return !IsWindowless();
}

void AlloyBrowserHostImpl::WindowDestroyed() {
  CEF_REQUIRE_UIT();
  DCHECK(!window_destroyed_);
  window_destroyed_ = true;

  // Destroy objects that may reference the window.
  menu_manager_.reset(nullptr);

  CloseBrowser(true);
}

bool AlloyBrowserHostImpl::WillBeDestroyed() const {
  CEF_REQUIRE_UIT();
  return destruction_state_ >= DESTRUCTION_STATE_ACCEPTED;
}

void AlloyBrowserHostImpl::DestroyBrowser() {
  CEF_REQUIRE_UIT();

  destruction_state_ = DESTRUCTION_STATE_COMPLETED;

  // Destroy any platform constructs first.
  if (javascript_dialog_manager_.get()) {
    javascript_dialog_manager_->Destroy();
  }
  if (menu_manager_.get()) {
    menu_manager_->Destroy();
  }

  // Disassociate the platform delegate from this browser. This will trigger
  // WebContents destruction in most cases.
  platform_delegate_->BrowserDestroyed(this);

  // Delete objects created by the platform delegate that may be referenced by
  // the WebContents.
  file_dialog_manager_.reset(nullptr);
  javascript_dialog_manager_.reset(nullptr);
  menu_manager_.reset(nullptr);

  // Delete the audio capturer
  if (recently_audible_timer_) {
    recently_audible_timer_->Stop();
    recently_audible_timer_.reset();
  }
  audio_capturer_.reset(nullptr);

  CefBrowserHostBase::DestroyBrowser();
}

void AlloyBrowserHostImpl::CancelContextMenu() {
  CEF_REQUIRE_UIT();
  if (menu_manager_) {
    menu_manager_->CancelContextMenu();
  }
}

bool AlloyBrowserHostImpl::MaybeAllowNavigation(
    content::RenderFrameHost* opener,
    const content::OpenURLParams& params) {
  const bool is_guest_view =
      IsBrowserPluginGuest(content::WebContents::FromRenderFrameHost(opener));
  if (is_guest_view && !params.is_pdf &&
      !params.url.SchemeIs(extensions::kExtensionScheme) &&
      !params.url.SchemeIs(content::kChromeUIScheme)) {
    // The PDF viewer will load the PDF extension in the guest view, and print
    // preview will load chrome://print in the guest view. The PDF renderer
    // used with PdfUnseasoned will set |params.is_pdf| when loading the PDF
    // stream (see PdfNavigationThrottle::WillStartRequest). All other guest
    // view navigations are passed to the owner browser.
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(
                      base::IgnoreResult(&AlloyBrowserHostImpl::OpenURLFromTab),
                      this, nullptr, params, base::NullCallback()));

    return false;
  }

  if (!is_guest_view && params.url.SchemeIs(content::kChromeUIScheme) &&
      !IsAllowedWebUIHost(params.url.host_piece())) {
    // Block navigation to non-allowlisted WebUI pages.
    LOG(WARNING) << "Navigation to " << params.url.spec()
                 << " is blocked in Alloy-style browser.";
    return false;
  }

  return true;
}

void AlloyBrowserHostImpl::OnSetFocus(cef_focus_source_t source) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&AlloyBrowserHostImpl::OnSetFocus,
                                          this, source));
    return;
  }

  if (contents_delegate_.OnSetFocus(source)) {
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->SetFocus(true);
  }
}

void AlloyBrowserHostImpl::EnterFullscreenModeForTab(
    content::RenderFrameHost* requesting_frame,
    const blink::mojom::FullscreenOptions& options) {
  contents_delegate_.EnterFullscreenModeForTab(requesting_frame, options);
  WasResized();
}

void AlloyBrowserHostImpl::ExitFullscreenModeForTab(
    content::WebContents* web_contents) {
  contents_delegate_.ExitFullscreenModeForTab(web_contents);
  WasResized();
}

bool AlloyBrowserHostImpl::IsFullscreenForTabOrPending(
    const content::WebContents* web_contents) {
  return is_fullscreen_;
}

blink::mojom::DisplayMode AlloyBrowserHostImpl::GetDisplayMode(
    const content::WebContents* web_contents) {
  return is_fullscreen_ ? blink::mojom::DisplayMode::kFullscreen
                        : blink::mojom::DisplayMode::kBrowser;
}

void AlloyBrowserHostImpl::FindReply(content::WebContents* web_contents,
                                     int request_id,
                                     int number_of_matches,
                                     const gfx::Rect& selection_rect,
                                     int active_match_ordinal,
                                     bool final_update) {
  auto alloy_delegate =
      static_cast<CefBrowserPlatformDelegateAlloy*>(platform_delegate());
  if (alloy_delegate->HandleFindReply(request_id, number_of_matches,
                                      selection_rect, active_match_ordinal,
                                      final_update)) {
    if (client_) {
      if (auto handler = client_->GetFindHandler()) {
        const auto& details = alloy_delegate->last_search_result();
        CefRect rect(details.selection_rect().x(), details.selection_rect().y(),
                     details.selection_rect().width(),
                     details.selection_rect().height());
        handler->OnFindResult(
            this, details.request_id(), details.number_of_matches(), rect,
            details.active_match_ordinal(), details.final_update());
      }
    }
  }
}

void AlloyBrowserHostImpl::ImeSetComposition(
    const CefString& text,
    const std::vector<CefCompositionUnderline>& underlines,
    const CefRange& replacement_range,
    const CefRange& selection_range) {
  if (!IsWindowless()) {
    DCHECK(false) << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&AlloyBrowserHostImpl::ImeSetComposition, this, text,
                       underlines, replacement_range, selection_range));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->ImeSetComposition(text, underlines, replacement_range,
                                          selection_range);
  }
}

void AlloyBrowserHostImpl::ImeCommitText(const CefString& text,
                                         const CefRange& replacement_range,
                                         int relative_cursor_pos) {
  if (!IsWindowless()) {
    DCHECK(false) << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&AlloyBrowserHostImpl::ImeCommitText, this,
                                 text, replacement_range, relative_cursor_pos));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->ImeCommitText(text, replacement_range,
                                      relative_cursor_pos);
  }
}

void AlloyBrowserHostImpl::ImeFinishComposingText(bool keep_selection) {
  if (!IsWindowless()) {
    DCHECK(false) << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&AlloyBrowserHostImpl::ImeFinishComposingText,
                                 this, keep_selection));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->ImeFinishComposingText(keep_selection);
  }
}

void AlloyBrowserHostImpl::ImeCancelComposition() {
  if (!IsWindowless()) {
    DCHECK(false) << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&AlloyBrowserHostImpl::ImeCancelComposition, this));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->ImeCancelComposition();
  }
}

void AlloyBrowserHostImpl::DragTargetDragEnter(
    CefRefPtr<CefDragData> drag_data,
    const CefMouseEvent& event,
    CefBrowserHost::DragOperationsMask allowed_ops) {
  if (!IsWindowless()) {
    DCHECK(false) << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&AlloyBrowserHostImpl::DragTargetDragEnter,
                                 this, drag_data, event, allowed_ops));
    return;
  }

  if (!drag_data.get()) {
    DCHECK(false);
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->DragTargetDragEnter(drag_data, event, allowed_ops);
  }
}

void AlloyBrowserHostImpl::DragTargetDragOver(
    const CefMouseEvent& event,
    CefBrowserHost::DragOperationsMask allowed_ops) {
  if (!IsWindowless()) {
    DCHECK(false) << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT, base::BindOnce(&AlloyBrowserHostImpl::DragTargetDragOver, this,
                                event, allowed_ops));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->DragTargetDragOver(event, allowed_ops);
  }
}

void AlloyBrowserHostImpl::DragTargetDragLeave() {
  if (!IsWindowless()) {
    DCHECK(false) << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&AlloyBrowserHostImpl::DragTargetDragLeave, this));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->DragTargetDragLeave();
  }
}

void AlloyBrowserHostImpl::DragTargetDrop(const CefMouseEvent& event) {
  if (!IsWindowless()) {
    DCHECK(false) << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&AlloyBrowserHostImpl::DragTargetDrop,
                                          this, event));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->DragTargetDrop(event);
  }
}

void AlloyBrowserHostImpl::DragSourceSystemDragEnded() {
  if (!IsWindowless()) {
    DCHECK(false) << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&AlloyBrowserHostImpl::DragSourceSystemDragEnded, this));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->DragSourceSystemDragEnded();
  }
}

void AlloyBrowserHostImpl::DragSourceEndedAt(
    int x,
    int y,
    CefBrowserHost::DragOperationsMask op) {
  if (!IsWindowless()) {
    DCHECK(false) << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&AlloyBrowserHostImpl::DragSourceEndedAt, this,
                                 x, y, op));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->DragSourceEndedAt(x, y, op);
  }
}

void AlloyBrowserHostImpl::SetAudioMuted(bool mute) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&AlloyBrowserHostImpl::SetAudioMuted,
                                          this, mute));
    return;
  }
  if (!web_contents()) {
    return;
  }
  web_contents()->SetAudioMuted(mute);
}

bool AlloyBrowserHostImpl::IsAudioMuted() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    DCHECK(false) << "called on invalid thread";
    return false;
  }
  if (!web_contents()) {
    return false;
  }
  return web_contents()->IsAudioMuted();
}

// content::WebContentsDelegate methods.
// -----------------------------------------------------------------------------

void AlloyBrowserHostImpl::PrintCrossProcessSubframe(
    content::WebContents* web_contents,
    const gfx::Rect& rect,
    int document_cookie,
    content::RenderFrameHost* subframe_host) const {
  auto* client = printing::PrintCompositeClient::FromWebContents(web_contents);
  if (client) {
    client->PrintCrossProcessSubframe(rect, document_cookie, subframe_host);
  }
}

content::WebContents* AlloyBrowserHostImpl::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  auto target_contents = contents_delegate_.OpenURLFromTabEx(
      source, params, navigation_handle_callback);
  if (target_contents) {
    // Start a navigation in the current browser that will result in the
    // creation of a new render process.
    LoadMainFrameURL(params);
    return target_contents;
  }

  // Cancel the navigation.
  return nullptr;
}

content::WebContents* AlloyBrowserHostImpl::AddNewContents(
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture,
    bool* was_blocked) {
  auto* new_contents_ptr = new_contents.get();
  platform_delegate_->AddNewContents(source, std::move(new_contents),
                                     target_url, disposition, window_features,
                                     user_gesture, was_blocked);
  return new_contents_ptr;
}

void AlloyBrowserHostImpl::LoadingStateChanged(content::WebContents* source,
                                               bool should_show_loading_ui) {
  contents_delegate_.LoadingStateChanged(source, should_show_loading_ui);
}

void AlloyBrowserHostImpl::CloseContents(content::WebContents* source) {
  CEF_REQUIRE_UIT();

  if (destruction_state_ == DESTRUCTION_STATE_COMPLETED) {
    return;
  }

  bool close_browser = true;

  // If this method is called in response to something other than
  // WindowDestroyed() ask the user if the browser should close.
  if (client_.get() && (IsWindowless() || !window_destroyed_)) {
    CefRefPtr<CefLifeSpanHandler> handler = client_->GetLifeSpanHandler();
    if (handler.get()) {
      close_browser = !handler->DoClose(this);
    }
  }

  if (close_browser) {
    if (destruction_state_ != DESTRUCTION_STATE_ACCEPTED) {
      destruction_state_ = DESTRUCTION_STATE_ACCEPTED;
    }

    if (!IsWindowless() && !window_destroyed_) {
      // A window exists so try to close it using the platform method. Will
      // result in a call to WindowDestroyed() if/when the window is destroyed
      // via the platform window destruction mechanism.
      platform_delegate_->CloseHostWindow();
    } else {
      // Keep a reference to the browser while it's in the process of being
      // destroyed.
      CefRefPtr<AlloyBrowserHostImpl> browser(this);

      if (source) {
        // Try to fast shutdown the associated process.
        source->GetPrimaryMainFrame()->GetProcess()->FastShutdownIfPossible(
            1, false);
      }

      // No window exists. Destroy the browser immediately. Don't call other
      // browser methods after calling DestroyBrowser().
      DestroyBrowser();
    }
  } else if (destruction_state_ != DESTRUCTION_STATE_NONE) {
    destruction_state_ = DESTRUCTION_STATE_NONE;
  }
}

void AlloyBrowserHostImpl::UpdateTargetURL(content::WebContents* source,
                                           const GURL& url) {
  contents_delegate_.UpdateTargetURL(source, url);
}

bool AlloyBrowserHostImpl::DidAddMessageToConsole(
    content::WebContents* source,
    blink::mojom::ConsoleMessageLevel level,
    const std::u16string& message,
    int32_t line_no,
    const std::u16string& source_id) {
  return contents_delegate_.DidAddMessageToConsole(source, level, message,
                                                   line_no, source_id);
}

void AlloyBrowserHostImpl::ContentsZoomChange(bool zoom_in) {
  zoom::PageZoom::Zoom(
      web_contents(), zoom_in ? content::PAGE_ZOOM_IN : content::PAGE_ZOOM_OUT);
}

void AlloyBrowserHostImpl::BeforeUnloadFired(content::WebContents* source,
                                             bool proceed,
                                             bool* proceed_to_fire_unload) {
  if (destruction_state_ == DESTRUCTION_STATE_ACCEPTED || proceed) {
    *proceed_to_fire_unload = true;
  } else if (!proceed) {
    *proceed_to_fire_unload = false;
    destruction_state_ = DESTRUCTION_STATE_NONE;
  }
}

bool AlloyBrowserHostImpl::TakeFocus(content::WebContents* source,
                                     bool reverse) {
  if (client_.get()) {
    CefRefPtr<CefFocusHandler> handler = client_->GetFocusHandler();
    if (handler.get()) {
      handler->OnTakeFocus(this, !reverse);
    }
  }

  return false;
}

void AlloyBrowserHostImpl::CanDownload(
    const GURL& url,
    const std::string& request_method,
    base::OnceCallback<void(bool)> callback) {
  contents_delegate_.CanDownload(url, request_method, std::move(callback));
}

KeyboardEventProcessingResult AlloyBrowserHostImpl::PreHandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  return contents_delegate_.PreHandleKeyboardEvent(source, event);
}

bool AlloyBrowserHostImpl::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  // Check to see if event should be ignored.
  if (event.skip_if_unhandled) {
    return false;
  }

  if (contents_delegate_.HandleKeyboardEvent(source, event)) {
    return true;
  }

  if (platform_delegate_) {
    return platform_delegate_->HandleKeyboardEvent(event);
  }
  return false;
}

bool AlloyBrowserHostImpl::CanDragEnter(content::WebContents* source,
                                        const content::DropData& data,
                                        blink::DragOperationsMask mask) {
  CefRefPtr<CefDragHandler> handler;
  if (client_) {
    handler = client_->GetDragHandler();
  }
  if (handler) {
    CefRefPtr<CefDragDataImpl> drag_data(new CefDragDataImpl(data));
    drag_data->SetReadOnly(true);
    if (handler->OnDragEnter(
            this, drag_data.get(),
            static_cast<CefDragHandler::DragOperationsMask>(mask))) {
      return false;
    }
  }
  return true;
}

void AlloyBrowserHostImpl::GetCustomWebContentsView(
    content::WebContents* web_contents,
    const GURL& target_url,
    int opener_render_process_id,
    int opener_render_frame_id,
    raw_ptr<content::WebContentsView>* view,
    raw_ptr<content::RenderViewHostDelegateView>* delegate_view) {
  CefBrowserInfoManager::GetInstance()->GetCustomWebContentsView(
      target_url,
      frame_util::MakeGlobalId(opener_render_process_id,
                               opener_render_frame_id),
      view, delegate_view);
}

void AlloyBrowserHostImpl::WebContentsCreated(
    content::WebContents* source_contents,
    int opener_render_process_id,
    int opener_render_frame_id,
    const std::string& frame_name,
    const GURL& target_url,
    content::WebContents* new_contents) {
  CefBrowserSettings settings;
  CefRefPtr<CefClient> client;
  std::unique_ptr<CefBrowserPlatformDelegate> platform_delegate;
  CefRefPtr<CefDictionaryValue> extra_info;

  CefBrowserInfoManager::GetInstance()->WebContentsCreated(
      target_url,
      frame_util::MakeGlobalId(opener_render_process_id,
                               opener_render_frame_id),
      settings, client, platform_delegate, extra_info, new_contents);

  scoped_refptr<CefBrowserInfo> info =
      CefBrowserInfoManager::GetInstance()->CreatePopupBrowserInfo(
          new_contents, platform_delegate->IsWindowless(),
          platform_delegate->IsPrintPreviewSupported(), extra_info);
  CHECK(info.get());
  CHECK(info->is_popup());

  CefRefPtr<AlloyBrowserHostImpl> opener =
      GetBrowserForContents(source_contents);
  if (!opener) {
    return;
  }

  // Popups must share the same RequestContext as the parent.
  CefRefPtr<CefRequestContextImpl> request_context = opener->request_context();
  CHECK(request_context);

  // We don't officially own |new_contents| until AddNewContents() is called.
  // However, we need to install observers/delegates here.
  CefRefPtr<AlloyBrowserHostImpl> browser = CreateInternal(
      settings, client, new_contents, /*own_web_contents=*/false, info, opener,
      /*is_devtools_popup=*/false, request_context,
      std::move(platform_delegate));
}

void AlloyBrowserHostImpl::RendererUnresponsive(
    content::WebContents* source,
    content::RenderWidgetHost* render_widget_host,
    base::RepeatingClosure hang_monitor_restarter) {
  hang_monitor::RendererUnresponsive(this, render_widget_host,
                                     hang_monitor_restarter);
}

void AlloyBrowserHostImpl::RendererResponsive(
    content::WebContents* source,
    content::RenderWidgetHost* render_widget_host) {
  hang_monitor::RendererResponsive(this, render_widget_host);
}

content::JavaScriptDialogManager*
AlloyBrowserHostImpl::GetJavaScriptDialogManager(content::WebContents* source) {
  return CefBrowserHostBase::GetJavaScriptDialogManager();
}

void AlloyBrowserHostImpl::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    scoped_refptr<content::FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  // This will eventually call into CefFileDialogManager.
  FileSelectHelper::RunFileChooser(render_frame_host, std::move(listener),
                                   params);
}

bool AlloyBrowserHostImpl::ShowContextMenu(
    const content::ContextMenuParams& params) {
  CEF_REQUIRE_UIT();
  if (!menu_manager_.get() && platform_delegate_) {
    menu_manager_ = std::make_unique<CefMenuManager>(
        this, platform_delegate_->CreateMenuRunner());
  }
  return menu_manager_->CreateContextMenu(params);
}

void AlloyBrowserHostImpl::UpdatePreferredSize(content::WebContents* source,
                                               const gfx::Size& pref_size) {
#if BUILDFLAG(IS_WIN) || (BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC))
  CEF_REQUIRE_UIT();
  if (platform_delegate_) {
    platform_delegate_->SizeTo(pref_size.width(), pref_size.height());
  }
#endif
}

void AlloyBrowserHostImpl::ResizeDueToAutoResize(content::WebContents* source,
                                                 const gfx::Size& new_size) {
  CEF_REQUIRE_UIT();

  if (client_) {
    CefRefPtr<CefDisplayHandler> handler = client_->GetDisplayHandler();
    if (handler && handler->OnAutoResize(
                       this, CefSize(new_size.width(), new_size.height()))) {
      return;
    }
  }

  UpdatePreferredSize(source, new_size);
}

void AlloyBrowserHostImpl::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  auto returned_callback = media_access_query::RequestMediaAccessPermission(
      this, request, std::move(callback), /*default_disallow=*/true);
  // Callback should not be returned.
  DCHECK(returned_callback.is_null());
}

bool AlloyBrowserHostImpl::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const url::Origin& security_origin,
    blink::mojom::MediaStreamType type) {
  return media_access_query::CheckMediaAccessPermission(this, render_frame_host,
                                                        security_origin, type);
}

content::PictureInPictureResult AlloyBrowserHostImpl::EnterPictureInPicture(
    content::WebContents* web_contents) {
  if (!IsPictureInPictureSupported()) {
    return content::PictureInPictureResult::kNotSupported;
  }

  return PictureInPictureWindowManager::GetInstance()
      ->EnterVideoPictureInPicture(web_contents);
}

void AlloyBrowserHostImpl::ExitPictureInPicture() {
  DCHECK(IsPictureInPictureSupported());
  PictureInPictureWindowManager::GetInstance()->ExitPictureInPicture();
}

bool AlloyBrowserHostImpl::IsBackForwardCacheSupported(
    content::WebContents& web_contents) {
  return true;
}

content::PreloadingEligibility AlloyBrowserHostImpl::IsPrerender2Supported(
    content::WebContents& web_contents) {
  // Prerender is not supported in CEF. See issue #3664.
  return content::PreloadingEligibility::kPreloadingDisabled;
}

void AlloyBrowserHostImpl::DraggableRegionsChanged(
    const std::vector<blink::mojom::DraggableRegionPtr>& regions,
    content::WebContents* contents) {
  contents_delegate_.DraggableRegionsChanged(regions, contents);
}

// content::WebContentsObserver methods.
// -----------------------------------------------------------------------------

void AlloyBrowserHostImpl::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (web_contents()) {
    auto cef_browser_context = CefBrowserContext::FromBrowserContext(
        web_contents()->GetBrowserContext());
    if (cef_browser_context) {
      cef_browser_context->AddVisitedURLs(
          navigation_handle->GetURL(), navigation_handle->GetRedirectChain(),
          navigation_handle->GetPageTransition());
    }
  }
}

void AlloyBrowserHostImpl::OnAudioStateChanged(bool audible) {
  if (audible) {
    if (recently_audible_timer_) {
      recently_audible_timer_->Stop();
    }

    StartAudioCapturer();
  } else if (audio_capturer_) {
    if (!recently_audible_timer_) {
      recently_audible_timer_ = std::make_unique<base::OneShotTimer>();
    }

    // If you have a media playing that has a short quiet moment, web_contents
    // will immediately switch to non-audible state. We don't want to stop
    // audio stream so quickly, let's give the stream some time to resume
    // playing.
    recently_audible_timer_->Start(
        FROM_HERE, kRecentlyAudibleTimeout,
        base::BindOnce(&AlloyBrowserHostImpl::OnRecentlyAudibleTimerFired,
                       this));
  }
}

void AlloyBrowserHostImpl::OnRecentlyAudibleTimerFired() {
  audio_capturer_.reset();
}

void AlloyBrowserHostImpl::AccessibilityEventReceived(
    const ui::AXUpdatesAndEvents& details) {
  // Only needed in windowless mode.
  if (IsWindowless()) {
    if (!web_contents() || !platform_delegate_) {
      return;
    }

    platform_delegate_->AccessibilityEventReceived(details);
  }
}

void AlloyBrowserHostImpl::AccessibilityLocationChangesReceived(
    const ui::AXTreeID& tree_id,
    ui::AXLocationAndScrollUpdates& details) {
  // Only needed in windowless mode.
  if (IsWindowless()) {
    if (!web_contents() || !platform_delegate_) {
      return;
    }

    platform_delegate_->AccessibilityLocationChangesReceived(tree_id, details);
  }
}

void AlloyBrowserHostImpl::WebContentsDestroyed() {
  // In case we're notified before the CefBrowserContentsDelegate,
  // reset it first for consistent state in DestroyWebContents.
  if (GetWebContents()) {
    contents_delegate_.WebContentsDestroyed();
  }

  auto wc = web_contents();
  content::WebContentsObserver::Observe(nullptr);
  DestroyWebContents(wc);

  if (destruction_state_ < DESTRUCTION_STATE_COMPLETED) {
    // We were not called via DestroyBrowser. This can occur when (for example)
    // a pending popup WebContents is destroyed during parent WebContents
    // destruction. Try to close the associated browser now.
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&AlloyBrowserHostImpl::CloseBrowser,
                                          this, /*force_close=*/true));
  }
}

void AlloyBrowserHostImpl::StartAudioCapturer() {
  if (!client_.get() || audio_capturer_) {
    return;
  }

  CefRefPtr<CefAudioHandler> audio_handler = client_->GetAudioHandler();
  if (!audio_handler.get()) {
    return;
  }

  CefAudioParameters params;
  params.channel_layout = CEF_CHANNEL_LAYOUT_STEREO;
  params.sample_rate = media::AudioParameters::kAudioCDSampleRate;
  params.frames_per_buffer = 1024;

  if (!audio_handler->GetAudioParameters(this, params)) {
    return;
  }

  audio_capturer_ =
      std::make_unique<CefAudioCapturer>(params, this, audio_handler);
}

// AlloyBrowserHostImpl private methods.
// -----------------------------------------------------------------------------

AlloyBrowserHostImpl::AlloyBrowserHostImpl(
    const CefBrowserSettings& settings,
    CefRefPtr<CefClient> client,
    content::WebContents* web_contents,
    scoped_refptr<CefBrowserInfo> browser_info,
    CefRefPtr<AlloyBrowserHostImpl> opener,
    CefRefPtr<CefRequestContextImpl> request_context,
    std::unique_ptr<CefBrowserPlatformDelegate> platform_delegate)
    : CefBrowserHostBase(settings,
                         client,
                         std::move(platform_delegate),
                         browser_info,
                         request_context),
      content::WebContentsObserver(web_contents),
      is_windowless_(platform_delegate_->IsWindowless()) {
  contents_delegate_.ObserveWebContents(web_contents);

  if (opener.get()) {
    opener_id_ = opener->GetIdentifier();

    if (!is_views_hosted_) {
      // GetOpenerWindowHandle() only returns a value for non-views-hosted
      // popup browsers.
      opener_window_handle_ = opener->GetWindowHandle();
    }
  }

  // Associate the platform delegate with this browser.
  platform_delegate_->BrowserCreated(this);

  // Make sure RenderFrameCreated is called at least one time.
  RenderFrameCreated(web_contents->GetPrimaryMainFrame());
}

bool AlloyBrowserHostImpl::CreateHostWindow() {
  // |host_window_handle_| will not change after initial host creation for
  // non-views-hosted browsers.
  bool success = true;
  if (!IsWindowless()) {
    success = platform_delegate_->CreateHostWindow();
  }
  if (success && !is_views_hosted_) {
    host_window_handle_ = platform_delegate_->GetHostWindowHandle();
  }
  return success;
}

gfx::Point AlloyBrowserHostImpl::GetScreenPoint(const gfx::Point& view,
                                                bool want_dip_coords) const {
  CEF_REQUIRE_UIT();
  if (platform_delegate_) {
    return platform_delegate_->GetScreenPoint(view, want_dip_coords);
  }
  return gfx::Point();
}

void AlloyBrowserHostImpl::StartDragging(
    const content::DropData& drop_data,
    blink::DragOperationsMask allowed_ops,
    const gfx::ImageSkia& image,
    const gfx::Vector2d& image_offset,
    const blink::mojom::DragEventSourceInfo& event_info,
    content::RenderWidgetHostImpl* source_rwh) {
  if (platform_delegate_) {
    platform_delegate_->StartDragging(drop_data, allowed_ops, image,
                                      image_offset, event_info, source_rwh);
  }
}

void AlloyBrowserHostImpl::UpdateDragOperation(
    ui::mojom::DragOperation operation,
    bool document_is_handling_drag) {
  if (platform_delegate_) {
    platform_delegate_->UpdateDragOperation(operation,
                                            document_is_handling_drag);
  }
}
