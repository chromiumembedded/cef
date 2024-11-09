// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_ROOT_WINDOW_MANAGER_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_ROOT_WINDOW_MANAGER_H_
#pragma once

#include <map>
#include <memory>
#include <set>

#include "include/cef_command_line.h"
#include "include/cef_request_context_handler.h"
#include "tests/cefclient/browser/image_cache.h"
#include "tests/cefclient/browser/root_window.h"
#include "tests/cefclient/browser/temp_window.h"

namespace client {

// Used to create/manage RootWindow instances. The methods of this class can be
// called from any browser process thread unless otherwise indicated.
class RootWindowManager : public RootWindow::Delegate {
 public:
  // If |terminate_when_all_windows_closed| is true quit the main message loop
  // after all windows have closed.
  explicit RootWindowManager(bool terminate_when_all_windows_closed);

  // Create a new top-level native window. This method can be called from
  // anywhere.
  scoped_refptr<RootWindow> CreateRootWindow(
      std::unique_ptr<RootWindowConfig> config);

  // Create a new native popup window.
  // If |with_controls| is true the window will show controls.
  // If |with_osr| is true the window will use off-screen rendering.
  // This method is called from ClientHandler::CreatePopupWindow() to
  // create a new popup or DevTools window. Must be called on the UI thread.
  scoped_refptr<RootWindow> CreateRootWindowAsPopup(
      bool use_views,
      bool use_alloy_style,
      bool with_controls,
      bool with_osr,
      int opener_browser_id,
      int popup_id,
      bool is_devtools,
      const CefPopupFeatures& popupFeatures,
      CefWindowInfo& windowInfo,
      CefRefPtr<CefClient>& client,
      CefBrowserSettings& settings);

  // Abort or close the popup matching the specified identifiers. If |popup_id|
  // is -1 then all popups for |opener_browser_id| will be impacted.
  void AbortOrClosePopup(int opener_browser_id, int popup_id);

  // Returns the RootWindow associated with the specified browser ID. Must be
  // called on the main thread.
  scoped_refptr<RootWindow> GetWindowForBrowser(int browser_id) const;

  // Returns the currently active/foreground RootWindow. May return nullptr.
  // Must be called on the main thread.
  scoped_refptr<RootWindow> GetActiveRootWindow() const;

  // Close all existing windows. If |force| is true onunload handlers will not
  // be executed.
  void CloseAllWindows(bool force);

  bool request_context_per_browser() const {
    return request_context_per_browser_;
  }

  // Track other browsers that are not directly associated with a RootWindow.
  // This may be an overlay browser, a popup created with `--use-default-popup`,
  // or a browser using default Chrome UI. |opener_browser_id| will be > 0 for
  // popup browsers.
  void OtherBrowserCreated(int browser_id, int opener_browser_id);
  void OtherBrowserClosed(int browser_id, int opener_browser_id);

 private:
  // Allow deletion via std::unique_ptr only.
  friend std::default_delete<RootWindowManager>;

  ~RootWindowManager() override;

  void OnRootWindowCreated(scoped_refptr<RootWindow> root_window);
  void OnAbortOrClosePopup(int opener_browser_id, int popup_id);

  // RootWindow::Delegate methods.
  CefRefPtr<CefRequestContext> GetRequestContext() override;
  void GetRequestContext(RequestContextCallback callback) override;
  scoped_refptr<ImageCache> GetImageCache() override;
  void OnTest(RootWindow* root_window, int test_id) override;
  void OnExit(RootWindow* root_window) override;
  void OnRootWindowDestroyed(RootWindow* root_window) override;
  void OnRootWindowActivated(RootWindow* root_window) override;

  // |callback| may be nullptr. Must be called on the main thread.
  CefRefPtr<CefRequestContext> CreateRequestContext(
      RequestContextCallback callback);

  void MaybeCleanup();
  void CleanupOnUIThread();

  const bool terminate_when_all_windows_closed_;
  bool request_context_per_browser_;
  bool request_context_shared_cache_;

  // Existing root windows. Only accessed on the main thread.
  using RootWindowSet = std::set<scoped_refptr<RootWindow>>;
  RootWindowSet root_windows_;

  // Count of browsers that are not directly associated with a RootWindow. Only
  // accessed on the main thread.
  int other_browser_ct_ = 0;

  // Map of owner browser ID to popup browser IDs for popups that don't have a
  // RootWindow. Only accessed on the main thread.
  using BrowserIdSet = std::set<int>;
  using BrowserOwnerMap = std::map<int, BrowserIdSet>;
  BrowserOwnerMap other_browser_owners_;

  // The currently active/foreground RootWindow. Only accessed on the main
  // thread.
  scoped_refptr<RootWindow> active_root_window_;

  // Singleton window used as the temporary parent for popup browsers.
  std::unique_ptr<TempWindow> temp_window_;

  CefRefPtr<CefRequestContext> shared_request_context_;

  scoped_refptr<ImageCache> image_cache_;

  DISALLOW_COPY_AND_ASSIGN(RootWindowManager);
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_ROOT_WINDOW_MANAGER_H_
