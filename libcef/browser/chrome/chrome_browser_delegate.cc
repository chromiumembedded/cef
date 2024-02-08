// Copyright 2020 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "libcef/browser/chrome/chrome_browser_delegate.h"

#include "libcef/browser/browser_contents_delegate.h"
#include "libcef/browser/browser_host_base.h"
#include "libcef/browser/browser_info_manager.h"
#include "libcef/browser/browser_platform_delegate.h"
#include "libcef/browser/chrome/chrome_browser_context.h"
#include "libcef/browser/chrome/chrome_browser_host_impl.h"
#include "libcef/browser/chrome/views/chrome_browser_view.h"
#include "libcef/browser/chrome/views/chrome_child_window.h"
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
    DCHECK(browser->is_type_picture_in_picture() ||
           browser->is_type_devtools());
    auto opener_host = ChromeBrowserHostImpl::GetBrowserForBrowser(opener);
    DCHECK(opener_host);
    if (opener_host) {
      opener_host_ = opener_host->GetWeakPtr();
    }
  }
}

ChromeBrowserDelegate::~ChromeBrowserDelegate() = default;

Browser* ChromeBrowserDelegate::CreateDevToolsBrowser(
    Profile* profile,
    Browser* opener,
    std::unique_ptr<content::WebContents>& devtools_contents) {
  // |opener| is the same value that will be passed to the ChromeBrowserDelegate
  // constructor for the new popup Browser. It may be nullptr in certain rare
  // situations (e.g. if DevTools is launched for a WebContents that is not a
  // Browser Tab). In that case, the popup browser host will instead be created
  // via SetAsDelegate.
  auto opener_browser_host =
      opener ? ChromeBrowserHostImpl::GetBrowserForBrowser(opener) : nullptr;
  if (!opener_browser_host) {
    return nullptr;
  }

  // We expect openers and popups to have the same Profile.
  CHECK_EQ(opener->profile(), profile);

  //
  // 1. Get configuration settings from the user and create the new platform
  // delegate. Logical equivalent of CefBrowserInfoManager::CanCreateWindow()
  // for normal popups.
  //

  auto opener_client = opener_browser_host->GetClient();
  auto life_span_handler =
      opener_client ? opener_client->GetLifeSpanHandler() : nullptr;

  CefBrowserCreateParams create_params;
  CefWindowInfo window_info;

  // If |client| is empty, or if the user clears |client| in
  // OnBeforeDevToolsPopup, we'll use the result of GetDefaultClient() later on
  // in CreateBrowserHost().
  if (pending_show_devtools_params_) {
    // Start with the params passed to CefBrowserHost::ShowDevTools().
    create_params.client = pending_show_devtools_params_->client_;
    create_params.settings = pending_show_devtools_params_->settings_;
    window_info = pending_show_devtools_params_->window_info_;

    // Pending params are only used a single time.
    pending_show_devtools_params_.reset();
  } else {
    // Start with the same client and settings as the opener.
    create_params.client = opener_client;
    create_params.settings = opener_browser_host->settings();

#if BUILDFLAG(IS_WIN)
    window_info.SetAsPopup(nullptr, CefString());
#endif
  }

  // Start with the same extra info as the opener, for consistency with
  // current Alloy runtime behavior (see CefDevToolsFrontend::Show). This
  // value, if non-empty, will be read-only.
  create_params.extra_info = opener_browser_host->browser_info()->extra_info();
  DCHECK(!create_params.extra_info || create_params.extra_info->IsReadOnly());

  // Use default (non-Views-hosted) window if OnBeforeDevToolsPopup is
  // unhandled.
  bool use_default_window = !life_span_handler;

  if (life_span_handler) {
    life_span_handler->OnBeforeDevToolsPopup(
        opener_browser_host.get(), window_info, create_params.client,
        create_params.settings, create_params.extra_info, &use_default_window);
  }

  if (opener_browser_host->platform_delegate()->HasExternalParent()) {
    // A parent window handle for DevTools creation is only supported if the
    // opener also has an external parent.
    create_params.MaybeSetWindowInfo(window_info);
  } else if (chrome_child_window::HasParentHandle(window_info)) {
    LOG(ERROR) << "Parent window handle not supported for this DevTools window";
  }

  create_params.popup_with_views_hosted_opener =
      CefBrowserInfoManager::ShouldCreateViewsHostedPopup(opener_browser_host,
                                                          use_default_window);

  auto platform_delegate = CefBrowserPlatformDelegate::Create(create_params);
  CHECK(platform_delegate);

  //
  // 2. Create the new browser host. Logical equivalent of WebContentsCreated()
  // for normal popups.
  //

  // Create a new browser host that remains alive until the associated
  // WebContents is destroyed. Associate that browser host with the WebContents
  // and execute initial client callbacks. Deliver required information to the
  // renderer process.
  auto browser_host = CreateBrowserHostForPopup(
      devtools_contents.get(), create_params.settings, create_params.client,
      create_params.extra_info, std::move(platform_delegate),
      /*is_devtools_popup=*/true, opener_browser_host);

  //
  // 3. Create the new Browser. Logical equivalent of AddWebContents() for
  // normal popups.
  //

  // Use Browser creation params specific to DevTools popups.
  auto chrome_params = Browser::CreateParams::CreateForDevTools(profile);

  // Pass |opener| to the ChromeBrowserDelegate constructor for the new popup
  // Browser.
  chrome_params.opener = opener;

  // Create a new Browser and give it ownership of the new WebContents.
  // Results in a call to SetAsDelegate to associate the Browser with the
  // browser host.
  browser_host->AddNewContents(std::move(devtools_contents),
                               std::move(chrome_params));

  // Give the opener browser a reference to the new DevTools browser. Do this
  // last because don't want the client to attempt access to the DevTools
  // browser via opener browser methods (e.g. ShowDevTools, CloseDevTools, etc)
  // while creation is still in progress.
  opener_browser_host->SetDevToolsBrowserHost(browser_host->GetWeakPtr());

  auto browser = browser_host->browser();
  CHECK(browser);
  return browser;
}

