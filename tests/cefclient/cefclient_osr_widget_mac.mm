// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>
#include <OpenGL/gl.h>

#include <vector>

#include "cefclient/cefclient_osr_widget_mac.h"

#include "include/cef_application_mac.h"
#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_url.h"
#include "include/wrapper/cef_helpers.h"
#include "cefclient/bytes_write_handler.h"
#include "cefclient/cefclient.h"
#include "cefclient/osrenderer.h"
#include "cefclient/resource_util.h"

namespace {

// This method will return YES for OS X versions 10.7.3 and later, and NO
// otherwise.
// Used to prevent a crash when building with the 10.7 SDK and accessing the
// notification below. See: http://crbug.com/260595.
BOOL SupportsBackingPropertiesChangedNotification() {
  // windowDidChangeBackingProperties: method has been added to the
  // NSWindowDelegate protocol in 10.7.3, at the same time as the
  // NSWindowDidChangeBackingPropertiesNotification notification was added.
  // If the protocol contains this method description, the notification should
  // be supported as well.
  Protocol* windowDelegateProtocol = NSProtocolFromString(@"NSWindowDelegate");
  struct objc_method_description methodDescription =
      protocol_getMethodDescription(
          windowDelegateProtocol,
          @selector(windowDidChangeBackingProperties:),
          NO,
          YES);

  // If the protocol does not contain the method, the returned method
  // description is {NULL, NULL}
  return methodDescription.name != NULL || methodDescription.types != NULL;
}

class ScopedGLContext {
 public:
  ScopedGLContext(ClientOpenGLView* view, bool swap_buffers)
    : swap_buffers_(swap_buffers) {
    context_ = [view openGLContext];
    [context_ makeCurrentContext];
  }
  ~ScopedGLContext() {
    [NSOpenGLContext clearCurrentContext];
    if (swap_buffers_)
      [context_ flushBuffer];
  }
 private:
  NSOpenGLContext* context_;
  const bool swap_buffers_;
};

}  // namespace

@interface ClientOpenGLView ()
- (void)resetDragDrop;
- (void)fillPasteboard;
- (void)populateDropData:(CefRefPtr<CefDragData>)data
          fromPasteboard:(NSPasteboard*)pboard;
- (NSPoint)flipWindowPointToView:(const NSPoint&)windowPoint;
- (float)getDeviceScaleFactor;
- (void)windowDidChangeBackingProperties:(NSNotification*)notification;

- (bool) isOverPopupWidgetX: (int) x andY: (int) y;
- (void) applyPopupOffsetToX: (int&) x andY: (int&) y;
- (int) getPopupXOffset;
- (int) getPopupYOffset;

- (void) sendMouseClick: (NSEvent *)event
                 button: (CefBrowserHost::MouseButtonType)type
                   isUp: (bool)isUp;

- (CefRect) convertRectFromBackingInternal: (const CefRect&) rect;

@property (readwrite, atomic) bool was_last_mouse_down_on_view;
@end

namespace {

static CefRect convertRect(const NSRect& target, const NSRect& frame) {
  NSRect rect = target;
  rect.origin.y = NSMaxY(frame) - NSMaxY(target);
  return CefRect(rect.origin.x,
                 rect.origin.y,
                 rect.size.width,
                 rect.size.height);
}

static NSString* const kCEFDragDummyPboardType = @"org.CEF.drag-dummy-type";
static NSString* const kNSURLTitlePboardType = @"public.url-name";

}  // namespace

ClientOSRHandler::ClientOSRHandler(ClientOpenGLView* view,
                                   OSRBrowserProvider* browser_provider)
    : view_(view),
      painting_popup_(false) {
  [view_ retain];
  view_->browser_provider_ = browser_provider;

  // Backing property notifications crash on 10.6 when building with the 10.7
  // SDK, see http://crbug.com/260595.
  static BOOL supportsBackingPropertiesNotification =
      SupportsBackingPropertiesChangedNotification();
  if (supportsBackingPropertiesNotification) {
    [[NSNotificationCenter defaultCenter]
      addObserver:view_
         selector:@selector(windowDidChangeBackingProperties:)
             name:NSWindowDidChangeBackingPropertiesNotification
           object:[view_ window]];
  }
}

ClientOSRHandler:: ~ClientOSRHandler() {
  static BOOL supportsBackingPropertiesNotification =
      SupportsBackingPropertiesChangedNotification();

  if (supportsBackingPropertiesNotification) {
    [[NSNotificationCenter defaultCenter]
        removeObserver:view_
                  name:NSWindowDidChangeBackingPropertiesNotification
                object:[view_ window]];
  }
}

void ClientOSRHandler::Disconnect() {
  [view_ release];
  view_ = nil;
}

// CefRenderHandler methods
void ClientOSRHandler::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  if (view_)
    view_->browser_provider_ = NULL;
}

bool ClientOSRHandler::GetViewRect(CefRefPtr<CefBrowser> browser,
                                   CefRect& rect) {
  CEF_REQUIRE_UI_THREAD();

  if (!view_)
    return false;

  // The simulated screen and view rectangle are the same. This is necessary
  // for popup menus to be located and sized inside the view.
  const NSRect bounds = [view_ bounds];
  rect.x = rect.y = 0;
  rect.width = bounds.size.width;
  rect.height = bounds.size.height;
  return true;
}

