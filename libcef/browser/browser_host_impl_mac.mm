// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/browser_host_impl.h"

#import <Cocoa/Cocoa.h>
#import <CoreServices/CoreServices.h>

#include "libcef/browser/render_widget_host_view_osr.h"
#include "libcef/browser/text_input_client_osr_mac.h"
#include "libcef/browser/thread_util.h"

#include "base/file_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#import "chrome/browser/ui/cocoa/nsview_additions.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/file_chooser_params.h"
#include "grit/ui_strings.h"
#include "net/base/mime_util.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/mac/WebInputEventFactory.h"
#import  "ui/base/cocoa/underlay_opengl_hosting_window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/gfx/rect.h"

// Wrapper NSView for the native view. Necessary to destroy the browser when
// the view is deleted.
@interface CefBrowserHostView : NSView {
 @private
  CefBrowserHostImpl* browser_;  // weak
}

@property (nonatomic, assign) CefBrowserHostImpl* browser;

@end

@implementation CefBrowserHostView

@synthesize browser = browser_;

- (void) dealloc {
  if (browser_) {
    // Force the browser to be destroyed and release the reference added in
    // PlatformCreateWindow().
    browser_->WindowDestroyed();
  }

  [super dealloc];
}

@end

// Receives notifications from the browser window. Will delete itself when done.
@interface CefWindowDelegate : NSObject <NSWindowDelegate> {
 @private
  CefBrowserHostImpl* browser_;  // weak
  NSWindow* window_;
}
- (id)initWithWindow:(NSWindow*)window andBrowser:(CefBrowserHostImpl*)browser;
@end

@implementation CefWindowDelegate

