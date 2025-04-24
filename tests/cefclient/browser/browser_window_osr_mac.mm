// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/browser_window_osr_mac.h"

#import <AppKit/NSAccessibility.h>
#include <Cocoa/Cocoa.h>
#include <OpenGL/gl.h>
#import <objc/runtime.h>

#include <optional>

#include "include/base/cef_logging.h"
#include "include/cef_parser.h"
#include "include/views/cef_display.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/cefclient/browser/bytes_write_handler.h"
#include "tests/cefclient/browser/main_context.h"
#include "tests/cefclient/browser/osr_accessibility_helper.h"
#include "tests/cefclient/browser/osr_accessibility_node.h"
#include "tests/cefclient/browser/text_input_client_osr_mac.h"
#include "tests/cefclient/browser/util_mac.h"
#include "tests/shared/browser/geometry_util.h"
#include "tests/shared/browser/main_message_loop.h"

// Begin disable NSOpenGL deprecation warnings.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

@interface BrowserOpenGLView
    : NSOpenGLView <NSDraggingSource, NSDraggingDestination, NSAccessibility> {
 @private
  NSTrackingArea* tracking_area_;
  client::BrowserWindowOsrMac* browser_window_;
  client::OsrRenderer* renderer_;
  NSPoint last_mouse_pos_;
  NSPoint cur_mouse_pos_;
  bool rotating_;

  bool was_last_mouse_down_on_view_;

  float device_scale_factor_;

  // Drag and drop.
  CefRefPtr<CefDragData> current_drag_data_;
  NSDragOperation current_drag_op_;
  NSDragOperation current_allowed_ops_;
  NSPasteboard* pasteboard_;
  NSString* fileUTI_;

  // For intreacting with IME.
  NSTextInputContext* text_input_context_osr_mac_;
  CefTextInputClientOSRMac* text_input_client_;

  // Manages Accessibility Tree
  client::OsrAccessibilityHelper* accessibility_helper_;

  // Event monitor for scroll wheel end event.
  id endWheelMonitor_;
}

@end  // @interface BrowserOpenGLView

namespace {

NSString* const kCEFDragDummyPboardType = @"org.CEF.drag-dummy-type";
NSString* const kNSURLTitlePboardType = @"public.url-name";

class ScopedGLContext {
 public:
  ScopedGLContext(BrowserOpenGLView* view, bool swap_buffers)
      : swap_buffers_(swap_buffers) {
    context_ = [view openGLContext];
    [context_ makeCurrentContext];
  }
  ~ScopedGLContext() {
    [NSOpenGLContext clearCurrentContext];
    if (swap_buffers_) {
      [context_ flushBuffer];
    }
  }

  NSOpenGLContext* context() const { return context_; }

 private:
  NSOpenGLContext* context_;
  const bool swap_buffers_;
};

NSPoint ConvertPointFromWindowToScreen(NSWindow* window, NSPoint point) {
  NSRect point_rect = NSMakeRect(point.x, point.y, 0, 0);
  return [window convertRectToScreen:point_rect].origin;
}

}  // namespace

@implementation BrowserOpenGLView

- (id)initWithFrame:(NSRect)frame
    andBrowserWindow:(client::BrowserWindowOsrMac*)browser_window
         andRenderer:(client::OsrRenderer*)renderer {
  NSOpenGLPixelFormat* pixelFormat = [[NSOpenGLPixelFormat alloc]
      initWithAttributes:(NSOpenGLPixelFormatAttribute[]){
                             NSOpenGLPFADoubleBuffer, NSOpenGLPFADepthSize, 32,
                             0}];
#if !__has_feature(objc_arc)
  [pixelFormat autorelease];
#endif  // !__has_feature(objc_arc)

  if (self = [super initWithFrame:frame pixelFormat:pixelFormat]) {
    browser_window_ = browser_window;
    renderer_ = renderer;
    rotating_ = false;
    endWheelMonitor_ = nil;
    device_scale_factor_ = 1.0f;

    tracking_area_ = [[NSTrackingArea alloc]
        initWithRect:frame
             options:NSTrackingMouseMoved | NSTrackingActiveInActiveApp |
                     NSTrackingInVisibleRect
               owner:self
            userInfo:nil];
    [self addTrackingArea:tracking_area_];

    // enable HiDPI buffer
    [self setWantsBestResolutionOpenGLSurface:YES];

    [self resetDragDrop];

    NSArray* types = [NSArray arrayWithObjects:kCEFDragDummyPboardType,
                                               NSPasteboardTypeFileURL,
                                               NSPasteboardTypeString, nil];
    [self registerForDraggedTypes:types];
  }

  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:NSWindowDidChangeBackingPropertiesNotification
              object:nil];
#if !__has_feature(objc_arc)
  if (text_input_context_osr_mac_) {
    [text_input_client_ release];
    [text_input_context_osr_mac_ release];
  }
  [super dealloc];
#endif  // !__has_feature(objc_arc)
}

- (void)detach {
  renderer_ = nullptr;
  browser_window_ = nullptr;
  if (text_input_client_) {
    [text_input_client_ detach];
  }
}

- (CefRefPtr<CefBrowser>)getBrowser {
  if (browser_window_) {
    return browser_window_->GetBrowser();
  }
  return nullptr;
}

- (void)setFrame:(NSRect)frameRect {
  CefRefPtr<CefBrowser> browser = [self getBrowser];
  if (!browser.get()) {
    return;
  }

  [super setFrame:frameRect];
  browser->GetHost()->WasResized();
}

- (void)sendMouseClick:(NSEvent*)event
                button:(CefBrowserHost::MouseButtonType)type
                  isUp:(bool)isUp {
  CefRefPtr<CefBrowser> browser = [self getBrowser];
  if (!browser.get()) {
    return;
  }

  CefMouseEvent mouseEvent;
  [self getMouseEvent:mouseEvent forEvent:event];

  // |point| is in OS X view coordinates.
  NSPoint point = [self getClickPointForEvent:event];

  // Convert to device coordinates.
  point = [self convertPointToBackingInternal:point];

  if (!isUp) {
    was_last_mouse_down_on_view_ = ![self isOverPopupWidgetX:point.x
                                                        andY:point.y];
  } else if (was_last_mouse_down_on_view_ &&
             [self isOverPopupWidgetX:point.x andY:point.y] &&
             ([self getPopupXOffset] || [self getPopupYOffset])) {
    return;
  }

  browser->GetHost()->SendMouseClickEvent(mouseEvent, type, isUp,
                                          static_cast<int>([event clickCount]));
}

- (void)mouseDown:(NSEvent*)event {
  [self sendMouseClick:event button:MBT_LEFT isUp:false];
}

- (void)rightMouseDown:(NSEvent*)event {
  if ([event modifierFlags] & NSEventModifierFlagShift) {
    // Start rotation effect.
    last_mouse_pos_ = cur_mouse_pos_ = [self getClickPointForEvent:event];
    rotating_ = true;
    return;
  }

  [self sendMouseClick:event button:MBT_RIGHT isUp:false];
}

- (void)otherMouseDown:(NSEvent*)event {
  [self sendMouseClick:event button:MBT_MIDDLE isUp:false];
}

- (void)mouseUp:(NSEvent*)event {
  [self sendMouseClick:event button:MBT_LEFT isUp:true];
}

- (void)rightMouseUp:(NSEvent*)event {
  if (rotating_) {
    // End rotation effect.
    renderer_->SetSpin(0, 0);
    rotating_ = false;
    [self setNeedsDisplay:YES];
    return;
  }
  [self sendMouseClick:event button:MBT_RIGHT isUp:true];
}

- (void)otherMouseUp:(NSEvent*)event {
  [self sendMouseClick:event button:MBT_MIDDLE isUp:true];
}

- (void)mouseMoved:(NSEvent*)event {
  CefRefPtr<CefBrowser> browser = [self getBrowser];
  if (!browser.get()) {
    return;
  }

  if (rotating_) {
    // Apply rotation effect.
    cur_mouse_pos_ = [self getClickPointForEvent:event];
    ;
    renderer_->IncrementSpin((cur_mouse_pos_.x - last_mouse_pos_.x),
                             (cur_mouse_pos_.y - last_mouse_pos_.y));
    last_mouse_pos_ = cur_mouse_pos_;
    [self setNeedsDisplay:YES];
    return;
  }

  CefMouseEvent mouseEvent;
  [self getMouseEvent:mouseEvent forEvent:event];

  browser->GetHost()->SendMouseMoveEvent(mouseEvent, false);
}

- (void)mouseDragged:(NSEvent*)event {
  [self mouseMoved:event];
}

- (void)rightMouseDragged:(NSEvent*)event {
  [self mouseMoved:event];
}

- (void)otherMouseDragged:(NSEvent*)event {
  [self mouseMoved:event];
}

- (void)mouseEntered:(NSEvent*)event {
  [self mouseMoved:event];
}