bool ClientOSRHandler::GetScreenPoint(CefRefPtr<CefBrowser> browser,
                                      int viewX,
                                      int viewY,
                                      int& screenX,
                                      int& screenY) {
  CEF_REQUIRE_UI_THREAD();

  if (!view_)
    return false;

  // Convert the point from view coordinates to actual screen coordinates.
  NSRect bounds = [view_ bounds];
  NSPoint view_pt = NSMakePoint(viewX, bounds.size.height - viewY);
  NSPoint window_pt = [view_ convertPoint:view_pt toView:nil];
  NSPoint screen_pt = [[view_ window] convertBaseToScreen:window_pt];
  screenX = screen_pt.x;
  screenY = screen_pt.y;
  return true;
}

bool ClientOSRHandler::GetScreenInfo(CefRefPtr<CefBrowser> browser,
                                     CefScreenInfo& screen_info) {
  CEF_REQUIRE_UI_THREAD();

  if (!view_)
    return false;

  NSWindow* window = [view_ window];
  if (!window)
    return false;

  screen_info.device_scale_factor = [view_ getDeviceScaleFactor];

  NSScreen* screen = [window screen];
  if (!screen)
    screen = [NSScreen deepestScreen];

  screen_info.depth = NSBitsPerPixelFromDepth([screen depth]);
  screen_info.depth_per_component = NSBitsPerSampleFromDepth([screen depth]);
  screen_info.is_monochrome =
      [[screen colorSpace] colorSpaceModel] == NSGrayColorSpaceModel;
  // screen_info.is_monochrome = true;
  screen_info.rect = convertRect([screen frame], [screen frame]);
  screen_info.available_rect =
      convertRect([screen visibleFrame], [screen frame]);

  return true;
}

void ClientOSRHandler::OnPopupShow(CefRefPtr<CefBrowser> browser,
                                   bool show) {
  CEF_REQUIRE_UI_THREAD();

  if (!view_)
    return;

  if (!show) {
    // Clear the popup rectangles, so that the paint triggered by Invalidate
    // will not repaint the popup content over the OpenGL view.
    view_->renderer_->ClearPopupRects();
    browser->GetHost()->Invalidate(PET_VIEW);
  }

  view_->renderer_->OnPopupShow(browser, show);
}

void ClientOSRHandler::OnPopupSize(CefRefPtr<CefBrowser> browser,
                                   const CefRect& rect) {
  CEF_REQUIRE_UI_THREAD();

  if (!view_)
    return;

  view_->renderer_->OnPopupSize(browser, rect);
}

void ClientOSRHandler::OnPaint(CefRefPtr<CefBrowser> browser,
                               PaintElementType type,
                               const RectList& dirtyRects,
                               const void* buffer,
                               int width, int height) {
  CEF_REQUIRE_UI_THREAD();

  if (!view_)
    return;

  if (painting_popup_) {
    view_->renderer_->OnPaint(browser, type, dirtyRects, buffer, width, height);
    return;
  }

  ScopedGLContext scoped_gl_context(view_, true);

  view_->renderer_->OnPaint(browser, type, dirtyRects, buffer, width, height);

  if (type == PET_VIEW && !view_->renderer_->popup_rect().IsEmpty()) {
    painting_popup_ = true;
    CefRect client_popup_rect(0, 0,
                              view_->renderer_->popup_rect().width,
                              view_->renderer_->popup_rect().height);

    browser->GetHost()->Invalidate(PET_POPUP);
    painting_popup_ = false;
  }

  view_->renderer_->Render();
}

void ClientOSRHandler::OnCursorChange(CefRefPtr<CefBrowser> browser,
                                      CefCursorHandle cursor,
                                      CursorType type,
                                      const CefCursorInfo& custom_cursor_info) {
  CEF_REQUIRE_UI_THREAD();
  [cursor set];
}

bool ClientOSRHandler::StartDragging(CefRefPtr<CefBrowser> browser,
                               CefRefPtr<CefDragData> drag_data,
                               CefRenderHandler::DragOperationsMask allowed_ops,
                               int x, int y) {
  CEF_REQUIRE_UI_THREAD();
  if (!view_)
    return false;
  return [view_ startDragging:drag_data
                  allowed_ops:static_cast<NSDragOperation>(allowed_ops)
                        point:NSMakePoint(x, y)];
}

void ClientOSRHandler::UpdateDragCursor(CefRefPtr<CefBrowser> browser,
                                 CefRenderHandler::DragOperation operation) {
  CEF_REQUIRE_UI_THREAD();
  if (!view_)
    return;
  view_->current_drag_op_ = operation;
}

void ClientOSRHandler::SetLoading(bool isLoading) {
}

@implementation ClientOpenGLView

@synthesize was_last_mouse_down_on_view = was_last_mouse_down_on_view_;

