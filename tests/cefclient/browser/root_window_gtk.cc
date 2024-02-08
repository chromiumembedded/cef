// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/root_window_gtk.h"

#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include <X11/Xlib.h>
#undef Success     // Definition conflicts with cef_message_router.h
#undef RootWindow  // Definition conflicts with root_window.h

#include "include/base/cef_callback.h"
#include "include/cef_app.h"
#include "tests/cefclient/browser/browser_window_osr_gtk.h"
#include "tests/cefclient/browser/browser_window_std_gtk.h"
#include "tests/cefclient/browser/main_context.h"
#include "tests/cefclient/browser/resource.h"
#include "tests/cefclient/browser/temp_window.h"
#include "tests/cefclient/browser/util_gtk.h"
#include "tests/cefclient/browser/window_test_runner_gtk.h"
#include "tests/shared/browser/main_message_loop.h"
#include "tests/shared/common/client_switches.h"

namespace client {

namespace {

const char kMenuIdKey[] = "menu_id";

void UseDefaultX11VisualForGtk(GtkWidget* widget) {
#if GTK_CHECK_VERSION(3, 15, 1)
  // GTK+ > 3.15.1 uses an X11 visual optimized for GTK+'s OpenGL stuff
  // since revid dae447728d: https://github.com/GNOME/gtk/commit/dae447728d
  // However, it breaks CEF: https://github.com/cztomczak/cefcapi/issues/9
  // Let's use the default X11 visual instead of the GTK's blessed one.
  // Copied from: https://github.com/cztomczak/cefcapi.
  GdkScreen* screen = gdk_screen_get_default();
  GList* visuals = gdk_screen_list_visuals(screen);

  GdkX11Screen* x11_screen = GDK_X11_SCREEN(screen);
  if (x11_screen == nullptr) {
    return;
  }

  Visual* default_xvisual = DefaultVisual(GDK_SCREEN_XDISPLAY(x11_screen),
                                          GDK_SCREEN_XNUMBER(x11_screen));
  GList* cursor = visuals;
  while (cursor != nullptr) {
    GdkVisual* visual = GDK_X11_VISUAL(cursor->data);
    if (default_xvisual->visualid ==
        gdk_x11_visual_get_xvisual(visual)->visualid) {
      gtk_widget_set_visual(widget, visual);
      break;
    }
    cursor = cursor->next;
  }
  g_list_free(visuals);
#endif
}

bool IsWindowMaximized(GtkWindow* window) {
  GdkWindow* gdk_window = gtk_widget_get_window(GTK_WIDGET(window));
  gint state = gdk_window_get_state(gdk_window);
  return (state & GDK_WINDOW_STATE_MAXIMIZED) ? true : false;
}

void MinimizeWindow(GtkWindow* window) {
  // Unmaximize the window before minimizing so restore behaves correctly.
  if (IsWindowMaximized(window)) {
    gtk_window_unmaximize(window);
  }

  gtk_window_iconify(window);
}

void MaximizeWindow(GtkWindow* window) {
  gtk_window_maximize(window);
}

}  // namespace

RootWindowGtk::RootWindowGtk()
    : with_controls_(false),
      always_on_top_(false),
      with_osr_(false),
      with_extension_(false),
      is_popup_(false),
      initialized_(false),
      window_(nullptr),
      back_button_(nullptr),
      forward_button_(nullptr),
      reload_button_(nullptr),
      stop_button_(nullptr),
      url_entry_(nullptr),
      toolbar_height_(0),
      menubar_height_(0),
      window_destroyed_(false),
      browser_destroyed_(false),
      force_close_(false),
      is_closing_(false) {}

RootWindowGtk::~RootWindowGtk() {
  REQUIRE_MAIN_THREAD();

  // The window and browser should already have been destroyed.
  DCHECK(window_destroyed_);
  DCHECK(browser_destroyed_);
}

void RootWindowGtk::Init(RootWindow::Delegate* delegate,
                         std::unique_ptr<RootWindowConfig> config,
                         const CefBrowserSettings& settings) {
  DCHECK(delegate);
  DCHECK(!initialized_);

  delegate_ = delegate;
  with_controls_ = config->with_controls;
  always_on_top_ = config->always_on_top;
  with_osr_ = config->with_osr;
  with_extension_ = config->window_type == WindowType::EXTENSION;
  start_rect_ = config->bounds;

  CreateBrowserWindow(config->url);

  initialized_ = true;

  // Always post asynchronously to avoid reentrancy of the GDK lock.
  MAIN_POST_CLOSURE(base::BindOnce(&RootWindowGtk::CreateRootWindow, this,
                                   settings, config->initially_hidden));
}

void RootWindowGtk::InitAsPopup(RootWindow::Delegate* delegate,
                                bool with_controls,
                                bool with_osr,
                                const CefPopupFeatures& popupFeatures,
                                CefWindowInfo& windowInfo,
                                CefRefPtr<CefClient>& client,
                                CefBrowserSettings& settings) {
  DCHECK(delegate);
  DCHECK(!initialized_);

  delegate_ = delegate;
  with_controls_ = with_controls;
  with_osr_ = with_osr;
  is_popup_ = true;

  if (popupFeatures.xSet) {
    start_rect_.x = popupFeatures.x;
  }
  if (popupFeatures.ySet) {
    start_rect_.y = popupFeatures.y;
  }
  if (popupFeatures.widthSet) {
    start_rect_.width = popupFeatures.width;
  }
  if (popupFeatures.heightSet) {
    start_rect_.height = popupFeatures.height;
  }

  CreateBrowserWindow(std::string());

  initialized_ = true;

  // The new popup is initially parented to a temporary window. The native root
  // window will be created after the browser is created and the popup window
  // will be re-parented to it at that time.
  browser_window_->GetPopupConfig(TempWindow::GetWindowHandle(), windowInfo,
                                  client, settings);
}

void RootWindowGtk::Show(ShowMode mode) {
  REQUIRE_MAIN_THREAD();

  if (!window_) {
    return;
  }

  ScopedGdkThreadsEnter scoped_gdk_threads;

  // Show the GTK window.
  UseDefaultX11VisualForGtk(GTK_WIDGET(window_));
  gtk_widget_show_all(window_);

  if (mode == ShowMinimized) {
    MinimizeWindow(GTK_WINDOW(window_));
  } else if (mode == ShowMaximized) {
    MaximizeWindow(GTK_WINDOW(window_));
  }

  // Flush the display to make sure the underlying X11 window gets created
  // immediately.
  GdkWindow* gdk_window = gtk_widget_get_window(window_);
  GdkDisplay* display = gdk_window_get_display(gdk_window);
  gdk_display_flush(display);
}

void RootWindowGtk::Hide() {
  REQUIRE_MAIN_THREAD();

  ScopedGdkThreadsEnter scoped_gdk_threads;

  if (window_) {
    gtk_widget_hide(window_);
  }
}

void RootWindowGtk::SetBounds(int x, int y, size_t width, size_t height) {
  REQUIRE_MAIN_THREAD();

  if (!window_) {
    return;
  }

  ScopedGdkThreadsEnter scoped_gdk_threads;

  GtkWindow* window = GTK_WINDOW(window_);
  GdkWindow* gdk_window = gtk_widget_get_window(window_);

  // Make sure the window isn't minimized or maximized.
  if (IsWindowMaximized(window)) {
    gtk_window_unmaximize(window);
  } else {
    gtk_window_present(window);
  }

  gdk_window_move_resize(gdk_window, x, y, width, height);
}

void RootWindowGtk::Close(bool force) {
  REQUIRE_MAIN_THREAD();

  if (window_) {
    ScopedGdkThreadsEnter scoped_gdk_threads;

    if (force) {
      NotifyForceClose();
    }
    gtk_widget_destroy(window_);
  }
}

void RootWindowGtk::SetDeviceScaleFactor(float device_scale_factor) {
  REQUIRE_MAIN_THREAD();

  if (browser_window_ && with_osr_) {
    browser_window_->SetDeviceScaleFactor(device_scale_factor);
  }
}

float RootWindowGtk::GetDeviceScaleFactor() const {
  REQUIRE_MAIN_THREAD();

  if (browser_window_ && with_osr_) {
    return browser_window_->GetDeviceScaleFactor();
  }

  NOTREACHED();
  return 0.0f;
}

CefRefPtr<CefBrowser> RootWindowGtk::GetBrowser() const {
  REQUIRE_MAIN_THREAD();

  if (browser_window_) {
    return browser_window_->GetBrowser();
  }
  return nullptr;
}

ClientWindowHandle RootWindowGtk::GetWindowHandle() const {
  REQUIRE_MAIN_THREAD();
  return window_;
}

bool RootWindowGtk::WithWindowlessRendering() const {
  REQUIRE_MAIN_THREAD();
  return with_osr_;
}

bool RootWindowGtk::WithExtension() const {
  REQUIRE_MAIN_THREAD();
  return with_extension_;
}

void RootWindowGtk::CreateBrowserWindow(const std::string& startup_url) {
  if (with_osr_) {
    OsrRendererSettings settings = {};
    MainContext::Get()->PopulateOsrSettings(&settings);
    browser_window_.reset(
        new BrowserWindowOsrGtk(this, with_controls_, startup_url, settings));
  } else {
    browser_window_.reset(
        new BrowserWindowStdGtk(this, with_controls_, startup_url));
  }
}

void RootWindowGtk::CreateRootWindow(const CefBrowserSettings& settings,
                                     bool initially_hidden) {
  REQUIRE_MAIN_THREAD();
  DCHECK(!window_);

  // TODO(port): If no x,y position is specified the window will always appear
  // in the upper-left corner. Maybe there's a better default place to put it?
  int x = start_rect_.x;
  int y = start_rect_.y;
  int width, height;
  if (start_rect_.IsEmpty()) {
    // TODO(port): Also, maybe there's a better way to choose the default size.
    width = 800;
    height = 600;
  } else {
    width = start_rect_.width;
    height = start_rect_.height;
  }

  ScopedGdkThreadsEnter scoped_gdk_threads;

  window_ = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  CHECK(window_);

  if (always_on_top_) {
    gtk_window_set_keep_above(GTK_WINDOW(window_), TRUE);
  }

  gtk_window_set_default_size(GTK_WINDOW(window_), width, height);
  g_signal_connect(G_OBJECT(window_), "focus-in-event",
                   G_CALLBACK(&RootWindowGtk::WindowFocusIn), this);
  g_signal_connect(G_OBJECT(window_), "window-state-event",
                   G_CALLBACK(&RootWindowGtk::WindowState), this);
  g_signal_connect(G_OBJECT(window_), "configure-event",
                   G_CALLBACK(&RootWindowGtk::WindowConfigure), this);
  g_signal_connect(G_OBJECT(window_), "destroy",
                   G_CALLBACK(&RootWindowGtk::WindowDestroy), this);
  g_signal_connect(G_OBJECT(window_), "delete_event",
                   G_CALLBACK(&RootWindowGtk::WindowDelete), this);

  const cef_color_t background_color = MainContext::Get()->GetBackgroundColor();
  GdkRGBA rgba = {0};
  rgba.red = CefColorGetR(background_color) * 65535 / 255;
  rgba.green = CefColorGetG(background_color) * 65535 / 255;
  rgba.blue = CefColorGetB(background_color) * 65535 / 255;
  rgba.alpha = 1;

  gchar* css = g_strdup_printf("#* { background-color: %s; }",
                               gdk_rgba_to_string(&rgba));
  GtkCssProvider* provider = gtk_css_provider_new();
  gtk_css_provider_load_from_data(provider, css, -1, nullptr);
  g_free(css);
  gtk_style_context_add_provider(gtk_widget_get_style_context(window_),
                                 GTK_STYLE_PROVIDER(provider),
                                 GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref(provider);

  GtkWidget* grid = gtk_grid_new();
  gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);
  g_signal_connect(grid, "size-allocate",
                   G_CALLBACK(&RootWindowGtk::GridSizeAllocated), this);
  gtk_container_add(GTK_CONTAINER(window_), grid);

  if (with_controls_) {
    GtkWidget* menu_bar = CreateMenuBar();
    g_signal_connect(menu_bar, "size-allocate",
                     G_CALLBACK(&RootWindowGtk::MenubarSizeAllocated), this);

    gtk_grid_attach(GTK_GRID(grid), menu_bar, 0, 0, 1, 1);

    GtkWidget* toolbar = gtk_toolbar_new();
    // Turn off the labels on the toolbar buttons.
    gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);
    g_signal_connect(toolbar, "size-allocate",
                     G_CALLBACK(&RootWindowGtk::ToolbarSizeAllocated), this);

    back_button_ = gtk_tool_button_new(
        gtk_image_new_from_icon_name("go-previous", GTK_ICON_SIZE_MENU),
        nullptr);
    g_signal_connect(back_button_, "clicked",
                     G_CALLBACK(&RootWindowGtk::BackButtonClicked), this);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), back_button_, -1 /* append */);

    forward_button_ = gtk_tool_button_new(
        gtk_image_new_from_icon_name("go-next", GTK_ICON_SIZE_MENU), nullptr);
    g_signal_connect(forward_button_, "clicked",
                     G_CALLBACK(&RootWindowGtk::ForwardButtonClicked), this);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), forward_button_, -1 /* append */);

    reload_button_ = gtk_tool_button_new(
        gtk_image_new_from_icon_name("view-refresh", GTK_ICON_SIZE_MENU),
        nullptr);
    g_signal_connect(reload_button_, "clicked",
                     G_CALLBACK(&RootWindowGtk::ReloadButtonClicked), this);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), reload_button_, -1 /* append */);

    stop_button_ = gtk_tool_button_new(
        gtk_image_new_from_icon_name("process-stop", GTK_ICON_SIZE_MENU),
        nullptr);
    g_signal_connect(stop_button_, "clicked",
                     G_CALLBACK(&RootWindowGtk::StopButtonClicked), this);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), stop_button_, -1 /* append */);

    url_entry_ = gtk_entry_new();
    g_signal_connect(url_entry_, "activate",
                     G_CALLBACK(&RootWindowGtk::URLEntryActivate), this);
    g_signal_connect(url_entry_, "button-press-event",
                     G_CALLBACK(&RootWindowGtk::URLEntryButtonPress), this);

    GtkToolItem* tool_item = gtk_tool_item_new();
    gtk_container_add(GTK_CONTAINER(tool_item), url_entry_);
    gtk_tool_item_set_expand(tool_item, TRUE);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), tool_item, -1);  // append

    gtk_grid_attach_next_to(GTK_GRID(grid), toolbar, menu_bar, GTK_POS_BOTTOM,
                            1, 1);
  }

  // Realize (show) the GTK widget. This must be done before the browser is
  // created because the underlying X11 Window is required. |browser_bounds_|
  // will be set at this point based on the GTK *SizeAllocated signal callbacks.
  Show(ShowNormal);

  // Most window managers ignore requests for initial window positions (instead
  // using a user-defined placement algorithm) and honor requests after the
  // window has already been shown.
  gtk_window_move(GTK_WINDOW(window_), x, y);

  // Windowed browsers are parented to the X11 Window underlying the GtkWindow*
  // and must be sized manually. The OSR GTK widget, on the other hand, can be
  // added to the grid container for automatic layout-based sizing.
  GtkWidget* parent = with_osr_ ? grid : window_;

  // Set the Display associated with the browser.
  ::Display* xdisplay = GDK_WINDOW_XDISPLAY(gtk_widget_get_window(window_));
  CHECK(xdisplay);
  if (with_osr_) {
    static_cast<BrowserWindowOsrGtk*>(browser_window_.get())
        ->set_xdisplay(xdisplay);
  } else {
    static_cast<BrowserWindowStdGtk*>(browser_window_.get())
        ->set_xdisplay(xdisplay);
  }

  if (!is_popup_) {
    // Create the browser window.
    browser_window_->CreateBrowser(parent, browser_bounds_, settings, nullptr,
                                   delegate_->GetRequestContext());
  } else {
    // With popups we already have a browser window. Parent the browser window
    // to the root window and show it in the correct location.
    browser_window_->ShowPopup(parent, browser_bounds_.x, browser_bounds_.y,
                               browser_bounds_.width, browser_bounds_.height);
  }
}

