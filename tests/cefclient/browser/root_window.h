// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_ROOT_WINDOW_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_ROOT_WINDOW_H_
#pragma once

#include <memory>
#include <set>
#include <string>

#include "include/base/cef_callback_forward.h"
#include "include/base/cef_ref_counted.h"
#include "include/cef_browser.h"
#include "include/cef_command_line.h"
#include "include/views/cef_window.h"
#include "tests/cefclient/browser/client_types.h"
#include "tests/cefclient/browser/image_cache.h"
#include "tests/shared/browser/main_message_loop.h"

namespace client {

enum class WindowType {
  NORMAL,

  // The window is hosting an extension app.
  EXTENSION,

  // The window is a modal dialog.
  DIALOG,

  // The window is a DevTools popup.
  DEVTOOLS,
};

// Used to configure how a RootWindow is created.
struct RootWindowConfig {
  // |command_line| will be non-nullptr when used for new window creation via
  // OnAlreadyRunningAppRelaunch. Otherwise, the global command-line will be
  // used.
  RootWindowConfig(CefRefPtr<CefCommandLine> command_line = nullptr);

  // Associated command-line.
  CefRefPtr<CefCommandLine> command_line;

  // Configure the window type.
  WindowType window_type = WindowType::NORMAL;

  // If true the window will always display above other windows.
  bool always_on_top = false;

  // If true the window will show controls.
  bool with_controls = true;

  // If true the window will use off-screen rendering.
  bool with_osr = false;

  // If true the window will be created initially hidden.
  bool initially_hidden = false;

  // Requested window position. If |bounds| and |source_bounds| are empty the
  // default window size and location will be used.
  CefRect bounds;

  // Position of the UI element that triggered the window creation. If |bounds|
  // is empty and |source_bounds| is non-empty the new window will be positioned
  // relative to |source_bounds|. This is currently only implemented for Views-
  // hosted windows when |initially_hidden| is also true.
  CefRect source_bounds;

  // Requested window show state. Only used when |bounds| is non-empty and
  // |initially_hidden| is false.
  cef_show_state_t show_state = CEF_SHOW_STATE_NORMAL;

  // Parent window. Only used for Views-hosted windows.
  CefRefPtr<CefWindow> parent_window;

  // Callback to be executed when the window is closed. Will be executed on the
  // main thread. This is currently only implemented for Views-hosted windows.
  base::OnceClosure close_callback;