- (id)initWithFrame:(NSRect)frame andTransparency:(bool)transparency
                                andShowUpdateRect:(bool)show_update_rect {
  NSOpenGLPixelFormat * pixelFormat =
      [[NSOpenGLPixelFormat alloc]
       initWithAttributes:(NSOpenGLPixelFormatAttribute[]) {
           NSOpenGLPFAWindow,
           NSOpenGLPFADoubleBuffer,
           NSOpenGLPFADepthSize,
           32,
           0}];
  [pixelFormat autorelease];

  self = [super initWithFrame:frame pixelFormat:pixelFormat];
  if (self) {
    renderer_ = new ClientOSRenderer(transparency, show_update_rect);
    rotating_ = false;
    endWheelMonitor_ = nil;

    tracking_area_ =
        [[NSTrackingArea alloc] initWithRect:frame
                                     options:NSTrackingMouseMoved |
                                             NSTrackingActiveInActiveApp |
                                             NSTrackingInVisibleRect
                                       owner:self
                                    userInfo:nil];
    [self addTrackingArea:tracking_area_];
  }

  if ([self respondsToSelector:@selector(setWantsBestResolutionOpenGLSurface:)]) {
    // enable HiDPI buffer
    [self setWantsBestResolutionOpenGLSurface:YES];
  }

  [self resetDragDrop];

  NSArray* types = [NSArray arrayWithObjects:
      kCEFDragDummyPboardType,
      NSStringPboardType,
      NSFilenamesPboardType,
      NSPasteboardTypeString,
      nil];
  [self registerForDraggedTypes:types];

  return self;
}

- (void)dealloc {
  CefRefPtr<CefBrowser> browser = [self getBrowser];
  if (browser) {
    static_cast<ClientOSRHandler*>(
      browser->GetHost()->GetClient()->GetRenderHandler().get())->Disconnect();
    browser->GetHost()->CloseBrowser(true);
    browser = NULL;
  }
  if (renderer_)
    delete renderer_;

  [super dealloc];
}

- (CefRefPtr<CefBrowser>)getBrowser {
  if (browser_provider_)
    return browser_provider_->GetBrowser();
  return NULL;
}

- (void)setFrame:(NSRect)frameRect {
  CefRefPtr<CefBrowser> browser = [self getBrowser];
  if (!browser)
    return;

  [super setFrame:frameRect];
  browser->GetHost()->WasResized();
}

- (void) sendMouseClick:(NSEvent *)event
                 button: (CefBrowserHost::MouseButtonType)type
                   isUp: (bool)isUp {
  CefRefPtr<CefBrowser> browser = [self getBrowser];
  if (!browser)
    return;

  CefMouseEvent mouseEvent;
  [self getMouseEvent: mouseEvent forEvent: event];
  NSPoint point = [self getClickPointForEvent:event];
  if (!isUp)
    self.was_last_mouse_down_on_view = ![self isOverPopupWidgetX: point.x
                                                            andY: point.y];
  else if (self.was_last_mouse_down_on_view &&
           [self isOverPopupWidgetX:point.x andY: point.y] &&
           ([self getPopupXOffset] || [self getPopupYOffset])) {
    return;
  }

  browser->GetHost()->SendMouseClickEvent(mouseEvent,
                                           type,
                                           isUp,
                                           [event clickCount]);
}

- (void)mouseDown:(NSEvent *)event {
  [self sendMouseClick: event button:MBT_LEFT isUp:false];
}

- (void)rightMouseDown:(NSEvent *)event {
  if ([event modifierFlags] & NSShiftKeyMask) {
    // Start rotation effect.
    last_mouse_pos_ = cur_mouse_pos_ = [self getClickPointForEvent:event];
    rotating_ = true;
    return;
  }

  [self sendMouseClick: event button:MBT_RIGHT isUp:false];
}

- (void)otherMouseDown:(NSEvent *)event {
  [self sendMouseClick: event button:MBT_MIDDLE isUp:false];
}

- (void)mouseUp:(NSEvent *)event {
  [self sendMouseClick: event button: MBT_LEFT isUp: true];
}

- (void)rightMouseUp:(NSEvent *)event {
  if (rotating_) {
    // End rotation effect.
    renderer_->SetSpin(0, 0);
    rotating_ = false;
    [self setNeedsDisplay:YES];
    return;
  }
  [self sendMouseClick: event button: MBT_RIGHT isUp: true];
}

- (void)otherMouseUp:(NSEvent *)event {
  [self sendMouseClick: event button: MBT_MIDDLE isUp: true];
}

- (void)mouseMoved:(NSEvent *)event {
  CefRefPtr<CefBrowser> browser = [self getBrowser];
  if (!browser)
    return;

  if (rotating_) {
    // Apply rotation effect.
    cur_mouse_pos_ = [self getClickPointForEvent:event];;
    renderer_->IncrementSpin((cur_mouse_pos_.x - last_mouse_pos_.x),
                             (cur_mouse_pos_.y - last_mouse_pos_.y));
    last_mouse_pos_ = cur_mouse_pos_;
    [self setNeedsDisplay:YES];
    return;
  }

  CefMouseEvent mouseEvent;
  [self getMouseEvent: mouseEvent forEvent: event];
  browser->GetHost()->SendMouseMoveEvent(mouseEvent, false);
}

- (void)mouseDragged:(NSEvent *)event {
  [self mouseMoved:event];
}

- (void)rightMouseDragged:(NSEvent *)event {
  [self mouseMoved:event];
}

- (void)otherMouseDragged:(NSEvent *)event {
  [self mouseMoved:event];
}

- (void)mouseEntered:(NSEvent *)event {
  [self mouseMoved:event];
}