void RootWindowGtk::OnBrowserCreated(CefRefPtr<CefBrowser> browser) {
  REQUIRE_MAIN_THREAD();

  // For popup browsers create the root window once the browser has been
  // created.
  if (is_popup_) {
    CreateRootWindow(CefBrowserSettings(), false);
  }

  delegate_->OnBrowserCreated(this, browser);
}

void RootWindowGtk::OnBrowserWindowClosing() {
  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(TID_UI,
                base::BindOnce(&RootWindowGtk::OnBrowserWindowClosing, this));
    return;
  }

  is_closing_ = true;
}

void RootWindowGtk::OnBrowserWindowDestroyed() {
  REQUIRE_MAIN_THREAD();

  browser_window_.reset();

  if (!window_destroyed_) {
    // The browser was destroyed first. This could be due to the use of
    // off-screen rendering or execution of JavaScript window.close().
    // Close the RootWindow.
    Close(true);
  }

  NotifyDestroyedIfDone(false, true);
}

void RootWindowGtk::OnSetAddress(const std::string& url) {
  REQUIRE_MAIN_THREAD();

  if (url_entry_) {
    ScopedGdkThreadsEnter scoped_gdk_threads;

    std::string urlStr(url);
    gtk_entry_set_text(GTK_ENTRY(url_entry_), urlStr.c_str());
  }
}

