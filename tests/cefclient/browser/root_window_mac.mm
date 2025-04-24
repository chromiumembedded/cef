// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/root_window_mac.h"

#include <Cocoa/Cocoa.h>

#include "include/base/cef_callback.h"
#include "include/cef_app.h"
#include "include/cef_application_mac.h"
#include "include/views/cef_display.h"
#include "tests/cefclient/browser/browser_window_osr_mac.h"
#include "tests/cefclient/browser/browser_window_std_mac.h"
#include "tests/cefclient/browser/client_prefs.h"
#include "tests/cefclient/browser/main_context.h"
#include "tests/cefclient/browser/osr_renderer_settings.h"
#include "tests/cefclient/browser/root_window_manager.h"
#include "tests/cefclient/browser/temp_window.h"
#include "tests/cefclient/browser/util_mac.h"
#include "tests/cefclient/browser/window_test_runner_mac.h"
#include "tests/shared/browser/main_message_loop.h"
#include "tests/shared/common/client_switches.h"

// Receives notifications from controls and the browser window. Will delete
// itself when done.
@interface RootWindowDelegate : NSObject <NSWindowDelegate> {
 @private
  NSWindow* window_;
  client::RootWindowMac* root_window_;
  std::optional<CefRect> last_visible_bounds_;
  bool force_close_;
}

@property(nonatomic, readonly) client::RootWindowMac* root_window;
@property(nonatomic, readwrite) std::optional<CefRect> last_visible_bounds;
@property(nonatomic, readwrite) bool force_close;

- (id)initWithWindow:(NSWindow*)window
       andRootWindow:(client::RootWindowMac*)root_window;
- (IBAction)goBack:(id)sender;
- (IBAction)goForward:(id)sender;
- (IBAction)reload:(id)sender;
- (IBAction)stopLoading:(id)sender;
- (IBAction)takeURLStringValueFrom:(NSTextField*)sender;
@end  // @interface RootWindowDelegate