- (void)mouseExited:(NSEvent *)event {
  CefRefPtr<CefBrowser> browser = [self getBrowser];
  if (!browser)
    return;

  CefMouseEvent mouseEvent;
  [self getMouseEvent: mouseEvent forEvent: event];
  browser->GetHost()->SendMouseMoveEvent(mouseEvent, true);
}

- (void)keyDown:(NSEvent *)event {
  CefRefPtr<CefBrowser> browser = [self getBrowser];
  if (!browser)
    return;

  if ([event type] != NSFlagsChanged) {
    browser->GetHost()->HandleKeyEventBeforeTextInputClient(event);

    // The return value of this method seems to always be set to YES,
    // thus we ignore it and ask the host view whether IME is active
    // or not.
    [[self inputContext] handleEvent:event];

    browser->GetHost()->HandleKeyEventAfterTextInputClient(event);
  }
}

- (void)keyUp:(NSEvent *)event {
  CefRefPtr<CefBrowser> browser = [self getBrowser];
  if (!browser)
    return;

  CefKeyEvent keyEvent;
  [self getKeyEvent:keyEvent forEvent:event];

  keyEvent.type = KEYEVENT_KEYUP;
  browser->GetHost()->SendKeyEvent(keyEvent);
}

- (void)flagsChanged:(NSEvent *)event {
  if ([self isKeyUpEvent:event])
    [self keyUp:event];
  else
    [self keyDown:event];
}

- (void)shortCircuitScrollWheelEvent:(NSEvent*)event {
  // Phase is only supported in OS-X 10.7 and newer.
  if ([event phase] != NSEventPhaseEnded &&
      [event phase] != NSEventPhaseCancelled)
    return;

  [self sendScrollWheelEvet:event];

  if (endWheelMonitor_) {
    [NSEvent removeMonitor:endWheelMonitor_];
    endWheelMonitor_ = nil;
  }
}

- (void)scrollWheel:(NSEvent *)event {
  // Phase is only supported in OS-X 10.7 and newer.
  // Use an NSEvent monitor to listen for the wheel-end end. This ensures that
  // the event is received even when the mouse cursor is no longer over the
  // view when the scrolling ends. Also it avoids sending duplicate scroll
  // events to the renderer.
  if ([event respondsToSelector:@selector(phase)] &&
      [event phase] == NSEventPhaseBegan && !endWheelMonitor_) {
    endWheelMonitor_ =
        [NSEvent addLocalMonitorForEventsMatchingMask:NSScrollWheelMask
            handler:^(NSEvent* blockEvent) {
              [self shortCircuitScrollWheelEvent:blockEvent];
              return blockEvent;
            }];
  }

  [self sendScrollWheelEvet:event];
}

- (void)sendScrollWheelEvet:(NSEvent *)event {
  CefRefPtr<CefBrowser> browser = [self getBrowser];
  if (!browser)
    return;

  CGEventRef cgEvent = [event CGEvent];
  DCHECK(cgEvent);

  int deltaX =
      CGEventGetIntegerValueField(cgEvent, kCGScrollWheelEventPointDeltaAxis2);
  int deltaY =
      CGEventGetIntegerValueField(cgEvent, kCGScrollWheelEventPointDeltaAxis1);

  CefMouseEvent mouseEvent;
  [self getMouseEvent: mouseEvent forEvent: event];
  browser->GetHost()->SendMouseWheelEvent(mouseEvent, deltaX, deltaY);
}

- (BOOL)canBecomeKeyView {
  CefRefPtr<CefBrowser> browser = [self getBrowser];
  return (browser != NULL);
}

- (BOOL)acceptsFirstResponder {
  CefRefPtr<CefBrowser> browser = [self getBrowser];
  return (browser != NULL);
}

- (BOOL)becomeFirstResponder {
  CefRefPtr<CefBrowser> browser = [self getBrowser];
  if (browser) {
    browser->GetHost()->SendFocusEvent(true);
    return [super becomeFirstResponder];
  }

  return NO;
}

- (BOOL)resignFirstResponder {
  CefRefPtr<CefBrowser> browser = [self getBrowser];
  if (browser) {
    browser->GetHost()->SendFocusEvent(false);
    return [super resignFirstResponder];
  }

  return NO;
}

- (void)undo:(id)sender {
  CefRefPtr<CefBrowser> browser = [self getBrowser];
  if (browser)
    browser->GetFocusedFrame()->Undo();
}

- (void)redo:(id)sender {
  CefRefPtr<CefBrowser> browser = [self getBrowser];
  if (browser)
    browser->GetFocusedFrame()->Redo();
}

- (void)cut:(id)sender {
  CefRefPtr<CefBrowser> browser = [self getBrowser];
  if (browser)
    browser->GetFocusedFrame()->Cut();
}

- (void)copy:(id)sender {
  CefRefPtr<CefBrowser> browser = [self getBrowser];
  if (browser)
    browser->GetFocusedFrame()->Copy();
}

- (void)paste:(id)sender {
  CefRefPtr<CefBrowser> browser = [self getBrowser];
  if (browser)
    browser->GetFocusedFrame()->Paste();
}

- (void)delete:(id)sender {
  CefRefPtr<CefBrowser> browser = [self getBrowser];
  if (browser)
    browser->GetFocusedFrame()->Delete();
}

