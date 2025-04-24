// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_ROOT_WINDOW_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_ROOT_WINDOW_H_
#pragma once

#include <memory>
#include <optional>
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
  explicit RootWindowConfig(CefRefPtr<CefCommandLine> command_line = nullptr);

  // Associated command-line.
  CefRefPtr<CefCommandLine> command_line;

  // If true the Views framework will be used.
  bool use_views;

  // If true Alloy style will be used. Alloy style is optional. Windowless
  // rendering requires Alloy style.
  bool use_alloy_style;

  // Configure the window type.
  WindowType window_type = WindowType::NORMAL;

  // If true the window will always display above other windows.
  bool always_on_top = false;

  // If true the window will show controls.
  bool with_controls = true;

  // If true the window will use windowless (off-screen) rendering.
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
    // Called to synchronously retrieve the CefRequestContext for browser. Only
    // called for non-popup browsers. Must be called on the main thread. This
    // method is only safe when using the global request context.
    // TODO: Delete this method and use the async version instead.
    virtual CefRefPtr<CefRequestContext> GetRequestContext() = 0;

    using RequestContextCallback =
        base::OnceCallback<void(CefRefPtr<CefRequestContext>)>;

    // Called to asynchronously retrieve the CefRequestContext for browser. Only
    // called for non-popup browsers. Save to call on any thread. |callback|
    // will be executed on the UI thread after the request context is
    // initialized.
    virtual void GetRequestContext(RequestContextCallback callback) = 0;

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

   protected:
    virtual ~Delegate() = default;
  };

  // Create a new RootWindow object. This method may be called on any thread.
  // Use RootWindowManager::CreateRootWindow() or CreateRootWindowAsPopup()
  // instead of calling this method directly. |use_views| will be true if the
  // Views framework should be used. |use_alloy_style| will be true if Alloy
  // style should be used.
  static scoped_refptr<RootWindow> Create(bool use_views, bool use_alloy_style);

  // Returns the RootWindow associated with the specified |browser_id|. Must be
  // called on the main thread.
  static scoped_refptr<RootWindow> GetForBrowser(int browser_id);

  // Returns true if the RootWindow is Views-hosted.
  virtual bool IsViewsHosted() const { return false; }

  // Returns true if the RootWindow is Alloy style, otherwise Chrome style.
  bool IsAlloyStyle() const { return use_alloy_style_; }

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

  // Set bounds in DIP screen coordinates. If |content_bounds| is true then the
  // specified bounds are for the browser's content area and will be expanded to
  // appropriate containing window bounds. Otherwise, the specified bounds are
  // for the containing window directly. Bounds will be constrained to the
  // containing display work area. Specific behavioral expectations depend on
  // platform and run mode. See the https://tests/window example for details.
  virtual void SetBounds(int x,
                         int y,
                         size_t width,
                         size_t height,
                         bool content_bounds) = 0;
  void SetBounds(const CefRect& bounds, bool content_bounds) {
    SetBounds(bounds.x, bounds.y, bounds.width, bounds.height, content_bounds);
  }

  // Returns true if this RootWindow should default to sizing by content bounds.
  virtual bool DefaultToContentBounds() const = 0;

  // Close the window. If |force| is true onunload handlers will not be
  // executed.
  virtual void Close(bool force) = 0;

  // Set the device scale factor. Only used in combination with off-screen
  // rendering.
  virtual void SetDeviceScaleFactor(float device_scale_factor) = 0;

  // Returns the device scale factor. Only used in combination with off-screen
  // rendering.
  virtual std::optional<float> GetDeviceScaleFactor() const = 0;

  // Returns the browser that this window contains, if any.
  virtual CefRefPtr<CefBrowser> GetBrowser() const = 0;

  // Returns the native handle for this window, if any.
  virtual ClientWindowHandle GetWindowHandle() const = 0;

  // Returns true if this window is using windowless rendering (osr).
  virtual bool WithWindowlessRendering() const = 0;

  // Returns true if this object has been initialized.
  bool IsInitialized() const { return initialized_; }

  // Returns true if the platform window has been created.
  bool IsWindowCreated() const;

  // Used to uniquely identify popup windows.
  void SetPopupId(int opener_browser_id, int popup_id);
  // If |popup_id| is -1 only match |opener_browser_id|.
  bool IsPopupIdMatch(int opener_browser_id, int popup_id) const;
  int opener_browser_id() const { return opener_browser_id_; }
  int popup_id() const { return popup_id_; }

 protected:
  // Allow deletion via scoped_refptr only.
  friend struct DeleteOnMainThread;
  friend class base::RefCountedThreadSafe<RootWindow, DeleteOnMainThread>;

  explicit RootWindow(bool use_alloy_style);
  virtual ~RootWindow();

  // Members set during initialization. Safe to access from any thread.
  Delegate* delegate_ = nullptr;
  bool initialized_ = false;

  // Only accessed on the main thread.
  bool window_created_ = false;

 private:
  const bool use_alloy_style_;

  // Members set during initialization. Safe to access from any thread.
  int opener_browser_id_ = 0;
  int popup_id_ = 0;
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_ROOT_WINDOW_H_