namespace client {

namespace {

// Sizes for URL bar layout.
#define BUTTON_HEIGHT 22
#define BUTTON_WIDTH 72
#define BUTTON_MARGIN 8
#define URLBAR_HEIGHT 32

NSButton* MakeButton(NSRect* rect, NSString* title, NSView* parent) {
  NSButton* button = [[NSButton alloc] initWithFrame:*rect];
#if !__has_feature(objc_arc)
  [button autorelease];
#endif  // !__has_feature(objc_arc)
  [button setTitle:title];
  [button setBezelStyle:NSBezelStyleSmallSquare];
  [button setAutoresizingMask:(NSViewMaxXMargin | NSViewMinYMargin)];
  [parent addSubview:button];
  rect->origin.x += BUTTON_WIDTH;
  return button;
}

// Keep the frame bounds inside the display work area.
NSRect ClampNSBoundsToWorkArea(const NSRect& frame_bounds,
                               const CefRect& display_bounds,
                               const CefRect& work_area) {
  NSRect bounds = frame_bounds;

  // Convert from DIP coordinates (top-left origin) to macOS coordinates
  // (bottom-left origin).
  const int work_area_y =
      display_bounds.height - work_area.height - work_area.y;

  if (bounds.size.width > work_area.width) {
    bounds.size.width = work_area.width;
  }
  if (bounds.size.height > work_area.height) {
    bounds.size.height = work_area.height;
  }

  if (bounds.origin.x < work_area.x) {
    bounds.origin.x = work_area.x;
  } else if (bounds.origin.x + bounds.size.width >=
             work_area.x + work_area.width) {
    bounds.origin.x = work_area.x + work_area.width - bounds.size.width;
  }

  if (bounds.origin.y < work_area_y) {
    bounds.origin.y = work_area_y;
  } else if (bounds.origin.y + bounds.size.height >=
             work_area_y + work_area.height) {
    bounds.origin.y = work_area_y + work_area.height - bounds.size.height;
  }

  return bounds;
}

// Get frame and content area rects matching the input DIP screen bounds. The
// resulting window frame will be kept inside the closest display work area. If
// |input_content_bounds| is true the input size is used for the content area
// and the input origin is used for the frame. Otherwise, both input size and
// origin are used for the frame.
void GetNSBoundsInDisplay(const CefRect& dip_bounds,
                          bool input_content_bounds,
                          NSWindowStyleMask style_mask,
                          bool add_controls,
                          NSRect& frame_rect,
                          NSRect& content_rect) {
  // Identify the closest display.
  auto display =
      CefDisplay::GetDisplayMatchingBounds(dip_bounds,
                                           /*input_pixel_coords=*/false);
  const auto display_bounds = display->GetBounds();
  const auto display_work_area = display->GetWorkArea();

  // Convert from DIP coordinates (top-left origin) to macOS coordinates
  // (bottom-left origin).
  NSRect requested_rect = NSMakeRect(dip_bounds.x, dip_bounds.y,
                                     dip_bounds.width, dip_bounds.height);
  requested_rect.origin.y = display_bounds.height - requested_rect.size.height -
                            requested_rect.origin.y;

  bool changed_content_bounds = false;

  // Calculate the equivalent frame and content bounds.
  if (input_content_bounds) {
    // Compute frame rect from content rect. Keep the requested origin.
    content_rect = requested_rect;
    frame_rect = [NSWindow frameRectForContentRect:content_rect
                                         styleMask:style_mask];
    if (add_controls) {
      frame_rect.size.height += URLBAR_HEIGHT;
    }
    frame_rect.origin = requested_rect.origin;
  } else {
    // Compute content rect from frame rect.
    frame_rect = requested_rect;
    content_rect = [NSWindow contentRectForFrameRect:frame_rect
                                           styleMask:style_mask];
    changed_content_bounds = true;
  }

  // Keep the frame inside the display work area.
  const NSRect new_frame_rect =
      ClampNSBoundsToWorkArea(frame_rect, display_bounds, display_work_area);
  if (!NSEqualRects(frame_rect, new_frame_rect)) {
    frame_rect = new_frame_rect;
    content_rect = [NSWindow contentRectForFrameRect:frame_rect
                                           styleMask:style_mask];
    changed_content_bounds = true;
  }

  if (changed_content_bounds && add_controls) {
    content_rect.origin.y -= URLBAR_HEIGHT;
    content_rect.size.height -= URLBAR_HEIGHT;
  }
}

}  // namespace

class RootWindowMacImpl
    : public base::RefCountedThreadSafe<RootWindowMacImpl, DeleteOnMainThread> {
 public:
  RootWindowMacImpl(RootWindowMac& root_window);
  ~RootWindowMacImpl();

  // Called by RootWindowDelegate after the associated NSWindow has been
  // closed.
  void OnNativeWindowClosed();

  void CreateBrowserWindow(const std::string& startup_url);
  void CreateRootWindow(const CefBrowserSettings& settings,
                        bool initially_hidden);

  // RootWindow methods.
  void Init(RootWindow::Delegate* delegate,
            std::unique_ptr<RootWindowConfig> config,
            const CefBrowserSettings& settings);
  void InitAsPopup(RootWindow::Delegate* delegate,
                   bool with_controls,
                   bool with_osr,
                   const CefPopupFeatures& popupFeatures,
                   CefWindowInfo& windowInfo,
                   CefRefPtr<CefClient>& client,
                   CefBrowserSettings& settings);
  void Show(RootWindow::ShowMode mode);
  void Hide();
  void SetBounds(int x,
                 int y,
                 size_t width,
                 size_t height,
                 bool content_bounds);
  bool DefaultToContentBounds() const;
  void Close(bool force);
  void SetDeviceScaleFactor(float device_scale_factor);
  std::optional<float> GetDeviceScaleFactor() const;
  CefRefPtr<CefBrowser> GetBrowser() const;
  ClientWindowHandle GetWindowHandle() const;
  bool WithWindowlessRendering() const;

  // BrowserWindow::Delegate methods.
  void OnBrowserCreated(CefRefPtr<CefBrowser> browser);
  void OnBrowserWindowDestroyed();
  void OnSetAddress(const std::string& url);
  void OnSetTitle(const std::string& title);
  void OnSetFullscreen(bool fullscreen);
  void OnAutoResize(const CefSize& new_size);
  void OnSetLoadingState(bool isLoading, bool canGoBack, bool canGoForward);
  void OnSetDraggableRegions(const std::vector<CefDraggableRegion>& regions);

  void NotifyDestroyedIfDone();

  // After initialization all members are only accessed on the main thread.
  // Members set during initialization.
  RootWindowMac& root_window_;
  bool with_controls_ = false;
  bool with_osr_ = false;
  OsrRendererSettings osr_settings_;
  bool is_popup_ = false;
  CefRect initial_bounds_;
  cef_show_state_t initial_show_state_ = CEF_SHOW_STATE_NORMAL;
  std::unique_ptr<BrowserWindow> browser_window_;

  // Main window.
  NSWindow* window_ = nil;
  RootWindowDelegate* window_delegate_ = nil;

  // Buttons.
  NSButton* back_button_ = nil;
  NSButton* forward_button_ = nil;
  NSButton* reload_button_ = nil;
  NSButton* stop_button_ = nil;

  // URL text field.
  NSTextField* url_textfield_ = nil;

  bool window_destroyed_ = false;
  bool browser_destroyed_ = false;
};

RootWindowMacImpl::RootWindowMacImpl(RootWindowMac& root_window)
    : root_window_(root_window) {}

RootWindowMacImpl::~RootWindowMacImpl() {
  REQUIRE_MAIN_THREAD();

  // The window and browser should already have been destroyed.
  DCHECK(window_destroyed_);
  DCHECK(browser_destroyed_);
}

void RootWindowMacImpl::Init(RootWindow::Delegate* delegate,
                             std::unique_ptr<RootWindowConfig> config,
                             const CefBrowserSettings& settings) {
  DCHECK(!root_window_.initialized_);

  with_controls_ = config->with_controls;
  with_osr_ = config->with_osr;

  if (!config->bounds.IsEmpty()) {
    // Initial state was specified via the config object.
    initial_bounds_ = config->bounds;
    initial_show_state_ = config->show_state;
  } else {
    // Initial state may be specified via the command-line or global
    // preferences.
    std::optional<CefRect> bounds;
    if (prefs::LoadWindowRestorePreferences(initial_show_state_, bounds) &&
        bounds) {
      initial_bounds_ = *bounds;
    }
  }

  CreateBrowserWindow(config->url);

  root_window_.initialized_ = true;

  CreateRootWindow(settings, config->initially_hidden);
}

void RootWindowMacImpl::InitAsPopup(RootWindow::Delegate* delegate,
                                    bool with_controls,
                                    bool with_osr,
                                    const CefPopupFeatures& popupFeatures,
                                    CefWindowInfo& windowInfo,
                                    CefRefPtr<CefClient>& client,
                                    CefBrowserSettings& settings) {
  DCHECK(delegate);
  DCHECK(!root_window_.initialized_);

  with_controls_ = with_controls;
  with_osr_ = with_osr;
  is_popup_ = true;

  if (popupFeatures.xSet) {
    initial_bounds_.x = popupFeatures.x;
  }
  if (popupFeatures.ySet) {
    initial_bounds_.y = popupFeatures.y;
  }
  if (popupFeatures.widthSet) {
    initial_bounds_.width = popupFeatures.width;
  }
  if (popupFeatures.heightSet) {
    initial_bounds_.height = popupFeatures.height;
  }

  CreateBrowserWindow(std::string());

  root_window_.initialized_ = true;

  // The new popup is initially parented to a temporary window. The native root
  // window will be created after the browser is created and the popup window
  // will be re-parented to it at that time.
  browser_window_->GetPopupConfig(TempWindow::GetWindowHandle(), windowInfo,
                                  client, settings);
}

void RootWindowMacImpl::Show(RootWindow::ShowMode mode) {
  REQUIRE_MAIN_THREAD();

  if (!window_) {
    return;
  }

  const bool is_visible = [window_ isVisible];
  const bool is_minimized = [window_ isMiniaturized];
  const bool is_maximized = [window_ isZoomed];

  if ((mode == RootWindow::ShowMinimized && is_minimized) ||
      (mode == RootWindow::ShowMaximized && is_maximized) ||
      (mode == RootWindow::ShowNormal && is_visible)) {
    // The window is already in the desired state.
    return;
  }

  // Undo the previous state since it's not the desired state.
  if (is_minimized) {
    [window_ deminiaturize:nil];
  } else if (is_maximized) {
    [window_ performZoom:nil];
  }

  // Window visibility may change after (for example) deminiaturizing the
  // window.
  if (![window_ isVisible]) {
    [window_ makeKeyAndOrderFront:nil];
  }

  if (mode == RootWindow::ShowMinimized) {
    [window_ performMiniaturize:nil];
  } else if (mode == RootWindow::ShowMaximized) {
    [window_ performZoom:nil];
  }
}

void RootWindowMacImpl::Hide() {
  REQUIRE_MAIN_THREAD();

  if (!window_) {
    return;
  }

  // Undo miniaturization, if any, so the window will actually be hidden.
  if ([window_ isMiniaturized]) {
    [window_ deminiaturize:nil];
  }

  // Hide the window.
  [window_ orderOut:nil];
}

void RootWindowMacImpl::SetBounds(int x,
                                  int y,
                                  size_t width,
                                  size_t height,
                                  bool content_bounds) {
  REQUIRE_MAIN_THREAD();

  if (!window_) {
    return;
  }

  const CefRect dip_bounds(x, y, static_cast<int>(width),
                           static_cast<int>(height));
  const bool add_controls = WithWindowlessRendering() || with_controls_;

  // Calculate the equivalent frame and content area bounds.
  NSRect frame_rect, content_rect;
  GetNSBoundsInDisplay(dip_bounds, content_bounds, [window_ styleMask],
                       add_controls, frame_rect, content_rect);

  [window_ setFrame:frame_rect display:YES];
}

bool RootWindowMacImpl::DefaultToContentBounds() const {
  if (!WithWindowlessRendering()) {
    // The root NSWindow will be queried by default.
    return false;
  }
  if (osr_settings_.real_screen_bounds) {
    // Root NSWindow bounds are provided via GetRootWindowRect.
    return false;
  }
  // The root NSWindow will not be queried by default.
  return true;
}

void RootWindowMacImpl::Close(bool force) {
  REQUIRE_MAIN_THREAD();

  if (window_) {
    static_cast<RootWindowDelegate*>([window_ delegate]).force_close = force;
    [window_ performClose:nil];
    window_destroyed_ = true;
  }
}

void RootWindowMacImpl::SetDeviceScaleFactor(float device_scale_factor) {
  REQUIRE_MAIN_THREAD();

  if (browser_window_ && with_osr_) {
    browser_window_->SetDeviceScaleFactor(device_scale_factor);
  }
}

std::optional<float> RootWindowMacImpl::GetDeviceScaleFactor() const {
  REQUIRE_MAIN_THREAD();

  if (browser_window_ && with_osr_) {
    return browser_window_->GetDeviceScaleFactor();
  }

  return std::nullopt;
}

CefRefPtr<CefBrowser> RootWindowMacImpl::GetBrowser() const {
  REQUIRE_MAIN_THREAD();

  if (browser_window_) {
    return browser_window_->GetBrowser();
  }
  return nullptr;
}

ClientWindowHandle RootWindowMacImpl::GetWindowHandle() const {
  REQUIRE_MAIN_THREAD();
  return CAST_NSVIEW_TO_CEF_WINDOW_HANDLE([window_ contentView]);
}

bool RootWindowMacImpl::WithWindowlessRendering() const {
  REQUIRE_MAIN_THREAD();
  DCHECK(root_window_.initialized_);
  return with_osr_;
}

void RootWindowMacImpl::OnNativeWindowClosed() {
  window_ = nil;
  window_destroyed_ = true;
  NotifyDestroyedIfDone();
}

void RootWindowMacImpl::CreateBrowserWindow(const std::string& startup_url) {
  if (with_osr_) {
    MainContext::Get()->PopulateOsrSettings(&osr_settings_);
    browser_window_.reset(new BrowserWindowOsrMac(&root_window_, with_controls_,
                                                  startup_url, osr_settings_));
  } else {
    browser_window_.reset(
        new BrowserWindowStdMac(&root_window_, with_controls_, startup_url));
  }
}

void RootWindowMacImpl::CreateRootWindow(const CefBrowserSettings& settings,
                                         bool initially_hidden) {
  REQUIRE_MAIN_THREAD();
  DCHECK(!window_);

  // TODO(port): If no x,y position is specified the window will always appear
  // in the upper-left corner. Maybe there's a better default place to put it?
  CefRect dip_bounds = initial_bounds_;

  // TODO(port): Also, maybe there's a better way to choose the default size.
  if (dip_bounds.width <= 0) {
    dip_bounds.width = 800;
  }
  if (dip_bounds.height <= 0) {
    dip_bounds.height = 600;
  }

  // For popups, the requested bounds are for the content area and the requested
  // origin is for the window.
  if (is_popup_ && with_controls_) {
    dip_bounds.height += URLBAR_HEIGHT;
  }

  const NSWindowStyleMask style_mask =
      (NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
       NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable);

  // Calculate the equivalent frame and content area bounds. Controls, if any,
  // are already included in the desired size.
  NSRect frame_rect, content_rect;
  GetNSBoundsInDisplay(dip_bounds, /*input_content_bounds=*/is_popup_,
                       style_mask, /*add_controls=*/false, frame_rect,
                       content_rect);

  // Create the main window.
  window_ = [[NSWindow alloc] initWithContentRect:content_rect
                                        styleMask:style_mask
                                          backing:NSBackingStoreBuffered
                                            defer:NO];
  [window_ setTitle:@"cefclient"];
  // No dark mode, please
  window_.appearance = [NSAppearance appearanceNamed:NSAppearanceNameAqua];

  // Create the delegate for control and browser window events.
  window_delegate_ = [[RootWindowDelegate alloc] initWithWindow:window_
                                                  andRootWindow:&root_window_];

  if (!initial_bounds_.IsEmpty()) {
    // Remember the bounds from the previous application run in case the user
    // does not move or resize the window during this application run.
    window_delegate_.last_visible_bounds = initial_bounds_;
  }

  // Rely on the window delegate to clean us up rather than immediately
  // releasing when the window gets closed. We use the delegate to do
  // everything from the autorelease pool so the window isn't on the stack
  // during cleanup (ie, a window close from javascript).
  [window_ setReleasedWhenClosed:NO];

  const cef_color_t background_color = MainContext::Get()->GetBackgroundColor();
  [window_
      setBackgroundColor:[NSColor
                             colorWithCalibratedRed:float(CefColorGetR(
                                                        background_color)) /
                                                    255.0f
                                              green:float(CefColorGetG(
                                                        background_color)) /
                                                    255.0f
                                               blue:float(CefColorGetB(
                                                        background_color)) /
                                                    255.0f
                                              alpha:1.f]];

  NSView* contentView = [window_ contentView];
  NSRect contentBounds = [contentView bounds];

  if (!with_osr_) {
    // Make the content view for the window have a layer. This will make all
    // sub-views have layers. This is necessary to ensure correct layer
    // ordering of all child views and their layers.
    [contentView setWantsLayer:YES];
  }

  if (with_controls_) {
    // Reduce the browser height by the URL bar height.
    contentBounds.size.height -= URLBAR_HEIGHT;

    // Create the buttons.
    NSRect button_rect = contentBounds;
    button_rect.origin.y =
        contentBounds.size.height + (URLBAR_HEIGHT - BUTTON_HEIGHT) / 2;
    button_rect.size.height = BUTTON_HEIGHT;
    button_rect.origin.x += BUTTON_MARGIN;
    button_rect.size.width = BUTTON_WIDTH;

    back_button_ = MakeButton(&button_rect, @"Back", contentView);
    [back_button_ setTarget:window_delegate_];
    [back_button_ setAction:@selector(goBack:)];
    [back_button_ setEnabled:NO];

    forward_button_ = MakeButton(&button_rect, @"Forward", contentView);
    [forward_button_ setTarget:window_delegate_];
    [forward_button_ setAction:@selector(goForward:)];
    [forward_button_ setEnabled:NO];

    reload_button_ = MakeButton(&button_rect, @"Reload", contentView);
    [reload_button_ setTarget:window_delegate_];
    [reload_button_ setAction:@selector(reload:)];
    [reload_button_ setEnabled:NO];

    stop_button_ = MakeButton(&button_rect, @"Stop", contentView);
    [stop_button_ setTarget:window_delegate_];
    [stop_button_ setAction:@selector(stopLoading:)];
    [stop_button_ setEnabled:NO];

    // Create the URL text field.
    button_rect.origin.x += BUTTON_MARGIN;
    button_rect.size.width =
        [contentView bounds].size.width - button_rect.origin.x - BUTTON_MARGIN;
    url_textfield_ = [[NSTextField alloc] initWithFrame:button_rect];
    [contentView addSubview:url_textfield_];
    [url_textfield_
        setAutoresizingMask:(NSViewWidthSizable | NSViewMinYMargin)];
    [url_textfield_ setTarget:window_delegate_];
    [url_textfield_ setAction:@selector(takeURLStringValueFrom:)];
    [url_textfield_ setEnabled:NO];
    [[url_textfield_ cell] setWraps:NO];
    [[url_textfield_ cell] setScrollable:YES];
  }

  // Place the window at the target point. This is required for proper placement
  // if the point is on a secondary display.
  [window_ setFrameOrigin:frame_rect.origin];

  if (with_osr_) {
    std::optional<float> parent_scale_factor;
    if (is_popup_) {
      if (auto parent_window =
              MainContext::Get()->GetRootWindowManager()->GetWindowForBrowser(
                  root_window_.opener_browser_id())) {
        parent_scale_factor = parent_window->GetDeviceScaleFactor();
      }
    }

    if (parent_scale_factor) {
      browser_window_->SetDeviceScaleFactor(*parent_scale_factor);
    } else {
      auto display = CefDisplay::GetDisplayMatchingBounds(
          dip_bounds, /*input_pixel_coords=*/false);
      browser_window_->SetDeviceScaleFactor(display->GetDeviceScaleFactor());
    }
  }

  if (!is_popup_) {
    // Create the browser window.
    browser_window_->CreateBrowser(
        CAST_NSVIEW_TO_CEF_WINDOW_HANDLE(contentView),
        CefRect(0, 0, contentBounds.size.width, contentBounds.size.height),
        settings, nullptr, root_window_.delegate_->GetRequestContext());
  } else {
    // With popups we already have a browser window. Parent the browser window
    // to the root window and show it in the correct location.
    browser_window_->ShowPopup(CAST_NSVIEW_TO_CEF_WINDOW_HANDLE(contentView), 0,
                               0, contentBounds.size.width,
                               contentBounds.size.height);
  }

  if (!initially_hidden) {
    auto mode = RootWindow::ShowNormal;
    if (initial_show_state_ == CEF_SHOW_STATE_MAXIMIZED) {
      mode = RootWindow::ShowMaximized;
    } else if (initial_show_state_ == CEF_SHOW_STATE_MINIMIZED) {
      mode = RootWindow::ShowMinimized;
    }

    // Show the window.
    Show(mode);
  }

  root_window_.window_created_ = true;
}

void RootWindowMacImpl::OnBrowserCreated(CefRefPtr<CefBrowser> browser) {
  REQUIRE_MAIN_THREAD();

  // For popup browsers create the root window once the browser has been
  // created.
  if (is_popup_) {
    CreateRootWindow(CefBrowserSettings(), false);
  }
}

void RootWindowMacImpl::OnBrowserWindowDestroyed() {
  REQUIRE_MAIN_THREAD();

  browser_window_.reset();

  if (!window_destroyed_) {
    // The browser was destroyed first. This could be due to the use of
    // off-screen rendering or execution of JavaScript window.close().
    // Close the RootWindow.
    Close(true);
  }

  browser_destroyed_ = true;
  NotifyDestroyedIfDone();
}

void RootWindowMacImpl::OnSetAddress(const std::string& url) {
  REQUIRE_MAIN_THREAD();

  if (url_textfield_) {
    std::string urlStr(url);
    NSString* str = [NSString stringWithUTF8String:urlStr.c_str()];
    [url_textfield_ setStringValue:str];
  }
}

void RootWindowMacImpl::OnSetDraggableRegions(
    const std::vector<CefDraggableRegion>& regions) {
  REQUIRE_MAIN_THREAD();
  // TODO(cef): Implement support for draggable regions on this platform.
}

void RootWindowMacImpl::OnSetTitle(const std::string& title) {
  REQUIRE_MAIN_THREAD();

  if (window_) {
    std::string titleStr(title);
    NSString* str = [NSString stringWithUTF8String:titleStr.c_str()];
    [window_ setTitle:str];
  }
}

void RootWindowMacImpl::OnSetFullscreen(bool fullscreen) {
  REQUIRE_MAIN_THREAD();

  CefRefPtr<CefBrowser> browser = GetBrowser();
  if (browser) {
    std::unique_ptr<window_test::WindowTestRunnerMac> test_runner(
        new window_test::WindowTestRunnerMac());
    if (fullscreen) {
      test_runner->Maximize(browser);
    } else {
      test_runner->Restore(browser);
    }
  }
}

void RootWindowMacImpl::OnAutoResize(const CefSize& new_size) {
  REQUIRE_MAIN_THREAD();

  if (!window_) {
    return;
  }

  CefRect dip_bounds(0, 0, static_cast<int>(new_size.width),
                     static_cast<int>(new_size.height));

  if (auto screen_bounds = GetWindowBoundsInScreen(window_)) {
    dip_bounds.x = (*screen_bounds).x;
    dip_bounds.y = (*screen_bounds).y;
  }

  // Calculate the equivalent frame and content area bounds.
  NSRect frame_rect, content_rect;
  GetNSBoundsInDisplay(dip_bounds, /*input_content_bounds=*/true,
                       [window_ styleMask], with_controls_, frame_rect,
                       content_rect);

  // Don't change the origin.
  frame_rect.origin = window_.frame.origin;

  [window_ setFrame:frame_rect display:YES];

  // Make sure the window is visible.
  Show(RootWindow::ShowNormal);
}

void RootWindowMacImpl::OnSetLoadingState(bool isLoading,
                                          bool canGoBack,
                                          bool canGoForward) {
  REQUIRE_MAIN_THREAD();

  if (with_controls_) {
    [url_textfield_ setEnabled:YES];
    [reload_button_ setEnabled:!isLoading];
    [stop_button_ setEnabled:isLoading];
    [back_button_ setEnabled:canGoBack];
    [forward_button_ setEnabled:canGoForward];
  }

  // After Loading is done, check if voiceover is running and accessibility
  // should be enabled.
  if (!isLoading) {
    Boolean keyExists = false;
    // On OSX there is no API to query if VoiceOver is active or not. The value
    // however is stored in preferences that can be queried.
    if (CFPreferencesGetAppBooleanValue(CFSTR("voiceOverOnOffKey"),
                                        CFSTR("com.apple.universalaccess"),
                                        &keyExists)) {
      GetBrowser()->GetHost()->SetAccessibilityState(STATE_ENABLED);
    }
  }
}

void RootWindowMacImpl::NotifyDestroyedIfDone() {
  // Notify once both the window and the browser have been destroyed.
  if (window_destroyed_ && browser_destroyed_) {
    root_window_.delegate_->OnRootWindowDestroyed(&root_window_);
  }
}

RootWindowMac::RootWindowMac(bool use_alloy_style)
    : RootWindow(use_alloy_style) {
  impl_ = new RootWindowMacImpl(*this);
}

RootWindowMac::~RootWindowMac() {}

BrowserWindow* RootWindowMac::browser_window() const {
  return impl_->browser_window_.get();
}

RootWindow::Delegate* RootWindowMac::delegate() const {
  return delegate_;
}

const OsrRendererSettings* RootWindowMac::osr_settings() const {
  return &impl_->osr_settings_;
}

void RootWindowMac::Init(RootWindow::Delegate* delegate,
                         std::unique_ptr<RootWindowConfig> config,
                         const CefBrowserSettings& settings) {
  DCHECK(delegate);
  delegate_ = delegate;
  impl_->Init(delegate, std::move(config), settings);
}

void RootWindowMac::InitAsPopup(RootWindow::Delegate* delegate,
                                bool with_controls,
                                bool with_osr,
                                const CefPopupFeatures& popupFeatures,
                                CefWindowInfo& windowInfo,
                                CefRefPtr<CefClient>& client,
                                CefBrowserSettings& settings) {
  DCHECK(delegate);
  delegate_ = delegate;
  impl_->InitAsPopup(delegate, with_controls, with_osr, popupFeatures,
                     windowInfo, client, settings);
}

void RootWindowMac::Show(ShowMode mode) {
  impl_->Show(mode);
}

void RootWindowMac::Hide() {
  impl_->Hide();
}

void RootWindowMac::SetBounds(int x,
                              int y,
                              size_t width,
                              size_t height,
                              bool content_bounds) {
  impl_->SetBounds(x, y, width, height, content_bounds);
}

bool RootWindowMac::DefaultToContentBounds() const {
  return impl_->DefaultToContentBounds();
}

void RootWindowMac::Close(bool force) {
  impl_->Close(force);
}

void RootWindowMac::SetDeviceScaleFactor(float device_scale_factor) {
  impl_->SetDeviceScaleFactor(device_scale_factor);
}

std::optional<float> RootWindowMac::GetDeviceScaleFactor() const {
  return impl_->GetDeviceScaleFactor();
}

CefRefPtr<CefBrowser> RootWindowMac::GetBrowser() const {
  return impl_->GetBrowser();
}

ClientWindowHandle RootWindowMac::GetWindowHandle() const {
  return impl_->GetWindowHandle();
}

bool RootWindowMac::WithWindowlessRendering() const {
  return impl_->WithWindowlessRendering();
}

void RootWindowMac::OnBrowserCreated(CefRefPtr<CefBrowser> browser) {
  impl_->OnBrowserCreated(browser);
}

void RootWindowMac::OnBrowserWindowDestroyed() {
  impl_->OnBrowserWindowDestroyed();
}

void RootWindowMac::OnSetAddress(const std::string& url) {
  impl_->OnSetAddress(url);
}

void RootWindowMac::OnSetTitle(const std::string& title) {
  impl_->OnSetTitle(title);
}

void RootWindowMac::OnSetFullscreen(bool fullscreen) {
  impl_->OnSetFullscreen(fullscreen);
}

void RootWindowMac::OnAutoResize(const CefSize& new_size) {
  impl_->OnAutoResize(new_size);
}

void RootWindowMac::OnSetLoadingState(bool isLoading,
                                      bool canGoBack,
                                      bool canGoForward) {
  impl_->OnSetLoadingState(isLoading, canGoBack, canGoForward);
}

void RootWindowMac::OnSetDraggableRegions(
    const std::vector<CefDraggableRegion>& regions) {
  impl_->OnSetDraggableRegions(regions);
}

void RootWindowMac::OnNativeWindowClosed() {
  impl_->OnNativeWindowClosed();
}

}  // namespace client

