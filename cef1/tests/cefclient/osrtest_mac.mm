 // Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#include <OpenGL/gl.h>

#include "cefclient/osrtest_mac.h"
#include "include/cef_browser.h"
#include "cefclient/osrenderer.h"
#include "cefclient/client_popup_handler.h"
#include "cefclient/resource_util.h"
#include "cefclient/util.h"

// The client OpenGL view.
@interface ClientOpenGLView : NSOpenGLView {
 @public
  NSTrackingArea* tracking_area_;
  CefRefPtr<CefBrowser> browser_;
  ClientOSRenderer* renderer_;
  NSPoint last_mouse_pos_;
  NSPoint cur_mouse_pos_;
  bool rotating_;
}

- (id)initWithFrame:(NSRect)frame andTransparency:(bool)transparency;
- (NSPoint)getClickPointForEvent:(NSEvent*)event;
- (void)getKeyInfo:(CefKeyInfo&)info forEvent:(NSEvent*)event;
- (int)getModifiersForEvent:(NSEvent*)event;
- (BOOL)isKeyUpEvent:(NSEvent*)event;
- (BOOL)isKeyPadEvent:(NSEvent*)event;
@end


namespace {

// Handler for off-screen rendering windows.
class ClientOSRHandler : public CefClient,
                         public CefLifeSpanHandler,
                         public CefLoadHandler,
                         public CefRequestHandler,
                         public CefDisplayHandler,
                         public CefRenderHandler {
 public:
  explicit ClientOSRHandler(ClientOpenGLView* view)
    : view_(view) {
  }
  ~ClientOSRHandler() {
  }

  void Disconnect() {
    view_ = nil;
  }

  // CefClient methods
  virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() OVERRIDE {
    return this;
  }
  virtual CefRefPtr<CefLoadHandler> GetLoadHandler() OVERRIDE {
    return this;
  }
  virtual CefRefPtr<CefRequestHandler> GetRequestHandler() OVERRIDE {
    return this;
  }
  virtual CefRefPtr<CefDisplayHandler> GetDisplayHandler() OVERRIDE {
    return this;
  }
  virtual CefRefPtr<CefRenderHandler> GetRenderHandler() OVERRIDE {
    return this;
  }

  // CefLifeSpanHandler methods

  virtual bool OnBeforePopup(CefRefPtr<CefBrowser> parentBrowser,
                             const CefPopupFeatures& popupFeatures,
                             CefWindowInfo& windowInfo,
                             const CefString& url,
                             CefRefPtr<CefClient>& client,
                             CefBrowserSettings& settings) OVERRIDE {
    REQUIRE_UI_THREAD();

    windowInfo.m_bWindowRenderingDisabled = TRUE;
    client = new ClientPopupHandler(view_->browser_);
    return false;
  }

  virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) OVERRIDE {
    REQUIRE_UI_THREAD();

    // Set the view size to match the window size.
    const NSRect bounds = [view_ bounds];
    browser->SetSize(PET_VIEW, bounds.size.width, bounds.size.height);

    view_->browser_ = browser;
  }

  virtual void OnBeforeClose(CefRefPtr<CefBrowser> browser)  OVERRIDE {
    if (view_)
      view_->browser_ = NULL;
  }

  // CefLoadHandler methods

  virtual void OnLoadStart(CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefFrame> frame) OVERRIDE {
    REQUIRE_UI_THREAD();

    if (!browser->IsPopup() && frame->IsMain()) {
      // We've just started loading a page
      SetLoading(true);
    }
  }

  virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         int httpStatusCode) OVERRIDE {
    REQUIRE_UI_THREAD();

    if (!browser->IsPopup() && frame->IsMain()) {
      // We've just finished loading a page
      SetLoading(false);
    }
  }

  // CefRequestHandler methods

  virtual bool OnBeforeResourceLoad(CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefRequest> request,
                                    CefString& redirectUrl,
                                    CefRefPtr<CefStreamReader>& resourceStream,
                                    CefRefPtr<CefResponse> response,
                                    int loadFlags) OVERRIDE {
    REQUIRE_IO_THREAD();

    std::string url = request->GetURL();
    if (url == "http://tests/osrtest") {
      // Show the osrtest HTML contents
      resourceStream = GetBinaryResourceReader("osrtest.html");
      response->SetMimeType("text/html");
      response->SetStatus(200);
    } else if (url == "http://tests/transparency") {
      // Show the osrtest HTML contents
      resourceStream = GetBinaryResourceReader("transparency.html");
      response->SetMimeType("text/html");
      response->SetStatus(200);
    } else if (strstr(url.c_str(), "/logoball.png") != NULL) {
      // Load the "logoball.png" image resource.
      resourceStream = GetBinaryResourceReader("logoball.png");
      response->SetMimeType("image/png");
      response->SetStatus(200);
    } 

    return false;
  }

  // CefDisplayHandler methods

  virtual void OnNavStateChange(CefRefPtr<CefBrowser> browser,
                                bool canGoBack,
                                bool canGoForward) OVERRIDE {
    REQUIRE_UI_THREAD();
  }

  virtual void OnAddressChange(CefRefPtr<CefBrowser> browser,
                               CefRefPtr<CefFrame> frame,
                               const CefString& url) OVERRIDE {
    REQUIRE_UI_THREAD();
  }

  virtual void OnTitleChange(CefRefPtr<CefBrowser> browser,
                             const CefString& title) OVERRIDE {
    REQUIRE_UI_THREAD();

    if (!view_)
      return;

    // Set the frame window title bar
    NSWindow* window = [view_ window];
    std::string titleStr(title);
    NSString* str = [NSString stringWithUTF8String:titleStr.c_str()];
    [window setTitle:str];
  }

  // CefRenderHandler methods

  virtual bool GetViewRect(CefRefPtr<CefBrowser> browser,
                           CefRect& rect) OVERRIDE {
    REQUIRE_UI_THREAD();

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

  virtual bool GetScreenRect(CefRefPtr<CefBrowser> browser,
                             CefRect& rect) OVERRIDE {
    return GetViewRect(browser, rect);
  }

  virtual bool GetScreenPoint(CefRefPtr<CefBrowser> browser,
                              int viewX,
                              int viewY,
                              int& screenX,
                              int& screenY) OVERRIDE {
    REQUIRE_UI_THREAD();

    if (!view_)
      return false;

    // Convert the point from view coordinates to actual screen coordinates.
    NSRect bounds = [view_ bounds];
    NSPoint view_pt = {viewX, bounds.size.height - viewY};
    NSPoint window_pt = [view_ convertPoint:view_pt toView:nil];
    NSPoint screen_pt = [[view_ window] convertBaseToScreen:window_pt];
    screenX = screen_pt.x;
    screenY = screen_pt.y;
    return true;
  }

  virtual void OnPopupShow(CefRefPtr<CefBrowser> browser,
                           bool show) OVERRIDE {
    REQUIRE_UI_THREAD();

    if (!view_)
      return;

    view_->renderer_->OnPopupShow(browser, show);
  }

  virtual void OnPopupSize(CefRefPtr<CefBrowser> browser,
                           const CefRect& rect) OVERRIDE {
    REQUIRE_UI_THREAD();

    if (!view_)
      return;

    view_->renderer_->OnPopupSize(browser, rect);
  }

  virtual void OnPaint(CefRefPtr<CefBrowser> browser,
                       PaintElementType type,
                       const RectList& dirtyRects,
                       const void* buffer) OVERRIDE {
    REQUIRE_UI_THREAD();

    if (!view_)
      return;

    [[view_ openGLContext] makeCurrentContext];

    view_->renderer_->OnPaint(browser, type, dirtyRects, buffer);

    // Notify the view to redraw the invalidated regions.
    {
      RectList::const_iterator i = dirtyRects.begin();
      for (; i != dirtyRects.end(); ++i) {
        NSRect rect = {{i->x, i->y}, {i->width, i->height}};
        [view_ setNeedsDisplayInRect:rect];
      }
    }
  }

  virtual void OnCursorChange(CefRefPtr<CefBrowser> browser,
                              CefCursorHandle cursor) OVERRIDE {
    REQUIRE_UI_THREAD();
    [cursor set];
  }

 private:
  void SetLoading(bool isLoading) {
  }

  ClientOpenGLView* view_;

  // Include the default reference counting implementation.
  IMPLEMENT_REFCOUNTING(ClientOSRPlugin);
};

}  // namespace


@implementation ClientOpenGLView

- (id)initWithFrame:(NSRect)frame andTransparency:(bool)transparency {
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
    renderer_ = new ClientOSRenderer(transparency);
    rotating_ = false;

    tracking_area_ =
        [[NSTrackingArea alloc] initWithRect:frame
                                     options:NSTrackingMouseMoved |
                                             NSTrackingActiveInActiveApp |
                                             NSTrackingInVisibleRect
                                       owner:self
                                    userInfo:nil];
    [self addTrackingArea:tracking_area_];
  }
  return self;
}

- (void)dealloc {
  if (browser_) {
    static_cast<ClientOSRHandler*>(browser_->GetClient().get())->Disconnect();
    browser_->CloseBrowser();
    browser_ = NULL;
  }
  if (renderer_)
    delete renderer_;

  [super dealloc];
}