- (void)selectAll:(id)sender {
  CefRefPtr<CefBrowser> browser = [self getBrowser];
  if (browser)
    browser->GetFocusedFrame()->SelectAll();
}

- (NSPoint)getClickPointForEvent:(NSEvent*)event {
  NSPoint windowLocal = [event locationInWindow];
  NSPoint contentLocal = [self convertPoint:windowLocal fromView:nil];

  NSPoint point;
  point.x = contentLocal.x;
  point.y = [self frame].size.height - contentLocal.y;  // Flip y.
  return point;
}

- (void)getKeyEvent:(CefKeyEvent &)keyEvent forEvent:(NSEvent *)event {
  if ([event type] == NSKeyDown || [event type] == NSKeyUp) {
    NSString* s = [event characters];
    if ([s length] > 0)
      keyEvent.character = [s characterAtIndex:0];

    s = [event charactersIgnoringModifiers];
    if ([s length] > 0)
      keyEvent.unmodified_character = [s characterAtIndex:0];
  }

  if ([event type] == NSFlagsChanged) {
    keyEvent.character = 0;
    keyEvent.unmodified_character = 0;
  }

  keyEvent.native_key_code = [event keyCode];

  keyEvent.modifiers = [self getModifiersForEvent:event];
}

- (NSTextInputContext*)inputContext {
  CefRefPtr<CefBrowser> browser = [self getBrowser];
  if (browser)
    return browser->GetHost()->GetNSTextInputContext();
  return NULL;
}

- (void)getMouseEvent:(CefMouseEvent&)mouseEvent forEvent:(NSEvent*)event {
  NSPoint point = [self getClickPointForEvent:event];
  mouseEvent.x = point.x;
  mouseEvent.y = point.y;

  if ([self isOverPopupWidgetX:mouseEvent.x andY: mouseEvent.y]) {
    [self applyPopupOffsetToX:mouseEvent.x andY: mouseEvent.y];
  }

  mouseEvent.modifiers = [self getModifiersForEvent:event];
}

- (int)getModifiersForEvent:(NSEvent*)event {
  int modifiers = 0;

  if ([event modifierFlags] & NSControlKeyMask)
    modifiers |= EVENTFLAG_CONTROL_DOWN;
  if ([event modifierFlags] & NSShiftKeyMask)
    modifiers |= EVENTFLAG_SHIFT_DOWN;
  if ([event modifierFlags] & NSAlternateKeyMask)
    modifiers |= EVENTFLAG_ALT_DOWN;
  if ([event modifierFlags] & NSCommandKeyMask)
    modifiers |= EVENTFLAG_COMMAND_DOWN;
  if ([event modifierFlags] & NSAlphaShiftKeyMask)
    modifiers |= EVENTFLAG_CAPS_LOCK_ON;

  if ([event type] == NSKeyUp ||
      [event type] == NSKeyDown ||
      [event type] == NSFlagsChanged) {
    // Only perform this check for key events
    if ([self isKeyPadEvent:event])
      modifiers |= EVENTFLAG_IS_KEY_PAD;
  }

  // OS X does not have a modifier for NumLock, so I'm not entirely sure how to
  // set EVENTFLAG_NUM_LOCK_ON;
  //
  // There is no EVENTFLAG for the function key either.

  // Mouse buttons
  switch ([event type]) {
    case NSLeftMouseDragged:
    case NSLeftMouseDown:
    case NSLeftMouseUp:
      modifiers |= EVENTFLAG_LEFT_MOUSE_BUTTON;
    break;
    case NSRightMouseDragged:
    case NSRightMouseDown:
    case NSRightMouseUp:
      modifiers |= EVENTFLAG_RIGHT_MOUSE_BUTTON;
    break;
    case NSOtherMouseDragged:
    case NSOtherMouseDown:
    case NSOtherMouseUp:
      modifiers |= EVENTFLAG_MIDDLE_MOUSE_BUTTON;
    break;
  }

  return modifiers;
}

- (BOOL)isKeyUpEvent:(NSEvent*)event {
  if ([event type] != NSFlagsChanged)
    return [event type] == NSKeyUp;

  // FIXME: This logic fails if the user presses both Shift keys at once, for
  // example: we treat releasing one of them as keyDown.
  switch ([event keyCode]) {
    case 54: // Right Command
    case 55: // Left Command
      return ([event modifierFlags] & NSCommandKeyMask) == 0;

    case 57: // Capslock
      return ([event modifierFlags] & NSAlphaShiftKeyMask) == 0;

    case 56: // Left Shift
    case 60: // Right Shift
      return ([event modifierFlags] & NSShiftKeyMask) == 0;

    case 58: // Left Alt
    case 61: // Right Alt
     return ([event modifierFlags] & NSAlternateKeyMask) == 0;

    case 59: // Left Ctrl
    case 62: // Right Ctrl
      return ([event modifierFlags] & NSControlKeyMask) == 0;

    case 63: // Function
      return ([event modifierFlags] & NSFunctionKeyMask) == 0;
  }
  return false;
}

- (BOOL)isKeyPadEvent:(NSEvent*)event {
  if ([event modifierFlags] & NSNumericPadKeyMask)
    return true;

  switch ([event keyCode]) {
    case 71: // Clear
    case 81: // =
    case 75: // /
    case 67: // *
    case 78: // -
    case 69: // +
    case 76: // Enter
    case 65: // .
    case 82: // 0
    case 83: // 1
    case 84: // 2
    case 85: // 3
    case 86: // 4
    case 87: // 5
    case 88: // 6
    case 89: // 7
    case 91: // 8
    case 92: // 9
      return true;
  }

  return false;
}

