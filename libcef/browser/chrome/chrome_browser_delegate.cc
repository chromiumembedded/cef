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
#include "libcef/browser/request_context_impl.h"
#include "libcef/common/app_manager.h"
#include "libcef/common/frame_util.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/native_web_keyboard_event.h"

using content::KeyboardEventProcessingResult;

ChromeBrowserDelegate::ChromeBrowserDelegate(
    Browser* browser,
    const CefBrowserCreateParams& create_params)
    : browser_(browser), create_params_(create_params) {
  DCHECK(browser_);
}

ChromeBrowserDelegate::~ChromeBrowserDelegate() = default;

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
      settings, client, platform_delegate, extra_info);

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

void ChromeBrowserDelegate::AddNewContents(
    content::WebContents* source_contents,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_rect,
    bool user_gesture,
    bool* was_blocked) {
  auto new_browser =
      ChromeBrowserHostImpl::GetBrowserForContents(new_contents.get());
  if (new_browser) {
    // Create a new Browser and give it ownership of the WebContents.
    new_browser->AddNewContents(std::move(new_contents));
    return;
  }

  // Fall back to default behavior from Browser::AddNewContents.
  chrome::AddWebContents(browser_, source_contents, std::move(new_contents),
                         target_url, disposition, initial_rect);
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

void ChromeBrowserDelegate::DidNavigatePrimaryMainFramePostCommit(
    content::WebContents* web_contents) {
  if (auto delegate = GetDelegateForWebContents(web_contents)) {
    delegate->DidNavigatePrimaryMainFramePostCommit(web_contents);
  }
}

void ChromeBrowserDelegate::EnterFullscreenModeForTab(
    content::RenderFrameHost* requesting_frame,
    const blink::mojom::FullscreenOptions& options) {
  auto web_contents =
      content::WebContents::FromRenderFrameHost(requesting_frame);
  if (!web_contents)
    return;

  if (auto delegate = GetDelegateForWebContents(web_contents)) {
    delegate->EnterFullscreenModeForTab(requesting_frame, options);
  }
}

void ChromeBrowserDelegate::ExitFullscreenModeForTab(
    content::WebContents* web_contents) {
  if (auto delegate = GetDelegateForWebContents(web_contents)) {
    delegate->ExitFullscreenModeForTab(web_contents);
  }
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
  if (browser_host)
    return browser_host->contents_delegate();
  return nullptr;
}

namespace cef {

// static
std::unique_ptr<BrowserDelegate> BrowserDelegate::Create(
    Browser* browser,
    scoped_refptr<CreateParams> cef_params) {
  CefBrowserCreateParams create_params;

  // Parameters from ChromeBrowserHostImpl::Create, or nullptr if the Browser
  // was created from somewhere else.
  auto params = static_cast<ChromeBrowserHostImpl::DelegateCreateParams*>(
      cef_params.get());
  if (params) {
    create_params = params->create_params_;

    // Clear these values so they're not persisted to additional Browsers.
#if defined(TOOLKIT_VIEWS)
    params->create_params_.browser_view = nullptr;
#endif
  }

  return std::make_unique<ChromeBrowserDelegate>(browser, create_params);
}

}  // namespace cef
