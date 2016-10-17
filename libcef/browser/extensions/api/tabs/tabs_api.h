// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_EXTENSIONS_API_TABS_TABS_API_H_
#define CEF_LIBCEF_BROWSER_EXTENSIONS_API_TABS_TABS_API_H_

#include "chrome/common/extensions/api/tabs.h"
#include "extensions/browser/extension_function.h"

// The contents of this file are extracted from
// chrome/browser/extensions/api/tabs/tabs_api.h.

namespace content {
class WebContents;
}

namespace extensions {
namespace cef {

class TabsGetFunction : public UIThreadExtensionFunction {
  ~TabsGetFunction() override {}

  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("tabs.get", TABS_GET)
};

class ZoomAPIFunction : public AsyncExtensionFunction {
 protected:
  ~ZoomAPIFunction() override {}

  // Gets the WebContents for |tab_id| if it is specified. Otherwise get the
  // WebContents for the active tab in the current window. Calling this function
  // may set error_.
  //
  // TODO(...) many other tabs API functions use similar behavior. There should
  // be a way to share this implementation somehow.
  content::WebContents* GetWebContents(int tab_id);
};

class TabsSetZoomFunction : public ZoomAPIFunction {
 private:
  ~TabsSetZoomFunction() override {}

  bool RunAsync() override;

  DECLARE_EXTENSION_FUNCTION("tabs.setZoom", TABS_SETZOOM)
};

class TabsGetZoomFunction : public ZoomAPIFunction {
 private:
  ~TabsGetZoomFunction() override {}

  bool RunAsync() override;

  DECLARE_EXTENSION_FUNCTION("tabs.getZoom", TABS_GETZOOM)
};

class TabsSetZoomSettingsFunction : public ZoomAPIFunction {
 private:
  ~TabsSetZoomSettingsFunction() override {}

  bool RunAsync() override;

  DECLARE_EXTENSION_FUNCTION("tabs.setZoomSettings", TABS_SETZOOMSETTINGS)
};

class TabsGetZoomSettingsFunction : public ZoomAPIFunction {
 private:
  ~TabsGetZoomSettingsFunction() override {}

  bool RunAsync() override;

  DECLARE_EXTENSION_FUNCTION("tabs.getZoomSettings", TABS_GETZOOMSETTINGS)
};

}  // namespace cef
}  // namespace extensions

#endif  // CEF_LIBCEF_BROWSER_EXTENSIONS_API_TABS_TABS_API_H_