- (void)windowDidChangeBackingProperties:(NSNotification*)notification {
  // This delegate method is only called on 10.7 and later, so don't worry about
  // other backing changes calling it on 10.6 or earlier
  CGFloat newBackingScaleFactor = [self getDeviceScaleFactor];
  NSNumber* oldBackingScaleFactor =
      [[notification userInfo] objectForKey:NSBackingPropertyOldScaleFactorKey];
  if (newBackingScaleFactor != [oldBackingScaleFactor doubleValue]) {
    CefRefPtr<CefBrowser> browser = [self getBrowser];
    if (!browser)
      return;

    browser->GetHost()->NotifyScreenInfoChanged();
  }
}

- (void)drawRect: (NSRect) dirtyRect {
  // The Invalidate below fixes flicker when resizing
  if ([self inLiveResize]) {
    CefRefPtr<CefBrowser> browser = [self getBrowser];
    if (!browser)
      return;

    browser->GetHost()->Invalidate(PET_VIEW);
  }
}

// Drag and drop

- (BOOL)startDragging:(CefRefPtr<CefDragData>)drag_data
          allowed_ops:(NSDragOperation)ops
                point:(NSPoint)position {
  DCHECK(!pasteboard_);
  DCHECK(!fileUTI_);
  DCHECK(!current_drag_data_.get());

  [self resetDragDrop];

  current_allowed_ops_ = ops;
  current_drag_data_ = drag_data;

  [self fillPasteboard];

  NSEvent* currentEvent = [NSApp currentEvent];
  NSWindow* window = [self window];
  NSTimeInterval eventTime = [currentEvent timestamp];

  NSEvent* dragEvent = [NSEvent mouseEventWithType:NSLeftMouseDragged
                                          location:position
                                     modifierFlags:NSLeftMouseDraggedMask
                                         timestamp:eventTime
                                      windowNumber:[window windowNumber]
                                           context:nil
                                       eventNumber:0
                                        clickCount:1
                                          pressure:1.0];

  [window dragImage:nil
                 at:position
             offset:NSZeroSize
              event:dragEvent
         pasteboard:pasteboard_
             source:self
          slideBack:YES];
  return YES;
}

// NSDraggingSource Protocol

- (NSDragOperation)draggingSession:(NSDraggingSession *)session
    sourceOperationMaskForDraggingContext:(NSDraggingContext)context {
  switch(context) {
    case NSDraggingContextOutsideApplication:
      return current_allowed_ops_;

    case NSDraggingContextWithinApplication:
    default:
      return current_allowed_ops_;
  }
}

- (NSArray*)namesOfPromisedFilesDroppedAtDestination:(NSURL*)dropDest {
  if (![dropDest isFileURL])
    return nil;

  if (!current_drag_data_)
    return nil;

  size_t expected_size = current_drag_data_->GetFileContents(NULL);
  if (expected_size == 0)
    return nil;

  std::string path = [[dropDest path] UTF8String];
  path.append("/");
  path.append(current_drag_data_->GetFileName().ToString());

  CefRefPtr<CefStreamWriter> writer = CefStreamWriter::CreateForFile(path);
  if (!writer)
    return nil;

  if (current_drag_data_->GetFileContents(writer) != expected_size)
    return nil;

  return @[ [NSString stringWithUTF8String:path.c_str()] ];
}

- (void)draggedImage:(NSImage*)anImage
             endedAt:(NSPoint)screenPoint
           operation:(NSDragOperation)operation {

  if (operation == (NSDragOperationMove | NSDragOperationCopy))
    operation &= ~NSDragOperationMove;

  NSPoint windowPoint = [[self window] convertScreenToBase: screenPoint];
  NSPoint pt = [self flipWindowPointToView:windowPoint];
  CefRenderHandler::DragOperation op =
      static_cast<CefRenderHandler::DragOperation>(operation);
  browser_provider_->GetBrowser()->GetHost()->DragSourceEndedAt(pt.x, pt.y, op);
  browser_provider_->GetBrowser()->GetHost()->DragSourceSystemDragEnded();
  [self resetDragDrop];
}

// NSDraggingDestination Protocol

- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)info {
  CefRefPtr<CefDragData> drag_data;
  if (!current_drag_data_) {
    drag_data = CefDragData::Create();
    [self populateDropData:drag_data
            fromPasteboard:[info draggingPasteboard]];
  } else {
    drag_data = current_drag_data_->Clone();
    drag_data->ResetFileContents();
  }

  NSPoint windowPoint = [info draggingLocation];
  NSPoint viewPoint = [self flipWindowPointToView:windowPoint];
  NSDragOperation mask = [info draggingSourceOperationMask];
  CefMouseEvent ev;
  ev.x = viewPoint.x;
  ev.y = viewPoint.y;
  ev.modifiers = [NSEvent modifierFlags];
  CefBrowserHost::DragOperationsMask allowed_ops =
      static_cast<CefBrowserHost::DragOperationsMask>(mask);
  browser_provider_->GetBrowser()->GetHost()->DragTargetDragEnter(
      drag_data, ev, allowed_ops);
  browser_provider_->GetBrowser()->GetHost()->DragTargetDragOver(ev,
      allowed_ops);

  current_drag_op_ = NSDragOperationCopy;
  return current_drag_op_;
}