@implementation RootWindowDelegate

@synthesize root_window = root_window_;
@synthesize last_visible_bounds = last_visible_bounds_;
@synthesize force_close = force_close_;

- (id)initWithWindow:(NSWindow*)window
       andRootWindow:(client::RootWindowMac*)root_window {
  if (self = [super init]) {
    window_ = window;
    [window_ setDelegate:self];
    root_window_ = root_window;
    force_close_ = false;

    // Register for application hide/unhide notifications.
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(applicationDidHide:)
               name:NSApplicationDidHideNotification
             object:nil];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(applicationDidUnhide:)
               name:NSApplicationDidUnhideNotification
             object:nil];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
#if !__has_feature(objc_arc)
  [super dealloc];
#endif  // !__has_feature(objc_arc)
}

- (IBAction)goBack:(id)sender {
  CefRefPtr<CefBrowser> browser = root_window_->GetBrowser();
  if (browser.get()) {
    browser->GoBack();
  }
}

- (IBAction)goForward:(id)sender {
  CefRefPtr<CefBrowser> browser = root_window_->GetBrowser();
  if (browser.get()) {
    browser->GoForward();
  }
}

- (IBAction)reload:(id)sender {
  CefRefPtr<CefBrowser> browser = root_window_->GetBrowser();
  if (browser.get()) {
    browser->Reload();
  }
}

