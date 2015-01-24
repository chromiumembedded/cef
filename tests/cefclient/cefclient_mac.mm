// Copyright (c) 2013 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#include <sstream>
#include "include/cef_app.h"
#import "include/cef_application_mac.h"
#include "include/cef_browser.h"
#include "include/cef_frame.h"
#include "cefclient/client_app.h"
#include "cefclient/client_handler.h"
#include "cefclient/client_switches.h"
#include "cefclient/main_context_impl.h"
#include "cefclient/main_message_loop_std.h"
#include "cefclient/osr_widget_mac.h"
#include "cefclient/resource.h"
#include "cefclient/resource_util.h"
#include "cefclient/test_runner.h"

namespace {

// The global ClientHandler reference.
CefRefPtr<client::ClientHandler> g_handler;

// Used by off-screen rendering to find the associated CefBrowser.
class MainBrowserProvider : public client::OSRBrowserProvider {
  virtual CefRefPtr<CefBrowser> GetBrowser() {
    if (g_handler.get())
      return g_handler->GetBrowser();

    return NULL;
  }
} g_main_browser_provider;


// Sizes for URL bar layout
#define BUTTON_HEIGHT 22
#define BUTTON_WIDTH 72
#define BUTTON_MARGIN 8
#define URLBAR_HEIGHT  32

// Content area size for newly created windows.
const int kWindowWidth = 800;
const int kWindowHeight = 600;


NSButton* MakeButton(NSRect* rect, NSString* title, NSView* parent) {
  NSButton* button = [[[NSButton alloc] initWithFrame:*rect] autorelease];
  [button setTitle:title];
  [button setBezelStyle:NSSmallSquareBezelStyle];
  [button setAutoresizingMask:(NSViewMaxXMargin | NSViewMinYMargin)];
  [parent addSubview:button];
  rect->origin.x += BUTTON_WIDTH;
  return button;
}

void AddMenuItem(NSMenu *menu, NSString* label, int idval) {
  NSMenuItem* item = [menu addItemWithTitle:label
                                     action:@selector(menuItemSelected:)
                              keyEquivalent:@""];
  [item setTag:idval];
}

}  // namespace

// Receives notifications from the application. Will delete itself when done.
@interface ClientAppDelegate : NSObject
- (void)createApplication:(id)object;
- (void)tryToTerminateApplication:(NSApplication*)app;
- (IBAction)menuItemSelected:(id)sender;
@end

// Provide the CefAppProtocol implementation required by CEF.
@interface ClientApplication : NSApplication<CefAppProtocol> {
@private
  BOOL handlingSendEvent_;
}
@end

@implementation ClientApplication
- (BOOL)isHandlingSendEvent {
  return handlingSendEvent_;
}

- (void)setHandlingSendEvent:(BOOL)handlingSendEvent {
  handlingSendEvent_ = handlingSendEvent;
}

- (void)sendEvent:(NSEvent*)event {
  CefScopedSendingEvent sendingEventScoper;
  [super sendEvent:event];
}