- (void)draggingExited:(id <NSDraggingInfo>)sender {
  browser_provider_->GetBrowser()->GetHost()->DragTargetDragLeave();
}

- (BOOL)prepareForDragOperation:(id <NSDraggingInfo>)info {
  return YES;
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)info {
  NSPoint windowPoint = [info draggingLocation];
  NSPoint viewPoint = [self flipWindowPointToView:windowPoint];
  CefMouseEvent ev;
  ev.x = viewPoint.x;
  ev.y = viewPoint.y;
  ev.modifiers = [NSEvent modifierFlags];
  browser_provider_->GetBrowser()->GetHost()->DragTargetDrop(ev);
  return YES;
}

- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)info {
  NSPoint windowPoint = [info draggingLocation];
  NSPoint viewPoint = [self flipWindowPointToView:windowPoint];
  NSDragOperation mask = [info draggingSourceOperationMask];
  CefMouseEvent ev;
  ev.x = viewPoint.x;
  ev.y = viewPoint.y;
  ev.modifiers = [NSEvent modifierFlags];
  CefBrowserHost::DragOperationsMask allowed_ops =
  static_cast<CefBrowserHost::DragOperationsMask>(mask);
  browser_provider_->GetBrowser()->GetHost()->DragTargetDragOver(ev,
      allowed_ops);

  return current_drag_op_;
}

// NSPasteboardOwner Protocol

- (void)pasteboard:(NSPasteboard *)pboard provideDataForType:(NSString *)type {
  if (!current_drag_data_) {
    return;
  }

  // URL.
  if ([type isEqualToString:NSURLPboardType]) {
    DCHECK(current_drag_data_->IsLink());
    NSString* strUrl = [NSString stringWithUTF8String:
        current_drag_data_->GetLinkURL().ToString().c_str()];
    NSURL* url = [NSURL URLWithString:strUrl];
    [url writeToPasteboard:pboard];
  // URL title.
  } else if ([type isEqualToString:kNSURLTitlePboardType]) {
    NSString* strTitle = [NSString stringWithUTF8String:
        current_drag_data_->GetLinkTitle().ToString().c_str()];
    [pboard setString:strTitle forType:kNSURLTitlePboardType];

  // File contents.
  } else if ([type isEqualToString:(NSString*)fileUTI_]) {
    size_t size = current_drag_data_->GetFileContents(NULL);
    DCHECK_GT(size, 0U);
    CefRefPtr<BytesWriteHandler> handler = new BytesWriteHandler(size);
    CefRefPtr<CefStreamWriter> writer =
        CefStreamWriter::CreateForHandler(handler.get());
    current_drag_data_->GetFileContents(writer);
    DCHECK_EQ(handler->GetDataSize(), static_cast<int64>(size));

    [pboard setData:[NSData dataWithBytes:handler->GetData()
                                   length:handler->GetDataSize()]
                                  forType:(NSString*)fileUTI_];

  // Plain text.
  } else if ([type isEqualToString:NSStringPboardType]) {
    NSString* strTitle = [NSString stringWithUTF8String:
        current_drag_data_->GetFragmentText().ToString().c_str()];
    [pboard setString:strTitle forType:NSStringPboardType];

  } else if ([type isEqualToString:kCEFDragDummyPboardType]) {
    // The dummy type _was_ promised and someone decided to call the bluff.
    [pboard setData:[NSData data]
            forType:kCEFDragDummyPboardType];

  }

}

// Utility - private
- (void)resetDragDrop {
  current_drag_op_ = NSDragOperationNone;
  current_allowed_ops_ = NSDragOperationNone;
  current_drag_data_ = NULL;
  if (fileUTI_) {
    CFRelease(fileUTI_);
    fileUTI_ = NULL;
  }
  if (pasteboard_) {
    [pasteboard_ release];
    pasteboard_ = nil;
  }
}

- (void)fillPasteboard {
  DCHECK(!pasteboard_);
  pasteboard_ = [[NSPasteboard pasteboardWithName:NSDragPboard] retain];

  [pasteboard_ declareTypes:@[ kCEFDragDummyPboardType ]
                      owner:self];

  // URL (and title).
  if (current_drag_data_->IsLink()) {
    [pasteboard_ addTypes:@[ NSURLPboardType, kNSURLTitlePboardType ]
                    owner:self];
  }

  // MIME type.
  CefString mimeType;
  size_t contents_size = current_drag_data_->GetFileContents(NULL);
  CefString download_metadata = current_drag_data_->GetLinkMetadata();
  CefString file_name = current_drag_data_->GetFileName();

  // File.
  if (contents_size > 0) {
      std::string file_name = current_drag_data_->GetFileName().ToString();
      size_t sep = file_name.find_last_of(".");
      CefString extension = file_name.substr(sep + 1);

      mimeType = CefGetMimeType(extension);

    if (!mimeType.empty()) {
      CFStringRef mimeTypeCF;
      mimeTypeCF = CFStringCreateWithCString(kCFAllocatorDefault,
          mimeType.ToString().c_str(), kCFStringEncodingUTF8);
      fileUTI_ = UTTypeCreatePreferredIdentifierForTag(kUTTagClassMIMEType,
          mimeTypeCF, NULL);
      CFRelease(mimeTypeCF);
      // File (HFS) promise.
      NSArray* fileUTIList = @[ (NSString*)fileUTI_ ];
      [pasteboard_ addTypes:@[ NSFilesPromisePboardType ] owner:self];
      [pasteboard_ setPropertyList:fileUTIList
                           forType:NSFilesPromisePboardType];

      [pasteboard_ addTypes:fileUTIList owner:self];
    }
  }

  // Plain text.
  if (!current_drag_data_->GetFragmentText().empty()) {
    [pasteboard_ addTypes:@[ NSStringPboardType ]
                    owner:self];
  }
}