- (IBAction)stopLoading:(id)sender {
  CefRefPtr<CefBrowser> browser = root_window_->GetBrowser();
  if (browser.get()) {
    browser->StopLoad();
  }
}

- (IBAction)takeURLStringValueFrom:(NSTextField*)sender {
  CefRefPtr<CefBrowser> browser = root_window_->GetBrowser();
  if (!browser.get()) {
    return;
  }

  NSString* url = [sender stringValue];

  // if it doesn't already have a prefix, add http. If we can't parse it,
  // just don't bother rather than making things worse.
  NSURL* tempUrl = [NSURL URLWithString:url];
  if (tempUrl && ![tempUrl scheme]) {
    url = [@"http://" stringByAppendingString:url];
  }

  std::string urlStr = [url UTF8String];
  browser->GetMainFrame()->LoadURL(urlStr);
}

// Called when we are activated (when we gain focus).
- (void)windowDidBecomeKey:(NSNotification*)notification {
  client::BrowserWindow* browser_window = root_window_->browser_window();
  if (browser_window) {
    browser_window->SetFocus(true);
  }
  root_window_->delegate()->OnRootWindowActivated(root_window_);
}

// Called when we are deactivated (when we lose focus).
- (void)windowDidResignKey:(NSNotification*)notification {
  client::BrowserWindow* browser_window = root_window_->browser_window();
  if (browser_window) {
    browser_window->SetFocus(false);
  }
}