- (void)mouseExited:(NSEvent*)event {
  CefRefPtr<CefBrowser> browser = [self getBrowser];
  if (!browser.get()) {
    return;
  }

  CefMouseEvent mouseEvent;
  [self getMouseEvent:mouseEvent forEvent:event];

  browser->GetHost()->SendMouseMoveEvent(mouseEvent, true);
}

- (void)keyDown:(NSEvent*)event {
  CefRefPtr<CefBrowser> browser = [self getBrowser];
  if (!browser.get() || !text_input_context_osr_mac_) {
    return;
  }

  if ([event type] != NSEventTypeFlagsChanged) {
    if (text_input_client_) {
      [text_input_client_ HandleKeyEventBeforeTextInputClient:event];

      // The return value of this method seems to always be set to YES, thus we
      // ignore it and ask the host view whether IME is active or not.
      [text_input_context_osr_mac_ handleEvent:event];

      CefKeyEvent keyEvent;
      [self getKeyEvent:keyEvent forEvent:event];

      [text_input_client_ HandleKeyEventAfterTextInputClient:keyEvent];
    }
  }

  // Check for Caps lock and Toggle Touch Emulation
  if (client::MainContext::Get()->TouchEventsEnabled()) {
    [self toggleTouchEmulation:event];
  }
}

// OSX does not have touch screens, so we emulate it by mapping multitouch
// events on TrackPad to Touch Events on Screen. To ensure it does not
// interfere with other Trackpad events, this mapping is only enabled if
// touch-events=enabled commandline is passed and caps lock key is on.
- (void)toggleTouchEmulation:(NSEvent*)event {
  if ([event type] == NSEventTypeFlagsChanged && [event keyCode] == 0x39) {
    NSUInteger flags = [event modifierFlags];
    BOOL touch_enabled = flags & NSEventModifierFlagCapsLock ? YES : NO;
    if (touch_enabled) {
      self.allowedTouchTypes |= NSTouchTypeMaskDirect;
    } else {
      self.allowedTouchTypes &= ~NSTouchTypeMaskDirect;
    }
  }
}

- (cef_touch_event_type_t)getTouchPhase:(NSTouchPhase)phase {
  cef_touch_event_type_t event_type = CEF_TET_RELEASED;
  switch (phase) {
    case NSTouchPhaseBegan:
      event_type = CEF_TET_PRESSED;
      break;
    case NSTouchPhaseMoved:
      event_type = CEF_TET_MOVED;
      break;
    case NSTouchPhaseEnded:
      event_type = CEF_TET_RELEASED;
      break;
    case NSTouchPhaseCancelled:
      event_type = CEF_TET_CANCELLED;
      break;
    default:
      break;
  }
  return event_type;
}

// Translate NSTouch events to CefTouchEvents and send to browser.
- (void)sendTouchEvent:(NSEvent*)event touchPhase:(NSTouchPhase)phase {
  int modifiers = [self getModifiersForEvent:event];
  CefRefPtr<CefBrowser> browser = [self getBrowser];

  NSSet* touches = [event touchesMatchingPhase:phase inView:self];

  for (NSTouch* touch in touches) {
    // Convert NSTouch to CefTouchEvent.
    CefTouchEvent touch_event;

    // NSTouch.identity is unique during the life of the touch
    touch_event.id = static_cast<int>(touch.identity.hash);
    touch_event.type = [self getTouchPhase:phase];

    NSPoint scaled_pos = [touch normalizedPosition];
    NSSize view_size = [self bounds].size;

    // Map point on Touch Device to View coordinates.
    NSPoint touch_point = NSMakePoint(scaled_pos.x * view_size.width,
                                      scaled_pos.y * view_size.height);

    NSPoint contentLocal = [self convertPoint:touch_point fromView:nil];
    NSPoint point;
    point.x = contentLocal.x;
    point.y = [self frame].size.height - contentLocal.y;  // Flip y.

    // Convert to device coordinates.
    point = [self convertPointToBackingInternal:point];

    int device_x = point.x;
    int device_y = point.y;

    const float device_scale_factor = [self getDeviceScaleFactor];
    // Convert to browser view coordinates.
    touch_event.x = client::DeviceToLogical(device_x, device_scale_factor);
    touch_event.y = client::DeviceToLogical(device_y, device_scale_factor);

    touch_event.radius_x = 0;
    touch_event.radius_y = 0;

    touch_event.rotation_angle = 0;
    touch_event.pressure = 0;

    touch_event.modifiers = modifiers;

    // Notify the browser of touch event.
    browser->GetHost()->SendTouchEvent(touch_event);
  }
}

- (void)touchesBeganWithEvent:(NSEvent*)event {
  [self sendTouchEvent:event touchPhase:NSTouchPhaseBegan];
}

- (void)touchesMovedWithEvent:(NSEvent*)event {
  [self sendTouchEvent:event touchPhase:NSTouchPhaseMoved];
}

- (void)touchesEndedWithEvent:(NSEvent*)event {
  [self sendTouchEvent:event touchPhase:NSTouchPhaseEnded];
}

- (void)touchesCancelledWithEvent:(NSEvent*)event {
  [self sendTouchEvent:event touchPhase:NSTouchPhaseCancelled];
}

- (void)keyUp:(NSEvent*)event {
  CefRefPtr<CefBrowser> browser = [self getBrowser];
  if (!browser.get()) {
    return;
  }

  CefKeyEvent keyEvent;
  [self getKeyEvent:keyEvent forEvent:event];

  keyEvent.type = KEYEVENT_KEYUP;
  browser->GetHost()->SendKeyEvent(keyEvent);
}

- (void)flagsChanged:(NSEvent*)event {
  if ([self isKeyUpEvent:event]) {
    [self keyUp:event];
  } else {
    [self keyDown:event];
  }
}

- (void)shortCircuitScrollWheelEvent:(NSEvent*)event {
  if ([event phase] != NSEventPhaseEnded &&
      [event phase] != NSEventPhaseCancelled) {
    return;
  }

  [self sendScrollWheelEvet:event];

  if (endWheelMonitor_) {
    [NSEvent removeMonitor:endWheelMonitor_];
    endWheelMonitor_ = nil;
  }
}

- (void)scrollWheel:(NSEvent*)event {
  // Use an NSEvent monitor to listen for the wheel-end end. This ensures that
  // the event is received even when the mouse cursor is no longer over the
  // view when the scrolling ends. Also it avoids sending duplicate scroll
  // events to the renderer.
  if ([event phase] == NSEventPhaseBegan && !endWheelMonitor_) {
    endWheelMonitor_ = [NSEvent
        addLocalMonitorForEventsMatchingMask:NSEventMaskScrollWheel
                                     handler:^(NSEvent* blockEvent) {
                                       [self shortCircuitScrollWheelEvent:
                                                 blockEvent];
                                       return blockEvent;
                                     }];
  }

  [self sendScrollWheelEvet:event];
}

- (void)sendScrollWheelEvet:(NSEvent*)event {
  CefRefPtr<CefBrowser> browser = [self getBrowser];
  if (!browser.get()) {
    return;
  }

  CGEventRef cgEvent = [event CGEvent];
  DCHECK(cgEvent);

  int deltaX = static_cast<int>(
      CGEventGetIntegerValueField(cgEvent, kCGScrollWheelEventPointDeltaAxis2));
  int deltaY = static_cast<int>(
      CGEventGetIntegerValueField(cgEvent, kCGScrollWheelEventPointDeltaAxis1));

  CefMouseEvent mouseEvent;
  [self getMouseEvent:mouseEvent forEvent:event];

  browser->GetHost()->SendMouseWheelEvent(mouseEvent, deltaX, deltaY);
}

- (BOOL)canBecomeKeyView {
  CefRefPtr<CefBrowser> browser = [self getBrowser];
  return (browser.get() != nullptr);
}

- (BOOL)acceptsFirstResponder {
  CefRefPtr<CefBrowser> browser = [self getBrowser];
  return (browser.get() != nullptr);
}

- (BOOL)becomeFirstResponder {
  CefRefPtr<CefBrowser> browser = [self getBrowser];
  if (browser.get()) {
    browser->GetHost()->SetFocus(true);
    return [super becomeFirstResponder];
  }

  return NO;
}

- (BOOL)resignFirstResponder {
  CefRefPtr<CefBrowser> browser = [self getBrowser];
  if (browser.get()) {
    browser->GetHost()->SetFocus(false);
    return [super resignFirstResponder];
  }

  return NO;
}

- (void)undo:(id)sender {
  CefRefPtr<CefBrowser> browser = [self getBrowser];
  if (browser.get()) {
    browser->GetFocusedFrame()->Undo();
  }
}

- (void)redo:(id)sender {
  CefRefPtr<CefBrowser> browser = [self getBrowser];
  if (browser.get()) {
    browser->GetFocusedFrame()->Redo();
  }
}

- (void)cut:(id)sender {
  CefRefPtr<CefBrowser> browser = [self getBrowser];
  if (browser.get()) {
    browser->GetFocusedFrame()->Cut();
  }
}

- (void)copy:(id)sender {
  CefRefPtr<CefBrowser> browser = [self getBrowser];
  if (browser.get()) {
    browser->GetFocusedFrame()->Copy();
  }
}