- (id)initWithWindow:(NSWindow*)window andBrowser:(CefBrowserHostImpl*)browser {
  if (self = [super init]) {
    window_ = window;
    browser_ = browser;

    [window_ setDelegate:self];

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

  [super dealloc];
}

// Called when we are activated (when we gain focus).
- (void)windowDidBecomeKey:(NSNotification*)notification {
  if (browser_)
    browser_->SetFocus(true);
}

// Called when we are deactivated (when we lose focus).
- (void)windowDidResignKey:(NSNotification*)notification {
  if (browser_)
    browser_->SetFocus(false);
}

// Called when we have been minimized.
- (void)windowDidMiniaturize:(NSNotification *)notification {
  if (browser_)
    browser_->SetWindowVisibility(false);
}

// Called when we have been unminimized.
- (void)windowDidDeminiaturize:(NSNotification *)notification {
  if (browser_)
    browser_->SetWindowVisibility(true);
}

// Called when the application has been hidden.
- (void)applicationDidHide:(NSNotification *)notification {
  // If the window is miniaturized then nothing has really changed.
  if (![window_ isMiniaturized]) {
    if (browser_)
      browser_->SetWindowVisibility(false);
  }
}

// Called when the application has been unhidden.
- (void)applicationDidUnhide:(NSNotification *)notification {
  // If the window is miniaturized then nothing has really changed.
  if (![window_ isMiniaturized]) {
    if (browser_)
      browser_->SetWindowVisibility(true);
  }
}

- (BOOL)windowShouldClose:(id)window {
  // Protect against multiple requests to close while the close is pending.
  if (browser_ && browser_->destruction_state() <=
      CefBrowserHostImpl::DESTRUCTION_STATE_PENDING) {
    if (browser_->destruction_state() ==
        CefBrowserHostImpl::DESTRUCTION_STATE_NONE) {
      // Request that the browser close.
      browser_->CloseBrowser(false);
    }

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
  [self release];
}

@end


namespace {

// Accept-types to file-types helper.
NSMutableArray* GetFileTypesFromAcceptTypes(
    const std::vector<base::string16>& accept_types) {
  NSMutableArray* acceptArray = [[NSMutableArray alloc] init];
  for (size_t i=0; i<accept_types.size(); i++) {
    std::string ascii_type = base::UTF16ToASCII(accept_types[i]);
    if (ascii_type.length()) {
      // Just treat as extension if contains '.' as the first character.
      if (ascii_type[0] == '.') {
        [acceptArray addObject:base::SysUTF8ToNSString(ascii_type.substr(1))];
      } else {
        // Otherwise convert mime type to one or more extensions.
        std::vector<base::FilePath::StringType> ext;
        net::GetExtensionsForMimeType(ascii_type, &ext);
        for (size_t x = 0; x < ext.size(); ++x)
          [acceptArray addObject:base::SysUTF8ToNSString(ext[x])];
      }
    }
  }
  return acceptArray;
}

void RunOpenFileDialog(const content::FileChooserParams& params,
                       NSView* view,
                       std::vector<base::FilePath>* files) {
  NSOpenPanel* openPanel = [NSOpenPanel openPanel];

  base::string16 title;
  if (!params.title.empty()) {
    title = params.title;
  } else {
    title = l10n_util::GetStringUTF16(
        params.mode == content::FileChooserParams::Open ?
            IDS_OPEN_FILE_DIALOG_TITLE : IDS_OPEN_FILES_DIALOG_TITLE);
  }
  [openPanel setTitle:base::SysUTF16ToNSString(title)];

  // Consider default file name if any.
  base::FilePath default_file_name(params.default_file_name);

  if (!default_file_name.empty()) {
    if (!default_file_name.BaseName().empty()) {
      NSString* defaultName = base::SysUTF8ToNSString(
          default_file_name.BaseName().value());
      [openPanel setNameFieldStringValue:defaultName];
    }

    if (!default_file_name.DirName().empty()) {
      NSString* defaultDir = base::SysUTF8ToNSString(
          default_file_name.DirName().value());
      [openPanel setDirectoryURL:[NSURL fileURLWithPath:defaultDir]];
    }
  }

  // Consider supported file types
  if (!params.accept_types.empty()) {
    [openPanel setAllowedFileTypes:GetFileTypesFromAcceptTypes(
        params.accept_types)];
  }

  // Further panel configuration.
  [openPanel setAllowsOtherFileTypes:YES];
  [openPanel setAllowsMultipleSelection:
      (params.mode == content::FileChooserParams::OpenMultiple)];
  [openPanel setCanChooseFiles:YES];
  [openPanel setCanChooseDirectories:NO];

  // Show panel.
  [openPanel beginSheetModalForWindow:[view window] completionHandler:nil];
  if ([openPanel runModal] == NSFileHandlingPanelOKButton) {
    NSArray *urls = [openPanel URLs];
    int i, count = [urls count];
    for (i=0; i<count; i++) {
      NSURL* url = [urls objectAtIndex:i];
      if ([url isFileURL])
        files->push_back(base::FilePath(base::SysNSStringToUTF8([url path])));
    }
  }
  [NSApp endSheet:openPanel];
}

bool RunSaveFileDialog(const content::FileChooserParams& params,
                       NSView* view,
                       base::FilePath* file) {
  NSSavePanel* savePanel = [NSSavePanel savePanel];

  base::string16 title;
  if (!params.title.empty())
    title = params.title;
  else
    title = l10n_util::GetStringUTF16(IDS_SAVE_AS_DIALOG_TITLE);
  [savePanel setTitle:base::SysUTF16ToNSString(title)];

  // Consider default file name if any.
  base::FilePath default_file_name(params.default_file_name);

  if (!default_file_name.empty()) {
    if (!default_file_name.BaseName().empty()) {
      NSString* defaultName = base::SysUTF8ToNSString(
          default_file_name.BaseName().value());
      [savePanel setNameFieldStringValue:defaultName];
    }

    if (!default_file_name.DirName().empty()) {
      NSString* defaultDir = base::SysUTF8ToNSString(
          default_file_name.DirName().value());
      [savePanel setDirectoryURL:[NSURL fileURLWithPath:defaultDir]];
    }
  }

  // Consider supported file types
  if (!params.accept_types.empty()) {
    [savePanel setAllowedFileTypes:GetFileTypesFromAcceptTypes(
        params.accept_types)];
  }

  [savePanel setAllowsOtherFileTypes:YES];

  bool success = false;

  [savePanel beginSheetModalForWindow:[view window] completionHandler:nil];
  if ([savePanel runModal] == NSFileHandlingPanelOKButton) {
    NSURL * url = [savePanel URL];
    NSString* path = [url path];
    *file = base::FilePath([path UTF8String]);
    success = true;
  }
  [NSApp endSheet:savePanel];

  return success;
}

}  // namespace

bool CefBrowserHostImpl::PlatformViewText(const std::string& text) {
  NOTIMPLEMENTED();
  return false;
}

CefTextInputContext CefBrowserHostImpl::GetNSTextInputContext() {
  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return NULL;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    NOTREACHED() << "Called on invalid thread";
    return NULL;
  }

  CefRenderWidgetHostViewOSR* rwhv = static_cast<CefRenderWidgetHostViewOSR*>(
      GetWebContents()->GetRenderWidgetHostView());

  return rwhv->GetNSTextInputContext();
}

void CefBrowserHostImpl::HandleKeyEventBeforeTextInputClient(
    CefEventHandle keyEvent) {
  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    NOTREACHED() << "Called on invalid thread";
    return;
  }

  CefRenderWidgetHostViewOSR* rwhv = static_cast<CefRenderWidgetHostViewOSR*>(
      GetWebContents()->GetRenderWidgetHostView());

  rwhv->HandleKeyEventBeforeTextInputClient(keyEvent);
}

