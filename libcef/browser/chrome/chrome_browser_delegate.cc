// Copyright 2020 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "libcef/browser/chrome/chrome_browser_delegate.h"

#include "libcef/browser/browser_contents_delegate.h"
#include "libcef/browser/browser_host_base.h"
#include "libcef/browser/browser_info_manager.h"
#include "libcef/browser/chrome/chrome_browser_host_impl.h"
#include "libcef/browser/request_context_impl.h"
#include "libcef/common/app_manager.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"

ChromeBrowserDelegate::ChromeBrowserDelegate(
    Browser* browser,
    const CefBrowserHostBase::CreateParams& create_params)
    : browser_(browser), create_params_(create_params) {
  DCHECK(browser_);
}

ChromeBrowserDelegate::~ChromeBrowserDelegate() = default;

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

  auto request_context_impl =
      CefRequestContextImpl::GetOrCreateForRequestContext(
          create_params_.request_context);

  // Check if chrome and CEF are using the same browser context.
  // TODO(chrome-runtime): Verify if/when this might occur.
  auto chrome_browser_context =
      CefBrowserContext::FromBrowserContext(browser_->create_params().profile);
  if (chrome_browser_context != request_context_impl->GetBrowserContext()) {
    LOG(WARNING) << "Creating a chrome browser with mismatched context";
  }

  CefRefPtr<CefClient> client = create_params_.client;
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

  auto browser_info = CefBrowserInfoManager::GetInstance()->CreateBrowserInfo(
      /*is_popup=*/false, /*is_windowless=*/false, create_params_.extra_info);

  browser_host = new ChromeBrowserHostImpl(create_params_.settings, client,
                                           browser_info, request_context_impl);
  browser_host->Attach(browser_, web_contents);
}

void ChromeBrowserDelegate::LoadingStateChanged(content::WebContents* source,
                                                bool to_different_document) {
  if (auto delegate = GetDelegateForWebContents(source)) {
    delegate->LoadingStateChanged(source, to_different_document);
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
    const base::string16& message,
    int32_t line_no,
    const base::string16& source_id) {
  if (auto delegate = GetDelegateForWebContents(source)) {
    return delegate->DidAddMessageToConsole(source, log_level, message, line_no,
                                            source_id);
  }
  return false;
}

void ChromeBrowserDelegate::DidNavigateMainFramePostCommit(
    content::WebContents* web_contents) {
  if (auto delegate = GetDelegateForWebContents(web_contents)) {
    delegate->DidNavigateMainFramePostCommit(web_contents);
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
  CefBrowserHostBase::CreateParams create_params;

  // Parameters from ChromeBrowserHostImpl::Create, or nullptr if the Browser
  // was created from somewhere else.
  auto params = static_cast<ChromeBrowserHostImpl::DelegateCreateParams*>(
      cef_params.get());
  if (params) {
    create_params = params->create_params_;
  }

  return std::make_unique<ChromeBrowserDelegate>(browser, create_params);
}

}  // namespace cef
