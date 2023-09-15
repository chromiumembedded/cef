// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/native/browser_platform_delegate_native_mac.h"

#import <Cocoa/Cocoa.h>
#import <CoreServices/CoreServices.h>

#include "include/internal/cef_types_mac.h"
#include "libcef/browser/alloy/alloy_browser_host_impl.h"
#include "libcef/browser/context.h"
#include "libcef/browser/native/javascript_dialog_runner_mac.h"
#include "libcef/browser/native/menu_runner_mac.h"
#include "libcef/browser/thread_util.h"

#include "base/apple/owned_objc.h"
#include "base/apple/scoped_nsautorelease_pool.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/renderer_host/render_widget_host_view_mac.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/input/native_web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#include "ui/display/screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/gfx/geometry/rect.h"

// Wrapper NSView for the native view. Necessary to destroy the browser when
// the view is deleted.
@interface CefBrowserHostView : NSView {
 @private
  CefBrowserHostBase* browser_;  // weak
}

@property(nonatomic, assign) CefBrowserHostBase* browser;

@end

@implementation CefBrowserHostView

@synthesize browser = browser_;

- (void)dealloc {
  if (browser_) {
    // Force the browser to be destroyed and release the reference added in
    // PlatformCreateWindow().
    static_cast<AlloyBrowserHostImpl*>(browser_)->WindowDestroyed();
  }
}

@end

// Receives notifications from the browser window. Will delete itself when done.
@interface CefWindowDelegate : NSObject <NSWindowDelegate> {
 @private
  CefBrowserHostBase* browser_;  // weak
  NSWindow* __strong window_;
}
- (id)initWithWindow:(NSWindow*)window andBrowser:(CefBrowserHostBase*)browser;
@end

@implementation CefWindowDelegate

