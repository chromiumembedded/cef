// Copyright 2015 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/extensions/extensions_api_client.h"

#include "include/internal/cef_types_wrappers.h"
#include "libcef/browser/browser_context.h"
#include "libcef/browser/extensions/api/file_system/cef_file_system_delegate.h"
#include "libcef/browser/extensions/api/storage/sync_value_store_cache.h"
#include "libcef/browser/extensions/extension_web_contents_observer.h"
#include "libcef/browser/extensions/mime_handler_view_guest_delegate.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "components/zoom/zoom_controller.h"
#include "extensions/browser/guest_view/extensions_guest_view_manager_delegate.h"
#include "printing/mojom/print.mojom.h"

namespace extensions {

CefExtensionsAPIClient::CefExtensionsAPIClient() = default;

AppViewGuestDelegate* CefExtensionsAPIClient::CreateAppViewGuestDelegate()
    const {
  // TODO(extensions): Implement to support Apps.
  DCHECK(false);
  return nullptr;
}

std::unique_ptr<guest_view::GuestViewManagerDelegate>
CefExtensionsAPIClient::CreateGuestViewManagerDelegate() const {
  // The GuestViewManager instance associated with the returned Delegate, which
  // will be retrieved in the future via GuestViewManager::FromBrowserContext,
  // will be associated with the CefBrowserContext.
  return base::WrapUnique(new extensions::ExtensionsGuestViewManagerDelegate());
}

std::unique_ptr<MimeHandlerViewGuestDelegate>
CefExtensionsAPIClient::CreateMimeHandlerViewGuestDelegate(
    MimeHandlerViewGuest* guest) const {
  return base::WrapUnique(new CefMimeHandlerViewGuestDelegate(guest));
}

void CefExtensionsAPIClient::AttachWebContentsHelpers(
    content::WebContents* web_contents) const {
  PrefsTabHelper::CreateForWebContents(web_contents);
  printing::PrintViewManager::CreateForWebContents(web_contents);

  // Used by the tabs extension API.
  zoom::ZoomController::CreateForWebContents(web_contents);
}

void CefExtensionsAPIClient::AddAdditionalValueStoreCaches(
    content::BrowserContext* context,
    const scoped_refptr<value_store::ValueStoreFactory>& factory,
    SettingsChangedCallback observer,
    std::map<settings_namespace::Namespace, ValueStoreCache*>* caches) {
  // Add support for chrome.storage.sync.
  // Because we don't support syncing with Google, we follow the behavior of
  // chrome.storage.sync as if Chrome were permanently offline, by using a local
  // store see: https://developer.chrome.com/apps/storage for more information
  (*caches)[settings_namespace::SYNC] = new cef::SyncValueStoreCache(factory);
}

FileSystemDelegate* CefExtensionsAPIClient::GetFileSystemDelegate() {
  if (!file_system_delegate_) {
    file_system_delegate_ = std::make_unique<cef::CefFileSystemDelegate>();
  }
  return file_system_delegate_.get();
}

}  // namespace extensions