- (void)drawRect: (NSRect)bounds {
  NSOpenGLContext* context = [self openGLContext];
  [context makeCurrentContext];

  renderer_->Render();

  [context flushBuffer];
}

- (void)setFrame:(NSRect)frameRect {
  [super setFrame:frameRect];

  int width = frameRect.size.width;
  int height = frameRect.size.height;
  
  [[self openGLContext] makeCurrentContext];
  
  renderer_->SetSize(width, height);

  if (browser_)
    browser_->SetSize(PET_VIEW, width, height);
}

- (void)mouseDown:(NSEvent *)event {
  if (!browser_)
    return;

  NSPoint point = [self getClickPointForEvent:event];
  int clickCount = [event clickCount];

  browser_->SendMouseClickEvent(point.x, point.y, MBT_LEFT, false, clickCount);
}

- (void)rightMouseDown:(NSEvent *)event {
  if (!browser_)
    return;

  NSPoint point = [self getClickPointForEvent:event];

  if ([event modifierFlags] & NSShiftKeyMask) {
    // Start rotation effect.
    last_mouse_pos_ = cur_mouse_pos_ = point;
    rotating_ = true;
    return;
  }

  int clickCount = [event clickCount];

  browser_->SendMouseClickEvent(point.x, point.y, MBT_RIGHT, false, clickCount);
}

- (void)otherMouseDown:(NSEvent *)event {
  if (!browser_)
    return;

  NSPoint point = [self getClickPointForEvent:event];
  int clickCount = [event clickCount];

  browser_->SendMouseClickEvent(point.x, point.y, MBT_MIDDLE, false,
                                clickCount);
}

- (void)mouseUp:(NSEvent *)event {
  if (!browser_)
    return;

  NSPoint point = [self getClickPointForEvent:event];
  int clickCount = [event clickCount];

  browser_->SendMouseClickEvent(point.x, point.y, MBT_LEFT, true, clickCount);
}

- (void)rightMouseUp:(NSEvent *)event {
  if (!browser_)
    return;

  if (rotating_) {
    // End rotation effect.
    renderer_->SetSpin(0, 0);
    rotating_ = false;
    [self setNeedsDisplay:YES];
    return;
  }

  NSPoint point = [self getClickPointForEvent:event];
  int clickCount = [event clickCount];

  browser_->SendMouseClickEvent(point.x, point.y, MBT_RIGHT, true, clickCount);
}

- (void)otherMouseUp:(NSEvent *)event {
  if (!browser_)
    return;

  NSPoint point = [self getClickPointForEvent:event];
  int clickCount = [event clickCount];

  browser_->SendMouseClickEvent(point.x, point.y, MBT_MIDDLE, true, clickCount);
}

- (void)mouseMoved:(NSEvent *)event {
  if (!browser_)
    return;

  NSPoint point = [self getClickPointForEvent:event];

  if (rotating_) {
    // Apply rotation effect.
    cur_mouse_pos_ = point;
    renderer_->IncrementSpin((cur_mouse_pos_.x - last_mouse_pos_.x),
                             (cur_mouse_pos_.y - last_mouse_pos_.y));
    last_mouse_pos_ = cur_mouse_pos_;
    [self setNeedsDisplay:YES];
    return;
  }

  browser_->SendMouseMoveEvent(point.x, point.y, false);
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
  if (!browser_)
    return;
  
  NSPoint point = [self getClickPointForEvent:event];

  browser_->SendMouseMoveEvent(point.x, point.y, true);
}

- (void)keyDown:(NSEvent *)event {
  if (!browser_)
    return;
  
  CefKeyInfo keyInfo;
  [self getKeyInfo:keyInfo forEvent:event];
  int modifiers = [self getModifiersForEvent:event];

  browser_->SendKeyEvent(KT_KEYDOWN, keyInfo, modifiers);

  if ([event modifierFlags] & (NSNumericPadKeyMask | NSFunctionKeyMask)) {
    // Don't send a Char event for non-char keys like arrows, function keys and
    // clear.
    switch (keyInfo.keyCode) {
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
        break;
      default:
        return;
    }
  }

  browser_->SendKeyEvent(KT_CHAR, keyInfo, modifiers);
}

- (void)keyUp:(NSEvent *)event {
  if (!browser_)
    return;

  CefKeyInfo keyInfo;
  [self getKeyInfo:keyInfo forEvent:event];
  int modifiers = [self getModifiersForEvent:event];

  browser_->SendKeyEvent(KT_KEYUP, keyInfo, modifiers);
}

- (void)flagsChanged:(NSEvent *)event {
  if ([self isKeyUpEvent:event])
    [self keyUp:event];
  else
    [self keyDown:event];
}