- (void)paste:(id)sender {
  CefRefPtr<CefBrowser> browser = [self getBrowser];
  if (browser.get()) {
    browser->GetFocusedFrame()->Paste();
  }
}

- (void)delete:(id)sender {
  CefRefPtr<CefBrowser> browser = [self getBrowser];
  if (browser.get()) {
    browser->GetFocusedFrame()->Delete();
  }
}

- (void)selectAll:(id)sender {
  CefRefPtr<CefBrowser> browser = [self getBrowser];
  if (browser.get()) {
    browser->GetFocusedFrame()->SelectAll();
  }
}

- (NSPoint)getClickPointForEvent:(NSEvent*)event {
  NSPoint windowLocal = [event locationInWindow];
  NSPoint contentLocal = [self convertPoint:windowLocal fromView:nil];

  NSPoint point;
  point.x = contentLocal.x;
  point.y = [self frame].size.height - contentLocal.y;  // Flip y.
  return point;
}

- (void)getKeyEvent:(CefKeyEvent&)keyEvent forEvent:(NSEvent*)event {
  if ([event type] == NSEventTypeKeyDown || [event type] == NSEventTypeKeyUp) {
    NSString* s = [event characters];
    if ([s length] > 0) {
      keyEvent.character = [s characterAtIndex:0];
    }

    s = [event charactersIgnoringModifiers];
    if ([s length] > 0) {
      keyEvent.unmodified_character = [s characterAtIndex:0];
    }
  }

  if ([event type] == NSEventTypeFlagsChanged) {
    keyEvent.character = 0;
    keyEvent.unmodified_character = 0;
  }

  keyEvent.native_key_code = [event keyCode];

  keyEvent.modifiers = [self getModifiersForEvent:event];
}

- (NSTextInputContext*)inputContext {
  if (!text_input_context_osr_mac_) {
    text_input_client_ =
        [[CefTextInputClientOSRMac alloc] initWithBrowser:[self getBrowser]];
    text_input_context_osr_mac_ =
        [[NSTextInputContext alloc] initWithClient:text_input_client_];
#if !__has_feature(objc_arc)
    [text_input_client_ retain];
    [text_input_context_osr_mac_ retain];
#endif  // !__has_feature(objc_arc)
  }

  return text_input_context_osr_mac_;
}

- (void)getMouseEvent:(CefMouseEvent&)mouseEvent forEvent:(NSEvent*)event {
  const float device_scale_factor = [self getDeviceScaleFactor];

  // |point| is in OS X view coordinates.
  NSPoint point = [self getClickPointForEvent:event];

  // Convert to device coordinates.
  point = [self convertPointToBackingInternal:point];

  int device_x = point.x;
  int device_y = point.y;
  if ([self isOverPopupWidgetX:device_x andY:device_y]) {
    [self applyPopupOffsetToX:device_x andY:device_y];
  }

  // Convert to browser view coordinates.
  mouseEvent.x = client::DeviceToLogical(device_x, device_scale_factor);
  mouseEvent.y = client::DeviceToLogical(device_y, device_scale_factor);

  mouseEvent.modifiers = [self getModifiersForEvent:event];
}

- (void)getMouseEvent:(CefMouseEvent&)mouseEvent
          forDragInfo:(id<NSDraggingInfo>)info {
  const float device_scale_factor = [self getDeviceScaleFactor];

  // |point| is in OS X view coordinates.
  NSPoint windowPoint = [info draggingLocation];
  NSPoint point = [self flipWindowPointToView:windowPoint];

  // Convert to device coordinates.
  point = [self convertPointToBackingInternal:point];

  // Convert to browser view coordinates.
  mouseEvent.x = client::DeviceToLogical(point.x, device_scale_factor);
  mouseEvent.y = client::DeviceToLogical(point.y, device_scale_factor);

  mouseEvent.modifiers = static_cast<uint32_t>([NSEvent modifierFlags]);
}

- (int)getModifiersForEvent:(NSEvent*)event {
  int modifiers = 0;

  if ([event modifierFlags] & NSEventModifierFlagControl) {
    modifiers |= EVENTFLAG_CONTROL_DOWN;
  }
  if ([event modifierFlags] & NSEventModifierFlagShift) {
    modifiers |= EVENTFLAG_SHIFT_DOWN;
  }
  if ([event modifierFlags] & NSEventModifierFlagOption) {
    modifiers |= EVENTFLAG_ALT_DOWN;
  }
  if ([event modifierFlags] & NSEventModifierFlagCommand) {
    modifiers |= EVENTFLAG_COMMAND_DOWN;
  }
  if ([event modifierFlags] & NSEventModifierFlagCapsLock) {
    modifiers |= EVENTFLAG_CAPS_LOCK_ON;
  }

  if ([event type] == NSEventTypeKeyUp || [event type] == NSEventTypeKeyDown ||
      [event type] == NSEventTypeFlagsChanged) {
    // Only perform this check for key events
    if ([self isKeyPadEvent:event]) {
      modifiers |= EVENTFLAG_IS_KEY_PAD;
    }
  }

  // OS X does not have a modifier for NumLock, so I'm not entirely sure how to
  // set EVENTFLAG_NUM_LOCK_ON;
  //
  // There is no EVENTFLAG for the function key either.

  // Mouse buttons
  switch ([event type]) {
    case NSEventTypeLeftMouseDragged:
    case NSEventTypeLeftMouseDown:
    case NSEventTypeLeftMouseUp:
      modifiers |= EVENTFLAG_LEFT_MOUSE_BUTTON;
      break;
    case NSEventTypeRightMouseDragged:
    case NSEventTypeRightMouseDown:
    case NSEventTypeRightMouseUp:
      modifiers |= EVENTFLAG_RIGHT_MOUSE_BUTTON;
      break;
    case NSEventTypeOtherMouseDragged:
    case NSEventTypeOtherMouseDown:
    case NSEventTypeOtherMouseUp:
      modifiers |= EVENTFLAG_MIDDLE_MOUSE_BUTTON;
      break;
    default:
      break;
  }

  return modifiers;
}

- (BOOL)isKeyUpEvent:(NSEvent*)event {
  if ([event type] != NSEventTypeFlagsChanged) {
    return [event type] == NSEventTypeKeyUp;
  }

  // FIXME: This logic fails if the user presses both Shift keys at once, for
  // example: we treat releasing one of them as keyDown.
  switch ([event keyCode]) {
    case 54:  // Right Command
    case 55:  // Left Command
      return ([event modifierFlags] & NSEventModifierFlagCommand) == 0;

    case 57:  // Capslock
      return ([event modifierFlags] & NSEventModifierFlagCapsLock) == 0;

    case 56:  // Left Shift
    case 60:  // Right Shift
      return ([event modifierFlags] & NSEventModifierFlagShift) == 0;

    case 58:  // Left Alt
    case 61:  // Right Alt
      return ([event modifierFlags] & NSEventModifierFlagOption) == 0;

    case 59:  // Left Ctrl
    case 62:  // Right Ctrl
      return ([event modifierFlags] & NSEventModifierFlagControl) == 0;

    case 63:  // Function
      return ([event modifierFlags] & NSEventModifierFlagFunction) == 0;
  }
  return false;
}

- (BOOL)isKeyPadEvent:(NSEvent*)event {
  if ([event modifierFlags] & NSEventModifierFlagNumericPad) {
    return true;
  }

  switch ([event keyCode]) {
    case 71:  // Clear
    case 81:  // =
    case 75:  // /
    case 67:  // *
    case 78:  // -
    case 69:  // +
    case 76:  // Enter
    case 65:  // .
    case 82:  // 0
    case 83:  // 1
    case 84:  // 2
    case 85:  // 3
    case 86:  // 4
    case 87:  // 5
    case 88:  // 6
    case 89:  // 7
    case 91:  // 8
    case 92:  // 9
      return true;
  }

  return false;
}

- (void)windowDidChangeBackingProperties:(NSNotification*)notification {
  // This delegate method is only called on 10.7 and later, so don't worry about
  // other backing changes calling it on 10.6 or earlier
  [self resetDeviceScaleFactor:std::nullopt];
}

- (void)drawRect:(NSRect)dirtyRect {
  CefRefPtr<CefBrowser> browser = [self getBrowser];
  if ([self inLiveResize] || !browser.get()) {
    // Fill with the background color.
    const cef_color_t background_color =
        client::MainContext::Get()->GetBackgroundColor();
    NSColor* color = [NSColor
        colorWithCalibratedRed:float(CefColorGetR(background_color)) / 255.0f
                         green:float(CefColorGetG(background_color)) / 255.0f
                          blue:float(CefColorGetB(background_color)) / 255.0f
                         alpha:1.f];
    [color setFill];
    NSRectFill(dirtyRect);
  }

  // The Invalidate below fixes flicker when resizing.
  if ([self inLiveResize] && browser.get()) {
    browser->GetHost()->Invalidate(PET_VIEW);
  }
}

// Drag and drop

