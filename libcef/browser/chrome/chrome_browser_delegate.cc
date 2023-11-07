// Copyright 2020 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "libcef/browser/chrome/chrome_browser_delegate.h"

#include "libcef/browser/browser_contents_delegate.h"
#include "libcef/browser/browser_host_base.h"
#include "libcef/browser/browser_info_manager.h"
#include "libcef/browser/browser_platform_delegate.h"
#include "libcef/browser/chrome/chrome_browser_host_impl.h"
#include "libcef/browser/chrome/views/chrome_browser_view.h"
#include "libcef/browser/media_access_query.h"
#include "libcef/browser/request_context_impl.h"
#include "libcef/browser/views/browser_view_impl.h"
#include "libcef/browser/views/window_impl.h"
#include "libcef/common/app_manager.h"
#include "libcef/common/frame_util.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/input/native_web_keyboard_event.h"

using content::KeyboardEventProcessingResult;

ChromeBrowserDelegate::ChromeBrowserDelegate(
    Browser* browser,
    const CefBrowserCreateParams& create_params,
    const Browser* opener)
    : browser_(browser), create_params_(create_params) {
  DCHECK(browser_);

  if (opener) {
    DCHECK(browser->is_type_picture_in_picture());
    auto opener_host = ChromeBrowserHostImpl::GetBrowserForBrowser(opener);
    DCHECK(opener_host);
    if (opener_host) {
      opener_host_ = opener_host->GetWeakPtr();
    }
  }
}

ChromeBrowserDelegate::~ChromeBrowserDelegate() = default;

std::unique_ptr<content::WebContents> ChromeBrowserDelegate::AddWebContents(
    std::unique_ptr<content::WebContents> new_contents) {
  if (CefBrowserInfoManager::GetInstance()->AddWebContents(
          new_contents.get())) {
    // The browser host should have been created in WebContentsCreated().
    auto new_browser =
        ChromeBrowserHostImpl::GetBrowserForContents(new_contents.get());
    if (new_browser) {
      // Create a new Browser and give it ownership of the new WebContents.
      new_browser->AddNewContents(std::move(new_contents));
    } else {
      LOG(ERROR) << "No host found for chrome popup browser";
    }
  }

  // Proceed with default chrome::AddWebContents behavior.
  return new_contents;
}

void ChromeBrowserDelegate::OnWebContentsCreated(
    content::WebContents* new_contents) {
  // Necessary to receive LoadingStateChanged calls during initial navigation.
  // This will be called again in Browser::SetAsDelegate, which should be fine.
  new_contents->SetDelegate(browser_);

  SetAsDelegate(new_contents, /*set_delegate=*/true);
}

void ChromeBrowserDelegate::SetAsDelegate(content::WebContents* web_contents,
                                          bool set_delegate) {
  DCHECK(web_contents);
  auto browser_host =
      ChromeBrowserHostImpl::GetBrowserForContents(web_contents);

  // |set_delegate=false| only makes sense if we already have a browser host.
  DCHECK(browser_host || set_delegate);

  if (browser_host) {
    // We already have a browser host, so just change the associated Browser.
    browser_host->SetBrowser(set_delegate ? browser_ : nullptr);
    return;
  }

  auto platform_delegate = CefBrowserPlatformDelegate::Create(create_params_);
  CHECK(platform_delegate);

  auto browser_info = CefBrowserInfoManager::GetInstance()->CreateBrowserInfo(
      /*is_popup=*/false, /*is_windowless=*/false, create_params_.extra_info);

  auto request_context_impl =
      CefRequestContextImpl::GetOrCreateForRequestContext(
          create_params_.request_context);

  CreateBrowser(web_contents, create_params_.settings, create_params_.client,
                std::move(platform_delegate), browser_info, /*opener=*/nullptr,
                request_context_impl);
}

bool ChromeBrowserDelegate::ShowStatusBubble(bool show_by_default) {
  if (!show_status_bubble_.has_value()) {
    show_status_bubble_ = show_by_default;
    if (auto browser = ChromeBrowserHostImpl::GetBrowserForBrowser(browser_)) {
      const auto& state = browser->settings().chrome_status_bubble;
      if (show_by_default && state == STATE_DISABLED) {
        show_status_bubble_ = false;
      } else if (!show_by_default && state == STATE_ENABLED) {
        show_status_bubble_ = true;
      }
    }
  }

  return *show_status_bubble_;
}

