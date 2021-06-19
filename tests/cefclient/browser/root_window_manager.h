// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_ROOT_WINDOW_MANAGER_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_ROOT_WINDOW_MANAGER_H_
#pragma once

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
      bool with_controls,
      bool with_osr,
      const CefPopupFeatures& popupFeatures,
      CefWindowInfo& windowInfo,
      CefRefPtr<CefClient>& client,
      CefBrowserSettings& settings);

  // Create a new top-level native window to host |extension|.
  // If |with_controls| is true the window will show controls.
  // If |with_osr| is true the window will use off-screen rendering.
  // This method can be called from anywhere.
  scoped_refptr<RootWindow> CreateRootWindowAsExtension(
      CefRefPtr<CefExtension> extension,
      const CefRect& source_bounds,
      CefRefPtr<CefWindow> parent_window,
      base::OnceClosure close_callback,
      bool with_controls,
      bool with_osr);

  // Returns true if a window hosting |extension| currently exists. Must be
  // called on the main thread.
  bool HasRootWindowAsExtension(CefRefPtr<CefExtension> extension);

  // Returns the RootWindow associated with the specified browser ID. Must be
  // called on the main thread.
  scoped_refptr<RootWindow> GetWindowForBrowser(int browser_id) const;

  // Returns the currently active/foreground RootWindow. May return nullptr.
  // Must be called on the main thread.
  scoped_refptr<RootWindow> GetActiveRootWindow() const;

  // Returns the currently active/foreground browser. May return nullptr. Safe
  // to call from any thread.
  CefRefPtr<CefBrowser> GetActiveBrowser() const;

  // Close all existing windows. If |force| is true onunload handlers will not
  // be executed.
  void CloseAllWindows(bool force);

  // Manage the set of loaded extensions. RootWindows will be notified via the
  // OnExtensionsChanged method.
  void AddExtension(CefRefPtr<CefExtension> extension);

  bool request_context_per_browser() const {
    return request_context_per_browser_;
  }

 private:
  // Allow deletion via std::unique_ptr only.
  friend std::default_delete<RootWindowManager>;

  ~RootWindowManager();

  void OnRootWindowCreated(scoped_refptr<RootWindow> root_window);
  void NotifyExtensionsChanged();

  // RootWindow::Delegate methods.
  CefRefPtr<CefRequestContext> GetRequestContext(
      RootWindow* root_window) override;
  scoped_refptr<ImageCache> GetImageCache() override;
  void OnTest(RootWindow* root_window, int test_id) override;
  void OnExit(RootWindow* root_window) override;
  void OnRootWindowDestroyed(RootWindow* root_window) override;
  void OnRootWindowActivated(RootWindow* root_window) override;
  void OnBrowserCreated(RootWindow* root_window,
                        CefRefPtr<CefBrowser> browser) override;
  void CreateExtensionWindow(CefRefPtr<CefExtension> extension,
                             const CefRect& source_bounds,
                             CefRefPtr<CefWindow> parent_window,
                             base::OnceClosure close_callback,
                             bool with_osr) override;

  void CleanupOnUIThread();

  const bool terminate_when_all_windows_closed_;
  bool request_context_per_browser_;
  bool request_context_shared_cache_;

  // Existing root windows. Only accessed on the main thread.
  typedef std::set<scoped_refptr<RootWindow>> RootWindowSet;
  RootWindowSet root_windows_;

  // The currently active/foreground RootWindow. Only accessed on the main
  // thread.
  scoped_refptr<RootWindow> active_root_window_;

  // The currently active/foreground browser. Access is protected by
  // |active_browser_lock_;
  mutable base::Lock active_browser_lock_;
  CefRefPtr<CefBrowser> active_browser_;

  // Singleton window used as the temporary parent for popup browsers.
  std::unique_ptr<TempWindow> temp_window_;

  CefRefPtr<CefRequestContext> shared_request_context_;

  // Loaded extensions. Only accessed on the main thread.
  ExtensionSet extensions_;

  scoped_refptr<ImageCache> image_cache_;

  DISALLOW_COPY_AND_ASSIGN(RootWindowManager);
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_ROOT_WINDOW_MANAGER_H_
