// Copyright (c) 2016 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/views_window.h"

#include <algorithm>

#include "include/base/cef_build.h"
#include "include/base/cef_callback.h"
#include "include/cef_app.h"
#include "include/cef_i18n_util.h"
#include "include/views/cef_box_layout.h"
#include "include/wrapper/cef_helpers.h"
#include "tests/cefclient/browser/main_context.h"
#include "tests/cefclient/browser/resource.h"
#include "tests/cefclient/browser/views_style.h"
#include "tests/shared/browser/extension_util.h"
#include "tests/shared/common/client_switches.h"

#if !defined(OS_WIN)
#define VK_ESCAPE 0x1B
#define VK_RETURN 0x0D
#define VK_MENU 0x12  // ALT key.
#endif

namespace client {

namespace {

const char kDefaultExtensionIcon[] = "window_icon";

// Control IDs for Views in the top-level Window.
enum ControlIds {
  ID_WINDOW = 1,
  ID_BROWSER_VIEW,
  ID_BACK_BUTTON,
  ID_FORWARD_BUTTON,
  ID_STOP_BUTTON,
  ID_RELOAD_BUTTON,
  ID_URL_TEXTFIELD,
  ID_MENU_BUTTON,

  // Reserved range of top menu button IDs.
  ID_TOP_MENU_FIRST,
  ID_TOP_MENU_LAST = ID_TOP_MENU_FIRST + 10,

