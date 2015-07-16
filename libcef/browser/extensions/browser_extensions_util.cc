// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/extensions/browser_extensions_util.h"

#include "content/browser/browser_plugin/browser_plugin_embedder.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/web_contents/web_contents_impl.h"

namespace extensions {

content::WebContents* GetGuestForOwnerContents(content::WebContents* guest) {
  content::WebContentsImpl* guest_impl =
      static_cast<content::WebContentsImpl*>(guest);
  content::BrowserPluginEmbedder* embedder =
      guest_impl->GetBrowserPluginEmbedder();
  if (embedder) {
    content::BrowserPluginGuest* guest = embedder->GetFullPageGuest();
    if (guest)
      return guest->web_contents();
  }
  return NULL;
}

content::WebContents* GetOwnerForGuestContents(content::WebContents* owner) {
  content::WebContentsImpl* owner_impl =
      static_cast<content::WebContentsImpl*>(owner);
  content::BrowserPluginGuest* guest = owner_impl->GetBrowserPluginGuest();
  if (guest)
    return guest->embedder_web_contents();
  return NULL;
}

}  // namespace extensions