// |-terminate:| is the entry point for orderly "quit" operations in Cocoa. This
// includes the application menu's quit menu item and keyboard equivalent, the
// application's dock icon menu's quit menu item, "quit" (not "force quit") in
// the Activity Monitor, and quits triggered by user logout and system restart
// and shutdown.
//
// The default |-terminate:| implementation ends the process by calling exit(),
// and thus never leaves the main run loop. This is unsuitable for Chromium
// since Chromium depends on leaving the main run loop to perform an orderly
// shutdown. We support the normal |-terminate:| interface by overriding the
// default implementation. Our implementation, which is very specific to the
// needs of Chromium, works by asking the application delegate to terminate
// using its |-tryToTerminateApplication:| method.
//
// |-tryToTerminateApplication:| differs from the standard
// |-applicationShouldTerminate:| in that no special event loop is run in the
// case that immediate termination is not possible (e.g., if dialog boxes
// allowing the user to cancel have to be shown). Instead, this method tries to
// close all browsers by calling CloseBrowser(false) via
// ClientHandler::CloseAllBrowsers. Calling CloseBrowser will result in a call
// to ClientHandler::DoClose and execution of |-performClose:| on the NSWindow.
// DoClose sets a flag that is used to differentiate between new close events
// (e.g., user clicked the window close button) and in-progress close events
// (e.g., user approved the close window dialog). The NSWindowDelegate
// |-windowShouldClose:| method checks this flag and either calls
// CloseBrowser(false) in the case of a new close event or destructs the
// NSWindow in the case of an in-progress close event.
// ClientHandler::OnBeforeClose will be called after the CEF NSView hosted in
// the NSWindow is dealloc'ed.
//
// After the final browser window has closed ClientHandler::OnBeforeClose will
// begin actual tear-down of the application by calling CefQuitMessageLoop.
// This ends the NSApplication event loop and execution then returns to the
// main() function for cleanup before application termination.
//
// The standard |-applicationShouldTerminate:| is not supported, and code paths
// leading to it must be redirected.
- (void)terminate:(id)sender {
  ClientAppDelegate* delegate =
      static_cast<ClientAppDelegate*>([NSApp delegate]);
  [delegate tryToTerminateApplication:self];
  // Return, don't exit. The application is responsible for exiting on its own.
}
@end


// Receives notifications from controls and the browser window. Will delete
// itself when done.
@interface ClientWindowDelegate : NSObject <NSWindowDelegate> {
 @private
  NSWindow* window_;
}
- (id)initWithWindow:(NSWindow*)window;
- (IBAction)goBack:(id)sender;
- (IBAction)goForward:(id)sender;
- (IBAction)reload:(id)sender;
- (IBAction)stopLoading:(id)sender;
- (IBAction)takeURLStringValueFrom:(NSTextField *)sender;
- (void)alert:(NSString*)title withMessage:(NSString*)message;
@end

@implementation ClientWindowDelegate

