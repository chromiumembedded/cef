// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "browser_webview_mac.h"

#import <Cocoa/Cocoa.h>

#include "browser_impl.h"
#include "cef_context.h"
#include "webwidget_host.h"

#include "base/scoped_ptr.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/gfx/rect.h"

@implementation BrowserWebView

@synthesize browser = browser_;

- (id)initWithFrame:(NSRect)frame {
  self = [super initWithFrame:frame];
  if (self) {
    trackingArea_ =
    [[NSTrackingArea alloc] initWithRect:frame
                                 options:NSTrackingMouseMoved |
                                         NSTrackingActiveInActiveApp |
                                         NSTrackingInVisibleRect
                                   owner:self
                                userInfo:nil];
    [self addTrackingArea:trackingArea_];
  }
  return self;
}

- (void) dealloc {
  browser_->UIT_DestroyBrowser();

  [self removeTrackingArea:trackingArea_];
  [trackingArea_ release];
  
  [super dealloc];
}

- (void)drawRect:(NSRect)rect {
  CGContextRef context =
      reinterpret_cast<CGContextRef>([[NSGraphicsContext currentContext]
                                      graphicsPort]);

  // start by filling the rect with magenta, so that we can see what's drawn
  CGContextSetRGBFillColor (context, 1, 0, 1, 1);
  CGContextFillRect(context, NSRectToCGRect(rect));

  if (browser_ && browser_->UIT_GetWebView()) {
    gfx::Rect client_rect(NSRectToCGRect(rect));
    // flip from cocoa coordinates
    client_rect.set_y([self frame].size.height -
                      client_rect.height() - client_rect.y());

    browser_->UIT_GetWebViewHost()->UpdatePaintRect(client_rect);
    browser_->UIT_GetWebViewHost()->Paint();
  }
}

- (void)mouseDown:(NSEvent *)theEvent {
  if (browser_ && browser_->UIT_GetWebView())
    browser_->UIT_GetWebViewHost()->MouseEvent(theEvent);
}

- (void)rightMouseDown:(NSEvent *)theEvent {
  if (browser_ && browser_->UIT_GetWebView())
    browser_->UIT_GetWebViewHost()->MouseEvent(theEvent);
}

- (void)otherMouseDown:(NSEvent *)theEvent {
  if (browser_ && browser_->UIT_GetWebView())
    browser_->UIT_GetWebViewHost()->MouseEvent(theEvent);
}

- (void)mouseUp:(NSEvent *)theEvent {
  if (browser_ && browser_->UIT_GetWebView())
    browser_->UIT_GetWebViewHost()->MouseEvent(theEvent);
}

- (void)rightMouseUp:(NSEvent *)theEvent {
  if (browser_ && browser_->UIT_GetWebView())
    browser_->UIT_GetWebViewHost()->MouseEvent(theEvent);
}

- (void)otherMouseUp:(NSEvent *)theEvent {
  if (browser_ && browser_->UIT_GetWebView())
    browser_->UIT_GetWebViewHost()->MouseEvent(theEvent);
}

- (void)mouseMoved:(NSEvent *)theEvent {
  if (browser_ && browser_->UIT_GetWebView())
    browser_->UIT_GetWebViewHost()->MouseEvent(theEvent);
}

- (void)mouseDragged:(NSEvent *)theEvent {
  if (browser_ && browser_->UIT_GetWebView())
    browser_->UIT_GetWebViewHost()->MouseEvent(theEvent);
}

- (void)scrollWheel:(NSEvent *)theEvent {
  if (browser_ && browser_->UIT_GetWebView())
    browser_->UIT_GetWebViewHost()->WheelEvent(theEvent);
}

- (void)rightMouseDragged:(NSEvent *)theEvent {
  if (browser_ && browser_->UIT_GetWebView())
    browser_->UIT_GetWebViewHost()->MouseEvent(theEvent);
}

- (void)otherMouseDragged:(NSEvent *)theEvent {
  if (browser_ && browser_->UIT_GetWebView())
    browser_->UIT_GetWebViewHost()->MouseEvent(theEvent);
}

- (void)mouseEntered:(NSEvent *)theEvent {
  if (browser_ && browser_->UIT_GetWebView())
    browser_->UIT_GetWebViewHost()->MouseEvent(theEvent);
}

- (void)mouseExited:(NSEvent *)theEvent {
  if (browser_ && browser_->UIT_GetWebView())
    browser_->UIT_GetWebViewHost()->MouseEvent(theEvent);
}

- (void)keyDown:(NSEvent *)theEvent {
  if (browser_ && browser_->UIT_GetWebView())
    browser_->UIT_GetWebViewHost()->KeyEvent(theEvent);
}

- (void)keyUp:(NSEvent *)theEvent {
  if (browser_ && browser_->UIT_GetWebView())
    browser_->UIT_GetWebViewHost()->KeyEvent(theEvent);
}

- (BOOL)isOpaque {
  return YES;
}

- (BOOL)canBecomeKeyView {
  return browser_ && browser_->UIT_GetWebView();
}

- (BOOL)acceptsFirstResponder {
  return browser_ && browser_->UIT_GetWebView();
}

- (BOOL)becomeFirstResponder {
  if (browser_ && browser_->UIT_GetWebView()) {
    browser_->UIT_GetWebViewHost()->SetFocus(YES);
    return YES;
  }

  return NO;
}

- (BOOL)resignFirstResponder {
  if (browser_ && browser_->UIT_GetWebView()) {
    browser_->UIT_GetWebViewHost()->SetFocus(NO);
    return YES;
  }

  return NO;
}

- (void)setFrame:(NSRect)frameRect {
  [super setFrame:frameRect];
  if (browser_ && browser_->UIT_GetWebView())
    browser_->UIT_GetWebViewHost()->Resize(gfx::Rect(NSRectToCGRect(frameRect)));
  [self setNeedsDisplay:YES];
}

@end