void RootWindowGtk::OnSetTitle(const std::string& title) {
  REQUIRE_MAIN_THREAD();

  if (window_) {
    ScopedGdkThreadsEnter scoped_gdk_threads;

    std::string titleStr(title);
    gtk_window_set_title(GTK_WINDOW(window_), titleStr.c_str());
  }
}

void RootWindowGtk::OnSetFullscreen(bool fullscreen) {
  REQUIRE_MAIN_THREAD();

  CefRefPtr<CefBrowser> browser = GetBrowser();
  if (browser) {
    std::unique_ptr<window_test::WindowTestRunnerGtk> test_runner(
        new window_test::WindowTestRunnerGtk());
    if (fullscreen) {
      test_runner->Maximize(browser);
    } else {
      test_runner->Restore(browser);
    }
  }
}

void RootWindowGtk::OnAutoResize(const CefSize& new_size) {
  REQUIRE_MAIN_THREAD();

  if (!window_) {
    return;
  }

  ScopedGdkThreadsEnter scoped_gdk_threads;

  GtkWindow* window = GTK_WINDOW(window_);
  GdkWindow* gdk_window = gtk_widget_get_window(window_);

  // Make sure the window isn't minimized or maximized.
  if (IsWindowMaximized(window)) {
    gtk_window_unmaximize(window);
  } else {
    gtk_window_present(window);
  }

  gdk_window_resize(gdk_window, new_size.width, new_size.height);
}