void CefBrowserHostImpl::HandleKeyEventAfterTextInputClient(
    CefEventHandle keyEvent) {
  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    NOTREACHED() << "Called on invalid thread";
    return;
  }

  CefRenderWidgetHostViewOSR* rwhv = static_cast<CefRenderWidgetHostViewOSR*>(
      GetWebContents()->GetRenderWidgetHostView());

  rwhv->HandleKeyEventAfterTextInputClient(keyEvent);
}

bool CefBrowserHostImpl::PlatformCreateWindow() {
  base::mac::ScopedNSAutoreleasePool autorelease_pool;

  NSWindow* newWnd = nil;

  NSView* parentView = window_info_.parent_view;
  NSRect contentRect = {{window_info_.x, window_info_.y},
                        {window_info_.width, window_info_.height}};
  if (parentView == nil) {
    // Create a new window.
    NSRect screen_rect = [[NSScreen mainScreen] visibleFrame];
    NSRect window_rect = {{window_info_.x,
                           screen_rect.size.height - window_info_.y},
                          {window_info_.width, window_info_.height}};
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
              styleMask:(NSTitledWindowMask |
                         NSClosableWindowMask |
                         NSMiniaturizableWindowMask |
                         NSResizableWindowMask |
                         NSUnifiedTitleAndToolbarWindowMask )
              backing:NSBackingStoreBuffered
              defer:NO];

    // Create the delegate for control and browser window events.
    [[CefWindowDelegate alloc] initWithWindow:newWnd andBrowser:this];

    parentView = [newWnd contentView];
    window_info_.parent_view = parentView;
  }

  // Make the content view for the window have a layer. This will make all
  // sub-views have layers. This is necessary to ensure correct layer
  // ordering of all child views and their layers.
  [[[parentView window] contentView] setWantsLayer:YES];

  // Add a reference that will be released in the dealloc handler.
  AddRef();

  // Create the browser view.
  CefBrowserHostView* browser_view =
      [[CefBrowserHostView alloc] initWithFrame:contentRect];
  browser_view.browser = this;
  [parentView addSubview:browser_view];
  [browser_view setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
  [browser_view setNeedsDisplay:YES];
  [browser_view release];

  // Parent the TabContents to the browser view.
  const NSRect bounds = [browser_view bounds];
  NSView* native_view = web_contents_->GetNativeView();
  [browser_view addSubview:native_view];
  [native_view setFrame:bounds];
  [native_view setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
  [native_view setNeedsDisplay:YES];

  window_info_.view = browser_view;

  if (newWnd != nil && !window_info_.hidden) {
    // Show the window.
    [newWnd makeKeyAndOrderFront: nil];
  }

  return true;
}

void CefBrowserHostImpl::PlatformCloseWindow() {
  if (window_info_.view != nil) {
    [[window_info_.view window]
        performSelectorOnMainThread:@selector(performClose:)
                         withObject:nil
                      waitUntilDone:NO];
  }
}

void CefBrowserHostImpl::PlatformSizeTo(int width, int height) {
  // Not needed; subviews are bound.
}

void CefBrowserHostImpl::PlatformSetFocus(bool focus) {
  if (web_contents_) {
    if (content::RenderWidgetHostView* view =
        web_contents_->GetRenderWidgetHostView()) {
      view->SetActive(focus);

      if (focus && !IsWindowless()) {
        // Give keyboard focus to the native view.
        NSView* view = web_contents_->GetContentNativeView();
        DCHECK([view canBecomeKeyView]);
        [[view window] makeFirstResponder:view];
      }
    }
  }
}

void CefBrowserHostImpl::PlatformSetWindowVisibility(bool visible) {
  if (web_contents_) {
    if (content::RenderWidgetHostView* view =
        web_contents_->GetRenderWidgetHostView()) {
      view->SetWindowVisibility(visible);
    }
  }
}

CefWindowHandle CefBrowserHostImpl::PlatformGetWindowHandle() {
  return IsWindowless() ? window_info_.parent_view : window_info_.view;
}

void CefBrowserHostImpl::PlatformHandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  // Give the top level menu equivalents a chance to handle the event.
  if ([event.os_event type] == NSKeyDown)
    [[NSApp mainMenu] performKeyEquivalent:event.os_event];
}