std::unique_ptr<content::WebContents> ChromeBrowserDelegate::AddWebContents(
    std::unique_ptr<content::WebContents> new_contents) {
  if (CefBrowserInfoManager::GetInstance()->AddWebContents(
          new_contents.get())) {
    // The browser host should have been created in WebContentsCreated().
    auto new_browser =
        ChromeBrowserHostImpl::GetBrowserForContents(new_contents.get());
    if (new_browser) {
      // Create a new Browser and give it ownership of the new WebContents.
      // Results in a call to SetAsDelegate to associate the Browser with the
      // browser host.
      new_browser->AddNewContents(std::move(new_contents), std::nullopt);
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

  const bool is_devtools_popup = browser_->is_type_devtools();

  // We should never reach here for DevTools popups that have an opener, as
  // CreateDevToolsBrowser should have already created the browser host.
  DCHECK(!is_devtools_popup || !opener_host_);

  auto platform_delegate = CefBrowserPlatformDelegate::Create(create_params_);
  CHECK(platform_delegate);

  auto browser_info = CefBrowserInfoManager::GetInstance()->CreateBrowserInfo(
      is_devtools_popup, /*is_windowless=*/false, create_params_.extra_info);

  auto request_context_impl =
      CefRequestContextImpl::GetOrCreateForRequestContext(
          create_params_.request_context);

  CreateBrowserHost(web_contents, create_params_.settings,
                    create_params_.client, std::move(platform_delegate),
                    browser_info, is_devtools_popup, /*opener=*/nullptr,
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
  // Verify that our enum matches Chromium's values.
  static_assert(static_cast<int>(CEF_WOD_MAX_VALUE) ==
                    static_cast<int>(WindowOpenDisposition::MAX_VALUE),
                "enum mismatch");

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
  if (auto cef_window_view = GetCefWindowView()) {
    cef_window_view->UpdateFindBarBoundingBox(bounds);
  }
}

void ChromeBrowserDelegate::UpdateDialogTopInset(int* dialog_top_y) {
  // This may be called during Browser initialization (before Tab/WebContents
  // creation), so we can't route through the ChromeBrowserHostImpl.
  if (auto cef_window_view = GetCefWindowView()) {
    cef_window_view->UpdateDialogTopInset(dialog_top_y);
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

std::optional<bool> ChromeBrowserDelegate::SupportsWindowFeature(
    int feature) const {
  // Override the default value from
  // Browser::PictureInPictureBrowserSupportsWindowFeature.
  if (feature == Browser::FEATURE_TITLEBAR &&
      browser_->is_type_picture_in_picture()) {
    // Return false to hide titlebar and enable draggable regions.
    return !SupportsFramelessPictureInPicture();
  }
  return std::nullopt;
}

bool ChromeBrowserDelegate::SupportsDraggableRegion() const {
  return SupportsFramelessPictureInPicture();
}

const std::optional<SkRegion> ChromeBrowserDelegate::GetDraggableRegion()
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
  if (auto cef_window_impl = GetCefWindowImpl()) {
    if (auto* delegate = cef_window_impl->delegate()) {
      // Give the CefWindowDelegate a chance to handle the event.
      delegate->OnWindowFullscreenTransition(cef_window_impl,
                                             /*is_completed=*/true);
    }
  }
#endif
}

bool ChromeBrowserDelegate::HasViewsHostedOpener() const {
  DCHECK(browser_->is_type_picture_in_picture() ||
         browser_->is_type_devtools());
  return opener_host_ && opener_host_->is_views_hosted();
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

  // Create a new browser host that remains alive until the associated
  // WebContents is destroyed. Associate that browser host with the WebContents
  // and execute initial client callbacks. Deliver required information to the
  // renderer process.
  CreateBrowserHostForPopup(new_contents, settings, client, extra_info,
                            std::move(platform_delegate),
                            /*is_devtools_popup=*/false, opener);
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

void ChromeBrowserDelegate::SetPendingShowDevToolsParams(
    std::unique_ptr<CefShowDevToolsParams> params) {
  DCHECK(!pending_show_devtools_params_);
  pending_show_devtools_params_ = std::move(params);
}

CefRefPtr<ChromeBrowserHostImpl> ChromeBrowserDelegate::CreateBrowserHost(
    content::WebContents* web_contents,
    const CefBrowserSettings& settings,
    CefRefPtr<CefClient> client,
    std::unique_ptr<CefBrowserPlatformDelegate> platform_delegate,
    scoped_refptr<CefBrowserInfo> browser_info,
    bool is_devtools_popup,
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

  // Get or create a ChromeBrowserContext for the browser Profile. Creation may
  // be necessary when selecting a new or incognito Profile for the first time
  // via the Chrome UI.
  auto chrome_browser_context =
      ChromeBrowserContext::GetOrCreateForProfile(browser_->profile());

  // If the provided CefRequestContext matches the ChromeBrowserContext then use
  // the provided one, as it will have the preferred CefRequestContextHandler.
  // Otherwise, get or create a CefRequestContext that matches.
  if (chrome_browser_context != request_context_impl->GetBrowserContext()) {
    CefRefPtr<CefRequestContextHandler> handler;
    if (auto app = CefAppManager::Get()->GetApplication()) {
      if (auto bph = app->GetBrowserProcessHandler()) {
        handler = bph->GetDefaultRequestContextHandler();
      }
    }

    request_context_impl = CefRequestContextImpl::GetOrCreateForBrowserContext(
        chrome_browser_context, handler);
  }

  // Remains alive until the associated WebContents is destroyed.
  CefRefPtr<ChromeBrowserHostImpl> browser_host =
      new ChromeBrowserHostImpl(settings, client, std::move(platform_delegate),
                                browser_info, request_context_impl);
  browser_host->Attach(web_contents, is_devtools_popup, opener);

  // The Chrome browser for a normal popup won't be created until
  // AddNewContents().
  if (!opener) {
    browser_host->SetBrowser(browser_);
  }

  return browser_host;
}

CefRefPtr<ChromeBrowserHostImpl>
ChromeBrowserDelegate::CreateBrowserHostForPopup(
    content::WebContents* web_contents,
    const CefBrowserSettings& settings,
    CefRefPtr<CefClient> client,
    CefRefPtr<CefDictionaryValue> extra_info,
    std::unique_ptr<CefBrowserPlatformDelegate> platform_delegate,
    bool is_devtools_popup,
    CefRefPtr<ChromeBrowserHostImpl> opener) {
  auto browser_info =
      CefBrowserInfoManager::GetInstance()->CreatePopupBrowserInfo(
          web_contents, /*is_windowless=*/false, extra_info);
  CHECK(browser_info->is_popup());

  // Popups must share the same RequestContext as the parent.
  auto request_context_impl = opener->request_context();
  CHECK(request_context_impl);

  // We don't officially own |web_contents| until AddNewContents() is called.
  // However, we need to install observers/delegates here.
  return CreateBrowserHost(web_contents, settings, client,
                           std::move(platform_delegate), browser_info,
                           is_devtools_popup, opener, request_context_impl);
}

CefBrowserContentsDelegate* ChromeBrowserDelegate::GetDelegateForWebContents(
    content::WebContents* web_contents) const {
  auto browser_host =
      ChromeBrowserHostImpl::GetBrowserForContents(web_contents);
  if (browser_host) {
    return browser_host->contents_delegate();
  }
  return nullptr;
}

bool ChromeBrowserDelegate::IsViewsHosted() const {
  return create_params_.browser_view != nullptr ||
         create_params_.popup_with_views_hosted_opener;
}

CefWindowImpl* ChromeBrowserDelegate::GetCefWindowImpl() const {
  if (IsViewsHosted()) {
    if (auto chrome_browser_view =
            static_cast<ChromeBrowserView*>(browser_->window())) {
      return chrome_browser_view->cef_browser_view()->cef_window_impl();
    }
  }
  return nullptr;
}

CefWindowView* ChromeBrowserDelegate::GetCefWindowView() const {
  if (auto cef_window_impl = GetCefWindowImpl()) {
    return cef_window_impl->cef_window_view();
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