- (BOOL)startDragging:(CefRefPtr<CefDragData>)drag_data
           allowedOps:(NSDragOperation)ops
                point:(NSPoint)position {
  DCHECK(!pasteboard_);
  DCHECK(!fileUTI_);
  DCHECK(!current_drag_data_.get());

  [self resetDragDrop];

  current_allowed_ops_ = ops;
  current_drag_data_ = drag_data;

  [self fillPasteboard];

  NSEvent* currentEvent = [[NSApplication sharedApplication] currentEvent];
  NSWindow* window = [self window];
  NSTimeInterval eventTime = [currentEvent timestamp];

  NSEvent* dragEvent = [NSEvent mouseEventWithType:NSEventTypeLeftMouseDragged
                                          location:position
                                     modifierFlags:NSEventMaskLeftMouseDragged
                                         timestamp:eventTime
                                      windowNumber:[window windowNumber]
                                           context:nil
                                       eventNumber:0
                                        clickCount:1
                                          pressure:1.0];

  // TODO(cef): Pass a non-nil value to dragImage (see issue #1715). For now
  // work around the "callee requires a non-null argument" error that occurs
  // when building with the 10.11 SDK.
  id nilArg = nil;
  [window dragImage:nilArg
                 at:position
             offset:NSZeroSize
              event:dragEvent
         pasteboard:pasteboard_
             source:self
          slideBack:YES];
  return YES;
}

- (void)setCurrentDragOp:(NSDragOperation)op {
  current_drag_op_ = op;
}

// NSDraggingSource Protocol

- (NSDragOperation)draggingSession:(NSDraggingSession*)session
    sourceOperationMaskForDraggingContext:(NSDraggingContext)context {
  switch (context) {
    case NSDraggingContextOutsideApplication:
      return current_allowed_ops_;

    case NSDraggingContextWithinApplication:
    default:
      return current_allowed_ops_;
  }
}

- (NSArray*)namesOfPromisedFilesDroppedAtDestination:(NSURL*)dropDest {
  if (![dropDest isFileURL]) {
    return nil;
  }

  if (!current_drag_data_) {
    return nil;
  }

  size_t expected_size = current_drag_data_->GetFileContents(nullptr);
  if (expected_size == 0) {
    return nil;
  }

  std::string path = [[dropDest path] UTF8String];
  path.append("/");
  path.append(current_drag_data_->GetFileName().ToString());

  CefRefPtr<CefStreamWriter> writer = CefStreamWriter::CreateForFile(path);
  if (!writer) {
    return nil;
  }

  if (current_drag_data_->GetFileContents(writer) != expected_size) {
    return nil;
  }

  return @[ [NSString stringWithUTF8String:path.c_str()] ];
}

- (void)draggedImage:(NSImage*)anImage
             endedAt:(NSPoint)screenPoint
           operation:(NSDragOperation)operation {
  CefRefPtr<CefBrowser> browser = [self getBrowser];
  if (!browser.get()) {
    return;
  }

  if (operation == (NSDragOperationMove | NSDragOperationCopy)) {
    operation &= ~NSDragOperationMove;
  }

  NSPoint windowPoint = [[self window] convertScreenToBase:screenPoint];
  NSPoint pt = [self flipWindowPointToView:windowPoint];
  CefRenderHandler::DragOperation op =
      static_cast<CefRenderHandler::DragOperation>(operation);
  browser->GetHost()->DragSourceEndedAt(pt.x, pt.y, op);
  browser->GetHost()->DragSourceSystemDragEnded();
  [self resetDragDrop];
}

// NSDraggingDestination Protocol

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)info {
  CefRefPtr<CefBrowser> browser = [self getBrowser];
  if (!browser.get()) {
    return NSDragOperationNone;
  }

  CefRefPtr<CefDragData> drag_data;
  if (!current_drag_data_) {
    drag_data = CefDragData::Create();
    [self populateDropData:drag_data fromPasteboard:[info draggingPasteboard]];
  } else {
    drag_data = current_drag_data_->Clone();
    drag_data->ResetFileContents();
  }

  CefMouseEvent mouseEvent;
  [self getMouseEvent:mouseEvent forDragInfo:info];

  NSDragOperation mask = [info draggingSourceOperationMask];
  CefBrowserHost::DragOperationsMask allowed_ops =
      static_cast<CefBrowserHost::DragOperationsMask>(mask);

  browser->GetHost()->DragTargetDragEnter(drag_data, mouseEvent, allowed_ops);
  browser->GetHost()->DragTargetDragOver(mouseEvent, allowed_ops);

  current_drag_op_ = NSDragOperationCopy;
  return current_drag_op_;
}

- (void)draggingExited:(id<NSDraggingInfo>)sender {
  CefRefPtr<CefBrowser> browser = [self getBrowser];
  if (browser.get()) {
    browser->GetHost()->DragTargetDragLeave();
  }
}

- (BOOL)prepareForDragOperation:(id<NSDraggingInfo>)info {
  return YES;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)info {
  CefRefPtr<CefBrowser> browser = [self getBrowser];
  if (!browser.get()) {
    return NO;
  }

  CefMouseEvent mouseEvent;
  [self getMouseEvent:mouseEvent forDragInfo:info];

  browser->GetHost()->DragTargetDrop(mouseEvent);

  return YES;
}

- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)info {
  CefRefPtr<CefBrowser> browser = [self getBrowser];
  if (!browser.get()) {
    return NSDragOperationNone;
  }

  CefMouseEvent mouseEvent;
  [self getMouseEvent:mouseEvent forDragInfo:info];

  NSDragOperation mask = [info draggingSourceOperationMask];
  CefBrowserHost::DragOperationsMask allowed_ops =
      static_cast<CefBrowserHost::DragOperationsMask>(mask);

  browser->GetHost()->DragTargetDragOver(mouseEvent, allowed_ops);

  return current_drag_op_;
}

// NSPasteboardOwner Protocol

- (void)pasteboard:(NSPasteboard*)pboard provideDataForType:(NSString*)type {
  if (!current_drag_data_) {
    return;
  }

  // URL.
  if ([type isEqualToString:NSPasteboardTypeURL]) {
    DCHECK(current_drag_data_->IsLink());
    NSString* strUrl =
        [NSString stringWithUTF8String:current_drag_data_->GetLinkURL()
                                           .ToString()
                                           .c_str()];
    NSURL* url = [NSURL URLWithString:strUrl];
    [url writeToPasteboard:pboard];
    // URL title.
  } else if ([type isEqualToString:kNSURLTitlePboardType]) {
    NSString* strTitle =
        [NSString stringWithUTF8String:current_drag_data_->GetLinkTitle()
                                           .ToString()
                                           .c_str()];
    [pboard setString:strTitle forType:kNSURLTitlePboardType];

    // File contents.
  } else if ([type isEqualToString:fileUTI_]) {
    size_t size = current_drag_data_->GetFileContents(nullptr);
    DCHECK_GT(size, 0U);
    CefRefPtr<client::BytesWriteHandler> handler =
        new client::BytesWriteHandler(size);
    CefRefPtr<CefStreamWriter> writer =
        CefStreamWriter::CreateForHandler(handler.get());
    current_drag_data_->GetFileContents(writer);
    DCHECK_EQ(handler->GetDataSize(), static_cast<int64_t>(size));

    [pboard setData:[NSData dataWithBytes:handler->GetData()
                                   length:handler->GetDataSize()]
            forType:fileUTI_];

    // Plain text.
  } else if ([type isEqualToString:NSPasteboardTypeString]) {
    NSString* strTitle =
        [NSString stringWithUTF8String:current_drag_data_->GetFragmentText()
                                           .ToString()
                                           .c_str()];
    [pboard setString:strTitle forType:NSPasteboardTypeString];

  } else if ([type isEqualToString:kCEFDragDummyPboardType]) {
    // The dummy type _was_ promised and someone decided to call the bluff.
    [pboard setData:[NSData data] forType:kCEFDragDummyPboardType];
  }
}

// NSAccessibility Protocol implementation.
- (BOOL)accessibilityIsIgnored {
  if (!accessibility_helper_) {
    return YES;
  } else {
    return NO;
  }
}

- (id)accessibilityAttributeValue:(NSString*)attribute {
  if (!accessibility_helper_) {
    return [super accessibilityAttributeValue:attribute];
  }
  if ([attribute isEqualToString:NSAccessibilityRoleAttribute]) {
    return NSAccessibilityGroupRole;
  } else if ([attribute isEqualToString:NSAccessibilityDescriptionAttribute]) {
    client::OsrAXNode* node = accessibility_helper_->GetRootNode();
    std::string desc = node ? node->AxDescription() : "";
    return [NSString stringWithUTF8String:desc.c_str()];
  } else if ([attribute isEqualToString:NSAccessibilityValueAttribute]) {
    client::OsrAXNode* node = accessibility_helper_->GetRootNode();
    std::string desc = node ? node->AxValue() : "";
    return [NSString stringWithUTF8String:desc.c_str()];
  } else if ([attribute
                 isEqualToString:NSAccessibilityRoleDescriptionAttribute]) {
    return NSAccessibilityRoleDescriptionForUIElement(self);
  } else if ([attribute isEqualToString:NSAccessibilityChildrenAttribute]) {
    client::OsrAXNode* node = accessibility_helper_->GetRootNode();
    // Add Root as first Kid
    NSMutableArray* kids = [NSMutableArray arrayWithCapacity:1];
    NSObject* child = CAST_CEF_NATIVE_ACCESSIBLE_TO_NSOBJECT(
        node->GetNativeAccessibleObject(nullptr));
    [kids addObject:child];
    return NSAccessibilityUnignoredChildren(kids);
  } else {
    return [super accessibilityAttributeValue:attribute];
  }
}