- (id)initWithWindow:(NSWindow*)window {
  if (self = [super init]) {
    window_ = window;
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

- (IBAction)goBack:(id)sender {
  if (g_handler.get() && g_handler->GetBrowserId())
    g_handler->GetBrowser()->GoBack();
}

- (IBAction)goForward:(id)sender {
  if (g_handler.get() && g_handler->GetBrowserId())
    g_handler->GetBrowser()->GoForward();
}

- (IBAction)reload:(id)sender {
  if (g_handler.get() && g_handler->GetBrowserId())
    g_handler->GetBrowser()->Reload();
}

- (IBAction)stopLoading:(id)sender {
  if (g_handler.get() && g_handler->GetBrowserId())
    g_handler->GetBrowser()->StopLoad();
}

- (IBAction)takeURLStringValueFrom:(NSTextField *)sender {
  if (!g_handler.get() || !g_handler->GetBrowserId())
    return;

  NSString *url = [sender stringValue];

  // if it doesn't already have a prefix, add http. If we can't parse it,
  // just don't bother rather than making things worse.
  NSURL* tempUrl = [NSURL URLWithString:url];
  if (tempUrl && ![tempUrl scheme])
    url = [@"http://" stringByAppendingString:url];

  std::string urlStr = [url UTF8String];
  g_handler->GetBrowser()->GetMainFrame()->LoadURL(urlStr);
}

- (void)alert:(NSString*)title withMessage:(NSString*)message {
  NSAlert *alert = [NSAlert alertWithMessageText:title
                                   defaultButton:@"OK"
                                 alternateButton:nil
                                     otherButton:nil
                       informativeTextWithFormat:@"%@", message];
  [alert runModal];
}

// Called when we are activated (when we gain focus).
- (void)windowDidBecomeKey:(NSNotification*)notification {
  if (g_handler.get()) {
    CefRefPtr<CefBrowser> browser = g_handler->GetBrowser();
    if (browser.get()) {
      if (CefCommandLine::GetGlobalCommandLine()->HasSwitch(
              client::switches::kOffScreenRenderingEnabled)) {
        browser->GetHost()->SendFocusEvent(true);
      } else {
        browser->GetHost()->SetFocus(true);
      }
    }
  }
}

// Called when we are deactivated (when we lose focus).
- (void)windowDidResignKey:(NSNotification*)notification {
  if (g_handler.get()) {
    CefRefPtr<CefBrowser> browser = g_handler->GetBrowser();
    if (browser.get()) {
      if (CefCommandLine::GetGlobalCommandLine()->HasSwitch(
              client::switches::kOffScreenRenderingEnabled)) {
        browser->GetHost()->SendFocusEvent(false);
      } else {
        browser->GetHost()->SetFocus(false);
      }
    }
  }
}

// Called when we have been minimized.
- (void)windowDidMiniaturize:(NSNotification *)notification {
  if (g_handler.get()) {
    CefRefPtr<CefBrowser> browser = g_handler->GetBrowser();
    if (browser.get())
      browser->GetHost()->SetWindowVisibility(false);
  }
}

// Called when we have been unminimized.
- (void)windowDidDeminiaturize:(NSNotification *)notification {
  if (g_handler.get()) {
    CefRefPtr<CefBrowser> browser = g_handler->GetBrowser();
    if (browser.get())
      browser->GetHost()->SetWindowVisibility(true);
  }
}

// Called when the application has been hidden.
- (void)applicationDidHide:(NSNotification *)notification {
  // If the window is miniaturized then nothing has really changed.
  if (![window_ isMiniaturized]) {
    if (g_handler.get()) {
      CefRefPtr<CefBrowser> browser = g_handler->GetBrowser();
      if (browser.get())
        browser->GetHost()->SetWindowVisibility(false);
    }
  }
}

// Called when the application has been unhidden.
- (void)applicationDidUnhide:(NSNotification *)notification {
  // If the window is miniaturized then nothing has really changed.
  if (![window_ isMiniaturized]) {
    if (g_handler.get()) {
      CefRefPtr<CefBrowser> browser = g_handler->GetBrowser();
      if (browser.get())
        browser->GetHost()->SetWindowVisibility(true);
    }
  }
}

// Called when the window is about to close. Perform the self-destruction
// sequence by getting rid of the window. By returning YES, we allow the window
// to be removed from the screen.
- (BOOL)windowShouldClose:(id)window {
  if (g_handler.get() && !g_handler->IsClosing()) {
    CefRefPtr<CefBrowser> browser = g_handler->GetBrowser();
    if (browser.get()) {
      // Notify the browser window that we would like to close it. This
      // will result in a call to ClientHandler::DoClose() if the
      // JavaScript 'onbeforeunload' event handler allows it.
      browser->GetHost()->CloseBrowser(false);

      // Cancel the close.
      return NO;
    }
  }

  // Try to make the window go away.
  [window autorelease];

  // Clean ourselves up after clearing the stack of anything that might have the
  // window on it.
  [self performSelectorOnMainThread:@selector(cleanup:)
                         withObject:window
                      waitUntilDone:NO];

  // Allow the close.
  return YES;
}

// Deletes itself.
- (void)cleanup:(id)window {
  [self release];
}

@end


@implementation ClientAppDelegate

// Create the application on the UI thread.
- (void)createApplication:(id)object {
  [NSApplication sharedApplication];
  [NSBundle loadNibNamed:@"MainMenu" owner:NSApp];

  // Set the delegate for application events.
  [NSApp setDelegate:self];

  // Add the Tests menu.
  NSMenu* menubar = [NSApp mainMenu];
  NSMenuItem *testItem = [[[NSMenuItem alloc] initWithTitle:@"Tests"
                                                     action:nil
                                              keyEquivalent:@""] autorelease];
  NSMenu *testMenu = [[[NSMenu alloc] initWithTitle:@"Tests"] autorelease];
  AddMenuItem(testMenu, @"Get Text",      ID_TESTS_GETSOURCE);
  AddMenuItem(testMenu, @"Get Source",    ID_TESTS_GETTEXT);
  AddMenuItem(testMenu, @"Popup Window",  ID_TESTS_POPUP);
  AddMenuItem(testMenu, @"Request",       ID_TESTS_REQUEST);
  AddMenuItem(testMenu, @"Plugin Info",   ID_TESTS_PLUGIN_INFO);
  AddMenuItem(testMenu, @"Zoom In",       ID_TESTS_ZOOM_IN);
  AddMenuItem(testMenu, @"Zoom Out",      ID_TESTS_ZOOM_OUT);
  AddMenuItem(testMenu, @"Zoom Reset",    ID_TESTS_ZOOM_RESET);
  AddMenuItem(testMenu, @"Begin Tracing", ID_TESTS_TRACING_BEGIN);
  AddMenuItem(testMenu, @"End Tracing",   ID_TESTS_TRACING_END);
  AddMenuItem(testMenu, @"Print",         ID_TESTS_PRINT);
  AddMenuItem(testMenu, @"Other Tests",   ID_TESTS_OTHER_TESTS);
  [testItem setSubmenu:testMenu];
  [menubar addItem:testItem];

  // Create the main application window.
  NSRect screen_rect = [[NSScreen mainScreen] visibleFrame];
  NSRect window_rect = { {0, screen_rect.size.height - kWindowHeight},
    {kWindowWidth, kWindowHeight} };
  NSWindow* mainWnd = [[UnderlayOpenGLHostingWindow alloc]
                       initWithContentRect:window_rect
                       styleMask:(NSTitledWindowMask |
                                  NSClosableWindowMask |
                                  NSMiniaturizableWindowMask |
                                  NSResizableWindowMask )
                       backing:NSBackingStoreBuffered
                       defer:NO];
  [mainWnd setTitle:@"cefclient"];

  // Create the delegate for control and browser window events.
  ClientWindowDelegate* delegate =
      [[ClientWindowDelegate alloc] initWithWindow:mainWnd];

  // Rely on the window delegate to clean us up rather than immediately
  // releasing when the window gets closed. We use the delegate to do
  // everything from the autorelease pool so the window isn't on the stack
  // during cleanup (ie, a window close from javascript).
  [mainWnd setReleasedWhenClosed:NO];

  NSView* contentView = [mainWnd contentView];

  // Create the buttons.
  NSRect button_rect = [contentView bounds];
  button_rect.origin.y = window_rect.size.height - URLBAR_HEIGHT +
      (URLBAR_HEIGHT - BUTTON_HEIGHT) / 2;
  button_rect.size.height = BUTTON_HEIGHT;
  button_rect.origin.x += BUTTON_MARGIN;
  button_rect.size.width = BUTTON_WIDTH;

  NSButton* button = MakeButton(&button_rect, @"Back", contentView);
  [button setTarget:delegate];
  [button setAction:@selector(goBack:)];

  button = MakeButton(&button_rect, @"Forward", contentView);
  [button setTarget:delegate];
  [button setAction:@selector(goForward:)];

  button = MakeButton(&button_rect, @"Reload", contentView);
  [button setTarget:delegate];
  [button setAction:@selector(reload:)];

  button = MakeButton(&button_rect, @"Stop", contentView);
  [button setTarget:delegate];
  [button setAction:@selector(stopLoading:)];

  // Create the URL text field.
  button_rect.origin.x += BUTTON_MARGIN;
  button_rect.size.width = [contentView bounds].size.width -
      button_rect.origin.x - BUTTON_MARGIN;
  NSTextField* editWnd = [[NSTextField alloc] initWithFrame:button_rect];
  [contentView addSubview:editWnd];
  [editWnd setAutoresizingMask:(NSViewWidthSizable | NSViewMinYMargin)];
  [editWnd setTarget:delegate];
  [editWnd setAction:@selector(takeURLStringValueFrom:)];
  [[editWnd cell] setWraps:NO];
  [[editWnd cell] setScrollable:YES];

  // Create the handler.
  g_handler = new client::ClientHandler();
  g_handler->SetMainWindowHandle(contentView);
  g_handler->SetEditWindowHandle(editWnd);

  // Create the browser view.
  CefWindowInfo window_info;
  CefBrowserSettings settings;

  // Populate the browser settings based on command line arguments.
  client::MainContext::Get()->PopulateBrowserSettings(&settings);

  CefRefPtr<CefCommandLine> command_line =
      CefCommandLine::GetGlobalCommandLine();
  if (command_line->HasSwitch(client::switches::kOffScreenRenderingEnabled)) {
    const bool transparent =
        command_line->HasSwitch(client::switches::kTransparentPaintingEnabled);
    const bool show_update_rect =
        command_line->HasSwitch(client::switches::kShowUpdateRect);

    CefRefPtr<client::OSRWindow> osr_window =
        client::OSRWindow::Create(&g_main_browser_provider, transparent,
            show_update_rect, contentView,
            CefRect(0, 0, kWindowWidth, kWindowHeight));
    window_info.SetAsWindowless(osr_window->GetWindowHandle(), transparent);
    g_handler->SetOSRHandler(osr_window->GetRenderHandler().get());
  } else {
    // Initialize window info to the defaults for a child window.
    window_info.SetAsChild(contentView, 0, 0, kWindowWidth, kWindowHeight);
  }

  CefBrowserHost::CreateBrowser(window_info, g_handler.get(),
                                g_handler->GetStartupURL(), settings, NULL);

  // Show the window.
  [mainWnd makeKeyAndOrderFront: nil];

  // Size the window.
  NSRect r = [mainWnd contentRectForFrameRect:[mainWnd frame]];
  r.size.width = kWindowWidth;
  r.size.height = kWindowHeight + URLBAR_HEIGHT;
  [mainWnd setFrame:[mainWnd frameRectForContentRect:r] display:YES];
}

- (void)tryToTerminateApplication:(NSApplication*)app {
  if (g_handler.get() && !g_handler->IsClosing())
    g_handler->CloseAllBrowsers(false);
}

- (IBAction)menuItemSelected:(id)sender {
  NSMenuItem *item = (NSMenuItem*)sender;
  if (g_handler.get() && g_handler->GetBrowserId())
    client::test_runner::RunTest(g_handler->GetBrowser(), [item tag]);
}

- (NSApplicationTerminateReply)applicationShouldTerminate:
      (NSApplication *)sender {
  return NSTerminateNow;
}

@end


namespace client {
namespace {

int RunMain(int argc, char* argv[]) {
  CefMainArgs main_args(argc, argv);
  CefRefPtr<ClientApp> app(new ClientApp);

  // Initialize the AutoRelease pool.
  NSAutoreleasePool* autopool = [[NSAutoreleasePool alloc] init];

  // Initialize the ClientApplication instance.
  [ClientApplication sharedApplication];

  // Create the main context object.
  scoped_ptr<MainContextImpl> context(new MainContextImpl(argc, argv));

  CefSettings settings;

  // Populate the settings based on command line arguments.
  context->PopulateSettings(&settings);

  // Create the main message loop object.
  scoped_ptr<MainMessageLoop> message_loop(new MainMessageLoopStd);

  // Initialize CEF.
  CefInitialize(main_args, settings, app.get(), NULL);

  // Register scheme handlers.
  test_runner::RegisterSchemeHandlers();

  // Create the application delegate and window.
  NSObject* delegate = [[ClientAppDelegate alloc] init];
  [delegate performSelectorOnMainThread:@selector(createApplication:)
                             withObject:nil
                          waitUntilDone:NO];

  // Run the message loop. This will block until Quit() is called.
  int result = message_loop->Run();

  // Shut down CEF.
  CefShutdown();

  // Release objects in reverse order of creation.
  g_handler = NULL;
  [delegate release];
  message_loop.reset();
  context.reset();
  [autopool release];

  return result;
}

}  // namespace
}  // namespace client


// Program entry point function.
int main(int argc, char* argv[]) {
  return client::RunMain(argc, argv);
}