// Called when we have been minimized.
- (void)windowDidMiniaturize:(NSNotification*)notification {
  client::BrowserWindow* browser_window = root_window_->browser_window();
  if (browser_window) {
    browser_window->Hide();
  }
}

// Called when we have been unminimized.
- (void)windowDidDeminiaturize:(NSNotification*)notification {
  client::BrowserWindow* browser_window = root_window_->browser_window();
  if (browser_window) {
    browser_window->Show();
  }
}

// Called when we have been resized.
- (void)windowDidResize:(NSNotification*)notification {
  // Track the last visible bounds for window restore purposes.
  const auto dip_bounds = client::GetWindowBoundsInScreen(window_);
  if (dip_bounds) {
    last_visible_bounds_ = dip_bounds;
  }
}

// Called when we have been moved.
- (void)windowDidMove:(NSNotification*)notification {
  // Track the last visible bounds for window restore purposes.
  const auto dip_bounds = client::GetWindowBoundsInScreen(window_);
  if (dip_bounds) {
    last_visible_bounds_ = dip_bounds;
  }

  if (root_window_->WithWindowlessRendering() &&
      root_window_->osr_settings()->real_screen_bounds) {
    // Send the new root window bounds to the renderer.
    if (auto* browser_window = root_window_->browser_window()) {
      if (auto browser = browser_window->GetBrowser()) {
        browser->GetHost()->NotifyScreenInfoChanged();
      }
    }
  }
}

