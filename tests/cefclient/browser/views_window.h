// Copyright (c) 2016 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_VIEWS_WINDOW_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_VIEWS_WINDOW_H_
#pragma once

#include <set>
#include <string>
#include <vector>

#include "include/base/cef_callback_forward.h"
#include "include/cef_menu_model_delegate.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_browser_view_delegate.h"
#include "include/views/cef_button_delegate.h"
#include "include/views/cef_label_button.h"
#include "include/views/cef_menu_button.h"
#include "include/views/cef_menu_button_delegate.h"
#include "include/views/cef_overlay_controller.h"
#include "include/views/cef_textfield.h"
#include "include/views/cef_textfield_delegate.h"
#include "include/views/cef_window.h"
#include "include/views/cef_window_delegate.h"
#include "tests/cefclient/browser/image_cache.h"
#include "tests/cefclient/browser/views_menu_bar.h"
#include "tests/cefclient/browser/views_overlay_controls.h"

namespace client {

typedef std::set<CefRefPtr<CefExtension>> ExtensionSet;

// Implements a CefWindow that hosts a single CefBrowserView and optional
// Views-based controls. All methods must be called on the browser process UI
// thread.
class ViewsWindow : public CefBrowserViewDelegate,
                    public CefMenuButtonDelegate,
                    public CefMenuModelDelegate,
                    public CefTextfieldDelegate,
                    public CefWindowDelegate,
                    public ViewsMenuBar::Delegate {
 public:
  // Delegate methods will be called on the browser process UI thread.
  class Delegate {
   public:
    // Return true if the window should show controls.
    virtual bool WithControls() = 0;

    // Return true if the window is hosting an extension.
    virtual bool WithExtension() = 0;

    // Return true if the window should be created initially hidden.
    virtual bool InitiallyHidden() = 0;

    // Returns the parent for this window.
    virtual CefRefPtr<CefWindow> GetParentWindow() = 0;

    // Return the initial window bounds.
    virtual CefRect GetWindowBounds() = 0;

    // Returns the ImageCache.
    virtual scoped_refptr<ImageCache> GetImageCache() = 0;

    // Called when the ViewsWindow is created.
    virtual void OnViewsWindowCreated(CefRefPtr<ViewsWindow> window) = 0;

    // Called when the ViewsWindow is destroyed. All references to |window|
    // should be released in this callback.
    virtual void OnViewsWindowDestroyed(CefRefPtr<ViewsWindow> window) = 0;

    // Called when the ViewsWindow is activated (becomes the foreground window).
    virtual void OnViewsWindowActivated(CefRefPtr<ViewsWindow> window) = 0;

    // Return the Delegate for the popup window controlled by |client|.
    virtual Delegate* GetDelegateForPopup(CefRefPtr<CefClient> client) = 0;

    // Create a window for |extension|. |source_bounds| are the bounds of the
    // UI element, like a button, that triggered the extension.
    virtual void CreateExtensionWindow(CefRefPtr<CefExtension> extension,
                                       const CefRect& source_bounds,
                                       CefRefPtr<CefWindow> parent_window,
                                       base::OnceClosure close_callback) = 0;

    // Called to execute a test. See resource.h for |test_id| values.
    virtual void OnTest(int test_id) = 0;

    // Called to exit the application.
    virtual void OnExit() = 0;

   protected:
    virtual ~Delegate() {}
  };

  // Create a new top-level ViewsWindow hosting a browser with the specified
  // configuration.
  static CefRefPtr<ViewsWindow> Create(
      Delegate* delegate,
      CefRefPtr<CefClient> client,
      const CefString& url,
      const CefBrowserSettings& settings,
      CefRefPtr<CefRequestContext> request_context);

  void Show();
  void Hide();
  void Minimize();
  void Maximize();
  void SetBounds(const CefRect& bounds);
  void SetBrowserSize(const CefSize& size,
                      bool has_position,
                      const CefPoint& position);
  void Close(bool force);
  void SetAddress(const std::string& url);
  void SetTitle(const std::string& title);
  void SetFavicon(CefRefPtr<CefImage> image);
  void SetFullscreen(bool fullscreen);
  void SetAlwaysOnTop(bool on_top);
  void SetLoadingState(bool isLoading, bool canGoBack, bool canGoForward);
  void SetDraggableRegions(const std::vector<CefDraggableRegion>& regions);
  void TakeFocus(bool next);
  void OnBeforeContextMenu(CefRefPtr<CefMenuModel> model);
  void OnExtensionsChanged(const ExtensionSet& extensions);

  // CefBrowserViewDelegate methods:
  CefRefPtr<CefBrowserViewDelegate> GetDelegateForPopupBrowserView(
      CefRefPtr<CefBrowserView> browser_view,
      const CefBrowserSettings& settings,
      CefRefPtr<CefClient> client,
      bool is_devtools) override;
  bool OnPopupBrowserViewCreated(CefRefPtr<CefBrowserView> browser_view,
                                 CefRefPtr<CefBrowserView> popup_browser_view,
                                 bool is_devtools) override;
  ChromeToolbarType GetChromeToolbarType() override;

  // CefButtonDelegate methods:
  void OnButtonPressed(CefRefPtr<CefButton> button) override;

  // CefMenuButtonDelegate methods:
  void OnMenuButtonPressed(
      CefRefPtr<CefMenuButton> menu_button,
      const CefPoint& screen_point,
      CefRefPtr<CefMenuButtonPressedLock> button_pressed_lock) override;

  // CefMenuModelDelegate methods:
  void ExecuteCommand(CefRefPtr<CefMenuModel> menu_model,
                      int command_id,
                      cef_event_flags_t event_flags) override;

  // CefTextfieldDelegate methods:
  bool OnKeyEvent(CefRefPtr<CefTextfield> textfield,
                  const CefKeyEvent& event) override;

  // CefWindowDelegate methods:
  void OnWindowCreated(CefRefPtr<CefWindow> window) override;
  void OnWindowDestroyed(CefRefPtr<CefWindow> window) override;
  CefRefPtr<CefWindow> GetParentWindow(CefRefPtr<CefWindow> window,
                                       bool* is_menu,
                                       bool* can_activate_menu) override;
  CefRect GetInitialBounds(CefRefPtr<CefWindow> window) override;
  cef_show_state_t GetInitialShowState(CefRefPtr<CefWindow> window) override;
  bool IsFrameless(CefRefPtr<CefWindow> window) override;
  bool CanResize(CefRefPtr<CefWindow> window) override;
  bool CanClose(CefRefPtr<CefWindow> window) override;
  bool OnAccelerator(CefRefPtr<CefWindow> window, int command_id) override;
  bool OnKeyEvent(CefRefPtr<CefWindow> window,
                  const CefKeyEvent& event) override;

  // CefViewDelegate methods:
  CefSize GetMinimumSize(CefRefPtr<CefView> view) override;
  void OnFocus(CefRefPtr<CefView> view) override;
  void OnBlur(CefRefPtr<CefView> view) override;
  void OnWindowChanged(CefRefPtr<CefView> view, bool added) override;
  void OnLayoutChanged(CefRefPtr<CefView> view,
                       const CefRect& new_bounds) override;

  // ViewsMenuBar::Delegate methods:
  void MenuBarExecuteCommand(CefRefPtr<CefMenuModel> menu_model,
                             int command_id,
                             cef_event_flags_t event_flags) override;

 private:
  // |delegate| is guaranteed to outlive this object.
  // |browser_view| may be nullptr, in which case SetBrowserView() will be
  // called.
  ViewsWindow(Delegate* delegate, CefRefPtr<CefBrowserView> browser_view);

  void SetBrowserView(CefRefPtr<CefBrowserView> browser_view);

  // Create controls.
  void CreateMenuModel();
  CefRefPtr<CefLabelButton> CreateBrowseButton(const std::string& label,
                                               int id);
  CefRefPtr<CefMenuButton> CreateMenuButton();
  CefRefPtr<CefView> CreateLocationBar();

  // Add the BrowserView to the Window.
  void AddBrowserView();

  // Add other controls to the Window.
  void AddControls();

  // Add keyboard accelerators to the Window.
  void AddAccelerators();

  // Control whether the top menu butons are focusable.
  void SetMenuFocusable(bool focusable);

  // Enable or disable a view by |id|.
  void EnableView(int id, bool enable);

  // Show/hide top controls on the Window.
  void ShowTopControls(bool show);

  // Update extension controls on the Window.
  void UpdateExtensionControls();

  void OnExtensionIconsLoaded(const ExtensionSet& extensions,
                              const ImageCache::ImageSet& images);
  void OnExtensionWindowClosed();

  CefRect GetInitialBounds() const;

  Delegate* delegate_;  // Not owned by this object.
  CefRefPtr<CefBrowserView> browser_view_;
  bool frameless_;
  bool with_controls_;
  bool with_overlay_controls_;
  ChromeToolbarType chrome_toolbar_type_;
  CefRefPtr<CefWindow> window_;

  CefRefPtr<CefMenuModel> button_menu_model_;
  CefRefPtr<ViewsMenuBar> top_menu_bar_;
  CefRefPtr<CefView> top_toolbar_;
  CefRefPtr<CefMenuButton> menu_button_;
  CefRefPtr<CefView> location_bar_;
  bool menu_has_focus_;
  int last_focused_view_;

  CefSize minimum_window_size_;
  cef_show_state_t initial_show_state_ = CEF_SHOW_STATE_NORMAL;

  CefRefPtr<ViewsOverlayControls> overlay_controls_;

  // Structure representing an extension.
  struct ExtensionInfo {
    ExtensionInfo(CefRefPtr<CefExtension> extension, CefRefPtr<CefImage> image)
        : extension_(extension), image_(image) {}

    CefRefPtr<CefExtension> extension_;
    CefRefPtr<CefImage> image_;
  };
  typedef std::vector<ExtensionInfo> ExtensionInfoSet;

  ExtensionInfoSet extensions_;
  CefRefPtr<CefPanel> extensions_panel_;
  CefRefPtr<CefMenuButtonPressedLock> extension_button_pressed_lock_;

  IMPLEMENT_REFCOUNTING(ViewsWindow);
  DISALLOW_COPY_AND_ASSIGN(ViewsWindow);
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_VIEWS_WINDOW_H_