- (id)initWithWindow:(NSWindow*)window andBrowser:(CefBrowserHostBase*)browser {
  if (self = [super init]) {
    window_ = window;
    browser_ = browser;
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (BOOL)windowShouldClose:(id)window {
  if (browser_ && !browser_->TryCloseBrowser()) {
    // Cancel the close.
    return NO;
  }

  // For an NSWindow object, the default is to be released on |close|. We
  // instead want it to remain valid until all strong references are released
  // via |cleanup:| and |BrowserDestroyed|.
  ((NSWindow*)window).releasedWhenClosed = NO;

  // Clean ourselves up after clearing the stack of anything that might have the
  // window on it.
  [self performSelectorOnMainThread:@selector(cleanup:)
                         withObject:window
                      waitUntilDone:NO];

  // Allow the close.
  return YES;
}

- (void)cleanup:(id)window {
  [window_ setDelegate:nil];
  window_ = nil;
}

@end

namespace {

NSTimeInterval currentEventTimestamp() {
  NSEvent* currentEvent = [NSApp currentEvent];
  if (currentEvent) {
    return [currentEvent timestamp];
  } else {
    // FIXME(API): In case there is no current event, the timestamp could be
    // obtained by getting the time since the application started. This involves
    // taking some more static functions from Chromium code.
    // Another option is to have the timestamp as a field in CefEvent structures
    // and let the client provide it.
    return 0;
  }
}

NSUInteger NativeModifiers(int cef_modifiers) {
  NSUInteger native_modifiers = 0;
  if (cef_modifiers & EVENTFLAG_SHIFT_DOWN) {
    native_modifiers |= NSEventModifierFlagShift;
  }
  if (cef_modifiers & EVENTFLAG_CONTROL_DOWN) {
    native_modifiers |= NSEventModifierFlagControl;
  }
  if (cef_modifiers & EVENTFLAG_ALT_DOWN) {
    native_modifiers |= NSEventModifierFlagOption;
  }
  if (cef_modifiers & EVENTFLAG_COMMAND_DOWN) {
    native_modifiers |= NSEventModifierFlagCommand;
  }
  if (cef_modifiers & EVENTFLAG_CAPS_LOCK_ON) {
    native_modifiers |= NSEventModifierFlagCapsLock;
  }
  if (cef_modifiers & EVENTFLAG_NUM_LOCK_ON) {
    native_modifiers |= NSEventModifierFlagNumericPad;
  }

  return native_modifiers;
}

constexpr int kDefaultHeight = 750;
constexpr int kDefaultWidth = 750;
constexpr NSWindowStyleMask kDefaultStyleMask =
    NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
    NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;

// Keep the frame bounds inside the display work area.
NSRect ClampNSBoundsToWorkArea(const NSRect& frame_bounds,
                               const gfx::Rect& display_bounds,
                               const gfx::Rect& work_area) {
  NSRect bounds = frame_bounds;

  // Convert from DIP coordinates (top-left origin) to macOS coordinates
  // (bottom-left origin).
  const int work_area_y =
      display_bounds.height() - work_area.height() - work_area.y();

  if (bounds.size.width > work_area.width()) {
    bounds.size.width = work_area.width();
  }
  if (bounds.size.height > work_area.height()) {
    bounds.size.height = work_area.height();
  }

  if (bounds.origin.x < work_area.x()) {
    bounds.origin.x = work_area.x();
  } else if (bounds.origin.x + bounds.size.width >=
             work_area.x() + work_area.width()) {
    bounds.origin.x = work_area.x() + work_area.width() - bounds.size.width;
  }

  if (bounds.origin.y < work_area_y) {
    bounds.origin.y = work_area_y;
  } else if (bounds.origin.y + bounds.size.height >=
             work_area_y + work_area.height()) {
    bounds.origin.y = work_area_y + work_area.height() - bounds.size.height;
  }

  return bounds;
}

// Get frame and content area rects matching the input DIP screen bounds. The
// resulting window frame will be kept inside the closest display work area. If
// |input_content_bounds| is true the input size is used for the content area
// and the input origin is used for the frame. Otherwise, both input size and
// origin are used for the frame.
void GetNSBoundsInDisplay(const gfx::Rect& dip_bounds,
                          bool input_content_bounds,
                          NSWindowStyleMask style_mask,
                          NSRect& frame_rect,
                          NSRect& content_rect) {
  // Identify the closest display.
  const auto display =
      display::Screen::GetScreen()->GetDisplayMatching(dip_bounds);
  const auto& display_bounds = display.bounds();
  const auto& display_work_area = display.work_area();

  // Convert from DIP coordinates (top-left origin) to macOS coordinates
  // (bottom-left origin).
  NSRect requested_rect = NSMakeRect(dip_bounds.x(), dip_bounds.y(),
                                     dip_bounds.width(), dip_bounds.height());
  requested_rect.origin.y = display_bounds.height() -
                            requested_rect.size.height -
                            requested_rect.origin.y;

  // Calculate the equivalent frame and content bounds.
  if (input_content_bounds) {
    // Compute frame rect from content rect. Keep the requested origin.
    content_rect = requested_rect;
    frame_rect = [NSWindow frameRectForContentRect:content_rect
                                         styleMask:style_mask];
    frame_rect.origin = requested_rect.origin;
  } else {
    // Compute content rect from frame rect.
    frame_rect = requested_rect;
    content_rect = [NSWindow contentRectForFrameRect:frame_rect
                                           styleMask:style_mask];
  }

  // Keep the frame inside the display work area.
  const NSRect new_frame_rect =
      ClampNSBoundsToWorkArea(frame_rect, display_bounds, display_work_area);
  if (!NSEqualRects(frame_rect, new_frame_rect)) {
    frame_rect = new_frame_rect;
    content_rect = [NSWindow contentRectForFrameRect:frame_rect
                                           styleMask:style_mask];
  }
}

}  // namespace

CefBrowserPlatformDelegateNativeMac::CefBrowserPlatformDelegateNativeMac(
    const CefWindowInfo& window_info,
    SkColor background_color)
    : CefBrowserPlatformDelegateNative(window_info, background_color) {}

void CefBrowserPlatformDelegateNativeMac::BrowserDestroyed(
    CefBrowserHostBase* browser) {
  CefBrowserPlatformDelegateNative::BrowserDestroyed(browser);

  if (host_window_created_) {
    // Release the references added in CreateHostWindow().
    browser->Release();
    window_delegate_ = nil;
  }
}

bool CefBrowserPlatformDelegateNativeMac::CreateHostWindow() {
  base::apple::ScopedNSAutoreleasePool autorelease_pool;

  NSWindow* new_window = nil;

  NSView* parent_view =
      CAST_CEF_WINDOW_HANDLE_TO_NSVIEW(window_info_.parent_view);
  NSRect browser_view_rect =
      NSMakeRect(window_info_.bounds.x, window_info_.bounds.y,
                 window_info_.bounds.width, window_info_.bounds.height);

  if (parent_view == nil) {
    // TODO(port): If no x,y position is specified the window will always appear
    // in the upper-left corner. Maybe there's a better default place to put it?
    const gfx::Rect dip_bounds(
        window_info_.bounds.x, window_info_.bounds.y,
        window_info_.bounds.width <= 0 ? kDefaultWidth
                                       : window_info_.bounds.width,
        window_info_.bounds.height <= 0 ? kDefaultHeight
                                        : window_info_.bounds.height);

    // Calculate the equivalent frame and content area bounds.
    NSRect frame_rect, content_rect;
    GetNSBoundsInDisplay(dip_bounds, /*input_content_bounds=*/true,
                         kDefaultStyleMask, frame_rect, content_rect);

    // Create a new window.
    new_window = [[NSWindow alloc] initWithContentRect:content_rect
                                             styleMask:kDefaultStyleMask
                                               backing:NSBackingStoreBuffered
                                                 defer:NO];

    // Create the delegate for control and browser window events.
    // Add a reference that will be released in BrowserDestroyed().
    window_delegate_ = [[CefWindowDelegate alloc] initWithWindow:new_window
                                                      andBrowser:browser_];
    [new_window setDelegate:window_delegate_];

    parent_view = [new_window contentView];
    browser_view_rect = [parent_view bounds];

    window_info_.parent_view = CAST_NSVIEW_TO_CEF_WINDOW_HANDLE(parent_view);

    // Make the content view for the window have a layer. This will make all
    // sub-views have layers. This is necessary to ensure correct layer
    // ordering of all child views and their layers.
    [parent_view setWantsLayer:YES];

    // Place the window at the target point. This is required for proper
    // placement if the point is on a secondary display.
    [new_window setFrameOrigin:frame_rect.origin];
  }

  host_window_created_ = true;

  // Add a reference that will be released in BrowserDestroyed().
  browser_->AddRef();

  // Create the browser view.
  CefBrowserHostView* browser_view =
      [[CefBrowserHostView alloc] initWithFrame:browser_view_rect];
  browser_view.browser = browser_;
  [parent_view addSubview:browser_view];
  [browser_view setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
  [browser_view setNeedsDisplay:YES];

  // Parent the WebContents to the browser view.
  const NSRect bounds = [browser_view bounds];
  NSView* native_view = web_contents_->GetNativeView().GetNativeNSView();
  [browser_view addSubview:native_view];
  [native_view setFrame:bounds];
  [native_view setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
  [native_view setNeedsDisplay:YES];

  window_info_.view = CAST_NSVIEW_TO_CEF_WINDOW_HANDLE(browser_view);

  if (new_window != nil && !window_info_.hidden) {
    // Show the window.
    [new_window makeKeyAndOrderFront:nil];
  }

  return true;
}

void CefBrowserPlatformDelegateNativeMac::CloseHostWindow() {
  NSView* nsview = CAST_CEF_WINDOW_HANDLE_TO_NSVIEW(window_info_.view);
  if (nsview != nil) {
    [[nsview window] performSelectorOnMainThread:@selector(performClose:)
                                      withObject:nil
                                   waitUntilDone:NO];
  }
}

CefWindowHandle CefBrowserPlatformDelegateNativeMac::GetHostWindowHandle()
    const {
  if (windowless_handler_) {
    return windowless_handler_->GetParentWindowHandle();
  }
  return window_info_.view;
}

void CefBrowserPlatformDelegateNativeMac::SendKeyEvent(
    const CefKeyEvent& event) {
  auto view = GetHostView();
  if (!view) {
    return;
  }

  content::NativeWebKeyboardEvent web_event = TranslateWebKeyEvent(event);
  view->ForwardKeyboardEvent(web_event, ui::LatencyInfo());
}

void CefBrowserPlatformDelegateNativeMac::SendMouseClickEvent(
    const CefMouseEvent& event,
    CefBrowserHost::MouseButtonType type,
    bool mouseUp,
    int clickCount) {
  auto view = GetHostView();
  if (!view) {
    return;
  }

  blink::WebMouseEvent web_event =
      TranslateWebClickEvent(event, type, mouseUp, clickCount);
  view->RouteOrProcessMouseEvent(web_event);
}

void CefBrowserPlatformDelegateNativeMac::SendMouseMoveEvent(
    const CefMouseEvent& event,
    bool mouseLeave) {
  auto view = GetHostView();
  if (!view) {
    return;
  }

  blink::WebMouseEvent web_event = TranslateWebMoveEvent(event, mouseLeave);
  view->RouteOrProcessMouseEvent(web_event);
}

void CefBrowserPlatformDelegateNativeMac::SendMouseWheelEvent(
    const CefMouseEvent& event,
    int deltaX,
    int deltaY) {
  auto view = GetHostView();
  if (!view) {
    return;
  }

  blink::WebMouseWheelEvent web_event =
      TranslateWebWheelEvent(event, deltaX, deltaY);
  view->RouteOrProcessMouseEvent(web_event);
}

void CefBrowserPlatformDelegateNativeMac::SendTouchEvent(
    const CefTouchEvent& event) {
  NOTIMPLEMENTED();
}

void CefBrowserPlatformDelegateNativeMac::SetFocus(bool setFocus) {
  auto view = GetHostView();
  if (view) {
    view->SetActive(setFocus);

    if (setFocus) {
      // Give keyboard focus to the native view.
      NSView* nsview = web_contents_->GetContentNativeView().GetNativeNSView();
      DCHECK([nsview canBecomeKeyView]);
      [[nsview window] makeFirstResponder:nsview];
    }
  }
}

gfx::Point CefBrowserPlatformDelegateNativeMac::GetScreenPoint(
    const gfx::Point& view,
    bool want_dip_coords) const {
  // Mac always operates in DIP coordinates so |want_dip_coords| is ignored.
  if (windowless_handler_) {
    return windowless_handler_->GetParentScreenPoint(view, want_dip_coords);
  }

  NSView* nsview = CAST_CEF_WINDOW_HANDLE_TO_NSVIEW(window_info_.parent_view);
  if (nsview) {
    NSRect bounds = [nsview bounds];
    NSPoint view_pt = {static_cast<CGFloat>(view.x()),
                       bounds.size.height - static_cast<CGFloat>(view.y())};
    NSPoint window_pt = [nsview convertPoint:view_pt toView:nil];
    NSPoint screen_pt = [[nsview window] convertPointToScreen:window_pt];
    return gfx::Point(screen_pt.x, screen_pt.y);
  }
  return gfx::Point();
}

void CefBrowserPlatformDelegateNativeMac::ViewText(const std::string& text) {
  // TODO(cef): Implement this functionality.
  NOTIMPLEMENTED();
}

bool CefBrowserPlatformDelegateNativeMac::HandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  // Give the top level menu equivalents a chance to handle the event.
  NSEvent* ns_event = event.os_event.Get();
  if (ns_event.type == NSEventTypeKeyDown) {
    return [[NSApp mainMenu] performKeyEquivalent:ns_event];
  }
  return false;
}

CefEventHandle CefBrowserPlatformDelegateNativeMac::GetEventHandle(
    const content::NativeWebKeyboardEvent& event) const {
  return CAST_NSEVENT_TO_CEF_EVENT_HANDLE(event.os_event.Get());
}

std::unique_ptr<CefJavaScriptDialogRunner>
CefBrowserPlatformDelegateNativeMac::CreateJavaScriptDialogRunner() {
  return base::WrapUnique(new CefJavaScriptDialogRunnerMac);
}

std::unique_ptr<CefMenuRunner>
CefBrowserPlatformDelegateNativeMac::CreateMenuRunner() {
  return base::WrapUnique(new CefMenuRunnerMac);
}

content::NativeWebKeyboardEvent
CefBrowserPlatformDelegateNativeMac::TranslateWebKeyEvent(
    const CefKeyEvent& key_event) const {
  content::NativeWebKeyboardEvent result(
      blink::WebInputEvent::Type::kUndefined,
      blink::WebInputEvent::Modifiers::kNoModifiers, ui::EventTimeForNow());

  // Use a synthetic NSEvent in order to obtain the windowsKeyCode member from
  // the NativeWebKeyboardEvent constructor. This is the only member which can
  // not be easily translated (without hardcoding keyCodes)
  // Determining whether a modifier key is left or right seems to be done
  // through the key code as well.
  NSEventType event_type;
  if (key_event.character == 0 && key_event.unmodified_character == 0) {
    // Check if both character and unmodified_characther are empty to determine
    // if this was a NSEventTypeFlagsChanged event.
    // A dead key will have an empty character, but a non-empty unmodified
    // character
    event_type = NSEventTypeFlagsChanged;
  } else {
    switch (key_event.type) {
      case KEYEVENT_RAWKEYDOWN:
      case KEYEVENT_KEYDOWN:
      case KEYEVENT_CHAR:
        event_type = NSEventTypeKeyDown;
        break;
      case KEYEVENT_KEYUP:
        event_type = NSEventTypeKeyUp;
        break;
    }
  }

  NSString* charactersIgnoringModifiers =
      [[NSString alloc] initWithCharacters:reinterpret_cast<const unichar*>(
                                               &key_event.unmodified_character)
                                    length:1];
  NSString* characters = [[NSString alloc]
      initWithCharacters:reinterpret_cast<const unichar*>(&key_event.character)
                  length:1];

  NSEvent* synthetic_event =
      [NSEvent keyEventWithType:event_type
                             location:NSMakePoint(0, 0)
                        modifierFlags:NativeModifiers(key_event.modifiers)
                            timestamp:currentEventTimestamp()
                         windowNumber:0
                              context:nil
                           characters:characters
          charactersIgnoringModifiers:charactersIgnoringModifiers
                            isARepeat:NO
                              keyCode:key_event.native_key_code];

  result = content::NativeWebKeyboardEvent(
      base::apple::OwnedNSEvent(synthetic_event));
  if (key_event.type == KEYEVENT_CHAR) {
    result.SetType(blink::WebInputEvent::Type::kChar);
  }

  result.is_system_key = key_event.is_system_key;

  return result;
}

blink::WebMouseEvent
CefBrowserPlatformDelegateNativeMac::TranslateWebClickEvent(
    const CefMouseEvent& mouse_event,
    CefBrowserHost::MouseButtonType type,
    bool mouseUp,
    int clickCount) const {
  blink::WebMouseEvent result;
  TranslateWebMouseEvent(result, mouse_event);

  switch (type) {
    case MBT_LEFT:
      result.SetType(mouseUp ? blink::WebInputEvent::Type::kMouseUp
                             : blink::WebInputEvent::Type::kMouseDown);
      result.button = blink::WebMouseEvent::Button::kLeft;
      break;
    case MBT_MIDDLE:
      result.SetType(mouseUp ? blink::WebInputEvent::Type::kMouseUp
                             : blink::WebInputEvent::Type::kMouseDown);
      result.button = blink::WebMouseEvent::Button::kMiddle;
      break;
    case MBT_RIGHT:
      result.SetType(mouseUp ? blink::WebInputEvent::Type::kMouseUp
                             : blink::WebInputEvent::Type::kMouseDown);
      result.button = blink::WebMouseEvent::Button::kRight;
      break;
    default:
      DCHECK(false);
  }

  result.click_count = clickCount;

  return result;
}

blink::WebMouseEvent CefBrowserPlatformDelegateNativeMac::TranslateWebMoveEvent(
    const CefMouseEvent& mouse_event,
    bool mouseLeave) const {
  blink::WebMouseEvent result;
  TranslateWebMouseEvent(result, mouse_event);

  if (!mouseLeave) {
    result.SetType(blink::WebInputEvent::Type::kMouseMove);
    if (mouse_event.modifiers & EVENTFLAG_LEFT_MOUSE_BUTTON) {
      result.button = blink::WebMouseEvent::Button::kLeft;
    } else if (mouse_event.modifiers & EVENTFLAG_MIDDLE_MOUSE_BUTTON) {
      result.button = blink::WebMouseEvent::Button::kMiddle;
    } else if (mouse_event.modifiers & EVENTFLAG_RIGHT_MOUSE_BUTTON) {
      result.button = blink::WebMouseEvent::Button::kRight;
    } else {
      result.button = blink::WebMouseEvent::Button::kNoButton;
    }
  } else {
    result.SetType(blink::WebInputEvent::Type::kMouseLeave);
    result.button = blink::WebMouseEvent::Button::kNoButton;
  }

  result.click_count = 0;

  return result;
}

blink::WebMouseWheelEvent
CefBrowserPlatformDelegateNativeMac::TranslateWebWheelEvent(
    const CefMouseEvent& mouse_event,
    int deltaX,
    int deltaY) const {
  blink::WebMouseWheelEvent result;
  TranslateWebMouseEvent(result, mouse_event);

  result.SetType(blink::WebInputEvent::Type::kMouseWheel);

  static const double scrollbarPixelsPerCocoaTick = 40.0;
  result.delta_x = deltaX;
  result.delta_y = deltaY;
  result.wheel_ticks_x = deltaX / scrollbarPixelsPerCocoaTick;
  result.wheel_ticks_y = deltaY / scrollbarPixelsPerCocoaTick;
  result.delta_units = ui::ScrollGranularity::kScrollByPrecisePixel;

  if (mouse_event.modifiers & EVENTFLAG_LEFT_MOUSE_BUTTON) {
    result.button = blink::WebMouseEvent::Button::kLeft;
  } else if (mouse_event.modifiers & EVENTFLAG_MIDDLE_MOUSE_BUTTON) {
    result.button = blink::WebMouseEvent::Button::kMiddle;
  } else if (mouse_event.modifiers & EVENTFLAG_RIGHT_MOUSE_BUTTON) {
    result.button = blink::WebMouseEvent::Button::kRight;
  } else {
    result.button = blink::WebMouseEvent::Button::kNoButton;
  }

  return result;
}

void CefBrowserPlatformDelegateNativeMac::TranslateWebMouseEvent(
    blink::WebMouseEvent& result,
    const CefMouseEvent& mouse_event) const {
  // position
  result.SetPositionInWidget(mouse_event.x, mouse_event.y);

  const gfx::Point& screen_pt = GetScreenPoint(
      gfx::Point(mouse_event.x, mouse_event.y), /*want_dip_coords=*/true);
  result.SetPositionInScreen(screen_pt.x(), screen_pt.y());

  // modifiers
  result.SetModifiers(result.GetModifiers() |
                      TranslateWebEventModifiers(mouse_event.modifiers));

  // timestamp
  result.SetTimeStamp(base::TimeTicks() +
                      base::Seconds(currentEventTimestamp()));

  result.pointer_type = blink::WebPointerProperties::PointerType::kMouse;
}

content::RenderWidgetHostViewMac*
CefBrowserPlatformDelegateNativeMac::GetHostView() const {
  if (!web_contents_) {
    return nullptr;
  }
  return static_cast<content::RenderWidgetHostViewMac*>(
      web_contents_->GetRenderWidgetHostView());
}