- (id)accessibilityFocusedUIElement {
  if (accessibility_helper_) {
    client::OsrAXNode* node = accessibility_helper_->GetFocusedNode();
    return node ? CAST_CEF_NATIVE_ACCESSIBLE_TO_NSOBJECT(
                      node->GetNativeAccessibleObject(nullptr))
                : nil;
  }
  return nil;
}

// Utility methods.
- (void)resetDragDrop {
  current_drag_op_ = NSDragOperationNone;
  current_allowed_ops_ = NSDragOperationNone;
  current_drag_data_ = nullptr;
  if (fileUTI_) {
#if !__has_feature(objc_arc)
    [fileUTI_ release];
#endif  // !__has_feature(objc_arc)
    fileUTI_ = nil;
  }
  if (pasteboard_) {
#if !__has_feature(objc_arc)
    [pasteboard_ release];
#endif  // !__has_feature(objc_arc)
    pasteboard_ = nil;
  }
}

- (void)fillPasteboard {
  DCHECK(!pasteboard_);
  pasteboard_ = [NSPasteboard pasteboardWithName:NSPasteboardNameDrag];
#if !__has_feature(objc_arc)
  [pasteboard_ retain];
#endif  // !__has_feature(objc_arc)

  [pasteboard_ declareTypes:@[ kCEFDragDummyPboardType ] owner:self];

  // URL (and title).
  if (current_drag_data_->IsLink()) {
    [pasteboard_ addTypes:@[ NSPasteboardTypeURL, kNSURLTitlePboardType ]
                    owner:self];
  }

  // MIME type.
  CefString mimeType;
  size_t contents_size = current_drag_data_->GetFileContents(nullptr);
  CefString download_metadata = current_drag_data_->GetLinkMetadata();

  // File.
  if (contents_size > 0) {
    std::string file_name = current_drag_data_->GetFileName().ToString();
    size_t sep = file_name.find_last_of(".");
    CefString extension = file_name.substr(sep + 1);

    mimeType = CefGetMimeType(extension);

    if (!mimeType.empty()) {
      CFStringRef mimeTypeCF;
      mimeTypeCF = CFStringCreateWithCString(kCFAllocatorDefault,
                                             mimeType.ToString().c_str(),
                                             kCFStringEncodingUTF8);
      fileUTI_ = (__bridge NSString*)UTTypeCreatePreferredIdentifierForTag(
          kUTTagClassMIMEType, mimeTypeCF, nullptr);
      CFRelease(mimeTypeCF);
      // File (HFS) promise.
      NSArray* fileUTIList = @[ fileUTI_ ];
      NSString* NSPasteboardTypeFileURLPromise =
#if __has_feature(objc_arc)
          (__bridge NSString*)kPasteboardTypeFileURLPromise;
#else
          (NSString*)kPasteboardTypeFileURLPromise;
#endif
      [pasteboard_ addTypes:@[ NSPasteboardTypeFileURLPromise ] owner:self];
      [pasteboard_ setPropertyList:fileUTIList
                           forType:NSPasteboardTypeFileURLPromise];

      [pasteboard_ addTypes:fileUTIList owner:self];
    }
  }

  // Plain text.
  if (!current_drag_data_->GetFragmentText().empty()) {
    [pasteboard_ addTypes:@[ NSPasteboardTypeString ] owner:self];
  }
}

- (void)populateDropData:(CefRefPtr<CefDragData>)data
          fromPasteboard:(NSPasteboard*)pboard {
  DCHECK(data);
  DCHECK(pboard);
  DCHECK(data && !data->IsReadOnly());
  NSArray* types = [pboard types];

  // Get plain text.
  if ([types containsObject:NSPasteboardTypeString]) {
    data->SetFragmentText(
        [[pboard stringForType:NSPasteboardTypeString] UTF8String]);
  }

  // Get files.
  if ([types containsObject:NSPasteboardTypeFileURL]) {
    NSArray* files = [pboard propertyListForType:NSPasteboardTypeFileURL];
    if ([files isKindOfClass:[NSArray class]] && [files count]) {
      for (NSUInteger i = 0; i < [files count]; i++) {
        NSString* filename = [files objectAtIndex:i];
        BOOL exists =
            [[NSFileManager defaultManager] fileExistsAtPath:filename];
        if (exists) {
          data->AddFile([filename UTF8String], CefString());
        }
      }
    }
  }
}

- (NSPoint)flipWindowPointToView:(const NSPoint&)windowPoint {
  NSPoint viewPoint = [self convertPoint:windowPoint fromView:nil];
  NSRect viewFrame = [self frame];
  viewPoint.y = viewFrame.size.height - viewPoint.y;
  return viewPoint;
}

- (void)resetDeviceScaleFactor:(std::optional<float>)requested_scale_factor {
  float device_scale_factor = requested_scale_factor.value_or(1.0f);
  if (!requested_scale_factor.has_value()) {
    NSWindow* window = [self window];
    if (window) {
      device_scale_factor = [window backingScaleFactor];
    }
  }
  [self setDeviceScaleFactor:device_scale_factor];
}

- (void)setDeviceScaleFactor:(float)device_scale_factor {
  if (device_scale_factor == device_scale_factor_) {
    return;
  }

  // Apply some sanity checks.
  if (device_scale_factor < 0.5f || device_scale_factor > 4.0f) {
    return;
  }

  device_scale_factor_ = device_scale_factor;

  CefRefPtr<CefBrowser> browser = [self getBrowser];
  if (browser) {
    browser->GetHost()->NotifyScreenInfoChanged();
    browser->GetHost()->WasResized();
  }
}

- (float)getDeviceScaleFactor {
  return device_scale_factor_;
}

- (void)viewDidChangeBackingProperties {
  [super viewDidChangeBackingProperties];
  const CGFloat device_scale_factor = [self getDeviceScaleFactor];

  if (device_scale_factor == device_scale_factor_) {
    return;
  }

  CefRefPtr<CefBrowser> browser = [self getBrowser];
  if (browser) {
    browser->GetHost()->NotifyScreenInfoChanged();
    browser->GetHost()->WasResized();
  }
}

- (bool)isOverPopupWidgetX:(int)x andY:(int)y {
  CefRect rc = renderer_->popup_rect();
  int popup_right = rc.x + rc.width;
  int popup_bottom = rc.y + rc.height;
  return (x >= rc.x) && (x < popup_right) && (y >= rc.y) && (y < popup_bottom);
}

- (int)getPopupXOffset {
  return renderer_->original_popup_rect().x - renderer_->popup_rect().x;
}

- (int)getPopupYOffset {
  return renderer_->original_popup_rect().y - renderer_->popup_rect().y;
}

- (void)applyPopupOffsetToX:(int&)x andY:(int&)y {
  if ([self isOverPopupWidgetX:x andY:y]) {
    x += [self getPopupXOffset];
    y += [self getPopupYOffset];
  }
}

// Convert from scaled coordinates to view coordinates.
- (NSPoint)convertPointFromBackingInternal:(NSPoint)aPoint {
  return [self convertPointFromBacking:aPoint];
}

// Convert from view coordinates to scaled coordinates.
- (NSPoint)convertPointToBackingInternal:(NSPoint)aPoint {
  return [self convertPointToBacking:aPoint];
}

// Convert from scaled coordinates to view coordinates.
- (NSRect)convertRectFromBackingInternal:(NSRect)aRect {
  return [self convertRectFromBacking:aRect];
}

// Convert from view coordinates to scaled coordinates.
- (NSRect)convertRectToBackingInternal:(NSRect)aRect {
  return [self convertRectToBacking:aRect];
}

- (void)ChangeCompositionRange:(CefRange)range
              character_bounds:(const CefRenderHandler::RectList&)bounds {
  if (text_input_client_) {
    [text_input_client_ ChangeCompositionRange:range character_bounds:bounds];
  }
}

- (void)UpdateAccessibilityTree:(CefRefPtr<CefValue>)value {
  if (!accessibility_helper_) {
    accessibility_helper_ =
        new client::OsrAccessibilityHelper(value, [self getBrowser]);
  } else {
    accessibility_helper_->UpdateAccessibilityTree(value);
  }

  if (accessibility_helper_) {
    NSAccessibilityPostNotification(self,
                                    NSAccessibilityValueChangedNotification);
  }
  return;
}

