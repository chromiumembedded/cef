// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/extensions/browser_extensions_util.h"

#include "content/browser/browser_plugin/browser_plugin_embedder.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_plugin_guest_manager.h"

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
  return NULL;
}

}  // namespace extensions