void CefBrowserHostImpl::PlatformRunFileChooser(
    const content::FileChooserParams& params,
    RunFileChooserCallback callback) {
  std::vector<base::FilePath> files;

  if (params.mode == content::FileChooserParams::Open ||
      params.mode == content::FileChooserParams::OpenMultiple) {
    RunOpenFileDialog(params, PlatformGetWindowHandle(), &files);
  } else if (params.mode == content::FileChooserParams::Save) {
    base::FilePath file;
    if (RunSaveFileDialog(params, PlatformGetWindowHandle(), &file))
      files.push_back(file);
  }  else {
    NOTIMPLEMENTED();
  }

  callback.Run(files);
}

void CefBrowserHostImpl::PlatformHandleExternalProtocol(const GURL& url) {
}

static NSTimeInterval currentEventTimestamp() {
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

static NSUInteger NativeModifiers(int cef_modifiers) {
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

void CefBrowserHostImpl::PlatformTranslateKeyEvent(
    content::NativeWebKeyboardEvent& native_event,
    const CefKeyEvent& key_event) {
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

  NSString* charactersIgnoringModifiers = [[[NSString alloc]
        initWithCharacters:&key_event.unmodified_character length:1]
        autorelease];
  NSString* characters = [[[NSString alloc]
        initWithCharacters:&key_event.character length:1] autorelease];

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

  native_event = content::NativeWebKeyboardEvent(synthetic_event);
  if (key_event.type == KEYEVENT_CHAR)
    native_event.type = blink::WebInputEvent::Char;

  native_event.isSystemKey = key_event.is_system_key;
}

void CefBrowserHostImpl::PlatformTranslateClickEvent(
    blink::WebMouseEvent& result,
    const CefMouseEvent& mouse_event,
    MouseButtonType type,
    bool mouseUp, int clickCount) {
  PlatformTranslateMouseEvent(result, mouse_event);

  switch (type) {
  case MBT_LEFT:
    result.type = mouseUp ? blink::WebInputEvent::MouseUp :
                            blink::WebInputEvent::MouseDown;
    result.button = blink::WebMouseEvent::ButtonLeft;
    break;
  case MBT_MIDDLE:
    result.type = mouseUp ? blink::WebInputEvent::MouseUp :
                            blink::WebInputEvent::MouseDown;
    result.button = blink::WebMouseEvent::ButtonMiddle;
    break;
  case MBT_RIGHT:
    result.type = mouseUp ? blink::WebInputEvent::MouseUp :
                            blink::WebInputEvent::MouseDown;
    result.button = blink::WebMouseEvent::ButtonRight;
    break;
  default:
    NOTREACHED();
  }

  result.clickCount = clickCount;
}

void CefBrowserHostImpl::PlatformTranslateMoveEvent(
    blink::WebMouseEvent& result,
    const CefMouseEvent& mouse_event,
    bool mouseLeave) {
  PlatformTranslateMouseEvent(result, mouse_event);

  if (!mouseLeave) {
    result.type = blink::WebInputEvent::MouseMove;
    if (mouse_event.modifiers & EVENTFLAG_LEFT_MOUSE_BUTTON)
      result.button = blink::WebMouseEvent::ButtonLeft;
    else if (mouse_event.modifiers & EVENTFLAG_MIDDLE_MOUSE_BUTTON)
      result.button = blink::WebMouseEvent::ButtonMiddle;
    else if (mouse_event.modifiers & EVENTFLAG_RIGHT_MOUSE_BUTTON)
      result.button = blink::WebMouseEvent::ButtonRight;
    else
      result.button = blink::WebMouseEvent::ButtonNone;
  } else {
    result.type = blink::WebInputEvent::MouseLeave;
    result.button = blink::WebMouseEvent::ButtonNone;
  }

  result.clickCount = 0;
}

void CefBrowserHostImpl::PlatformTranslateWheelEvent(
    blink::WebMouseWheelEvent& result,
    const CefMouseEvent& mouse_event,
    int deltaX, int deltaY) {
  result = blink::WebMouseWheelEvent();
  PlatformTranslateMouseEvent(result, mouse_event);

  result.type = blink::WebInputEvent::MouseWheel;

  static const double scrollbarPixelsPerCocoaTick = 40.0;
  result.deltaX = deltaX;
  result.deltaY = deltaY;
  result.wheelTicksX = result.deltaX / scrollbarPixelsPerCocoaTick;
  result.wheelTicksY = result.deltaY / scrollbarPixelsPerCocoaTick;
  result.hasPreciseScrollingDeltas = true;

  // Unless the phase and momentumPhase are passed in as parameters to this
  // function, there is no way to know them
  result.phase = blink::WebMouseWheelEvent::PhaseNone;
  result.momentumPhase = blink::WebMouseWheelEvent::PhaseNone;

  if (mouse_event.modifiers & EVENTFLAG_LEFT_MOUSE_BUTTON)
    result.button = blink::WebMouseEvent::ButtonLeft;
  else if (mouse_event.modifiers & EVENTFLAG_MIDDLE_MOUSE_BUTTON)
    result.button = blink::WebMouseEvent::ButtonMiddle;
  else if (mouse_event.modifiers & EVENTFLAG_RIGHT_MOUSE_BUTTON)
    result.button = blink::WebMouseEvent::ButtonRight;
  else
    result.button = blink::WebMouseEvent::ButtonNone;
}

void CefBrowserHostImpl::PlatformTranslateMouseEvent(
    blink::WebMouseEvent& result,
    const CefMouseEvent& mouse_event) {
  // position
  result.x = mouse_event.x;
  result.y = mouse_event.y;
  result.windowX = result.x;
  result.windowY = result.y;
  result.globalX = result.x;
  result.globalY = result.y;

  if (IsWindowless()) {
    GetClient()->GetRenderHandler()->GetScreenPoint(
        GetBrowser(),
        result.x, result.y,
        result.globalX, result.globalY);
  } else {
    NSView* view = window_info_.parent_view;
    if (view) {
      NSRect bounds = [view bounds];
      NSPoint view_pt = {result.x, bounds.size.height - result.y};
      NSPoint window_pt = [view convertPoint:view_pt toView:nil];
      NSPoint screen_pt = [[view window] convertBaseToScreen:window_pt];
      result.globalX = screen_pt.x;
      result.globalY = screen_pt.y;
    }
  }

  // modifiers
  result.modifiers |= TranslateModifiers(mouse_event.modifiers);

  // timestamp - Mac OSX specific
  result.timeStampSeconds = currentEventTimestamp();
}
