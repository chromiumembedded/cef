// Copyright (c) 2025 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "include/capi/cef_app_capi.h"
#include "include/cef_api_hash.h"
#include "include/cef_application_mac.h"
#include "include/wrapper/cef_library_loader.h"
#include "tests/cefsimple_capi/simple_app.h"
#include "tests/cefsimple_capi/simple_handler.h"
#include "tests/cefsimple_capi/simple_utils.h"

// Receives notifications from the application.
@interface SimpleAppDelegate : NSObject <NSApplicationDelegate>
- (void)createApplication:(id)object;
- (void)tryToTerminateApplication:(NSApplication*)app;
@end

// Provide the CefAppProtocol implementation required by CEF.
@interface SimpleApplication : NSApplication <CefAppProtocol> {
 @private
  BOOL handlingSendEvent_;
}
@end

@implementation SimpleApplication
- (BOOL)isHandlingSendEvent {
  return handlingSendEvent_;
}

- (void)setHandlingSendEvent:(BOOL)handlingSendEvent {
  handlingSendEvent_ = handlingSendEvent;
}

- (void)sendEvent:(NSEvent*)event {
  BOOL wasHandlingSendEvent = [self isHandlingSendEvent];
  [self setHandlingSendEvent:YES];
  [super sendEvent:event];
  [self setHandlingSendEvent:wasHandlingSendEvent];
}

- (void)terminate:(id)sender {
  SimpleAppDelegate* delegate = (SimpleAppDelegate*)[NSApp delegate];
  [delegate tryToTerminateApplication:self];
  // Return, don't exit. The application is responsible for exiting on its own.
}
@end

@implementation SimpleAppDelegate

// Create the application on the UI thread.
- (void)createApplication:(id)object {
  [[NSBundle mainBundle] loadNibNamed:@"MainMenu"
                                owner:NSApp
                      topLevelObjects:nil];

  // Set the delegate for application events.
  [[NSApplication sharedApplication] setDelegate:self];
}

- (void)tryToTerminateApplication:(NSApplication*)app {
  simple_handler_t* handler = simple_handler_get_instance();
  if (handler && !handler->is_closing) {
    simple_handler_close_all_browsers(handler, 0);
  }
}

- (NSApplicationTerminateReply)applicationShouldTerminate:
    (NSApplication*)sender {
  return NSTerminateNow;
}

// Called when the user clicks the app dock icon while the application is
// already running.
- (BOOL)applicationShouldHandleReopen:(NSApplication*)theApplication
                    hasVisibleWindows:(BOOL)flag {
  simple_handler_t* handler = simple_handler_get_instance();
  if (handler && !handler->is_closing) {
    simple_handler_show_main_window(handler);
  }
  return NO;
}

// Requests that any state restoration archive be created with secure encoding
// (macOS 12+ only). See https://crrev.com/c737387656 for details. This also
// fixes an issue with macOS default behavior incorrectly restoring windows
// after hard reset (holding down the power button).
- (BOOL)applicationSupportsSecureRestorableState:(NSApplication*)app {
  return YES;
}
@end

// Entry point function for the browser process.
int main(int argc, char* argv[]) {
  // Load the CEF framework library at runtime instead of linking directly
  // as required by the macOS sandbox implementation.
  void* library_loader = cef_scoped_library_loader_create(/*helper=*/0);
  if (!library_loader) {
    return 1;
  }

  // Configure the CEF API version. This must be called before any other CEF
  // API functions.
  cef_api_hash(CEF_API_VERSION, 0);

  // Provide CEF with command-line arguments.
  cef_main_args_t main_args = {};
  main_args.argc = argc;
  main_args.argv = argv;

  @autoreleasepool {
    // Initialize the SimpleApplication instance.
    [SimpleApplication sharedApplication];

    // If there was an invocation to NSApp prior to this method, then the NSApp
    // will not be a SimpleApplication, but will instead be an NSApplication.
    // This is undesirable and we must enforce that this doesn't happen.
    if (![NSApp isKindOfClass:[SimpleApplication class]]) {
      return 1;
    }

    // Create the application instance.
    simple_app_t* app = simple_app_create();
    CHECK(app);

    // Specify CEF global settings here.
    cef_settings_t settings = {};
    settings.size = sizeof(cef_settings_t);

    // When generating projects with CMake the CEF_USE_SANDBOX value will be
    // defined automatically. Pass -DUSE_SANDBOX=OFF to the CMake command-line
    // to disable use of the sandbox.
#if !defined(CEF_USE_SANDBOX)
    settings.no_sandbox = 1;
#endif

    // Initialize the CEF browser process. May return false if initialization
    // fails or if early exit is desired (for example, due to process singleton
    // relaunch behavior).
    if (!cef_initialize(&main_args, &settings, &app->app, NULL)) {
      app->app.base.release(&app->app.base);
      return cef_get_exit_code();
    }

    // Create the application delegate.
    SimpleAppDelegate* delegate = [[SimpleAppDelegate alloc] init];
    // Set as the delegate for application events.
    NSApp.delegate = delegate;

    [delegate performSelectorOnMainThread:@selector(createApplication:)
                               withObject:nil
                            waitUntilDone:NO];

    // Run the CEF message loop. This will block until cef_quit_message_loop()
    // is called.
    cef_run_message_loop();

    // Shut down CEF.
    cef_shutdown();

    // Release the delegate.
#if !__has_feature(objc_arc)
    [delegate release];
#endif  // !__has_feature(objc_arc)
    delegate = nil;

    // Note: We DON'T release the app here.
    // We transferred our creation reference to CEF via cef_initialize.
    // CEF will release it during cef_shutdown().
  }  // @autoreleasepool

  // Unload the CEF framework library.
  cef_scoped_library_loader_free(library_loader);

  return 0;
}