- (void)UpdateAccessibilityLocation:(CefRefPtr<CefValue>)value {
  if (accessibility_helper_) {
    accessibility_helper_->UpdateAccessibilityLocation(value);
  }

  if (accessibility_helper_) {
    NSAccessibilityPostNotification(self,
                                    NSAccessibilityValueChangedNotification);
  }
  return;
}
@end

namespace client {

class BrowserWindowOsrMacImpl {
 public:
  BrowserWindowOsrMacImpl(BrowserWindow::Delegate* delegate,
                          const std::string& startup_url,
                          const OsrRendererSettings& settings,
                          BrowserWindowOsrMac& browser_window);
  ~BrowserWindowOsrMacImpl();

  // BrowserWindow methods.
  void CreateBrowser(ClientWindowHandle parent_handle,
                     const CefRect& rect,
                     const CefBrowserSettings& settings,
                     CefRefPtr<CefDictionaryValue> extra_info,
                     CefRefPtr<CefRequestContext> request_context);
  void GetPopupConfig(CefWindowHandle temp_handle,
                      CefWindowInfo& windowInfo,
                      CefRefPtr<CefClient>& client,
                      CefBrowserSettings& settings);
  void ShowPopup(ClientWindowHandle parent_handle,
                 int x,
                 int y,
                 size_t width,
                 size_t height);
  void Show();
  void Hide();
  void SetBounds(int x, int y, size_t width, size_t height);
  void SetFocus(bool focus);
  void SetDeviceScaleFactor(float device_scale_factor);
  float GetDeviceScaleFactor() const;
  ClientWindowHandle GetWindowHandle() const;

  // ClientHandlerOsr::OsrDelegate methods.
  void OnAfterCreated(CefRefPtr<CefBrowser> browser);
  void OnBeforeClose(CefRefPtr<CefBrowser> browser);
  bool GetRootScreenRect(CefRefPtr<CefBrowser> browser, CefRect& rect);
  void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect);
  bool GetScreenPoint(CefRefPtr<CefBrowser> browser,
                      int viewX,
                      int viewY,
                      int& screenX,
                      int& screenY);
  bool GetScreenInfo(CefRefPtr<CefBrowser> browser, CefScreenInfo& screen_info);
  void OnPopupShow(CefRefPtr<CefBrowser> browser, bool show);
  void OnPopupSize(CefRefPtr<CefBrowser> browser, const CefRect& rect);
  void OnPaint(CefRefPtr<CefBrowser> browser,
               CefRenderHandler::PaintElementType type,
               const CefRenderHandler::RectList& dirtyRects,
               const void* buffer,
               int width,
               int height);
  void OnAcceleratedPaint(CefRefPtr<CefBrowser> browser,
                          CefRenderHandler::PaintElementType type,
                          const CefRenderHandler::RectList& dirtyRects,
                          const CefAcceleratedPaintInfo& info);
  void OnCursorChange(CefRefPtr<CefBrowser> browser,
                      CefCursorHandle cursor,
                      cef_cursor_type_t type,
                      const CefCursorInfo& custom_cursor_info);
  bool StartDragging(CefRefPtr<CefBrowser> browser,
                     CefRefPtr<CefDragData> drag_data,
                     CefRenderHandler::DragOperationsMask allowed_ops,
                     int x,
                     int y);
  void UpdateDragCursor(CefRefPtr<CefBrowser> browser,
                        CefRenderHandler::DragOperation operation);
  void OnImeCompositionRangeChanged(
      CefRefPtr<CefBrowser> browser,
      const CefRange& selection_range,
      const CefRenderHandler::RectList& character_bounds);

  void UpdateAccessibilityTree(CefRefPtr<CefValue> value);
  void UpdateAccessibilityLocation(CefRefPtr<CefValue> value);

 private:
  // Create the NSView.
  void Create(ClientWindowHandle parent_handle, const CefRect& rect);

  BrowserWindowOsrMac& browser_window_;
  // The below members will only be accessed on the main thread which should be
  // the same as the CEF UI thread.
  OsrRenderer renderer_;
  std::optional<float> initial_scale_factor_;
  BrowserOpenGLView* native_browser_view_;
  bool hidden_;
  bool painting_popup_;
};

BrowserWindowOsrMacImpl::BrowserWindowOsrMacImpl(
    BrowserWindow::Delegate* delegate,
    const std::string& startup_url,
    const OsrRendererSettings& settings,
    BrowserWindowOsrMac& browser_window)
    : browser_window_(browser_window),
      renderer_(settings),
      native_browser_view_(nil),
      hidden_(false),
      painting_popup_(false) {}

BrowserWindowOsrMacImpl::~BrowserWindowOsrMacImpl() {
  if (native_browser_view_) {
    // Disassociate the view with |this|.
    [native_browser_view_ detach];
  }
}

void BrowserWindowOsrMacImpl::CreateBrowser(
    ClientWindowHandle parent_handle,
    const CefRect& rect,
    const CefBrowserSettings& settings,
    CefRefPtr<CefDictionaryValue> extra_info,
    CefRefPtr<CefRequestContext> request_context) {
  REQUIRE_MAIN_THREAD();

  // Create the native NSView.
  Create(parent_handle, rect);

  CefWindowInfo window_info;
  window_info.SetAsWindowless(
      CAST_NSVIEW_TO_CEF_WINDOW_HANDLE(native_browser_view_));

  window_info.shared_texture_enabled =
      renderer_.settings().shared_texture_enabled;
  window_info.external_begin_frame_enabled =
      renderer_.settings().external_begin_frame_enabled;

  // Windowless rendering requires Alloy style.
  DCHECK_EQ(CEF_RUNTIME_STYLE_ALLOY, window_info.runtime_style);

  // Create the browser asynchronously.
  CefBrowserHost::CreateBrowser(window_info, browser_window_.client_handler_,
                                browser_window_.client_handler_->startup_url(),
                                settings, extra_info, request_context);
}

void BrowserWindowOsrMacImpl::GetPopupConfig(CefWindowHandle temp_handle,
                                             CefWindowInfo& windowInfo,
                                             CefRefPtr<CefClient>& client,
                                             CefBrowserSettings& settings) {
  CEF_REQUIRE_UI_THREAD();

  windowInfo.SetAsWindowless(temp_handle);

  // Windowless rendering requires Alloy style.
  DCHECK_EQ(CEF_RUNTIME_STYLE_ALLOY, windowInfo.runtime_style);

  windowInfo.shared_texture_enabled =
      renderer_.settings().shared_texture_enabled;
  windowInfo.external_begin_frame_enabled =
      renderer_.settings().external_begin_frame_enabled;

  client = browser_window_.client_handler_;
}

void BrowserWindowOsrMacImpl::ShowPopup(ClientWindowHandle parent_handle,
                                        int x,
                                        int y,
                                        size_t width,
                                        size_t height) {
  REQUIRE_MAIN_THREAD();
  DCHECK(browser_window_.browser_.get());

  // Create the native NSView.
  Create(parent_handle,
         CefRect(x, y, static_cast<int>(width), static_cast<int>(height)));

  // Send resize notification so the compositor is assigned the correct
  // viewport size and begins rendering.
  browser_window_.browser_->GetHost()->WasResized();

  Show();
}

void BrowserWindowOsrMacImpl::Show() {
  REQUIRE_MAIN_THREAD();

  if (hidden_) {
    // Set the browser as visible.
    browser_window_.browser_->GetHost()->WasHidden(false);
    hidden_ = false;
  }

  // Give focus to the browser.
  browser_window_.browser_->GetHost()->SetFocus(true);
}

void BrowserWindowOsrMacImpl::Hide() {
  REQUIRE_MAIN_THREAD();

  if (!browser_window_.browser_.get()) {
    return;
  }

  // Remove focus from the browser.
  browser_window_.browser_->GetHost()->SetFocus(false);

  if (!hidden_) {
    // Set the browser as hidden.
    browser_window_.browser_->GetHost()->WasHidden(true);
    hidden_ = true;
  }
}

void BrowserWindowOsrMacImpl::SetBounds(int x,
                                        int y,
                                        size_t width,
                                        size_t height) {
  REQUIRE_MAIN_THREAD();
  // Nothing to do here. Cocoa will take care of positioning in the container.
}

void BrowserWindowOsrMacImpl::SetFocus(bool focus) {
  REQUIRE_MAIN_THREAD();
  if (native_browser_view_) {
    [native_browser_view_.window makeFirstResponder:native_browser_view_];
  }
}

void BrowserWindowOsrMacImpl::SetDeviceScaleFactor(float device_scale_factor) {
  REQUIRE_MAIN_THREAD();
  if (native_browser_view_) {
    [native_browser_view_ setDeviceScaleFactor:device_scale_factor];
  } else {
    initial_scale_factor_ = device_scale_factor;
  }
}

float BrowserWindowOsrMacImpl::GetDeviceScaleFactor() const {
  REQUIRE_MAIN_THREAD();
  if (native_browser_view_) {
    return [native_browser_view_ getDeviceScaleFactor];
  }
  return initial_scale_factor_.value_or(1.0f);
}