void RootWindowGtk::OnSetLoadingState(bool isLoading,
                                      bool canGoBack,
                                      bool canGoForward) {
  REQUIRE_MAIN_THREAD();

  if (with_controls_) {
    ScopedGdkThreadsEnter scoped_gdk_threads;

    gtk_widget_set_sensitive(GTK_WIDGET(stop_button_), isLoading);
    gtk_widget_set_sensitive(GTK_WIDGET(reload_button_), !isLoading);
    gtk_widget_set_sensitive(GTK_WIDGET(back_button_), canGoBack);
    gtk_widget_set_sensitive(GTK_WIDGET(forward_button_), canGoForward);
  }
}

void RootWindowGtk::OnSetDraggableRegions(
    const std::vector<CefDraggableRegion>& regions) {
  REQUIRE_MAIN_THREAD();
  // TODO(cef): Implement support for draggable regions on this platform.
}

void RootWindowGtk::NotifyMoveOrResizeStarted() {
  if (!CURRENTLY_ON_MAIN_THREAD()) {
    MAIN_POST_CLOSURE(
        base::BindOnce(&RootWindowGtk::NotifyMoveOrResizeStarted, this));
    return;
  }

  // Called when size, position or stack order changes.
  CefRefPtr<CefBrowser> browser = GetBrowser();
  if (browser.get()) {
    // Notify the browser of move/resize events so that:
    // - Popup windows are displayed in the correct location and dismissed
    //   when the window moves.
    // - Drag&drop areas are updated accordingly.
    browser->GetHost()->NotifyMoveOrResizeStarted();
  }
}