  // Initial URL to load.
  std::string url;
};

typedef std::set<CefRefPtr<CefExtension>> ExtensionSet;

// Represents a top-level native window in the browser process. While references
// to this object are thread-safe the methods must be called on the main thread
// unless otherwise indicated.
class RootWindow
    : public base::RefCountedThreadSafe<RootWindow, DeleteOnMainThread> {
 public:
  // This interface is implemented by the owner of the RootWindow. The methods
  // of this class will be called on the main thread.
  class Delegate {
   public:
    // Called to retrieve the CefRequestContext for browser. Only called for
    // non-popup browsers. May return nullptr.
    virtual CefRefPtr<CefRequestContext> GetRequestContext(
        RootWindow* root_window) = 0;

    // Returns the ImageCache.
    virtual scoped_refptr<ImageCache> GetImageCache() = 0;

    // Called to execute a test. See resource.h for |test_id| values.
    virtual void OnTest(RootWindow* root_window, int test_id) = 0;

    // Called to exit the application.
    virtual void OnExit(RootWindow* root_window) = 0;

    // Called when the RootWindow has been destroyed.
    virtual void OnRootWindowDestroyed(RootWindow* root_window) = 0;

    // Called when the RootWindow is activated (becomes the foreground window).
    virtual void OnRootWindowActivated(RootWindow* root_window) = 0;

    // Called when the browser is created for the RootWindow.
    virtual void OnBrowserCreated(RootWindow* root_window,
                                  CefRefPtr<CefBrowser> browser) = 0;

    // Create a window for |extension|. |source_bounds| are the bounds of the
    // UI element, like a button, that triggered the extension.
    virtual void CreateExtensionWindow(CefRefPtr<CefExtension> extension,
                                       const CefRect& source_bounds,
                                       CefRefPtr<CefWindow> parent_window,
                                       base::OnceClosure close_callback,
                                       bool with_osr) = 0;

   protected:
    virtual ~Delegate() {}
  };

  // Create a new RootWindow object. This method may be called on any thread.
  // Use RootWindowManager::CreateRootWindow() or CreateRootWindowAsPopup()
  // instead of calling this method directly. |use_views| will be true if the
  // Views framework should be used. |parent_window| will be non-nullptr for
  // popup browsers with a RootWindow parent (on the UI thread only).
  static scoped_refptr<RootWindow> Create(
      bool use_views,
      scoped_refptr<RootWindow> parent_window);

  // Returns the RootWindow associated with the specified |browser_id|. Must be
  // called on the main thread.
  static scoped_refptr<RootWindow> GetForBrowser(int browser_id);

  // Returns true if the RootWindow is Views-hosted.
  virtual bool IsViewsHosted() const { return false; }

  // Initialize as a normal window. This will create and show a native window
  // hosting a single browser instance. This method may be called on any thread.
  // |delegate| must be non-nullptr and outlive this object.
  // Use RootWindowManager::CreateRootWindow() instead of calling this method
  // directly.
  virtual void Init(RootWindow::Delegate* delegate,
                    std::unique_ptr<RootWindowConfig> config,
                    const CefBrowserSettings& settings) = 0;

  // Initialize as a popup window. This is used to attach a new native window to
  // a single browser instance that will be created later. The native window
  // will be created and shown once the browser is available. This method may be
  // called on any thread. |delegate| must be non-nullptr and outlive this
  // object. Use RootWindowManager::CreateRootWindowAsPopup() instead of calling
  // this method directly. Called on the UI thread.
  virtual void InitAsPopup(RootWindow::Delegate* delegate,
                           bool with_controls,
                           bool with_osr,
                           const CefPopupFeatures& popupFeatures,
                           CefWindowInfo& windowInfo,
                           CefRefPtr<CefClient>& client,
                           CefBrowserSettings& settings) = 0;

  enum ShowMode {
    ShowNormal,
    ShowMinimized,
    ShowMaximized,
    ShowNoActivate,
  };

  // Show the window.
  virtual void Show(ShowMode mode) = 0;

  // Hide the window.
  virtual void Hide() = 0;

  // Set the window bounds in screen coordinates.
  virtual void SetBounds(int x, int y, size_t width, size_t height) = 0;

  // Close the window. If |force| is true onunload handlers will not be
  // executed.
  virtual void Close(bool force) = 0;

  // Set the device scale factor. Only used in combination with off-screen
  // rendering.
  virtual void SetDeviceScaleFactor(float device_scale_factor) = 0;

  // Returns the device scale factor. Only used in combination with off-screen
  // rendering.
  virtual float GetDeviceScaleFactor() const = 0;

  // Returns the browser that this window contains, if any.
  virtual CefRefPtr<CefBrowser> GetBrowser() const = 0;

  // Returns the native handle for this window, if any.
  virtual ClientWindowHandle GetWindowHandle() const = 0;

  // Returns true if this window is using windowless rendering (osr).
  virtual bool WithWindowlessRendering() const = 0;

  // Returns true if this window is hosting an extension app.
  virtual bool WithExtension() const = 0;

  // Called when the set of loaded extensions changes. The default
  // implementation will create a single window instance for each extension.
  virtual void OnExtensionsChanged(const ExtensionSet& extensions);

 protected:
  // Allow deletion via scoped_refptr only.
  friend struct DeleteOnMainThread;
  friend class base::RefCountedThreadSafe<RootWindow, DeleteOnMainThread>;

  RootWindow();
  virtual ~RootWindow();

  Delegate* delegate_;
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_ROOT_WINDOW_H_