ClientWindowHandle BrowserWindowOsrMacImpl::GetWindowHandle() const {
  REQUIRE_MAIN_THREAD();
  return CAST_NSVIEW_TO_CEF_WINDOW_HANDLE(native_browser_view_);
}

void BrowserWindowOsrMacImpl::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
}

void BrowserWindowOsrMacImpl::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();
  REQUIRE_MAIN_THREAD();

  // Detach |this| from the ClientHandlerOsr.
  auto handler =
      ClientHandlerOsr::GetForClient(browser_window_.client_handler_);
  CHECK(handler);
  handler->DetachOsrDelegate();
}

bool BrowserWindowOsrMacImpl::GetRootScreenRect(CefRefPtr<CefBrowser> browser,
                                                CefRect& rect) {
  CEF_REQUIRE_UI_THREAD();
  if (!renderer_.settings().real_screen_bounds) {
    return false;
  }

  if (!native_browser_view_) {
    return false;
  }

  if (auto screen_rect =
          GetWindowBoundsInScreen([native_browser_view_ window])) {
    rect = *screen_rect;
  }
  if (rect.width == 0) {
    rect.width = 1;
  }
  if (rect.height == 0) {
    rect.height = 1;
  }
  return true;
}

void BrowserWindowOsrMacImpl::GetViewRect(CefRefPtr<CefBrowser> browser,
                                          CefRect& rect) {
  CEF_REQUIRE_UI_THREAD();
  REQUIRE_MAIN_THREAD();

  // Keep (0,0) origin for proper layout on macOS.
  rect.x = rect.y = 0;

  if (!native_browser_view_) {
    // Never return an empty rectangle.
    rect.width = rect.height = 1;
    return;
  }

  const float device_scale_factor = [native_browser_view_ getDeviceScaleFactor];

  // |bounds| is in OS X view coordinates.
  NSRect bounds = native_browser_view_.bounds;

  // Convert to device coordinates.
  bounds = [native_browser_view_ convertRectToBackingInternal:bounds];

  // Convert to DIP coordinates.
  rect.width = DeviceToLogical(bounds.size.width, device_scale_factor);
  rect.height = DeviceToLogical(bounds.size.height, device_scale_factor);
  if (rect.width == 0) {
    rect.width = 1;
  }
  if (rect.height == 0) {
    rect.height = 1;
  }
}

bool BrowserWindowOsrMacImpl::GetScreenPoint(CefRefPtr<CefBrowser> browser,
                                             int viewX,
                                             int viewY,
                                             int& screenX,
                                             int& screenY) {
  CEF_REQUIRE_UI_THREAD();
  REQUIRE_MAIN_THREAD();

  if (!native_browser_view_) {
    return false;
  }

  const float device_scale_factor = [native_browser_view_ getDeviceScaleFactor];

  // (viewX, viewX) is in browser DIP coordinates.
  // Convert to device coordinates.
  NSPoint view_pt = NSMakePoint(LogicalToDevice(viewX, device_scale_factor),
                                LogicalToDevice(viewY, device_scale_factor));

  // Convert to OS X view coordinates.
  view_pt = [native_browser_view_ convertPointFromBackingInternal:view_pt];

  // Reverse the Y component.
  const NSRect bounds = native_browser_view_.bounds;
  view_pt.y = bounds.size.height - view_pt.y;

  // Convert to screen coordinates.
  NSPoint window_pt = [native_browser_view_ convertPoint:view_pt toView:nil];
  NSPoint screen_pt =
      ConvertPointFromWindowToScreen(native_browser_view_.window, window_pt);

  screenX = screen_pt.x;
  screenY = screen_pt.y;
  return true;
}

bool BrowserWindowOsrMacImpl::GetScreenInfo(CefRefPtr<CefBrowser> browser,
                                            CefScreenInfo& screen_info) {
  CEF_REQUIRE_UI_THREAD();
  REQUIRE_MAIN_THREAD();

  if (!native_browser_view_) {
    return false;
  }

  screen_info.device_scale_factor = [native_browser_view_ getDeviceScaleFactor];

  if (renderer_.settings().real_screen_bounds) {
    CefRect root_rect;
    GetRootScreenRect(browser, root_rect);

    auto display = CefDisplay::GetDisplayMatchingBounds(
        root_rect, /*input_pixel_coords=*/false);
    screen_info.rect = display->GetBounds();
    screen_info.available_rect = display->GetWorkArea();
  } else {
    CefRect view_rect;
    GetViewRect(browser, view_rect);

    // Keep HTML select popups inside the view rectangle.
    screen_info.rect = view_rect;
    screen_info.available_rect = view_rect;
  }
  return true;
}

void BrowserWindowOsrMacImpl::OnPopupShow(CefRefPtr<CefBrowser> browser,
                                          bool show) {
  CEF_REQUIRE_UI_THREAD();
  REQUIRE_MAIN_THREAD();

  if (!native_browser_view_) {
    return;
  }

  if (!show) {
    renderer_.ClearPopupRects();
    browser->GetHost()->Invalidate(PET_VIEW);
  }
  renderer_.OnPopupShow(browser, show);
}

void BrowserWindowOsrMacImpl::OnPopupSize(CefRefPtr<CefBrowser> browser,
                                          const CefRect& rect) {
  CEF_REQUIRE_UI_THREAD();
  REQUIRE_MAIN_THREAD();

  if (!native_browser_view_) {
    return;
  }

  const float device_scale_factor = [native_browser_view_ getDeviceScaleFactor];

  // |rect| is in browser view coordinates. Convert to device coordinates.
  CefRect device_rect = LogicalToDevice(rect, device_scale_factor);

  renderer_.OnPopupSize(browser, device_rect);
}

void BrowserWindowOsrMacImpl::OnPaint(
    CefRefPtr<CefBrowser> browser,
    CefRenderHandler::PaintElementType type,
    const CefRenderHandler::RectList& dirtyRects,
    const void* buffer,
    int width,
    int height) {
  CEF_REQUIRE_UI_THREAD();
  REQUIRE_MAIN_THREAD();

  if (!native_browser_view_) {
    return;
  }

  if (width <= 2 && height <= 2) {
    // Ignore really small buffer sizes while the widget is starting up.
    return;
  }

  if (painting_popup_) {
    renderer_.OnPaint(browser, type, dirtyRects, buffer, width, height);
    return;
  }

  ScopedGLContext scoped_gl_context(native_browser_view_, true);

  renderer_.OnPaint(browser, type, dirtyRects, buffer, width, height);
  if (type == PET_VIEW && !renderer_.popup_rect().IsEmpty()) {
    painting_popup_ = true;
    browser->GetHost()->Invalidate(PET_POPUP);
    painting_popup_ = false;
  }
  renderer_.Render();
}

void BrowserWindowOsrMacImpl::OnAcceleratedPaint(
    CefRefPtr<CefBrowser> browser,
    CefRenderHandler::PaintElementType type,
    const CefRenderHandler::RectList& dirtyRects,
    const CefAcceleratedPaintInfo& info) {
  CEF_REQUIRE_UI_THREAD();
  REQUIRE_MAIN_THREAD();

  if (!native_browser_view_) {
    return;
  }

  ScopedGLContext scoped_gl_context(native_browser_view_, true);

  IOSurfaceRef io_surface = (IOSurfaceRef)info.shared_texture_io_surface;

  GLuint rectTexture;
  glGenTextures(1, &rectTexture);
  glEnable(GL_TEXTURE_RECTANGLE_ARB);
  glBindTexture(GL_TEXTURE_RECTANGLE_ARB, rectTexture);

  CGLContextObj cgl_context = CGLGetCurrentContext();

  GLsizei width = (GLsizei)IOSurfaceGetWidth(io_surface);
  GLsizei height = (GLsizei)IOSurfaceGetHeight(io_surface);

  CGLTexImageIOSurface2D(cgl_context, GL_TEXTURE_RECTANGLE_ARB, GL_RGBA8, width,
                         height, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                         io_surface, 0);

  glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  glBindTexture(GL_TEXTURE_RECTANGLE_ARB, 0);

  renderer_.OnAcceleratedPaint(browser, type, dirtyRects, rectTexture, width,
                               height);
  renderer_.Render();
}

void BrowserWindowOsrMacImpl::OnCursorChange(
    CefRefPtr<CefBrowser> browser,
    CefCursorHandle cursor,
    cef_cursor_type_t type,
    const CefCursorInfo& custom_cursor_info) {
  CEF_REQUIRE_UI_THREAD();
  REQUIRE_MAIN_THREAD();

  [CAST_CEF_CURSOR_HANDLE_TO_NSCURSOR(cursor) set];
}