void RootWindowGtk::NotifySetFocus() {
  if (!CURRENTLY_ON_MAIN_THREAD()) {
    MAIN_POST_CLOSURE(base::BindOnce(&RootWindowGtk::NotifySetFocus, this));
    return;
  }

  if (!browser_window_.get()) {
    return;
  }

  browser_window_->SetFocus(true);
  delegate_->OnRootWindowActivated(this);
}

void RootWindowGtk::NotifyVisibilityChange(bool show) {
  if (!CURRENTLY_ON_MAIN_THREAD()) {
    MAIN_POST_CLOSURE(
        base::BindOnce(&RootWindowGtk::NotifyVisibilityChange, this, show));
    return;
  }

  if (!browser_window_.get()) {
    return;
  }

  if (show) {
    browser_window_->Show();
  } else {
    browser_window_->Hide();
  }
}

void RootWindowGtk::NotifyMenuBarHeight(int height) {
  if (!CURRENTLY_ON_MAIN_THREAD()) {
    MAIN_POST_CLOSURE(
        base::BindOnce(&RootWindowGtk::NotifyMenuBarHeight, this, height));
    return;
  }

  menubar_height_ = height;
}

void RootWindowGtk::NotifyContentBounds(int x, int y, int width, int height) {
  if (!CURRENTLY_ON_MAIN_THREAD()) {
    MAIN_POST_CLOSURE(base::BindOnce(&RootWindowGtk::NotifyContentBounds, this,
                                     x, y, width, height));
    return;
  }

  // Offset browser positioning by any controls that will appear in the client
  // area.
  const int ux_height = toolbar_height_ + menubar_height_;
  const int browser_x = x;
  const int browser_y = y + ux_height;
  const int browser_width = width;
  const int browser_height = height - ux_height;

  // Size the browser window to match the GTK widget.
  browser_bounds_ =
      CefRect(browser_x, browser_y, browser_width, browser_height);
  if (browser_window_.get()) {
    browser_window_->SetBounds(browser_x, browser_y, browser_width,
                               browser_height);
  }
}