// Called when the application has been hidden.
- (void)applicationDidHide:(NSNotification*)notification {
  // If the window is miniaturized then nothing has really changed.
  if (![window_ isMiniaturized]) {
    client::BrowserWindow* browser_window = root_window_->browser_window();
    if (browser_window) {
      browser_window->Hide();
    }
  }
}

// Called when the application has been unhidden.
- (void)applicationDidUnhide:(NSNotification*)notification {
  // If the window is miniaturized then nothing has really changed.
  if (![window_ isMiniaturized]) {
    client::BrowserWindow* browser_window = root_window_->browser_window();
    if (browser_window) {
      browser_window->Show();
    }
  }
}

// Called when the window is about to close. Perform the self-destruction
// sequence by getting rid of the window. By returning YES, we allow the window
// to be removed from the screen.
- (BOOL)windowShouldClose:(NSWindow*)window {
  if (!force_close_) {
    client::BrowserWindow* browser_window = root_window_->browser_window();
    if (browser_window && !browser_window->IsClosing()) {
      CefRefPtr<CefBrowser> browser = browser_window->GetBrowser();
      if (browser.get()) {
        // Notify the browser window that we would like to close it. This
        // will result in a call to ClientHandler::DoClose() if the
        // JavaScript 'onbeforeunload' event handler allows it.
        browser->GetHost()->CloseBrowser(false);

        // Cancel the close.
        return NO;
      }
    }
  }

  // Save window restore position.
  std::optional<CefRect> dip_bounds;
  cef_show_state_t show_state = CEF_SHOW_STATE_NORMAL;
  if ([window_ isMiniaturized]) {
    show_state = CEF_SHOW_STATE_MINIMIZED;
  } else if ([window_ isZoomed]) {
    show_state = CEF_SHOW_STATE_MAXIMIZED;
  } else {
    dip_bounds = client::GetWindowBoundsInScreen(window_);
  }

  if (!dip_bounds) {
    dip_bounds = last_visible_bounds_;
  }

  client::prefs::SaveWindowRestorePreferences(show_state, dip_bounds);

  // Clean ourselves up after clearing the stack of anything that might have the
  // window on it.
  [self cleanup];

  // Allow the close.
  return YES;
}

// Deletes itself.
- (void)cleanup {
  // Don't want any more delegate callbacks after we destroy ourselves.
  window_.delegate = nil;
  window_.contentView = [[NSView alloc] initWithFrame:NSZeroRect];
  // Delete the window.
#if !__has_feature(objc_arc)
  [window_ autorelease];
#endif  // !__has_feature(objc_arc)
  window_ = nil;
  root_window_->OnNativeWindowClosed();
}

@end  // @implementation RootWindowDelegate
