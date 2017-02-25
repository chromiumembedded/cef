// Copyright (c) 2016 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/views_window.h"

#include <algorithm>

#include "include/base/cef_bind.h"
#include "include/base/cef_build.h"
#include "include/views/cef_box_layout.h"
#include "include/wrapper/cef_helpers.h"
#include "include/cef_app.h"
#include "tests/cefclient/browser/resource.h"
#include "tests/cefclient/browser/views_style.h"
#include "tests/shared/browser/resource_util.h"
#include "tests/shared/common/client_switches.h"

#if !defined(OS_WIN)
#define VK_RETURN 0x0D
#define VK_MENU   0x12  // ALT key.
#endif

namespace client {

namespace {

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
};

typedef std::vector<CefRefPtr<CefLabelButton> > LabelButtons;

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
  test_menu->AddItem(ID_TESTS_GETSOURCE,      "Get Source");
  test_menu->AddItem(ID_TESTS_GETTEXT,        "Get Text");
  test_menu->AddItem(ID_TESTS_WINDOW_NEW,     "New Window");
  test_menu->AddItem(ID_TESTS_WINDOW_POPUP,   "Popup Window");
  test_menu->AddItem(ID_TESTS_REQUEST,        "Request");
  test_menu->AddItem(ID_TESTS_PLUGIN_INFO,    "Plugin Info");
  test_menu->AddItem(ID_TESTS_ZOOM_IN,        "Zoom In");
  test_menu->AddItem(ID_TESTS_ZOOM_OUT,       "Zoom Out");
  test_menu->AddItem(ID_TESTS_ZOOM_RESET,     "Zoom Reset");
  test_menu->AddItem(ID_TESTS_TRACING_BEGIN,  "Begin Tracing");
  test_menu->AddItem(ID_TESTS_TRACING_END,    "End Tracing");
  test_menu->AddItem(ID_TESTS_PRINT,          "Print");
  test_menu->AddItem(ID_TESTS_PRINT_TO_PDF,   "Print to PDF");
  test_menu->AddItem(ID_TESTS_OTHER_TESTS,    "Other Tests");
}