bool ChromeBrowserDelegate::HandleCommand(int command_id,
                                          WindowOpenDisposition disposition) {
  if (auto browser = ChromeBrowserHostImpl::GetBrowserForBrowser(browser_)) {
    if (auto client = browser->GetClient()) {
      if (auto handler = client->GetCommandHandler()) {
        return handler->OnChromeCommand(
            browser.get(), command_id,
            static_cast<cef_window_open_disposition_t>(disposition));
      }
    }
  }
  return false;
}

bool ChromeBrowserDelegate::IsAppMenuItemVisible(int command_id) {
  if (auto browser = ChromeBrowserHostImpl::GetBrowserForBrowser(browser_)) {
    if (auto client = browser->GetClient()) {
      if (auto handler = client->GetCommandHandler()) {
        return handler->IsChromeAppMenuItemVisible(browser.get(), command_id);
      }
    }
  }
  return true;
}

bool ChromeBrowserDelegate::IsAppMenuItemEnabled(int command_id) {
  if (auto browser = ChromeBrowserHostImpl::GetBrowserForBrowser(browser_)) {
    if (auto client = browser->GetClient()) {
      if (auto handler = client->GetCommandHandler()) {
        return handler->IsChromeAppMenuItemEnabled(browser.get(), command_id);
      }
    }
  }
  return true;
}

bool ChromeBrowserDelegate::IsPageActionIconVisible(
    PageActionIconType icon_type) {
  // Verify that our enum matches Chromium's values.
  static_assert(static_cast<int>(CEF_CPAIT_MAX_VALUE) ==
                    static_cast<int>(PageActionIconType::kMaxValue),
                "enum mismatch");

  if (auto client = create_params_.client) {
    if (auto handler = client->GetCommandHandler()) {
      return handler->IsChromePageActionIconVisible(
          static_cast<cef_chrome_page_action_icon_type_t>(icon_type));
    }
  }
  return true;
}

bool ChromeBrowserDelegate::IsToolbarButtonVisible(
    ToolbarButtonType button_type) {
  // Verify that our enum matches BrowserDelegate's values.
  static_assert(static_cast<int>(CEF_CTBT_MAX_VALUE) ==
                    static_cast<int>(ToolbarButtonType::kMaxValue),
                "enum mismatch");

  if (auto client = create_params_.client) {
    if (auto handler = client->GetCommandHandler()) {
      return handler->IsChromeToolbarButtonVisible(
          static_cast<cef_chrome_toolbar_button_type_t>(button_type));
    }
  }
  return true;
}

void ChromeBrowserDelegate::UpdateFindBarBoundingBox(gfx::Rect* bounds) {
  if (auto browser = ChromeBrowserHostImpl::GetBrowserForBrowser(browser_)) {
    browser->platform_delegate()->UpdateFindBarBoundingBox(bounds);
  }
}

content::MediaResponseCallback
ChromeBrowserDelegate::RequestMediaAccessPermissionEx(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  if (auto browser = ChromeBrowserHostImpl::GetBrowserForBrowser(browser_)) {
    return media_access_query::RequestMediaAccessPermission(
        browser.get(), request, std::move(callback),
        /*default_disallow=*/false);
  }
  return callback;
}

bool ChromeBrowserDelegate::SupportsFramelessPictureInPicture() const {
  if (!browser_->is_type_picture_in_picture()) {
    return false;
  }

  if (frameless_pip_.has_value()) {
    return *frameless_pip_;
  }

  frameless_pip_ = false;

  if (opener_host_) {
    if (auto chrome_browser_view = opener_host_->chrome_browser_view()) {
      if (auto cef_delegate = chrome_browser_view->cef_delegate()) {
        frameless_pip_ = cef_delegate->UseFramelessWindowForPictureInPicture(
            chrome_browser_view->cef_browser_view());
      }
    }
  }

  return *frameless_pip_;
}

absl::optional<bool> ChromeBrowserDelegate::SupportsWindowFeature(
    int feature) const {
  // Override the default value from
  // Browser::PictureInPictureBrowserSupportsWindowFeature.
  if (feature == Browser::FEATURE_TITLEBAR &&
      browser_->is_type_picture_in_picture()) {
    // Return false to hide titlebar and enable draggable regions.
    return !SupportsFramelessPictureInPicture();
  }
  return absl::nullopt;
}

