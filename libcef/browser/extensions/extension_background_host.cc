// Copyright 2017 the Chromium Embedded Framework Authors. Portions copyright
// 2013 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#include "libcef/browser/extensions/extension_background_host.h"

#include "libcef/browser/alloy/alloy_browser_host_impl.h"
#include "libcef/browser/extensions/extension_host_delegate.h"

#include "base/functional/callback.h"

namespace extensions {

CefExtensionBackgroundHost::CefExtensionBackgroundHost(
    AlloyBrowserHostImpl* browser,
    base::OnceClosure deleted_callback,
    const Extension* extension,
    content::WebContents* host_contents,
    const GURL& url,
    mojom::ViewType host_type)
    : ExtensionHost(new CefExtensionHostDelegate(browser),
                    extension,
                    host_contents->GetBrowserContext(),
                    host_contents,
                    url,
                    host_type),
      deleted_callback_(std::move(deleted_callback)) {
  DCHECK(!deleted_callback_.is_null());

  // Only used for background pages.
  DCHECK(host_type == mojom::ViewType::kExtensionBackgroundPage);
}

CefExtensionBackgroundHost::~CefExtensionBackgroundHost() {
  std::move(deleted_callback_).Run();
}

bool CefExtensionBackgroundHost::
    ShouldAllowRendererInitiatedCrossProcessNavigation(
        bool is_main_frame_navigation) {
  // Block navigations that cause the main frame to navigate to non-extension
  // content (i.e. to web content).
  return !is_main_frame_navigation;
}

}  // namespace extensions
