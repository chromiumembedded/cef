// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/native/browser_platform_delegate_native_mac.h"

#import <Cocoa/Cocoa.h>
#import <CoreServices/CoreServices.h>

#include "libcef/browser/alloy/alloy_browser_host_impl.h"
#include "libcef/browser/context.h"
#include "libcef/browser/native/file_dialog_runner_mac.h"
#include "libcef/browser/native/javascript_dialog_runner_mac.h"
#include "libcef/browser/native/menu_runner_mac.h"
#include "libcef/browser/thread_util.h"

#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/renderer_host/render_widget_host_view_mac.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#import "ui/base/cocoa/cocoa_base_utils.h"
#import "ui/base/cocoa/underlay_opengl_hosting_window.h"
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

  [super dealloc];
}

@end

// Receives notifications from the browser window. Will delete itself when done.
@interface CefWindowDelegate : NSObject <NSWindowDelegate> {
 @private
  CefBrowserHostBase* browser_;  // weak
  NSWindow* window_;
}
- (id)initWithWindow:(NSWindow*)window andBrowser:(CefBrowserHostBase*)browser;
@end

@implementation CefWindowDelegate

- (id)initWithWindow:(NSWindow*)window andBrowser:(CefBrowserHostBase*)browser {
  if (self = [super init]) {
    window_ = window;
    browser_ = browser;

    [window_ setDelegate:self];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];

  [super dealloc];
}

- (BOOL)windowShouldClose:(id)window {
  if (browser_ && !browser_->TryCloseBrowser()) {
    // Cancel the close.
    return NO;
  }

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
  [self release];
}

@end