bool ChromeBrowserDelegate::SupportsDraggableRegion() const {
  return SupportsFramelessPictureInPicture();
}

const absl::optional<SkRegion> ChromeBrowserDelegate::GetDraggableRegion()
    const {
  DCHECK(SupportsDraggableRegion());
  return draggable_region_;
}

void ChromeBrowserDelegate::UpdateDraggableRegion(const SkRegion& region) {
  DCHECK(SupportsDraggableRegion());
  draggable_region_ = region;
}

void ChromeBrowserDelegate::WindowFullscreenStateChanged() {
  // Use a synchronous callback for notification on Windows/Linux. MacOS gets
  // notified asynchronously via CefNativeWidgetMac callbacks.
#if !BUILDFLAG(IS_MAC)
  if (auto browser = ChromeBrowserHostImpl::GetBrowserForBrowser(browser_)) {
    if (auto chrome_browser_view = browser->chrome_browser_view()) {
      auto* cef_window = chrome_browser_view->cef_browser_view()->cef_window();
      if (auto* delegate = cef_window->delegate()) {
        // Give the CefWindowDelegate a chance to handle the event.
        delegate->OnWindowFullscreenTransition(cef_window,
                                               /*is_completed=*/true);
      }
    }
  }
#endif
}

void ChromeBrowserDelegate::WebContentsCreated(
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

  auto opener = ChromeBrowserHostImpl::GetBrowserForContents(source_contents);
  if (!opener) {
    LOG(ERROR) << "No opener found for chrome popup browser";
    return;
  }

  auto browser_info =
      CefBrowserInfoManager::GetInstance()->CreatePopupBrowserInfo(
          new_contents, /*is_windowless=*/false, extra_info);
  CHECK(browser_info->is_popup());

  // Popups must share the same RequestContext as the parent.
  auto request_context_impl = opener->request_context();
  CHECK(request_context_impl);

  // We don't officially own |new_contents| until AddNewContents() is called.
  // However, we need to install observers/delegates here.
  CreateBrowser(new_contents, settings, client, std::move(platform_delegate),
                browser_info, opener, request_context_impl);
}

content::WebContents* ChromeBrowserDelegate::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  // |source| may be nullptr when opening a link from chrome UI such as the
  // Reading List sidebar. In that case we default to using the Browser's
  // currently active WebContents.
  if (!source) {
    source = browser_->tab_strip_model()->GetActiveWebContents();
    DCHECK(source);
  }

  // Return nullptr to cancel the navigation. Otherwise, proceed with default
  // chrome handling.
  if (auto delegate = GetDelegateForWebContents(source)) {
    return delegate->OpenURLFromTab(source, params);
  }
  return nullptr;
}

void ChromeBrowserDelegate::LoadingStateChanged(content::WebContents* source,
                                                bool should_show_loading_ui) {
  if (auto delegate = GetDelegateForWebContents(source)) {
    delegate->LoadingStateChanged(source, should_show_loading_ui);
  }
}

void ChromeBrowserDelegate::UpdateTargetURL(content::WebContents* source,
                                            const GURL& url) {
  if (auto delegate = GetDelegateForWebContents(source)) {
    delegate->UpdateTargetURL(source, url);
  }
}

bool ChromeBrowserDelegate::DidAddMessageToConsole(
    content::WebContents* source,
    blink::mojom::ConsoleMessageLevel log_level,
    const std::u16string& message,
    int32_t line_no,
    const std::u16string& source_id) {
  if (auto delegate = GetDelegateForWebContents(source)) {
    return delegate->DidAddMessageToConsole(source, log_level, message, line_no,
                                            source_id);
  }
  return false;
}

void ChromeBrowserDelegate::EnterFullscreenModeForTab(
    content::RenderFrameHost* requesting_frame,
    const blink::mojom::FullscreenOptions& options) {
  auto web_contents =
      content::WebContents::FromRenderFrameHost(requesting_frame);
  if (!web_contents) {
    return;
  }

  if (auto delegate = GetDelegateForWebContents(web_contents)) {
    delegate->EnterFullscreenModeForTab(requesting_frame, options);
  }
}

