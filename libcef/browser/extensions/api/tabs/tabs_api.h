// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_EXTENSIONS_API_TABS_TABS_API_H_
#define CEF_LIBCEF_BROWSER_EXTENSIONS_API_TABS_TABS_API_H_

#include "libcef/browser/extensions/extension_function_details.h"

#include "chrome/common/extensions/api/tabs.h"
#include "extensions/browser/api/execute_code_function.h"
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

class TabsCreateFunction : public UIThreadExtensionFunction {
 public:
  TabsCreateFunction();
  ~TabsCreateFunction() override {}

  ResponseAction Run() override;

  DECLARE_EXTENSION_FUNCTION("tabs.create", TABS_CREATE)

 private:
  const CefExtensionFunctionDetails cef_details_;
};

// Implement API call tabs.executeScript and tabs.insertCSS.
class ExecuteCodeInTabFunction : public ExecuteCodeFunction {
 public:
  ExecuteCodeInTabFunction();

 protected:
  ~ExecuteCodeInTabFunction() override;

  // Initializes |execute_tab_id_| and |details_|.
  InitResult Init() override;
  bool CanExecuteScriptOnPage(std::string* error) override;
  ScriptExecutor* GetScriptExecutor(std::string* error) override;
  bool IsWebView() const override;
  const GURL& GetWebViewSrc() const override;
  bool LoadFile(const std::string& file, std::string* error) override;

 private:
  const CefExtensionFunctionDetails cef_details_;

  void LoadFileComplete(const std::string& file,
                        std::unique_ptr<std::string> data);

  // Id of tab which executes code.
  int execute_tab_id_;
};

class TabsExecuteScriptFunction : public ExecuteCodeInTabFunction {
 protected:
  bool ShouldInsertCSS() const override;

 private:
  ~TabsExecuteScriptFunction() override {}

  DECLARE_EXTENSION_FUNCTION("tabs.executeScript", TABS_EXECUTESCRIPT)
};

class TabsInsertCSSFunction : public ExecuteCodeInTabFunction {
 private:
  ~TabsInsertCSSFunction() override {}

  bool ShouldInsertCSS() const override;

  DECLARE_EXTENSION_FUNCTION("tabs.insertCSS", TABS_INSERTCSS)
};

// Based on ChromeAsyncExtensionFunction.
class ZoomAPIFunction : public UIThreadExtensionFunction {
 public:
  ZoomAPIFunction();

 protected:
  ~ZoomAPIFunction() override {}

  // Gets the WebContents for |tab_id| if it is specified. Otherwise get the
  // WebContents for the active tab in the current window. Calling this function
  // may set |error_|.
  content::WebContents* GetWebContents(int tab_id);

  virtual bool RunAsync() = 0;

  // Responds with success/failure. |results_| or |error_| should be set
  // accordingly.
  void SendResponse(bool success);

  // Exposed versions of ExtensionFunction::results_ and
  // ExtensionFunction::error_ that are curried into the response.
  // These need to keep the same name to avoid breaking existing
  // implementations, but this should be temporary with crbug.com/648275
  // and crbug.com/634140.
  std::unique_ptr<base::ListValue> results_;
  std::string error_;

 private:
  ResponseAction Run() final;

  const CefExtensionFunctionDetails cef_details_;
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
