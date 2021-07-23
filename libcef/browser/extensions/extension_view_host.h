// Copyright 2017 the Chromium Embedded Framework Authors. Portions copyright
// 2013 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_EXTENSIONS_EXTENSION_VIEW_HOST_H_
#define CEF_LIBCEF_BROWSER_EXTENSIONS_EXTENSION_VIEW_HOST_H_

#include <memory>

#include "base/macros.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/extension_host.h"

class AlloyBrowserHostImpl;

namespace content {
class WebContents;
}  // namespace content

namespace extensions {

// The ExtensionHost for an extension that backs a view in the browser UI. For
// example, this could be an extension popup or dialog, but not a background
// page. Object lifespan is managed by AlloyBrowserHostImpl. Based on
// chrome/browser/extensions/extension_view_host.h.
class CefExtensionViewHost : public ExtensionHost,
                             public content::NotificationObserver {
 public:
  CefExtensionViewHost(AlloyBrowserHostImpl* browser,
                       const Extension* extension,
                       content::WebContents* host_contents,
                       const GURL& url,
                       mojom::ViewType host_type);
  ~CefExtensionViewHost() override;

  // ExtensionHost methods:
  void OnDidStopFirstLoad() override;
  void LoadInitialURL() override;
  bool IsBackgroundPage() const override;

  // content::WebContentsDelegate methods:
  bool ShouldAllowRendererInitiatedCrossProcessNavigation(
      bool is_main_frame_navigation) override;
  bool PreHandleGestureEvent(content::WebContents* source,
                             const blink::WebGestureEvent& event) override;

  // extensions::ExtensionFunctionDispatcher::Delegate methods:
  content::WebContents* GetVisibleWebContents() const override;

  // content::NotificationObserver methods:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

 private:
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(CefExtensionViewHost);
};

}  // namespace extensions

#endif  // CEF_LIBCEF_BROWSER_EXTENSIONS_EXTENSION_VIEW_HOST_H_