void RootWindowGtk::NotifyLoadURL(const std::string& url) {
  if (!CURRENTLY_ON_MAIN_THREAD()) {
    MAIN_POST_CLOSURE(base::BindOnce(&RootWindowGtk::NotifyLoadURL, this, url));
    return;
  }

  CefRefPtr<CefBrowser> browser = GetBrowser();
  if (browser.get()) {
    browser->GetMainFrame()->LoadURL(url);
  }
}

void RootWindowGtk::NotifyButtonClicked(int id) {
  if (!CURRENTLY_ON_MAIN_THREAD()) {
    MAIN_POST_CLOSURE(
        base::BindOnce(&RootWindowGtk::NotifyButtonClicked, this, id));
    return;
  }

  CefRefPtr<CefBrowser> browser = GetBrowser();
  if (!browser.get()) {
    return;
  }

  switch (id) {
    case IDC_NAV_BACK:
      browser->GoBack();
      break;
    case IDC_NAV_FORWARD:
      browser->GoForward();
      break;
    case IDC_NAV_RELOAD:
      browser->Reload();
      break;
    case IDC_NAV_STOP:
      browser->StopLoad();
      break;
    default:
      NOTREACHED() << "id=" << id;
  }
}

void RootWindowGtk::NotifyMenuItem(int id) {
  if (!CURRENTLY_ON_MAIN_THREAD()) {
    MAIN_POST_CLOSURE(base::BindOnce(&RootWindowGtk::NotifyMenuItem, this, id));
    return;
  }

  // Run the test.
  if (delegate_) {
    delegate_->OnTest(this, id);
  }
}

void RootWindowGtk::NotifyForceClose() {
  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(TID_UI, base::BindOnce(&RootWindowGtk::NotifyForceClose, this));
    return;
  }

  force_close_ = true;
}

void RootWindowGtk::NotifyCloseBrowser() {
  if (!CURRENTLY_ON_MAIN_THREAD()) {
    MAIN_POST_CLOSURE(base::BindOnce(&RootWindowGtk::NotifyCloseBrowser, this));
    return;
  }

  CefRefPtr<CefBrowser> browser = GetBrowser();
  if (browser) {
    browser->GetHost()->CloseBrowser(false);
  }
}

void RootWindowGtk::NotifyDestroyedIfDone(bool window_destroyed,
                                          bool browser_destroyed) {
  // Each call will to this method will set only one state flag.
  DCHECK_EQ(1, window_destroyed + browser_destroyed);

  if (!CURRENTLY_ON_MAIN_THREAD()) {
    MAIN_POST_CLOSURE(base::BindOnce(&RootWindowGtk::NotifyDestroyedIfDone,
                                     this, window_destroyed,
                                     browser_destroyed));
    return;
  }

  if (window_destroyed) {
    window_destroyed_ = true;
  }
  if (browser_destroyed) {
    browser_destroyed_ = true;
  }

  // Notify once both the window and the browser have been destroyed.
  if (window_destroyed_ && browser_destroyed_) {
    delegate_->OnRootWindowDestroyed(this);
  }
}

// static
gboolean RootWindowGtk::WindowFocusIn(GtkWidget* widget,
                                      GdkEventFocus* event,
                                      RootWindowGtk* self) {
  REQUIRE_MAIN_THREAD();

  if (event->in) {
    self->NotifySetFocus();

    // Return true for a windowed browser so that focus is not passed to GTK.
    return self->with_osr_ ? FALSE : TRUE;
  }

  return FALSE;
}