void ChromeBrowserDelegate::ExitFullscreenModeForTab(
    content::WebContents* web_contents) {
  if (auto delegate = GetDelegateForWebContents(web_contents)) {
    delegate->ExitFullscreenModeForTab(web_contents);
  }

  // Workaround for https://crbug.com/1500371. Ensure WebContents exits
  // fullscreen state by explicitly sending a resize message.
  if (auto* rwhv = web_contents->GetRenderWidgetHostView()) {
    if (auto* render_widget_host = rwhv->GetRenderWidgetHost()) {
      render_widget_host->SynchronizeVisualProperties();
    }
  }
}

void ChromeBrowserDelegate::CanDownload(
    const GURL& url,
    const std::string& request_method,
    base::OnceCallback<void(bool)> callback) {
  auto source = browser_->tab_strip_model()->GetActiveWebContents();
  DCHECK(source);

  if (auto delegate = GetDelegateForWebContents(source)) {
    delegate->CanDownload(url, request_method, std::move(callback));
    return;
  }
  std::move(callback).Run(true);
}

KeyboardEventProcessingResult ChromeBrowserDelegate::PreHandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  if (auto delegate = GetDelegateForWebContents(source)) {
    return delegate->PreHandleKeyboardEvent(source, event);
  }
  return KeyboardEventProcessingResult::NOT_HANDLED;
}

bool ChromeBrowserDelegate::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  if (auto delegate = GetDelegateForWebContents(source)) {
    return delegate->HandleKeyboardEvent(source, event);
  }
  return false;
}

void ChromeBrowserDelegate::CreateBrowser(
    content::WebContents* web_contents,
    CefBrowserSettings settings,
    CefRefPtr<CefClient> client,
    std::unique_ptr<CefBrowserPlatformDelegate> platform_delegate,
    scoped_refptr<CefBrowserInfo> browser_info,
    CefRefPtr<ChromeBrowserHostImpl> opener,
    CefRefPtr<CefRequestContextImpl> request_context_impl) {
  CEF_REQUIRE_UIT();
  DCHECK(web_contents);
  DCHECK(platform_delegate);
  DCHECK(browser_info);
  DCHECK(request_context_impl);

  // If |opener| is non-nullptr it must be a popup window.
  DCHECK(!opener.get() || browser_info->is_popup());

  if (!client) {
    if (auto app = CefAppManager::Get()->GetApplication()) {
      if (auto bph = app->GetBrowserProcessHandler()) {
        client = bph->GetDefaultClient();
      }
    }
  }

  if (!client) {
    LOG(WARNING) << "Creating a chrome browser without a client";
  }

  // Check if chrome and CEF are using the same browser context.
  // TODO(chrome-runtime): Verify if/when this might occur.
  auto chrome_browser_context =
      CefBrowserContext::FromBrowserContext(browser_->create_params().profile);
  if (chrome_browser_context != request_context_impl->GetBrowserContext()) {
    LOG(WARNING) << "Creating a chrome browser with mismatched context";
  }

  // Remains alive until the associated WebContents is destroyed.
  CefRefPtr<ChromeBrowserHostImpl> browser_host =
      new ChromeBrowserHostImpl(settings, client, std::move(platform_delegate),
                                browser_info, request_context_impl);
  browser_host->Attach(web_contents, opener);

  // The Chrome browser for a popup won't be created until AddNewContents().
  if (!opener) {
    browser_host->SetBrowser(browser_);
  }
}

CefBrowserContentsDelegate* ChromeBrowserDelegate::GetDelegateForWebContents(
    content::WebContents* web_contents) {
  auto browser_host =
      ChromeBrowserHostImpl::GetBrowserForContents(web_contents);
  if (browser_host) {
    return browser_host->contents_delegate();
  }
  return nullptr;
}

namespace cef {

// static
std::unique_ptr<BrowserDelegate> BrowserDelegate::Create(
    Browser* browser,
    scoped_refptr<CreateParams> cef_params,
    const Browser* opener) {
  if (!cef::IsChromeRuntimeEnabled()) {
    return nullptr;
  }

  CefBrowserCreateParams create_params;

  // Parameters from ChromeBrowserHostImpl::Create, or nullptr if the Browser
  // was created from somewhere else.
  auto params = static_cast<ChromeBrowserHostImpl::DelegateCreateParams*>(
      cef_params.get());
  if (params) {
    create_params = params->create_params_;

    // Clear these values so they're not persisted to additional Browsers.
    params->create_params_.window_info.reset();
    params->create_params_.browser_view = nullptr;
  }

  return std::make_unique<ChromeBrowserDelegate>(browser, create_params,
                                                 opener);
}

}  // namespace cef
