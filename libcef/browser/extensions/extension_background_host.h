// Copyright 2017 the Chromium Embedded Framework Authors. Portions copyright
// 2013 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_EXTENSIONS_EXTENSION_BACKGROUND_HOST_H_
#define CEF_LIBCEF_BROWSER_EXTENSIONS_EXTENSION_BACKGROUND_HOST_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "extensions/browser/extension_host.h"

class AlloyBrowserHostImpl;

namespace content {
class WebContents;
}  // namespace content

namespace extensions {

// The ExtensionHost for a background page. This is a thin wrapper around the
// ExtensionHost base class to support CEF-specific constructor. Object lifespan
// is managed by ProcessManager.
class CefExtensionBackgroundHost : public ExtensionHost {
 public:
  CefExtensionBackgroundHost(AlloyBrowserHostImpl* browser,
                             base::OnceClosure deleted_callback,
                             const Extension* extension,
                             content::WebContents* host_contents,
                             const GURL& url,
                             mojom::ViewType host_type);

  CefExtensionBackgroundHost(const CefExtensionBackgroundHost&) = delete;
  CefExtensionBackgroundHost& operator=(const CefExtensionBackgroundHost&) =
      delete;

  ~CefExtensionBackgroundHost() override;

  // content::WebContentsDelegate methods:
  bool ShouldAllowRendererInitiatedCrossProcessNavigation(
      bool is_main_frame_navigation) override;

 private:
  // Callback that will be executed on host deletion.
  base::OnceClosure deleted_callback_;
};

}  // namespace extensions

#endif  // CEF_LIBCEF_BROWSER_EXTENSIONS_EXTENSION_BACKGROUND_HOST_H_
