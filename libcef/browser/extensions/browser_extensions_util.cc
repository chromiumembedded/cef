// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/extensions/browser_extensions_util.h"

#include "libcef/browser/browser_context.h"
#include "libcef/browser/browser_info_manager.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/extensions/extensions_util.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "content/browser/browser_plugin/browser_plugin_embedder.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_plugin_guest_manager.h"
#include "extensions/browser/extension_registry.h"

namespace extensions {

namespace {

bool InsertWebContents(std::vector<content::WebContents*>* vector,
                       content::WebContents* web_contents) {
  vector->push_back(web_contents);
  return false;  // Continue iterating.
}

}  // namespace

content::WebContents* GetFullPageGuestForOwnerContents(
    content::WebContents* owner) {
  content::WebContentsImpl* owner_impl =
      static_cast<content::WebContentsImpl*>(owner);
  content::BrowserPluginEmbedder* plugin_embedder =
      owner_impl->GetBrowserPluginEmbedder();
  if (plugin_embedder) {
    content::BrowserPluginGuest* plugin_guest =
        plugin_embedder->GetFullPageGuest();
    if (plugin_guest)
      return plugin_guest->web_contents();
  }
  return NULL;
}

void GetAllGuestsForOwnerContents(content::WebContents* owner,
                                  std::vector<content::WebContents*>* guests) {
  content::BrowserPluginGuestManager* plugin_guest_manager =
      owner->GetBrowserContext()->GetGuestManager();
  plugin_guest_manager->ForEachGuest(owner,
                                     base::Bind(InsertWebContents, guests));
}

content::WebContents* GetOwnerForGuestContents(content::WebContents* guest) {
  content::WebContentsImpl* guest_impl =
      static_cast<content::WebContentsImpl*>(guest);
  content::BrowserPluginGuest* plugin_guest =
      guest_impl->GetBrowserPluginGuest();
  if (plugin_guest)
    return plugin_guest->embedder_web_contents();

  // Maybe it's a print preview dialog.
  auto print_preview_controller =
      g_browser_process->print_preview_dialog_controller();
  return print_preview_controller->GetInitiator(guest);
}

CefRefPtr<CefBrowserHostImpl> GetOwnerBrowserForFrameRoute(
    int render_process_id,
    int render_routing_id,
    bool* is_guest_view) {
  if (CEF_CURRENTLY_ON_UIT()) {
    // Use the non-thread-safe but potentially faster approach.
    content::RenderFrameHost* host =
        content::RenderFrameHost::FromID(render_process_id, render_routing_id);
    if (host)
      return GetOwnerBrowserForHost(host, is_guest_view);
    return NULL;
  } else {
    // Use the thread-safe approach.
    scoped_refptr<CefBrowserInfo> info =
        CefBrowserInfoManager::GetInstance()->GetBrowserInfoForFrameRoute(
            render_process_id, render_routing_id, is_guest_view);
    if (info.get()) {
      CefRefPtr<CefBrowserHostImpl> browser = info->browser();
      if (!browser.get()) {
        LOG(WARNING) << "Found browser id " << info->browser_id()
                     << " but no browser object matching view process id "
                     << render_process_id << " and frame routing id "
                     << render_routing_id;
      }
      return browser;
    }
    return NULL;
  }
}

CefRefPtr<CefBrowserHostImpl> GetOwnerBrowserForHost(
    content::RenderViewHost* host,
    bool* is_guest_view) {
  if (is_guest_view)
    *is_guest_view = false;

  CefRefPtr<CefBrowserHostImpl> browser =
      CefBrowserHostImpl::GetBrowserForHost(host);
  if (!browser.get() && ExtensionsEnabled()) {
    // Retrieve the owner browser, if any.
    content::WebContents* owner = GetOwnerForGuestContents(
        content::WebContents::FromRenderViewHost(host));
    if (owner) {
      browser = CefBrowserHostImpl::GetBrowserForContents(owner);
      if (browser.get() && is_guest_view)
        *is_guest_view = true;
    }
  }
  return browser;
}

CefRefPtr<CefBrowserHostImpl> GetOwnerBrowserForHost(
    content::RenderFrameHost* host,
    bool* is_guest_view) {
  if (is_guest_view)
    *is_guest_view = false;

  CefRefPtr<CefBrowserHostImpl> browser =
      CefBrowserHostImpl::GetBrowserForHost(host);
  if (!browser.get() && ExtensionsEnabled()) {
    // Retrieve the owner browser, if any.
    content::WebContents* owner = GetOwnerForGuestContents(
        content::WebContents::FromRenderFrameHost(host));
    if (owner) {
      browser = CefBrowserHostImpl::GetBrowserForContents(owner);
      if (browser.get() && is_guest_view)
        *is_guest_view = true;
    }
  }
  return browser;
}

CefRefPtr<CefBrowserHostImpl> GetBrowserForTabId(
    int tab_id,
    content::BrowserContext* browser_context) {
  CEF_REQUIRE_UIT();
  DCHECK(browser_context);
  if (tab_id < 0 || !browser_context)
    return nullptr;

  CefBrowserContext* browser_context_impl =
      CefBrowserContext::GetForContext(browser_context);

  for (const auto& browser_info :
       CefBrowserInfoManager::GetInstance()->GetBrowserInfoList()) {
    CefRefPtr<CefBrowserHostImpl> current_browser = browser_info->browser();
    if (current_browser && current_browser->GetIdentifier() == tab_id) {
      // Make sure we're operating in the same BrowserContextImpl.
      if (CefBrowserContext::GetForContext(
              current_browser->GetBrowserContext()) == browser_context_impl) {
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
  if (!registry)
    return nullptr;
  std::string extension_id = url.host();
  return registry->enabled_extensions().GetByID(extension_id);
}

}  // namespace extensions
