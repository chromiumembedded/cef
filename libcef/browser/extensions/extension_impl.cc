// Copyright 2025 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/browser/extensions/extension_impl.h"

#include "cef/libcef/browser/browser_context.h"
#include "cef/libcef/browser/browser_host_base.h"
#include "cef/libcef/browser/extensions/extension_action_impl.h"
#include "cef/libcef/browser/request_context_impl.h"
#include "cef/libcef/browser/thread_util.h"
#include "cef/libcef/common/values_impl.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/ui_util.h"
#include "extensions/common/extension.h"

namespace cef {

CefExtensionImpl::CefExtensionImpl(const extensions::Extension* extension,
                                   bool enabled,
                                   CefRefPtr<CefRequestContext> request_context)
    : id_(extension->id()),
      path_(extension->path()),
      name_(extension->name()),
      version_(extension->VersionString()),
      manifest_(extension->manifest()->value()->Clone()),
      enabled_(enabled),
      show_in_extensions_ui_(
          extensions::ui_util::ShouldDisplayInExtensionSettings(*extension)),
      request_context_(request_context) {}

CefExtensionImpl::~CefExtensionImpl() = default;

CefString CefExtensionImpl::GetIdentifier() {
  return id_;
}

CefString CefExtensionImpl::GetPath() {
  return path_.value();
}

CefString CefExtensionImpl::GetName() {
  return name_;
}

CefString CefExtensionImpl::GetVersion() {
  return version_;
}

CefRefPtr<CefDictionaryValue> CefExtensionImpl::GetManifest() {
  // The snapshot dict is captured at construction time; hand callers a
  // read-only copy so they cannot mutate our stored manifest.
  return new CefDictionaryValueImpl(manifest_.Clone(), /*read_only=*/true);
}

bool CefExtensionImpl::IsEnabled() {
  return enabled_;
}

bool CefExtensionImpl::ShouldShowInExtensionsUI() {
  return show_in_extensions_ui_;
}

CefRefPtr<CefImage> CefExtensionImpl::GetActionIcon(
    CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UIT();
  auto* impl = static_cast<CefRequestContextImpl*>(request_context_.get());
  if (!impl) {
    return nullptr;
  }
  auto* cef_browser_context = impl->GetBrowserContext();
  if (!cef_browser_context) {
    return nullptr;
  }
  auto* browser_context = cef_browser_context->AsBrowserContext();
  if (!browser_context) {
    return nullptr;
  }

  // Resolve |browser| (optional) to a session tab id so the icon factory can
  // honor per-tab icon overrides (chrome.action.setIcon({tabId})). When no
  // browser is supplied, fall back to the action's default icon.
  int tab_id = -1;
  if (browser) {
    auto host = static_cast<CefBrowserHostBase*>(browser->GetHost().get());
    if (host) {
      if (auto* web_contents = host->GetWebContents()) {
        tab_id = sessions::SessionTabHelper::IdForTab(web_contents).id();
      }
    }
  }

  return GetExtensionActionIcon(browser_context, id_, tab_id);
}

bool CefExtensionImpl::IsValid() {
  auto* impl = static_cast<CefRequestContextImpl*>(request_context_.get());
  return impl && impl->HasExtension(id_);
}

bool CefExtensionImpl::IsSame(CefRefPtr<CefExtension> that) {
  if (!that.get()) {
    return false;
  }
  return id_ == that->GetIdentifier().ToString();
}

CefRefPtr<CefRequestContext> CefExtensionImpl::GetRequestContext() {
  return request_context_;
}

void CefExtensionImpl::SetEnabled(bool enable) {
  auto* impl = static_cast<CefRequestContextImpl*>(request_context_.get());
  if (impl) {
    impl->SetExtensionEnabled(id_, enable);
  }
}

void CefExtensionImpl::Uninstall() {
  auto* impl = static_cast<CefRequestContextImpl*>(request_context_.get());
  if (impl) {
    impl->UninstallExtension(id_);
  }
}

}  // namespace cef