void AddFileMenuItems(CefRefPtr<CefMenuModel> file_menu) {
  file_menu->AddItem(ID_QUIT, "E&xit");

  // Show the accelerator shortcut text in the menu.
  file_menu->SetAcceleratorAt(file_menu->GetCount() - 1,
                              'X', false, false, true);
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
  CefRefPtr<ViewsWindow> views_window = new ViewsWindow(delegate, NULL);

  // Create a new BrowserView.
  CefRefPtr<CefBrowserView> browser_view = CefBrowserView::CreateBrowserView(
      client, url, settings, request_context, views_window);

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
  if (!window_ || !with_controls_)
    return;

  CefRefPtr<CefView> view = window_->GetViewForID(ID_URL_TEXTFIELD);
  if (view && view->AsTextfield())
    view->AsTextfield()->SetText(url);
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

void ViewsWindow::SetLoadingState(bool isLoading,
                                  bool canGoBack,
                                  bool canGoForward) {
  CEF_REQUIRE_UI_THREAD();
  if (!window_ || !with_controls_)
    return;

  EnableView(ID_BACK_BUTTON, canGoBack);
  EnableView(ID_FORWARD_BUTTON, canGoForward);
  EnableView(ID_RELOAD_BUTTON, !isLoading);
  EnableView(ID_STOP_BUTTON, isLoading);
  EnableView(ID_URL_TEXTFIELD, true);
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

  window_->SetDraggableRegions(window_regions);
}

void ViewsWindow::TakeFocus(bool next) {
  CEF_REQUIRE_UI_THREAD();

  if (!window_ || !with_controls_)
    return;

  // Give focus to the URL textfield.
  window_->GetViewForID(ID_URL_TEXTFIELD)->RequestFocus();
}

void ViewsWindow::OnBeforeContextMenu(CefRefPtr<CefMenuModel> model) {
  CEF_REQUIRE_UI_THREAD();

  views_style::ApplyTo(model);
}

bool ViewsWindow::OnPopupBrowserViewCreated(
    CefRefPtr<CefBrowserView> browser_view,
    CefRefPtr<CefBrowserView> popup_browser_view,
    bool is_devtools) {
  CEF_REQUIRE_UI_THREAD();

  // The popup browser client is created in CefLifeSpanHandler::OnBeforePopup()
  // (e.g. via RootWindowViews::InitAsPopup()). The Delegate (RootWindowViews)
  // knows the association between |client| and itself.
  Delegate* popup_delegate = delegate_->GetDelegateForPopup(
      popup_browser_view->GetBrowser()->GetHost()->GetClient());

  // Create a new ViewsWindow for the popup BrowserView.
  CefRefPtr<ViewsWindow> popup_window =
      new ViewsWindow(popup_delegate, popup_browser_view);

  // Create a new top-level Window for the popup. It will show itself after
  // creation.
  CefWindow::CreateTopLevelWindow(popup_window);

  // We created the Window.
  return true;
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

void ViewsWindow::OnMenuButtonPressed(CefRefPtr<CefMenuButton> menu_button,
                                      const CefPoint& screen_point) {
  CEF_REQUIRE_UI_THREAD();
  DCHECK(with_controls_);
  DCHECK_EQ(ID_MENU_BUTTON, menu_button->GetID());

  menu_button->ShowMenu(button_menu_model_, screen_point,
                        CEF_MENU_ANCHOR_TOPRIGHT);
}

void ViewsWindow::ExecuteCommand(CefRefPtr<CefMenuModel> menu_model,
                                 int command_id,
                                 cef_event_flags_t event_flags) {
  CEF_REQUIRE_UI_THREAD();
  DCHECK(with_controls_);

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
  DCHECK(with_controls_);
  DCHECK_EQ(ID_URL_TEXTFIELD, textfield->GetID());

  // Trigger when the return key is pressed.
  if (window_ && browser_view_ &&
      event.type == KEYEVENT_RAWKEYDOWN &&
      event.windows_key_code == VK_RETURN) {
    CefRefPtr<CefBrowser> browser = browser_view_->GetBrowser();
    if (browser) {
      CefRefPtr<CefView> view = window_->GetViewForID(ID_URL_TEXTFIELD);
      if (view && view->AsTextfield()) {
        const CefString& url = view->AsTextfield()->GetText();
        if (!url.empty())
          browser->GetMainFrame()->LoadURL(url);
      }
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

  CefRect bounds = delegate_->GetWindowBounds();
  if (bounds.IsEmpty()) {
    // Use the default size.
    bounds.width = 800;
    bounds.height = 600;
  }

  if (bounds.x == 0 && bounds.y == 0) {
    // Size the Window and center it.
    window_->CenterWindow(CefSize(bounds.width, bounds.height));
  } else {
    // Set the Window bounds as specified.
    window_->SetBounds(bounds);
  }

  // Set the background color for regions that are not obscured by other Views.
  views_style::ApplyTo(window_.get());

  if (with_controls_) {
    // Add the BrowserView and other controls to the Window.
    AddControls();

    // Add keyboard accelerators to the Window.
    AddAccelerators();
  } else {
    // Add the BrowserView as the only child of the Window.
    window_->AddChildView(browser_view_);

    // Choose a reasonable minimum window size.
    minimum_window_size_ = CefSize(100, 100);
  }
 
  // Show the Window.
  window_->Show();

  // Give keyboard focus to the BrowserView.
  browser_view_->RequestFocus();
}

void ViewsWindow::OnWindowDestroyed(CefRefPtr<CefWindow> window) {
  CEF_REQUIRE_UI_THREAD();
  DCHECK(window_);

  delegate_->OnViewsWindowDestroyed(this);

  browser_view_ = NULL;
  button_menu_model_ = NULL;
  if (top_menu_bar_) {
    top_menu_bar_->Reset();
    top_menu_bar_ = NULL;
  }
  window_ = NULL;
}

bool ViewsWindow::CanClose(CefRefPtr<CefWindow> window) {
  CEF_REQUIRE_UI_THREAD();

  // Allow the window to close if the browser says it's OK.
  CefRefPtr<CefBrowser> browser = browser_view_->GetBrowser();
  if (browser)
    return browser->GetHost()->TryCloseBrowser();
  return true;
}

bool ViewsWindow::IsFrameless(CefRefPtr<CefWindow> window) {
  CEF_REQUIRE_UI_THREAD();
  return frameless_;
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

  if (!window_ || !with_controls_)
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
  frameless_ = command_line->HasSwitch(switches::kHideFrame);

  if (!command_line->HasSwitch(switches::kHideTopMenu)) {
    top_menu_bar_ = new ViewsMenuBar(this, ID_TOP_MENU_FIRST);
  }
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
    AddFileMenuItems(top_menu_bar_->CreateMenuModel("&File", NULL));
    AddTestMenuItems(top_menu_bar_->CreateMenuModel("&Tests", NULL));
  }
}

CefRefPtr<CefLabelButton> ViewsWindow::CreateBrowseButton(
    const std::string& label,
    int id) {
  // The default framed button image resources (IDR_BUTTON_*) look pretty bad
  // with non-default background colors, so we'll use frameless buttons with
  // ink drop when a background color is specified.
  const bool with_frame = !views_style::IsSet();

  CefRefPtr<CefLabelButton> button =
      CefLabelButton::CreateLabelButton(this, label, with_frame);
  button->SetID(id);
  button->SetEnabled(false);  // Disabled by default.
  button->SetFocusable(false);  // Don't give focus to the button.

  if (!with_frame) {
    views_style::ApplyTo(button);
    button->SetInkDropEnabled(true);
    button->SetHorizontalAlignment(CEF_HORIZONTAL_ALIGNMENT_CENTER);
  }

  return button;
}

void ViewsWindow::AddControls() {
  // Create the MenuModel that will be displayed via the menu button.
  CreateMenuModel();

  CefRefPtr<CefPanel> top_menu_panel;
  if (top_menu_bar_)
    top_menu_panel = top_menu_bar_->GetMenuPanel();

  // Create the browse buttons.
  LabelButtons browse_buttons;
  browse_buttons.push_back(CreateBrowseButton("Back", ID_BACK_BUTTON));
  browse_buttons.push_back(CreateBrowseButton("Forward", ID_FORWARD_BUTTON));
  browse_buttons.push_back(CreateBrowseButton("Reload", ID_RELOAD_BUTTON));
  browse_buttons.push_back(CreateBrowseButton("Stop", ID_STOP_BUTTON));

  // Create the URL textfield.
  CefRefPtr<CefTextfield> url_textfield = CefTextfield::CreateTextfield(this);
  url_textfield->SetID(ID_URL_TEXTFIELD);
  url_textfield->SetEnabled(false);  // Disabled by default.
  views_style::ApplyTo(url_textfield);

  // Create the menu button.
  CefRefPtr<CefMenuButton> menu_button =
      CefMenuButton::CreateMenuButton(this, CefString(), false, false);
  menu_button->SetID(ID_MENU_BUTTON);
  menu_button->SetImage(CEF_BUTTON_STATE_NORMAL, LoadImageIcon("menu_icon"));
  views_style::ApplyTo(menu_button.get());
  menu_button->SetInkDropEnabled(true);
  // Override the default minimum size.
  menu_button->SetMinimumSize(CefSize(0, 0));

  // Create the top panel.
  CefRefPtr<CefPanel> top_panel = CefPanel::CreatePanel(NULL);

  // Use a horizontal box layout for |top_panel|.
  CefBoxLayoutSettings top_panel_layout_settings;
  top_panel_layout_settings.horizontal = true;
  CefRefPtr<CefBoxLayout> top_panel_layout =
      top_panel->SetToBoxLayout(top_panel_layout_settings);

  // Add the buttons and URL textfield to |top_panel|.
  for (size_t i = 0U; i < browse_buttons.size(); ++i)
    top_panel->AddChildView(browse_buttons[i]);
  top_panel->AddChildView(url_textfield);
  top_panel->AddChildView(menu_button);
  views_style::ApplyTo(top_panel);

  // Allow |url_textfield| to grow and fill any remaining space.
  top_panel_layout->SetFlexForView(url_textfield, 1);

  // Use a vertical box layout for |window|.
  CefBoxLayoutSettings window_layout_settings;
  window_layout_settings.horizontal = false;
  window_layout_settings.between_child_spacing = 2;
  CefRefPtr<CefBoxLayout> window_layout =
      window_->SetToBoxLayout(window_layout_settings);

  // Add the top panel and browser view to |window|.
  if (top_menu_panel)
    window_->AddChildView(top_menu_panel);
  window_->AddChildView(top_panel);
  window_->AddChildView(browser_view_);

  // Allow |browser_view_| to grow and fill any remaining space.
  window_layout->SetFlexForView(browser_view_, 1);

  // Lay out |window| so we can get the default button sizes.
  window_->Layout();

  // Make all browse buttons the same size.
  MakeButtonsSameSize(browse_buttons);

  // Lay out |window| again with the new button sizes.
  window_->Layout();

  // Minimum window width is the size of all buttons plus some extra.
  const int min_width = browse_buttons[0]->GetBounds().width * 4 +
                        menu_button->GetBounds().width + 100;
  // Minimum window height is the hight of the top toolbar plus some extra.
  int min_height = top_panel->GetBounds().height + 100;
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
  CefRefPtr<CefView> view = window_->GetViewForID(id);
  if (view)
    view->SetEnabled(enable);
}

void ViewsWindow::ShowTopControls(bool show) {
  if (!window_ || !with_controls_)
    return;

  // Change the visibility of the panel that contains the buttons.
  CefRefPtr<CefView> parent_view =
      window_->GetViewForID(ID_BACK_BUTTON)->GetParentView();
  if (parent_view->IsVisible() != show) {
    parent_view->SetVisible(show);
    parent_view->InvalidateLayout();
  }
}

}  // namespace client
