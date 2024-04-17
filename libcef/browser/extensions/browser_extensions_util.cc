// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/extensions/browser_extensions_util.h"

#include "libcef/browser/alloy/alloy_browser_host_impl.h"
#include "libcef/browser/browser_context.h"
#include "libcef/browser/browser_info_manager.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/extensions/extensions_util.h"
#include "libcef/features/runtime_checks.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_plugin_guest_manager.h"
#include "extensions/browser/extension_registry.h"

namespace extensions {

namespace {

content::WebContents* GetOwnerForBrowserPluginGuest(
    const content::WebContents* guest) {
  auto* guest_impl = static_cast<const content::WebContentsImpl*>(guest);
  content::BrowserPluginGuest* plugin_guest =
      guest_impl->GetBrowserPluginGuest();
  if (plugin_guest) {
    return plugin_guest->owner_web_contents();
  }
  return nullptr;
}

content::WebContents* GetInitiatorForPrintPreviewDialog(
    const content::WebContents* guest) {
  auto print_preview_controller =
      g_browser_process->print_preview_dialog_controller();
  return print_preview_controller->GetInitiator(
      const_cast<content::WebContents*>(guest));
}

}  // namespace

content::WebContents* GetOwnerForGuestContents(
    const content::WebContents* guest) {
  // Maybe it's a guest view. This occurs while loading the PDF viewer.
  if (auto* owner = GetOwnerForBrowserPluginGuest(guest)) {
    return owner;
  }

  // Maybe it's a print preview dialog. This occurs while loading the print
  // preview dialog.
  if (auto* initiator = GetInitiatorForPrintPreviewDialog(guest)) {
    // Maybe the dialog is parented to a guest view. This occurs while loading
    // the print preview dialog from inside the PDF viewer.
    if (auto* owner = GetOwnerForBrowserPluginGuest(initiator)) {
      return owner;
    }
    return initiator;
  }

  return nullptr;
}

bool IsBrowserPluginGuest(const content::WebContents* web_contents) {
  return !!GetOwnerForBrowserPluginGuest(web_contents);
}

bool IsPrintPreviewDialog(const content::WebContents* web_contents) {
  return !!GetInitiatorForPrintPreviewDialog(web_contents);
}

CefRefPtr<AlloyBrowserHostImpl> GetBrowserForTabId(
    int tab_id,
    content::BrowserContext* browser_context) {
  REQUIRE_ALLOY_RUNTIME();
  CEF_REQUIRE_UIT();
  DCHECK(browser_context);
  if (tab_id < 0 || !browser_context) {
    return nullptr;
  }

  auto cef_browser_context =
      CefBrowserContext::FromBrowserContext(browser_context);

  for (const auto& browser_info :
       CefBrowserInfoManager::GetInstance()->GetBrowserInfoList()) {
    auto current_browser =
        AlloyBrowserHostImpl::FromBaseChecked(browser_info->browser());
    if (current_browser && current_browser->GetIdentifier() == tab_id) {
      // Make sure we're operating in the same CefBrowserContext.
      if (CefBrowserContext::FromBrowserContext(
              current_browser->GetBrowserContext()) == cef_browser_context) {
        return current_browser;
      } else {
        LOG(WARNING) << "Browser with tabId " << tab_id
                     << " cannot be accessed because is uses a different "
                        "CefRequestContext";
        break;
      }
    }
  }

  return nullptr;
}

const Extension* GetExtensionForUrl(content::BrowserContext* browser_context,
                                    const GURL& url) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context);
  if (!registry) {
    return nullptr;
  }
  std::string extension_id = url.host();
  return registry->enabled_extensions().GetByID(extension_id);
}

}  // namespace extensions