  // Reserved range of extension button IDs.
  ID_EXTENSION_BUTTON_FIRST,
  ID_EXTENSION_BUTTON_LAST = ID_EXTENSION_BUTTON_FIRST + 10,
};

typedef std::vector<CefRefPtr<CefLabelButton>> LabelButtons;

// Make all |buttons| the same size.
void MakeButtonsSameSize(const LabelButtons& buttons) {
  CefSize size;

  // Determine the largest button size.
  for (size_t i = 0U; i < buttons.size(); ++i) {
    const CefSize& button_size = buttons[i]->GetPreferredSize();
    if (size.width < button_size.width)
      size.width = button_size.width;
    if (size.height < button_size.height)
      size.height = button_size.height;
  }

  for (size_t i = 0U; i < buttons.size(); ++i) {
    // Set the button's minimum size.
    buttons[i]->SetMinimumSize(size);

    // Re-layout the button and all parent Views.
    buttons[i]->InvalidateLayout();
  }
}

void AddTestMenuItems(CefRefPtr<CefMenuModel> test_menu) {
  test_menu->AddItem(ID_TESTS_GETSOURCE, "Get Source");
  test_menu->AddItem(ID_TESTS_GETTEXT, "Get Text");
  test_menu->AddItem(ID_TESTS_WINDOW_NEW, "New Window");
  test_menu->AddItem(ID_TESTS_WINDOW_POPUP, "Popup Window");
  test_menu->AddItem(ID_TESTS_REQUEST, "Request");
  test_menu->AddItem(ID_TESTS_PLUGIN_INFO, "Plugin Info");
  test_menu->AddItem(ID_TESTS_ZOOM_IN, "Zoom In");
  test_menu->AddItem(ID_TESTS_ZOOM_OUT, "Zoom Out");
  test_menu->AddItem(ID_TESTS_ZOOM_RESET, "Zoom Reset");
  test_menu->AddItem(ID_TESTS_TRACING_BEGIN, "Begin Tracing");
  test_menu->AddItem(ID_TESTS_TRACING_END, "End Tracing");
  test_menu->AddItem(ID_TESTS_PRINT, "Print");
  test_menu->AddItem(ID_TESTS_PRINT_TO_PDF, "Print to PDF");
  test_menu->AddItem(ID_TESTS_MUTE_AUDIO, "Mute Audio");
  test_menu->AddItem(ID_TESTS_UNMUTE_AUDIO, "Unmute Audio");
  test_menu->AddItem(ID_TESTS_OTHER_TESTS, "Other Tests");
}

void AddFileMenuItems(CefRefPtr<CefMenuModel> file_menu) {
  file_menu->AddItem(ID_QUIT, "E&xit");

  // Show the accelerator shortcut text in the menu.
  file_menu->SetAcceleratorAt(file_menu->GetCount() - 1, 'X', false, false,
                              true);
}

}  // namespace

// static
CefRefPtr<ViewsWindow> ViewsWindow::Create(
    Delegate* delegate,
    CefRefPtr<CefClient> client,
    const CefString& url,
    const CefBrowserSettings& settings,
    CefRefPtr<CefRequestContext> request_context) {
  CEF_REQUIRE_UI_THREAD();
  DCHECK(delegate);

  // Create a new ViewsWindow.
  CefRefPtr<ViewsWindow> views_window = new ViewsWindow(delegate, nullptr);

  // Create a new BrowserView.
  CefRefPtr<CefBrowserView> browser_view = CefBrowserView::CreateBrowserView(
      client, url, settings, nullptr, request_context, views_window);

  // Associate the BrowserView with the ViewsWindow.
  views_window->SetBrowserView(browser_view);

  // Create a new top-level Window. It will show itself after creation.
  CefWindow::CreateTopLevelWindow(views_window);

  return views_window;
}

void ViewsWindow::Show() {
  CEF_REQUIRE_UI_THREAD();
  if (window_)
    window_->Show();
  if (browser_view_ && !window_->IsMinimized()) {
    // Give keyboard focus to the BrowserView.
    browser_view_->RequestFocus();
  }
}

void ViewsWindow::Hide() {
  CEF_REQUIRE_UI_THREAD();
  if (window_)
    window_->Hide();
}

void ViewsWindow::Minimize() {
  CEF_REQUIRE_UI_THREAD();
  if (window_)
    window_->Minimize();
}

void ViewsWindow::Maximize() {
  CEF_REQUIRE_UI_THREAD();
  if (window_)
    window_->Maximize();
}

void ViewsWindow::SetBounds(const CefRect& bounds) {
  CEF_REQUIRE_UI_THREAD();
  if (window_)
    window_->SetBounds(bounds);
}

void ViewsWindow::SetBrowserSize(const CefSize& size,
                                 bool has_position,
                                 const CefPoint& position) {
  CEF_REQUIRE_UI_THREAD();
  if (browser_view_)
    browser_view_->SetSize(size);
  if (window_) {
    window_->SizeToPreferredSize();
    if (has_position)
      window_->SetPosition(position);
  }
}

void ViewsWindow::Close(bool force) {
  CEF_REQUIRE_UI_THREAD();
  if (!browser_view_)
    return;

  CefRefPtr<CefBrowser> browser = browser_view_->GetBrowser();
  if (browser) {
    // This will result in a call to CefWindow::Close() which will then call
    // ViewsWindow::CanClose().
    browser->GetHost()->CloseBrowser(force);
  }
}

void ViewsWindow::SetAddress(const std::string& url) {
  CEF_REQUIRE_UI_THREAD();
  if (!window_)
    return;

  // |location_bar_| may instead be a Chrome toolbar.
  if (location_bar_ && location_bar_->AsTextfield())
    location_bar_->AsTextfield()->SetText(url);
}

void ViewsWindow::SetTitle(const std::string& title) {
  CEF_REQUIRE_UI_THREAD();
  if (window_)
    window_->SetTitle(title);
}

void ViewsWindow::SetFavicon(CefRefPtr<CefImage> image) {
  CEF_REQUIRE_UI_THREAD();

  // Window icons should be 16 DIP in size.
  DCHECK_EQ(std::max(image->GetWidth(), image->GetHeight()), 16U);

  if (window_)
    window_->SetWindowIcon(image);
}

void ViewsWindow::SetFullscreen(bool fullscreen) {
  CEF_REQUIRE_UI_THREAD();
  if (window_) {
    // Hide the top controls while in full-screen mode.
    if (with_controls_)
      ShowTopControls(!fullscreen);

    window_->SetFullscreen(fullscreen);
  }
}

void ViewsWindow::SetAlwaysOnTop(bool on_top) {
  CEF_REQUIRE_UI_THREAD();
  if (window_) {
    window_->SetAlwaysOnTop(on_top);
  }
}

void ViewsWindow::SetLoadingState(bool isLoading,
                                  bool canGoBack,
                                  bool canGoForward) {
  CEF_REQUIRE_UI_THREAD();
  if (!window_ || chrome_toolbar_type_ == CEF_CTT_NORMAL)
    return;

  if (with_controls_) {
    EnableView(ID_BACK_BUTTON, canGoBack);
    EnableView(ID_FORWARD_BUTTON, canGoForward);
    EnableView(ID_RELOAD_BUTTON, !isLoading);
    EnableView(ID_STOP_BUTTON, isLoading);
  }
  if (location_bar_) {
    EnableView(ID_URL_TEXTFIELD, true);
  }
}

void ViewsWindow::SetDraggableRegions(
    const std::vector<CefDraggableRegion>& regions) {
  CEF_REQUIRE_UI_THREAD();

  if (!window_ || !browser_view_)
    return;

  std::vector<CefDraggableRegion> window_regions;

  // Convert the regions from BrowserView to Window coordinates.
  std::vector<CefDraggableRegion>::const_iterator it = regions.begin();
  for (; it != regions.end(); ++it) {
    CefDraggableRegion region = *it;
    CefPoint origin = CefPoint(region.bounds.x, region.bounds.y);
    browser_view_->ConvertPointToWindow(origin);
    region.bounds.x = origin.x;
    region.bounds.y = origin.y;
    window_regions.push_back(region);
  }

  if (overlay_controls_) {
    // Exclude all regions obscured by overlays.
    overlay_controls_->UpdateDraggableRegions(window_regions);
  }

  window_->SetDraggableRegions(window_regions);
}

void ViewsWindow::TakeFocus(bool next) {
  CEF_REQUIRE_UI_THREAD();

  if (!window_)
    return;

  if (chrome_toolbar_type_ == CEF_CTT_NORMAL) {
    top_toolbar_->RequestFocus();
  } else if (location_bar_) {
    // Give focus to the location bar.
    location_bar_->RequestFocus();
  }
}

void ViewsWindow::OnBeforeContextMenu(CefRefPtr<CefMenuModel> model) {
  CEF_REQUIRE_UI_THREAD();

  views_style::ApplyTo(model);
}

void ViewsWindow::OnExtensionsChanged(const ExtensionSet& extensions) {
  CEF_REQUIRE_UI_THREAD();

  if (extensions.empty()) {
    if (!extensions_.empty()) {
      extensions_.clear();
      UpdateExtensionControls();
    }
    return;
  }

  ImageCache::ImageInfoSet image_set;

  ExtensionSet::const_iterator it = extensions.begin();
  for (; it != extensions.end(); ++it) {
    CefRefPtr<CefExtension> extension = *it;
    bool internal = false;
    const std::string& icon_path =
        extension_util::GetExtensionIconPath(extension, &internal);
    if (!icon_path.empty()) {
      // Load the extension icon.
      image_set.push_back(
          ImageCache::ImageInfo::Create1x(icon_path, icon_path, internal));
    } else {
      // Get a nullptr image and use the default icon.
      image_set.push_back(ImageCache::ImageInfo::Empty());
    }
  }

  delegate_->GetImageCache()->LoadImages(
      image_set,
      base::BindOnce(&ViewsWindow::OnExtensionIconsLoaded, this, extensions));
}

CefRefPtr<CefBrowserViewDelegate> ViewsWindow::GetDelegateForPopupBrowserView(
    CefRefPtr<CefBrowserView> browser_view,
    const CefBrowserSettings& settings,
    CefRefPtr<CefClient> client,
    bool is_devtools) {
  CEF_REQUIRE_UI_THREAD();

  // The popup browser client is created in CefLifeSpanHandler::OnBeforePopup()
  // (e.g. via RootWindowViews::InitAsPopup()). The Delegate (RootWindowViews)
  // knows the association between |client| and itself.
  Delegate* popup_delegate = delegate_->GetDelegateForPopup(client);

  // Should not be the same RootWindowViews that owns |this|.
  DCHECK(popup_delegate && popup_delegate != delegate_);

  // Create a new ViewsWindow for the popup BrowserView.
  return new ViewsWindow(popup_delegate, nullptr);
}

bool ViewsWindow::OnPopupBrowserViewCreated(
    CefRefPtr<CefBrowserView> browser_view,
    CefRefPtr<CefBrowserView> popup_browser_view,
    bool is_devtools) {
  CEF_REQUIRE_UI_THREAD();

  // Retrieve the ViewsWindow created in GetDelegateForPopupBrowserView.
  CefRefPtr<ViewsWindow> popup_window =
      static_cast<ViewsWindow*>(static_cast<CefBrowserViewDelegate*>(
          popup_browser_view->GetDelegate().get()));

  // Should not be the same ViewsWindow as |this|.
  DCHECK(popup_window && popup_window != this);

  // Associate the ViewsWindow with the new popup browser.
  popup_window->SetBrowserView(popup_browser_view);

  // Create a new top-level Window for the popup. It will show itself after
  // creation.
  CefWindow::CreateTopLevelWindow(popup_window);

  // We created the Window.
  return true;
}

CefBrowserViewDelegate::ChromeToolbarType ViewsWindow::GetChromeToolbarType() {
  return chrome_toolbar_type_;
}

void ViewsWindow::OnButtonPressed(CefRefPtr<CefButton> button) {
  CEF_REQUIRE_UI_THREAD();
  DCHECK(with_controls_);

  if (!browser_view_)
    return;

  CefRefPtr<CefBrowser> browser = browser_view_->GetBrowser();
  if (!browser)
    return;

  switch (button->GetID()) {
    case ID_BACK_BUTTON:
      browser->GoBack();
      break;
    case ID_FORWARD_BUTTON:
      browser->GoForward();
      break;
    case ID_STOP_BUTTON:
      browser->StopLoad();
      break;
    case ID_RELOAD_BUTTON:
      browser->Reload();
      break;
    case ID_MENU_BUTTON:
      break;
    default:
      NOTREACHED();
      break;
  }
}

void ViewsWindow::OnMenuButtonPressed(
    CefRefPtr<CefMenuButton> menu_button,
    const CefPoint& screen_point,
    CefRefPtr<CefMenuButtonPressedLock> button_pressed_lock) {
  CEF_REQUIRE_UI_THREAD();

  const int id = menu_button->GetID();
  if (id >= ID_EXTENSION_BUTTON_FIRST && id <= ID_EXTENSION_BUTTON_LAST) {
    const size_t extension_idx = id - ID_EXTENSION_BUTTON_FIRST;
    if (extension_idx >= extensions_.size()) {
      LOG(ERROR) << "Invalid extension index " << extension_idx;
      return;
    }

    // Keep the button pressed until the extension window is closed.
    extension_button_pressed_lock_ = button_pressed_lock;

    // Create a window for the extension.
    delegate_->CreateExtensionWindow(
        extensions_[extension_idx].extension_, menu_button->GetBoundsInScreen(),
        window_, base::BindOnce(&ViewsWindow::OnExtensionWindowClosed, this));
    return;
  }

  DCHECK(with_controls_ || with_overlay_controls_);
  DCHECK_EQ(ID_MENU_BUTTON, id);

  auto point = screen_point;
  if (with_overlay_controls_) {
    // Align the menu correctly under the button.
    const int button_width = menu_button->GetSize().width;
    if (CefIsRTL()) {
      point.x += button_width - 4;
    } else {
      point.x -= button_width - 4;
    }
  }

  menu_button->ShowMenu(button_menu_model_, point,
                        with_overlay_controls_ ? CEF_MENU_ANCHOR_TOPLEFT
                                               : CEF_MENU_ANCHOR_TOPRIGHT);
}

void ViewsWindow::ExecuteCommand(CefRefPtr<CefMenuModel> menu_model,
                                 int command_id,
                                 cef_event_flags_t event_flags) {
  CEF_REQUIRE_UI_THREAD();
  DCHECK(with_controls_ || with_overlay_controls_);

  if (command_id == ID_QUIT) {
    delegate_->OnExit();
  } else if (command_id >= ID_TESTS_FIRST && command_id <= ID_TESTS_LAST) {
    delegate_->OnTest(command_id);
  } else {
    NOTREACHED();
  }
}

bool ViewsWindow::OnKeyEvent(CefRefPtr<CefTextfield> textfield,
                             const CefKeyEvent& event) {
  CEF_REQUIRE_UI_THREAD();
  DCHECK_EQ(ID_URL_TEXTFIELD, textfield->GetID());

  // Trigger when the return key is pressed.
  if (window_ && browser_view_ && event.type == KEYEVENT_RAWKEYDOWN &&
      event.windows_key_code == VK_RETURN) {
    CefRefPtr<CefBrowser> browser = browser_view_->GetBrowser();
    if (browser) {
      const CefString& url = textfield->GetText();
      if (!url.empty())
        browser->GetMainFrame()->LoadURL(url);
    }

    // We handled the event.
    return true;
  }

  return false;
}

void ViewsWindow::OnWindowCreated(CefRefPtr<CefWindow> window) {
  CEF_REQUIRE_UI_THREAD();
  DCHECK(browser_view_);
  DCHECK(!window_);
  DCHECK(window);

  window_ = window;
  window_->SetID(ID_WINDOW);

  with_controls_ = delegate_->WithControls();

  delegate_->OnViewsWindowCreated(this);

  const CefRect bounds = GetInitialBounds();
  if (bounds.x == 0 && bounds.y == 0) {
    // Size the Window and center it.
    window_->CenterWindow(CefSize(bounds.width, bounds.height));
  } else {
    // Set the Window bounds as specified.
    window_->SetBounds(bounds);
  }

  // Set the background color for regions that are not obscured by other Views.
  views_style::ApplyTo(window_.get());

  if (with_controls_ || with_overlay_controls_) {
    // Create the MenuModel that will be displayed via the menu button.
    CreateMenuModel();
  }

  if (with_controls_) {
    // Add the BrowserView and other controls to the Window.
    AddBrowserView();

    // Add keyboard accelerators to the Window.
    AddAccelerators();

    // Hide the top controls while in full-screen mode.
    if (initial_show_state_ == CEF_SHOW_STATE_FULLSCREEN) {
      ShowTopControls(false);
    }
  } else {
    // Add the BrowserView as the only child of the Window.
    window_->AddChildView(browser_view_);

    if (!delegate_->WithExtension()) {
      // Choose a reasonable minimum window size.
      minimum_window_size_ = CefSize(100, 100);
    }
  }

  if (!delegate_->InitiallyHidden()) {
    // Show the Window.
    Show();
  }
}

void ViewsWindow::OnWindowDestroyed(CefRefPtr<CefWindow> window) {
  CEF_REQUIRE_UI_THREAD();
  DCHECK(window_);

  delegate_->OnViewsWindowDestroyed(this);

  browser_view_ = nullptr;
  button_menu_model_ = nullptr;
  if (top_menu_bar_) {
    top_menu_bar_->Reset();
    top_menu_bar_ = nullptr;
  }
  extensions_panel_ = nullptr;
  menu_button_ = nullptr;
  window_ = nullptr;
}

bool ViewsWindow::CanClose(CefRefPtr<CefWindow> window) {
  CEF_REQUIRE_UI_THREAD();

  // Allow the window to close if the browser says it's OK.
  CefRefPtr<CefBrowser> browser = browser_view_->GetBrowser();
  if (browser)
    return browser->GetHost()->TryCloseBrowser();
  return true;
}

CefRefPtr<CefWindow> ViewsWindow::GetParentWindow(CefRefPtr<CefWindow> window,
                                                  bool* is_menu,
                                                  bool* can_activate_menu) {
  CEF_REQUIRE_UI_THREAD();
  CefRefPtr<CefWindow> parent_window = delegate_->GetParentWindow();
  if (parent_window) {
    // Should be an extension window, in which case we want it to behave as a
    // menu and allow activation.
    DCHECK(delegate_->WithExtension());
    *is_menu = true;
    *can_activate_menu = true;
  }
  return parent_window;
}

CefRect ViewsWindow::GetInitialBounds(CefRefPtr<CefWindow> window) {
  CEF_REQUIRE_UI_THREAD();
  if (frameless_) {
    // Need to provide a size for frameless windows that will be centered.
    const CefRect bounds = GetInitialBounds();
    if (bounds.x == 0 && bounds.y == 0) {
      return bounds;
    }
  }
  return CefRect();
}

cef_show_state_t ViewsWindow::GetInitialShowState(CefRefPtr<CefWindow> window) {
  CEF_REQUIRE_UI_THREAD();
  return initial_show_state_;
}

bool ViewsWindow::IsFrameless(CefRefPtr<CefWindow> window) {
  CEF_REQUIRE_UI_THREAD();
  return frameless_;
}

bool ViewsWindow::CanResize(CefRefPtr<CefWindow> window) {
  CEF_REQUIRE_UI_THREAD();
  // Don't allow windows hosting extensions to resize.
  return !delegate_->WithExtension();
}

bool ViewsWindow::OnAccelerator(CefRefPtr<CefWindow> window, int command_id) {
  CEF_REQUIRE_UI_THREAD();

  if (command_id == ID_QUIT) {
    delegate_->OnExit();
    return true;
  }

  return false;
}

bool ViewsWindow::OnKeyEvent(CefRefPtr<CefWindow> window,
                             const CefKeyEvent& event) {
  CEF_REQUIRE_UI_THREAD();

  if (!window_)
    return false;

  if (delegate_->WithExtension() && event.type == KEYEVENT_RAWKEYDOWN &&
      event.windows_key_code == VK_ESCAPE) {
    // Close the extension window on escape.
    Close(false);
    return true;
  }

  if (!with_controls_)
    return false;

  if (event.type == KEYEVENT_RAWKEYDOWN && event.windows_key_code == VK_MENU) {
    // ALT key is pressed.
    int last_focused_view = last_focused_view_;
    bool menu_had_focus = menu_has_focus_;

    // Toggle menu button focusable.
    SetMenuFocusable(!menu_has_focus_);

    if (menu_had_focus && last_focused_view != 0) {
      // Restore focus to the view that was previously focused.
      window_->GetViewForID(last_focused_view)->RequestFocus();
    }

    return true;
  }

  if (menu_has_focus_ && top_menu_bar_)
    return top_menu_bar_->OnKeyEvent(event);

  return false;
}

CefSize ViewsWindow::GetMinimumSize(CefRefPtr<CefView> view) {
  CEF_REQUIRE_UI_THREAD();

  if (view->GetID() == ID_WINDOW)
    return minimum_window_size_;

  return CefSize();
}

void ViewsWindow::OnFocus(CefRefPtr<CefView> view) {
  CEF_REQUIRE_UI_THREAD();

  const int view_id = view->GetID();

  // Keep track of the non-menu view that was last focused.
  if (last_focused_view_ != view_id &&
      (!top_menu_bar_ || !top_menu_bar_->HasMenuId(view_id))) {
    last_focused_view_ = view_id;
  }

  // When focus leaves the menu buttons make them unfocusable.
  if (menu_has_focus_) {
    if (top_menu_bar_) {
      if (!top_menu_bar_->HasMenuId(view_id))
        SetMenuFocusable(false);
    } else if (view_id != ID_MENU_BUTTON) {
      SetMenuFocusable(false);
    }
  }

  if (view_id == ID_BROWSER_VIEW)
    delegate_->OnViewsWindowActivated(this);
}

void ViewsWindow::OnBlur(CefRefPtr<CefView> view) {
  CEF_REQUIRE_UI_THREAD();

  const int view_id = view->GetID();
  if (view_id == ID_BROWSER_VIEW && delegate_->WithExtension()) {
    // Close windows hosting extensions when the browser loses focus.
    Close(false);
  }
}

void ViewsWindow::OnWindowChanged(CefRefPtr<CefView> view, bool added) {
  const int view_id = view->GetID();
  if (view_id != ID_BROWSER_VIEW)
    return;

  if (added) {
    if (with_controls_) {
      AddControls();
    }

    if (with_overlay_controls_) {
      overlay_controls_ = new ViewsOverlayControls();
      overlay_controls_->Initialize(window_, CreateMenuButton(),
                                    CreateLocationBar(),
                                    chrome_toolbar_type_ != CEF_CTT_NONE);
    }
  } else {
    if (overlay_controls_) {
      // Overlay controls may include the Chrome toolbar, in which case they
      // need to be removed before the BrowserView.
      overlay_controls_->Destroy();
      overlay_controls_ = nullptr;
      location_bar_ = nullptr;
    }
  }
}

void ViewsWindow::OnLayoutChanged(CefRefPtr<CefView> view,
                                  const CefRect& new_bounds) {
  const int view_id = view->GetID();
  if (view_id != ID_BROWSER_VIEW)
    return;

  if (overlay_controls_) {
    overlay_controls_->UpdateControls();
  }
}

void ViewsWindow::MenuBarExecuteCommand(CefRefPtr<CefMenuModel> menu_model,
                                        int command_id,
                                        cef_event_flags_t event_flags) {
  ExecuteCommand(menu_model, command_id, event_flags);
}

ViewsWindow::ViewsWindow(Delegate* delegate,
                         CefRefPtr<CefBrowserView> browser_view)
    : delegate_(delegate),
      with_controls_(false),
      menu_has_focus_(false),
      last_focused_view_(false) {
  DCHECK(delegate_);
  if (browser_view)
    SetBrowserView(browser_view);

  CefRefPtr<CefCommandLine> command_line =
      CefCommandLine::GetGlobalCommandLine();

  const bool hide_frame = command_line->HasSwitch(switches::kHideFrame);
  const bool hide_overlays = command_line->HasSwitch(switches::kHideOverlays);

  // Without a window frame.
  frameless_ = hide_frame || delegate_->WithExtension();

  // With an overlay that mimics window controls.
  with_overlay_controls_ =
      hide_frame && !hide_overlays && !delegate_->WithControls();

  if (MainContext::Get()->UseChromeRuntime()) {
    const std::string& toolbar_type =
        command_line->GetSwitchValue(switches::kShowChromeToolbar);
    if (toolbar_type == "none") {
      chrome_toolbar_type_ = CEF_CTT_NONE;
    } else if (toolbar_type == "location") {
      chrome_toolbar_type_ = CEF_CTT_LOCATION;
    } else {
      chrome_toolbar_type_ =
          with_overlay_controls_ ? CEF_CTT_LOCATION : CEF_CTT_NORMAL;
    }
  } else {
    chrome_toolbar_type_ = CEF_CTT_NONE;
  }

  const std::string& show_state =
      command_line->GetSwitchValue(switches::kInitialShowState);
  if (show_state == "minimized") {
    initial_show_state_ = CEF_SHOW_STATE_MINIMIZED;
  } else if (show_state == "maximized") {
    initial_show_state_ = CEF_SHOW_STATE_MAXIMIZED;
  } else if (show_state == "fullscreen") {
    initial_show_state_ = CEF_SHOW_STATE_FULLSCREEN;
  }

#if !defined(OS_MAC)
  // On Mac we don't show a top menu on the window. The options are available in
  // the app menu instead.
  if (!command_line->HasSwitch(switches::kHideTopMenu)) {
    top_menu_bar_ = new ViewsMenuBar(this, ID_TOP_MENU_FIRST);
  }
#endif
}

void ViewsWindow::SetBrowserView(CefRefPtr<CefBrowserView> browser_view) {
  DCHECK(!browser_view_);
  DCHECK(browser_view);
  DCHECK(browser_view->IsValid());
  DCHECK(!browser_view->IsAttached());
  browser_view_ = browser_view;
  browser_view_->SetID(ID_BROWSER_VIEW);
}

void ViewsWindow::CreateMenuModel() {
  // Create the menu button model.
  button_menu_model_ = CefMenuModel::CreateMenuModel(this);
  CefRefPtr<CefMenuModel> test_menu =
      button_menu_model_->AddSubMenu(0, "&Tests");
  views_style::ApplyTo(button_menu_model_);
  AddTestMenuItems(test_menu);
  AddFileMenuItems(button_menu_model_);

  if (top_menu_bar_) {
    // Add the menus to the top menu bar.
    AddFileMenuItems(top_menu_bar_->CreateMenuModel("&File", nullptr));
    AddTestMenuItems(top_menu_bar_->CreateMenuModel("&Tests", nullptr));
  }
}

CefRefPtr<CefLabelButton> ViewsWindow::CreateBrowseButton(
    const std::string& label,
    int id) {
  CefRefPtr<CefLabelButton> button =
      CefLabelButton::CreateLabelButton(this, label);
  button->SetID(id);
  views_style::ApplyTo(button.get());
  button->SetInkDropEnabled(true);
  button->SetEnabled(false);    // Disabled by default.
  button->SetFocusable(false);  // Don't give focus to the button.

  return button;
}

CefRefPtr<CefMenuButton> ViewsWindow::CreateMenuButton() {
  // Create the menu button.
  DCHECK(!menu_button_);
  menu_button_ = CefMenuButton::CreateMenuButton(this, CefString());
  menu_button_->SetID(ID_MENU_BUTTON);
  menu_button_->SetImage(
      CEF_BUTTON_STATE_NORMAL,
      delegate_->GetImageCache()->GetCachedImage("menu_icon"));
  views_style::ApplyTo(menu_button_.get());
  menu_button_->SetInkDropEnabled(true);
  // Override the default minimum size.
  menu_button_->SetMinimumSize(CefSize(0, 0));
  return menu_button_;
}

CefRefPtr<CefView> ViewsWindow::CreateLocationBar() {
  DCHECK(!location_bar_);
  if (chrome_toolbar_type_ == CEF_CTT_LOCATION) {
    // Chrome will provide a minimal location bar.
    location_bar_ = browser_view_->GetChromeToolbar();
    DCHECK(location_bar_);
    views_style::ApplyBackgroundTo(location_bar_);
  }
  if (!location_bar_) {
    // Create the URL textfield.
    CefRefPtr<CefTextfield> url_textfield = CefTextfield::CreateTextfield(this);
    url_textfield->SetID(ID_URL_TEXTFIELD);
    url_textfield->SetEnabled(false);  // Disabled by default.
    views_style::ApplyTo(url_textfield);
    location_bar_ = url_textfield;
  }
  return location_bar_;
}

void ViewsWindow::AddBrowserView() {
  // Use a vertical box layout for |window|.
  CefBoxLayoutSettings window_layout_settings;
  window_layout_settings.horizontal = false;
  window_layout_settings.between_child_spacing = 2;
  CefRefPtr<CefBoxLayout> window_layout =
      window_->SetToBoxLayout(window_layout_settings);

  window_->AddChildView(browser_view_);

  // Allow |browser_view_| to grow and fill any remaining space.
  window_layout->SetFlexForView(browser_view_, 1);

  // Remaining setup will be performed in OnWindowChanged after the BrowserView
  // is added to the CefWindow. This is necessary because Chrome toolbars are
  // only available after the BrowserView is added.
}

void ViewsWindow::AddControls() {
  // Build the remainder of the UI now that the BrowserView has been added to
  // the CefWindow. This is a requirement to use Chrome toolbars.

  CefRefPtr<CefPanel> top_menu_panel;
  if (top_menu_bar_)
    top_menu_panel = top_menu_bar_->GetMenuPanel();

  LabelButtons browse_buttons;

  if (chrome_toolbar_type_ == CEF_CTT_NORMAL) {
    // Chrome will provide a normal toolbar with location, menu, etc.
    top_toolbar_ = browser_view_->GetChromeToolbar();
    DCHECK(top_toolbar_);
  }

  if (!top_toolbar_) {
    // Create the browse buttons.
    browse_buttons.push_back(CreateBrowseButton("Back", ID_BACK_BUTTON));
    browse_buttons.push_back(CreateBrowseButton("Forward", ID_FORWARD_BUTTON));
    browse_buttons.push_back(CreateBrowseButton("Reload", ID_RELOAD_BUTTON));
    browse_buttons.push_back(CreateBrowseButton("Stop", ID_STOP_BUTTON));

    CreateLocationBar();
    CreateMenuButton();

    // Create the top panel.
    CefRefPtr<CefPanel> top_panel = CefPanel::CreatePanel(nullptr);

    // Use a horizontal box layout for |top_panel|.
    CefBoxLayoutSettings top_panel_layout_settings;
    top_panel_layout_settings.horizontal = true;
    CefRefPtr<CefBoxLayout> top_panel_layout =
        top_panel->SetToBoxLayout(top_panel_layout_settings);

    // Add the buttons and URL textfield to |top_panel|.
    for (size_t i = 0U; i < browse_buttons.size(); ++i)
      top_panel->AddChildView(browse_buttons[i]);
    top_panel->AddChildView(location_bar_);

    UpdateExtensionControls();
    DCHECK(extensions_panel_);
    top_panel->AddChildView(extensions_panel_);

    top_panel->AddChildView(menu_button_);
    views_style::ApplyTo(top_panel);

    // Allow |location| to grow and fill any remaining space.
    top_panel_layout->SetFlexForView(location_bar_, 1);

    top_toolbar_ = top_panel;
  }

  // Add the top panel and browser view to |window|.
  int top_index = 0;
  if (top_menu_panel)
    window_->AddChildViewAt(top_menu_panel, top_index++);
  window_->AddChildViewAt(top_toolbar_, top_index);

  // Lay out |window| so we can get the default button sizes.
  window_->Layout();

  int min_width = 200;
  if (!browse_buttons.empty()) {
    // Make all browse buttons the same size.
    MakeButtonsSameSize(browse_buttons);

    // Lay out |window| again with the new button sizes.
    window_->Layout();

    // Minimum window width is the size of all buttons plus some extra.
    min_width = browse_buttons[0]->GetBounds().width * 4 +
                menu_button_->GetBounds().width + 100;
  }

  // Minimum window height is the hight of the top toolbar plus some extra.
  int min_height = top_toolbar_->GetBounds().height + 100;
  if (top_menu_panel)
    min_height += top_menu_panel->GetBounds().height;

  minimum_window_size_ = CefSize(min_width, min_height);
}

void ViewsWindow::AddAccelerators() {
  // Trigger accelerators without first forwarding to web content.
  browser_view_->SetPreferAccelerators(true);

  // Specify the accelerators to handle. OnAccelerator will be called when the
  // accelerator is triggered.
  window_->SetAccelerator(ID_QUIT, 'X', false, false, true);
}

void ViewsWindow::SetMenuFocusable(bool focusable) {
  if (!window_ || !with_controls_)
    return;

  if (top_menu_bar_) {
    top_menu_bar_->SetMenuFocusable(focusable);
  } else {
    window_->GetViewForID(ID_MENU_BUTTON)->SetFocusable(focusable);

    if (focusable) {
      // Give focus to menu button.
      window_->GetViewForID(ID_MENU_BUTTON)->RequestFocus();
    }
  }

  menu_has_focus_ = focusable;
}

void ViewsWindow::EnableView(int id, bool enable) {
  if (!window_)
    return;
  // Special handling for |location_bar_| which may be an overlay (e.g. not a
  // child of this view).
  CefRefPtr<CefView> view =
      id == ID_URL_TEXTFIELD ? location_bar_ : window_->GetViewForID(id);
  if (view)
    view->SetEnabled(enable);
}

void ViewsWindow::ShowTopControls(bool show) {
  if (!window_ || !with_controls_)
    return;

  // Change the visibility of the top toolbar.
  if (top_toolbar_->IsVisible() != show) {
    top_toolbar_->SetVisible(show);
    top_toolbar_->InvalidateLayout();
  }
}

void ViewsWindow::UpdateExtensionControls() {
  CEF_REQUIRE_UI_THREAD();

  if (!window_ || !with_controls_)
    return;

  if (!extensions_panel_) {
    extensions_panel_ = CefPanel::CreatePanel(nullptr);

    // Use a horizontal box layout for |top_panel|.
    CefBoxLayoutSettings top_panel_layout_settings;
    top_panel_layout_settings.horizontal = true;
    CefRefPtr<CefBoxLayout> top_panel_layout =
        extensions_panel_->SetToBoxLayout(top_panel_layout_settings);
  } else {
    extensions_panel_->RemoveAllChildViews();
  }

  if (extensions_.size() >
      ID_EXTENSION_BUTTON_LAST - ID_EXTENSION_BUTTON_FIRST) {
    LOG(WARNING) << "Too many extensions loaded. Some will be ignored.";
  }

  ExtensionInfoSet::const_iterator it = extensions_.begin();
  for (int id = ID_EXTENSION_BUTTON_FIRST;
       it != extensions_.end() && id <= ID_EXTENSION_BUTTON_LAST; ++id, ++it) {
    CefRefPtr<CefMenuButton> button =
        CefMenuButton::CreateMenuButton(this, CefString());
    button->SetID(id);
    button->SetImage(CEF_BUTTON_STATE_NORMAL, (*it).image_);
    views_style::ApplyTo(button.get());
    button->SetInkDropEnabled(true);
    // Override the default minimum size.
    button->SetMinimumSize(CefSize(0, 0));

    extensions_panel_->AddChildView(button);
  }

  CefRefPtr<CefView> parent_view = extensions_panel_->GetParentView();
  if (parent_view)
    parent_view->InvalidateLayout();
}

void ViewsWindow::OnExtensionIconsLoaded(const ExtensionSet& extensions,
                                         const ImageCache::ImageSet& images) {
  if (!CefCurrentlyOn(TID_UI)) {
    // Execute this method on the UI thread.
    CefPostTask(TID_UI, base::BindOnce(&ViewsWindow::OnExtensionIconsLoaded,
                                       this, extensions, images));
    return;
  }

  DCHECK_EQ(extensions.size(), images.size());

  extensions_.clear();

  ExtensionSet::const_iterator it1 = extensions.begin();
  ImageCache::ImageSet::const_iterator it2 = images.begin();
  for (; it1 != extensions.end() && it2 != images.end(); ++it1, ++it2) {
    CefRefPtr<CefImage> icon = *it2;
    if (!icon)
      icon = delegate_->GetImageCache()->GetCachedImage(kDefaultExtensionIcon);
    extensions_.push_back(ExtensionInfo(*it1, icon));
  }

  UpdateExtensionControls();
}

void ViewsWindow::OnExtensionWindowClosed() {
  if (!CefCurrentlyOn(TID_UI)) {
    // Execute this method on the UI thread.
    CefPostTask(TID_UI,
                base::BindOnce(&ViewsWindow::OnExtensionWindowClosed, this));
    return;
  }

  // Restore the button state.
  extension_button_pressed_lock_ = nullptr;
}

CefRect ViewsWindow::GetInitialBounds() const {
  CEF_REQUIRE_UI_THREAD();
  CefRect bounds = delegate_->GetWindowBounds();
  if (bounds.IsEmpty()) {
    // Use the default size.
    bounds.width = 800;
    bounds.height = 600;
  }
  return bounds;
}

}  // namespace client