// static
gboolean RootWindowGtk::WindowState(GtkWidget* widget,
                                    GdkEventWindowState* event,
                                    RootWindowGtk* self) {
  REQUIRE_MAIN_THREAD();

  // Called when the root window is iconified or restored. Hide the browser
  // window when the root window is iconified to reduce resource usage.
  if (event->changed_mask & GDK_WINDOW_STATE_ICONIFIED) {
    self->NotifyVisibilityChange(
        !(event->new_window_state & GDK_WINDOW_STATE_ICONIFIED));
  }

  return TRUE;
}

// static
gboolean RootWindowGtk::WindowConfigure(GtkWindow* window,
                                        GdkEvent* event,
                                        RootWindowGtk* self) {
  REQUIRE_MAIN_THREAD();
  self->NotifyMoveOrResizeStarted();
  return FALSE;  // Don't stop this message.
}

// static
void RootWindowGtk::WindowDestroy(GtkWidget* widget, RootWindowGtk* self) {
  // May be called on the main thread or the UI thread.
  self->NotifyDestroyedIfDone(true, false);
}

// static
gboolean RootWindowGtk::WindowDelete(GtkWidget* widget,
                                     GdkEvent* event,
                                     RootWindowGtk* self) {
  REQUIRE_MAIN_THREAD();

  // Called to query whether the root window should be closed.
  if (self->force_close_) {
    return FALSE;  // Allow the close.
  }

  if (!self->is_closing_) {
    // Notify the browser window that we would like to close it. This
    // will result in a call to ClientHandler::DoClose() if the
    // JavaScript 'onbeforeunload' event handler allows it.
    self->NotifyCloseBrowser();

    // Cancel the close.
    return TRUE;
  }

  // Allow the close.
  return FALSE;
}

// static
void RootWindowGtk::GridSizeAllocated(GtkWidget* widget,
                                      GtkAllocation* allocation,
                                      RootWindowGtk* self) {
  // May be called on the main thread and the UI thread.
  self->NotifyContentBounds(allocation->x, allocation->y, allocation->width,
                            allocation->height);
}

// static
void RootWindowGtk::MenubarSizeAllocated(GtkWidget* widget,
                                         GtkAllocation* allocation,
                                         RootWindowGtk* self) {
  // May be called on the main thread and the UI thread.
  self->NotifyMenuBarHeight(allocation->height);
}

// static
gboolean RootWindowGtk::MenuItemActivated(GtkWidget* widget,
                                          RootWindowGtk* self) {
  REQUIRE_MAIN_THREAD();

  // Retrieve the menu ID set in AddMenuEntry.
  int id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), kMenuIdKey));
  self->NotifyMenuItem(id);

  return FALSE;  // Don't stop this message.
}

// static
void RootWindowGtk::ToolbarSizeAllocated(GtkWidget* widget,
                                         GtkAllocation* allocation,
                                         RootWindowGtk* self) {
  self->toolbar_height_ = allocation->height;
}

// static
void RootWindowGtk::BackButtonClicked(GtkButton* button, RootWindowGtk* self) {
  REQUIRE_MAIN_THREAD();
  self->NotifyButtonClicked(IDC_NAV_BACK);
}

// static
void RootWindowGtk::ForwardButtonClicked(GtkButton* button,
                                         RootWindowGtk* self) {
  REQUIRE_MAIN_THREAD();
  self->NotifyButtonClicked(IDC_NAV_FORWARD);
}

// static
void RootWindowGtk::StopButtonClicked(GtkButton* button, RootWindowGtk* self) {
  REQUIRE_MAIN_THREAD();
  self->NotifyButtonClicked(IDC_NAV_STOP);
}

// static
void RootWindowGtk::ReloadButtonClicked(GtkButton* button,
                                        RootWindowGtk* self) {
  REQUIRE_MAIN_THREAD();
  self->NotifyButtonClicked(IDC_NAV_RELOAD);
}

// static
void RootWindowGtk::URLEntryActivate(GtkEntry* entry, RootWindowGtk* self) {
  REQUIRE_MAIN_THREAD();
  const gchar* url = gtk_entry_get_text(entry);
  self->NotifyLoadURL(std::string(url));
}