namespace {

NSTimeInterval currentEventTimestamp() {
  NSEvent* currentEvent = [NSApp currentEvent];
  if (currentEvent)
    return [currentEvent timestamp];
  else {
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
  if (cef_modifiers & EVENTFLAG_SHIFT_DOWN)
    native_modifiers |= NSShiftKeyMask;
  if (cef_modifiers & EVENTFLAG_CONTROL_DOWN)
    native_modifiers |= NSControlKeyMask;
  if (cef_modifiers & EVENTFLAG_ALT_DOWN)
    native_modifiers |= NSAlternateKeyMask;
  if (cef_modifiers & EVENTFLAG_COMMAND_DOWN)
    native_modifiers |= NSCommandKeyMask;
  if (cef_modifiers & EVENTFLAG_CAPS_LOCK_ON)
    native_modifiers |= NSAlphaShiftKeyMask;
  if (cef_modifiers & EVENTFLAG_NUM_LOCK_ON)
    native_modifiers |= NSNumericPadKeyMask;

  return native_modifiers;
}

}  // namespace

CefBrowserPlatformDelegateNativeMac::CefBrowserPlatformDelegateNativeMac(
    const CefWindowInfo& window_info,
    SkColor background_color)
    : CefBrowserPlatformDelegateNative(window_info, background_color),
      host_window_created_(false) {}

void CefBrowserPlatformDelegateNativeMac::BrowserDestroyed(
    CefBrowserHostBase* browser) {
  CefBrowserPlatformDelegateNative::BrowserDestroyed(browser);

  if (host_window_created_) {
    // Release the reference added in CreateHostWindow().
    browser->Release();
  }
}

bool CefBrowserPlatformDelegateNativeMac::CreateHostWindow() {
  base::mac::ScopedNSAutoreleasePool autorelease_pool;

  NSWindow* newWnd = nil;

  NSView* parentView =
      CAST_CEF_WINDOW_HANDLE_TO_NSVIEW(window_info_.parent_view);
  NSRect contentRect = {{static_cast<CGFloat>(window_info_.bounds.x),
                         static_cast<CGFloat>(window_info_.bounds.y)},
                        {static_cast<CGFloat>(window_info_.bounds.width),
                         static_cast<CGFloat>(window_info_.bounds.height)}};
  if (parentView == nil) {
    // Create a new window.
    NSRect screen_rect = [[NSScreen mainScreen] visibleFrame];
    NSRect window_rect = {
        {static_cast<CGFloat>(window_info_.bounds.x),
         screen_rect.size.height - static_cast<CGFloat>(window_info_.bounds.y)},
        {static_cast<CGFloat>(window_info_.bounds.width),
         static_cast<CGFloat>(window_info_.bounds.height)}};
    if (window_rect.size.width == 0)
      window_rect.size.width = 750;
    if (window_rect.size.height == 0)
      window_rect.size.height = 750;

    contentRect.origin.x = 0;
    contentRect.origin.y = 0;
    contentRect.size.width = window_rect.size.width;
    contentRect.size.height = window_rect.size.height;

    newWnd = [[UnderlayOpenGLHostingWindow alloc]
        initWithContentRect:window_rect
                  styleMask:(NSTitledWindowMask | NSClosableWindowMask |
                             NSMiniaturizableWindowMask |
                             NSResizableWindowMask |
                             NSUnifiedTitleAndToolbarWindowMask)
                    backing:NSBackingStoreBuffered
                      defer:NO];

    // Create the delegate for control and browser window events.
    [[CefWindowDelegate alloc] initWithWindow:newWnd andBrowser:browser_];

    parentView = [newWnd contentView];
    window_info_.parent_view = parentView;

    // Make the content view for the window have a layer. This will make all
    // sub-views have layers. This is necessary to ensure correct layer
    // ordering of all child views and their layers.
    [parentView setWantsLayer:YES];
  }

  host_window_created_ = true;

  // Add a reference that will be released in BrowserDestroyed().
  browser_->AddRef();

  // Create the browser view.
  CefBrowserHostView* browser_view =
      [[CefBrowserHostView alloc] initWithFrame:contentRect];
  browser_view.browser = browser_;
  [parentView addSubview:browser_view];
  [browser_view setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
  [browser_view setNeedsDisplay:YES];
  [browser_view release];

  // Parent the TabContents to the browser view.
  const NSRect bounds = [browser_view bounds];
  NSView* native_view = web_contents_->GetNativeView().GetNativeNSView();
  [browser_view addSubview:native_view];
  [native_view setFrame:bounds];
  [native_view setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
  [native_view setNeedsDisplay:YES];

  window_info_.view = browser_view;

  if (newWnd != nil && !window_info_.hidden) {
    // Show the window.
    [newWnd makeKeyAndOrderFront:nil];
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
  if (windowless_handler_)
    return windowless_handler_->GetParentWindowHandle();
  return window_info_.view;
}

void CefBrowserPlatformDelegateNativeMac::SendKeyEvent(
    const CefKeyEvent& event) {
  auto view = GetHostView();
  if (!view)
    return;

  content::NativeWebKeyboardEvent web_event = TranslateWebKeyEvent(event);
  view->ForwardKeyboardEvent(web_event, ui::LatencyInfo());
}

void CefBrowserPlatformDelegateNativeMac::SendMouseClickEvent(
    const CefMouseEvent& event,
    CefBrowserHost::MouseButtonType type,
    bool mouseUp,
    int clickCount) {
  auto view = GetHostView();
  if (!view)
    return;

  blink::WebMouseEvent web_event =
      TranslateWebClickEvent(event, type, mouseUp, clickCount);
  view->RouteOrProcessMouseEvent(web_event);
}

void CefBrowserPlatformDelegateNativeMac::SendMouseMoveEvent(
    const CefMouseEvent& event,
    bool mouseLeave) {
  auto view = GetHostView();
  if (!view)
    return;

  blink::WebMouseEvent web_event = TranslateWebMoveEvent(event, mouseLeave);
  view->RouteOrProcessMouseEvent(web_event);
}

void CefBrowserPlatformDelegateNativeMac::SendMouseWheelEvent(
    const CefMouseEvent& event,
    int deltaX,
    int deltaY) {
  auto view = GetHostView();
  if (!view)
    return;

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
      NSView* view = web_contents_->GetContentNativeView().GetNativeNSView();
      DCHECK([view canBecomeKeyView]);
      [[view window] makeFirstResponder:view];
    }
  }
}

gfx::Point CefBrowserPlatformDelegateNativeMac::GetScreenPoint(
    const gfx::Point& view) const {
  if (windowless_handler_)
    return windowless_handler_->GetParentScreenPoint(view);

  NSView* nsview = CAST_CEF_WINDOW_HANDLE_TO_NSVIEW(window_info_.parent_view);
  if (nsview) {
    NSRect bounds = [nsview bounds];
    NSPoint view_pt = {static_cast<CGFloat>(view.x()),
                       bounds.size.height - static_cast<CGFloat>(view.y())};
    NSPoint window_pt = [nsview convertPoint:view_pt toView:nil];
    NSPoint screen_pt =
        ui::ConvertPointFromWindowToScreen([nsview window], window_pt);
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
  if ([event.os_event type] == NSKeyDown)
    return [[NSApp mainMenu] performKeyEquivalent:event.os_event];
  return false;
}

// static
void CefBrowserPlatformDelegate::HandleExternalProtocol(const GURL& url) {}

CefEventHandle CefBrowserPlatformDelegateNativeMac::GetEventHandle(
    const content::NativeWebKeyboardEvent& event) const {
  return event.os_event;
}

std::unique_ptr<CefFileDialogRunner>
CefBrowserPlatformDelegateNativeMac::CreateFileDialogRunner() {
  return base::WrapUnique(new CefFileDialogRunnerMac);
}

std::unique_ptr<CefJavaScriptDialogRunner>
CefBrowserPlatformDelegateNativeMac::CreateJavaScriptDialogRunner() {
  return base::WrapUnique(new CefJavaScriptDialogRunnerMac);
}

std::unique_ptr<CefMenuRunner>
CefBrowserPlatformDelegateNativeMac::CreateMenuRunner() {
  return base::WrapUnique(new CefMenuRunnerMac);
}

gfx::Point CefBrowserPlatformDelegateNativeMac::GetDialogPosition(
    const gfx::Size& size) {
  // Dialogs are always re-positioned by the constrained window sheet controller
  // so nothing interesting to return yet.
  return gfx::Point();
}

gfx::Size CefBrowserPlatformDelegateNativeMac::GetMaximumDialogSize() {
  if (!web_contents_)
    return gfx::Size();

  // The dialog should try to fit within the overlay for the web contents.
  // Note that, for things like print preview, this is just a suggested maximum.
  return web_contents_->GetContainerBounds().size();
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
    // if this was a NSFlagsChanged event.
    // A dead key will have an empty character, but a non-empty unmodified
    // character
    event_type = NSFlagsChanged;
  } else {
    switch (key_event.type) {
      case KEYEVENT_RAWKEYDOWN:
      case KEYEVENT_KEYDOWN:
      case KEYEVENT_CHAR:
        event_type = NSKeyDown;
        break;
      case KEYEVENT_KEYUP:
        event_type = NSKeyUp;
        break;
    }
  }

  NSString* charactersIgnoringModifiers =
      [[[NSString alloc] initWithCharacters:&key_event.unmodified_character
                                     length:1] autorelease];
  NSString* characters =
      [[[NSString alloc] initWithCharacters:&key_event.character
                                     length:1] autorelease];

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

  result = content::NativeWebKeyboardEvent(synthetic_event);
  if (key_event.type == KEYEVENT_CHAR)
    result.SetType(blink::WebInputEvent::Type::kChar);

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
      NOTREACHED();
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
    if (mouse_event.modifiers & EVENTFLAG_LEFT_MOUSE_BUTTON)
      result.button = blink::WebMouseEvent::Button::kLeft;
    else if (mouse_event.modifiers & EVENTFLAG_MIDDLE_MOUSE_BUTTON)
      result.button = blink::WebMouseEvent::Button::kMiddle;
    else if (mouse_event.modifiers & EVENTFLAG_RIGHT_MOUSE_BUTTON)
      result.button = blink::WebMouseEvent::Button::kRight;
    else
      result.button = blink::WebMouseEvent::Button::kNoButton;
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

  if (mouse_event.modifiers & EVENTFLAG_LEFT_MOUSE_BUTTON)
    result.button = blink::WebMouseEvent::Button::kLeft;
  else if (mouse_event.modifiers & EVENTFLAG_MIDDLE_MOUSE_BUTTON)
    result.button = blink::WebMouseEvent::Button::kMiddle;
  else if (mouse_event.modifiers & EVENTFLAG_RIGHT_MOUSE_BUTTON)
    result.button = blink::WebMouseEvent::Button::kRight;
  else
    result.button = blink::WebMouseEvent::Button::kNoButton;

  return result;
}

void CefBrowserPlatformDelegateNativeMac::TranslateWebMouseEvent(
    blink::WebMouseEvent& result,
    const CefMouseEvent& mouse_event) const {
  // position
  result.SetPositionInWidget(mouse_event.x, mouse_event.y);

  const gfx::Point& screen_pt =
      GetScreenPoint(gfx::Point(mouse_event.x, mouse_event.y));
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
  if (!web_contents_)
    return nullptr;
  return static_cast<content::RenderWidgetHostViewMac*>(
      web_contents_->GetRenderWidgetHostView());
}
