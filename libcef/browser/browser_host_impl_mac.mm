// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/browser_host_impl.h"

#import <Cocoa/Cocoa.h>

#include "content/public/browser/web_contents_view.h"
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
    browser_->DestroyBrowser();
    browser_->Release();
  }

  [super dealloc];
}

@end


// Common base class for CEF browser windows. Contains methods relating to hole
// punching required in order to display OpenGL underlay windows.
@interface CefBrowserWindow : NSWindow {
@private
  int underlaySurfaceCount_;
}

// Informs the window that an underlay surface has been added/removed. The
// window is non-opaque while underlay surfaces are present.
- (void)underlaySurfaceAdded;
- (void)underlaySurfaceRemoved;

@end

@implementation CefBrowserWindow

- (void)underlaySurfaceAdded {
  DCHECK_GE(underlaySurfaceCount_, 0);
  ++underlaySurfaceCount_;

  // We're having the OpenGL surface render under the window, so the window
  // needs to be not opaque.
  if (underlaySurfaceCount_ == 1)
    [self setOpaque:NO];
}

- (void)underlaySurfaceRemoved {
  --underlaySurfaceCount_;
  DCHECK_GE(underlaySurfaceCount_, 0);

  if (underlaySurfaceCount_ == 0)
    [self setOpaque:YES];
}

@end


bool CefBrowserHostImpl::PlatformViewText(const std::string& text) {
  NOTIMPLEMENTED();
  return false;
}

bool CefBrowserHostImpl::PlatformCreateWindow() {
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

    newWnd = [[CefBrowserWindow alloc]
              initWithContentRect:window_rect
              styleMask:(NSTitledWindowMask |
                         NSClosableWindowMask |
                         NSMiniaturizableWindowMask |
                         NSResizableWindowMask |
                         NSUnifiedTitleAndToolbarWindowMask )
              backing:NSBackingStoreBuffered
              defer:NO];
    parentView = [newWnd contentView];
    window_info_.parent_view = parentView;
  }

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
  NSView* native_view = tab_contents_->GetView()->GetNativeView();
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
    [[window_info_.view window] performSelector:@selector(performClose:)
                                     withObject:nil
                                     afterDelay:0];
  }
}

void CefBrowserHostImpl::PlatformSizeTo(int width, int height) {
  // Not needed; subviews are bound.
}

CefWindowHandle CefBrowserHostImpl::PlatformGetWindowHandle() {
  return window_info_.view;
}