// static
gboolean RootWindowGtk::URLEntryButtonPress(GtkWidget* widget,
                                            GdkEventButton* event,
                                            RootWindowGtk* self) {
  REQUIRE_MAIN_THREAD();

  // Give focus to the GTK window. This is a work-around for bad focus-related
  // interaction between the root window managed by GTK and the browser managed
  // by X11.
  GtkWidget* window = gtk_widget_get_ancestor(widget, GTK_TYPE_WINDOW);
  GdkWindow* gdk_window = gtk_widget_get_window(window);
  ::Display* xdisplay = GDK_WINDOW_XDISPLAY(gdk_window);
  ::Window xwindow = GDK_WINDOW_XID(gdk_window);

  // Retrieve the atoms required by the below XSendEvent call.
  const char* kAtoms[] = {"WM_PROTOCOLS", "WM_TAKE_FOCUS"};
  Atom atoms[2];
  int result =
      XInternAtoms(xdisplay, const_cast<char**>(kAtoms), 2, false, atoms);
  if (!result) {
    NOTREACHED();
  }

  XEvent e;
  e.type = ClientMessage;
  e.xany.display = xdisplay;
  e.xany.window = xwindow;
  e.xclient.format = 32;
  e.xclient.message_type = atoms[0];
  e.xclient.data.l[0] = atoms[1];
  e.xclient.data.l[1] = CurrentTime;
  e.xclient.data.l[2] = 0;
  e.xclient.data.l[3] = 0;
  e.xclient.data.l[4] = 0;

  XSendEvent(xdisplay, xwindow, false, 0, &e);

  return FALSE;
}

GtkWidget* RootWindowGtk::CreateMenuBar() {
  GtkWidget* menu_bar = gtk_menu_bar_new();

  // Create the test menu.
  GtkWidget* test_menu = CreateMenu(menu_bar, "Tests");
  AddMenuEntry(test_menu, "Get Source", ID_TESTS_GETSOURCE);
  AddMenuEntry(test_menu, "Get Text", ID_TESTS_GETTEXT);
  AddMenuEntry(test_menu, "New Window", ID_TESTS_WINDOW_NEW);
  AddMenuEntry(test_menu, "Popup Window", ID_TESTS_WINDOW_POPUP);
  AddMenuEntry(test_menu, "Request", ID_TESTS_REQUEST);
  AddMenuEntry(test_menu, "Zoom In", ID_TESTS_ZOOM_IN);
  AddMenuEntry(test_menu, "Zoom Out", ID_TESTS_ZOOM_OUT);
  AddMenuEntry(test_menu, "Zoom Reset", ID_TESTS_ZOOM_RESET);
  if (with_osr_) {
    AddMenuEntry(test_menu, "Set FPS", ID_TESTS_OSR_FPS);
    AddMenuEntry(test_menu, "Set Scale Factor", ID_TESTS_OSR_DSF);
  }
  AddMenuEntry(test_menu, "Begin Tracing", ID_TESTS_TRACING_BEGIN);
  AddMenuEntry(test_menu, "End Tracing", ID_TESTS_TRACING_END);
  AddMenuEntry(test_menu, "Print", ID_TESTS_PRINT);
  AddMenuEntry(test_menu, "Print to PDF", ID_TESTS_PRINT_TO_PDF);
  AddMenuEntry(test_menu, "Mute Audio", ID_TESTS_MUTE_AUDIO);
  AddMenuEntry(test_menu, "Unmute Audio", ID_TESTS_UNMUTE_AUDIO);
  AddMenuEntry(test_menu, "Other Tests", ID_TESTS_OTHER_TESTS);

  return menu_bar;
}

GtkWidget* RootWindowGtk::CreateMenu(GtkWidget* menu_bar, const char* text) {
  GtkWidget* menu_widget = gtk_menu_new();
  GtkWidget* menu_header = gtk_menu_item_new_with_label(text);
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_header), menu_widget);
  gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), menu_header);
  return menu_widget;
}

GtkWidget* RootWindowGtk::AddMenuEntry(GtkWidget* menu_widget,
                                       const char* text,
                                       int id) {
  GtkWidget* entry = gtk_menu_item_new_with_label(text);
  g_signal_connect(entry, "activate",
                   G_CALLBACK(&RootWindowGtk::MenuItemActivated), this);

  // Set the menu ID that will be retrieved in MenuItemActivated.
  g_object_set_data(G_OBJECT(entry), kMenuIdKey, GINT_TO_POINTER(id));

  gtk_menu_shell_append(GTK_MENU_SHELL(menu_widget), entry);
  return entry;
}

}  // namespace client