- (void)scrollWheel:(NSEvent *)event {
  if (!browser_)
    return;

  CGEventRef cgEvent = [event CGEvent];
  ASSERT(cgEvent);

  NSPoint point = [self getClickPointForEvent:event];
  int deltaX =
      CGEventGetIntegerValueField(cgEvent, kCGScrollWheelEventPointDeltaAxis2);
  int deltaY =
      CGEventGetIntegerValueField(cgEvent, kCGScrollWheelEventPointDeltaAxis1);

  browser_->SendMouseWheelEvent(point.x, point.y, deltaX, deltaY);
}

- (BOOL)canBecomeKeyView {
  return (browser_ != NULL);
}

- (BOOL)acceptsFirstResponder {
  return (browser_ != NULL);
}

- (BOOL)becomeFirstResponder {
  if (browser_) {
    browser_->SendFocusEvent(true);
    return [super becomeFirstResponder];
  }

  return NO;
}

- (BOOL)resignFirstResponder {
  if (browser_) {
    browser_->SendFocusEvent(false);
    return [super resignFirstResponder];
  }

  return NO;
}

- (void)undo:(id)sender {
  if (browser_)
    browser_->GetFocusedFrame()->Undo();
}

- (void)redo:(id)sender {
  if (browser_)
    browser_->GetFocusedFrame()->Redo();
}

- (void)cut:(id)sender {
  if (browser_)
    browser_->GetFocusedFrame()->Cut();
}

- (void)copy:(id)sender {
  if (browser_)
    browser_->GetFocusedFrame()->Copy();
}

- (void)paste:(id)sender {
  if (browser_)
    browser_->GetFocusedFrame()->Paste();
}

- (void)delete:(id)sender {
  if (browser_)
    browser_->GetFocusedFrame()->Delete();
}

- (void)selectAll:(id)sender {
  if (browser_)
    browser_->GetFocusedFrame()->SelectAll();
}

- (NSPoint)getClickPointForEvent:(NSEvent*)event {
  NSPoint windowLocal = [event locationInWindow];
  NSPoint contentLocal = [self convertPoint:windowLocal fromView:nil];
  int x = contentLocal.x;
  int y = [self frame].size.height - contentLocal.y;  // Flip y.

  return {x,y};
}

- (void)getKeyInfo:(CefKeyInfo&)info forEvent:(NSEvent*)event {
  if ([event type] == NSKeyDown || [event type] == NSKeyUp) {
    NSString* s = [event characters];
    if ([s length] > 0)
      info.character = [s characterAtIndex:0];

    s = [event charactersIgnoringModifiers];
    if ([s length] > 0)
      info.characterNoModifiers = [s characterAtIndex:0];
  }

  info.keyCode = [event keyCode];
}

- (int)getModifiersForEvent:(NSEvent*)event {
  int modifiers = 0;

  if ([event modifierFlags] & NSControlKeyMask)
    modifiers |= KEY_CTRL;
  if ([event modifierFlags] & NSShiftKeyMask)
    modifiers |= KEY_SHIFT; 
  if ([event modifierFlags] & NSAlternateKeyMask)
    modifiers |= KEY_ALT;
  if ([event modifierFlags] & NSCommandKeyMask)
    modifiers |= KEY_META;
  if ([self isKeyPadEvent:event])
    modifiers |= KEY_KEYPAD;

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

@end


namespace osrtest {

void RunTest(bool transparent) {
  NSRect screen_rect = [[NSScreen mainScreen] visibleFrame];
  NSRect window_rect = {{0, screen_rect.size.height}, {700, 700}};
  NSWindow* newWnd = [[NSWindow alloc]
                      initWithContentRect:window_rect
                                styleMask:(NSTitledWindowMask |
                                           NSClosableWindowMask |
                                           NSMiniaturizableWindowMask |
                                           NSResizableWindowMask |
                                           NSUnifiedTitleAndToolbarWindowMask)
                                  backing:NSBackingStoreBuffered
                                    defer:NO];
  ASSERT(newWnd);

  ClientOpenGLView* view = [[ClientOpenGLView alloc] initWithFrame:window_rect
                                                   andTransparency:transparent];
  [view setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];

  [newWnd setContentView:view];
  [view release];

  CefWindowInfo info;
  CefBrowserSettings settings;

  // Initialize the window info as off-screen.
  info.SetAsOffScreen(view);
  info.SetTransparentPainting(transparent);

  // Creat the browser window.
  CefBrowser::CreateBrowser(info, new ClientOSRHandler(view),
      "http://tests/osrtest", settings);

  [newWnd makeKeyAndOrderFront: nil];
}

}  // namespace osrtest