bool BrowserWindowOsrMacImpl::StartDragging(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefDragData> drag_data,
    CefRenderHandler::DragOperationsMask allowed_ops,
    int x,
    int y) {
  CEF_REQUIRE_UI_THREAD();
  REQUIRE_MAIN_THREAD();

  if (!native_browser_view_) {
    return false;
  }

  static float device_scale_factor =
      [native_browser_view_ getDeviceScaleFactor];

  // |point| is in browser view coordinates.
  NSPoint point = NSMakePoint(x, y);

  // Convert to device coordinates.
  point.x = LogicalToDevice(point.x, device_scale_factor);
  point.y = LogicalToDevice(point.y, device_scale_factor);

  // Convert to OS X view coordinates.
  point = [native_browser_view_ convertPointFromBackingInternal:point];

  return [native_browser_view_
      startDragging:drag_data
         allowedOps:static_cast<NSDragOperation>(allowed_ops)
              point:point];
}

void BrowserWindowOsrMacImpl::UpdateDragCursor(
    CefRefPtr<CefBrowser> browser,
    CefRenderHandler::DragOperation operation) {
  CEF_REQUIRE_UI_THREAD();
  REQUIRE_MAIN_THREAD();

  if (native_browser_view_) {
    [native_browser_view_ setCurrentDragOp:operation];
  }
}

void BrowserWindowOsrMacImpl::OnImeCompositionRangeChanged(
    CefRefPtr<CefBrowser> browser,
    const CefRange& selection_range,
    const CefRenderHandler::RectList& bounds) {
  CEF_REQUIRE_UI_THREAD();

  if (native_browser_view_) {
    [native_browser_view_ ChangeCompositionRange:selection_range
                                character_bounds:bounds];
  }
}

void BrowserWindowOsrMacImpl::UpdateAccessibilityTree(
    CefRefPtr<CefValue> value) {
  CEF_REQUIRE_UI_THREAD();

  if (native_browser_view_) {
    [native_browser_view_ UpdateAccessibilityTree:value];
  }
}

void BrowserWindowOsrMacImpl::UpdateAccessibilityLocation(
    CefRefPtr<CefValue> value) {
  CEF_REQUIRE_UI_THREAD();

  if (native_browser_view_) {
    [native_browser_view_ UpdateAccessibilityLocation:value];
  }
}

void BrowserWindowOsrMacImpl::Create(ClientWindowHandle parent_handle,
                                     const CefRect& rect) {
  REQUIRE_MAIN_THREAD();
  DCHECK(!native_browser_view_);

  NSRect window_rect = NSMakeRect(rect.x, rect.y, rect.width, rect.height);
  native_browser_view_ =
      [[BrowserOpenGLView alloc] initWithFrame:window_rect
                              andBrowserWindow:&browser_window_
                                   andRenderer:&renderer_];
  native_browser_view_.autoresizingMask =
      (NSViewWidthSizable | NSViewHeightSizable);
  native_browser_view_.autoresizesSubviews = YES;
  [CAST_CEF_WINDOW_HANDLE_TO_NSVIEW(parent_handle)
      addSubview:native_browser_view_];

  // Determine the default scale factor.
  [native_browser_view_ resetDeviceScaleFactor:initial_scale_factor_];

  [[NSNotificationCenter defaultCenter]
      addObserver:native_browser_view_
         selector:@selector(windowDidChangeBackingProperties:)
             name:NSWindowDidChangeBackingPropertiesNotification
           object:native_browser_view_.window];
}

BrowserWindowOsrMac::BrowserWindowOsrMac(BrowserWindow::Delegate* delegate,
                                         bool with_controls,
                                         const std::string& startup_url,
                                         const OsrRendererSettings& settings)
    : BrowserWindow(delegate) {
  client_handler_ =
      new ClientHandlerOsr(this, this, with_controls, startup_url);
  impl_.reset(
      new BrowserWindowOsrMacImpl(delegate, startup_url, settings, *this));
}

BrowserWindowOsrMac::~BrowserWindowOsrMac() {}

void BrowserWindowOsrMac::CreateBrowser(
    ClientWindowHandle parent_handle,
    const CefRect& rect,
    const CefBrowserSettings& settings,
    CefRefPtr<CefDictionaryValue> extra_info,
    CefRefPtr<CefRequestContext> request_context) {
  impl_->CreateBrowser(parent_handle, rect, settings, extra_info,
                       request_context);
}

void BrowserWindowOsrMac::GetPopupConfig(CefWindowHandle temp_handle,
                                         CefWindowInfo& windowInfo,
                                         CefRefPtr<CefClient>& client,
                                         CefBrowserSettings& settings) {
  impl_->GetPopupConfig(temp_handle, windowInfo, client, settings);
}

void BrowserWindowOsrMac::ShowPopup(ClientWindowHandle parent_handle,
                                    int x,
                                    int y,
                                    size_t width,
                                    size_t height) {
  impl_->ShowPopup(parent_handle, x, y, width, height);
}

void BrowserWindowOsrMac::Show() {
  impl_->Show();
}

void BrowserWindowOsrMac::Hide() {
  impl_->Hide();
}

void BrowserWindowOsrMac::SetBounds(int x, int y, size_t width, size_t height) {
  impl_->SetBounds(x, y, width, height);
}

void BrowserWindowOsrMac::SetFocus(bool focus) {
  impl_->SetFocus(focus);
}

void BrowserWindowOsrMac::SetDeviceScaleFactor(float device_scale_factor) {
  impl_->SetDeviceScaleFactor(device_scale_factor);
}

float BrowserWindowOsrMac::GetDeviceScaleFactor() const {
  return impl_->GetDeviceScaleFactor();
}

ClientWindowHandle BrowserWindowOsrMac::GetWindowHandle() const {
  return impl_->GetWindowHandle();
}

void BrowserWindowOsrMac::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  impl_->OnAfterCreated(browser);
}

void BrowserWindowOsrMac::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  impl_->OnBeforeClose(browser);
}

bool BrowserWindowOsrMac::GetRootScreenRect(CefRefPtr<CefBrowser> browser,
                                            CefRect& rect) {
  return impl_->GetRootScreenRect(browser, rect);
}

void BrowserWindowOsrMac::GetViewRect(CefRefPtr<CefBrowser> browser,
                                      CefRect& rect) {
  impl_->GetViewRect(browser, rect);
}

bool BrowserWindowOsrMac::GetScreenPoint(CefRefPtr<CefBrowser> browser,
                                         int viewX,
                                         int viewY,
                                         int& screenX,
                                         int& screenY) {
  return impl_->GetScreenPoint(browser, viewX, viewY, screenX, screenY);
}

bool BrowserWindowOsrMac::GetScreenInfo(CefRefPtr<CefBrowser> browser,
                                        CefScreenInfo& screen_info) {
  return impl_->GetScreenInfo(browser, screen_info);
}

void BrowserWindowOsrMac::OnPopupShow(CefRefPtr<CefBrowser> browser,
                                      bool show) {
  impl_->OnPopupShow(browser, show);
}

void BrowserWindowOsrMac::OnPopupSize(CefRefPtr<CefBrowser> browser,
                                      const CefRect& rect) {
  impl_->OnPopupSize(browser, rect);
}

void BrowserWindowOsrMac::OnPaint(CefRefPtr<CefBrowser> browser,
                                  CefRenderHandler::PaintElementType type,
                                  const CefRenderHandler::RectList& dirtyRects,
                                  const void* buffer,
                                  int width,
                                  int height) {
  impl_->OnPaint(browser, type, dirtyRects, buffer, width, height);
}

void BrowserWindowOsrMac::OnAcceleratedPaint(
    CefRefPtr<CefBrowser> browser,
    CefRenderHandler::PaintElementType type,
    const CefRenderHandler::RectList& dirtyRects,
    const CefAcceleratedPaintInfo& info) {
  impl_->OnAcceleratedPaint(browser, type, dirtyRects, info);
}

void BrowserWindowOsrMac::OnCursorChange(
    CefRefPtr<CefBrowser> browser,
    CefCursorHandle cursor,
    cef_cursor_type_t type,
    const CefCursorInfo& custom_cursor_info) {
  impl_->OnCursorChange(browser, cursor, type, custom_cursor_info);
}

bool BrowserWindowOsrMac::StartDragging(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefDragData> drag_data,
    CefRenderHandler::DragOperationsMask allowed_ops,
    int x,
    int y) {
  return impl_->StartDragging(browser, drag_data, allowed_ops, x, y);
}

void BrowserWindowOsrMac::UpdateDragCursor(
    CefRefPtr<CefBrowser> browser,
    CefRenderHandler::DragOperation operation) {
  impl_->UpdateDragCursor(browser, operation);
}

void BrowserWindowOsrMac::OnImeCompositionRangeChanged(
    CefRefPtr<CefBrowser> browser,
    const CefRange& selection_range,
    const CefRenderHandler::RectList& character_bounds) {
  impl_->OnImeCompositionRangeChanged(browser, selection_range,
                                      character_bounds);
}

void BrowserWindowOsrMac::UpdateAccessibilityTree(CefRefPtr<CefValue> value) {
  impl_->UpdateAccessibilityTree(value);
}

void BrowserWindowOsrMac::UpdateAccessibilityLocation(
    CefRefPtr<CefValue> value) {
  impl_->UpdateAccessibilityLocation(value);
}

}  // namespace client

// End disable NSOpenGL deprecation warnings.
#pragma clang diagnostic pop