- (void)populateDropData:(CefRefPtr<CefDragData>)data
          fromPasteboard:(NSPasteboard*)pboard {
  DCHECK(data);
  DCHECK(pboard);
  DCHECK(data && !data->IsReadOnly());
  NSArray* types = [pboard types];

  // Get plain text.
  if ([types containsObject:NSStringPboardType]) {
    data->SetFragmentText(
        [[pboard stringForType:NSStringPboardType] UTF8String]);
  }

  // Get files.
  if ([types containsObject:NSFilenamesPboardType]) {
    NSArray* files = [pboard propertyListForType:NSFilenamesPboardType];
    if ([files isKindOfClass:[NSArray class]] && [files count]) {
      for (NSUInteger i = 0; i < [files count]; i++) {
        NSString* filename = [files objectAtIndex:i];
        BOOL exists = [[NSFileManager defaultManager]
            fileExistsAtPath:filename];
        if (exists) {
          data->AddFile([filename UTF8String], CefString());
        }
      }
    }
  }
}

- (NSPoint)flipWindowPointToView:(const NSPoint&)windowPoint {
  NSPoint viewPoint =  [self convertPoint:windowPoint fromView:nil];
  NSRect viewFrame = [self frame];
  viewPoint.y = viewFrame.size.height - viewPoint.y;
  return viewPoint;
}

- (float)getDeviceScaleFactor {
  float deviceScaleFactor = 1;
  NSWindow* window = [self window];
  if (!window)
    return deviceScaleFactor;

  if ([window respondsToSelector:@selector(backingScaleFactor)])
    deviceScaleFactor = [window backingScaleFactor];
  else
    deviceScaleFactor = [window userSpaceScaleFactor];

  return deviceScaleFactor;
}

- (bool) isOverPopupWidgetX: (int) x andY: (int) y {
  CefRect rc = [self convertRectFromBackingInternal:renderer_->popup_rect()];
  int popup_right = rc.x + rc.width;
  int popup_bottom = rc.y + rc.height;
  return (x >= rc.x) && (x < popup_right) &&
         (y >= rc.y) && (y < popup_bottom);
}

- (int) getPopupXOffset {
  int original_x =
      [self convertRectFromBackingInternal:renderer_->original_popup_rect()].x;
  int popup_x =
      [self convertRectFromBackingInternal:renderer_->popup_rect()].x;

  return original_x - popup_x;
}

- (int) getPopupYOffset {
  int original_y =
      [self convertRectFromBackingInternal:renderer_->original_popup_rect()].y;
  int popup_y =
      [self convertRectFromBackingInternal:renderer_->popup_rect()].y;

  return original_y - popup_y;
}

- (void) applyPopupOffsetToX: (int&) x andY: (int&) y {
  if ([self isOverPopupWidgetX:x andY:y]) {
    x += [self getPopupXOffset];
    y += [self getPopupYOffset];
  }
}

- (CefRect) convertRectFromBackingInternal: (const CefRect&) rect {
  if ([self respondsToSelector:@selector(convertRectFromBacking:)]) {
    NSRect old_rect = NSMakeRect(rect.x, rect.y, rect.width, rect.height);
    NSRect scaled_rect = [self convertRectFromBacking:old_rect];
    return CefRect((int)scaled_rect.origin.x,
                   (int)scaled_rect.origin.y,
                   (int)scaled_rect.size.width,
                   (int)scaled_rect.size.height);
  }

  return rect;
}

@end


CefRefPtr<OSRWindow> OSRWindow::Create(OSRBrowserProvider* browser_provider,
                                       bool transparent,
                                       bool show_update_rect,
                                       CefWindowHandle parentView,
                                       const CefRect& frame) {
  return new OSRWindow(browser_provider, transparent, show_update_rect,
                       parentView, frame);
}

OSRWindow::OSRWindow(OSRBrowserProvider* browser_provider,
                     bool transparent,
                     bool show_update_rect,
                     CefWindowHandle parentView,
                     const CefRect& frame) {
  NSRect window_rect = NSMakeRect(frame.x, frame.y, frame.width, frame.height);
  ClientOpenGLView* view =
      [[ClientOpenGLView alloc] initWithFrame:window_rect
                               andTransparency:transparent
                             andShowUpdateRect:show_update_rect];
  this->view_ = view;
  [parentView addSubview:view];
  [view setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
  [view setAutoresizesSubviews: true];

  this->render_client = new ClientOSRHandler(view, browser_provider);
}

OSRWindow::~OSRWindow() {
}
