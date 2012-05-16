// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_WEBVIEW_MAC_H_
#define CEF_LIBCEF_BROWSER_WEBVIEW_MAC_H_
#pragma once

#import <Cocoa/Cocoa.h>
#import "base/mac/cocoa_protocols.h"
#include "base/memory/scoped_nsobject.h"

class CefBrowserImpl;
@class WebDragSource;
@class WebDropTarget;
struct WebDropData;

// A view to wrap the WebCore view and help it live in a Cocoa world. The
// (rough) equivalent of Apple's WebView.

@interface BrowserWebView : NSView {
 @private
  CefBrowserImpl* browser_;  // weak
  NSTrackingArea* trackingArea_;
  bool is_in_setfocus_;

  scoped_nsobject<WebDragSource> dragSource_;
  scoped_nsobject<WebDropTarget> dropTarget_;
}

- (void)mouseDown:(NSEvent *)theEvent;
- (void)rightMouseDown:(NSEvent *)theEvent;
- (void)otherMouseDown:(NSEvent *)theEvent;
- (void)mouseUp:(NSEvent *)theEvent;
- (void)rightMouseUp:(NSEvent *)theEvent;
- (void)otherMouseUp:(NSEvent *)theEvent;
- (void)mouseMoved:(NSEvent *)theEvent;
- (void)mouseDragged:(NSEvent *)theEvent;
- (void)scrollWheel:(NSEvent *)theEvent;
- (void)rightMouseDragged:(NSEvent *)theEvent;
- (void)otherMouseDragged:(NSEvent *)theEvent;
- (void)mouseEntered:(NSEvent *)theEvent;
- (void)mouseExited:(NSEvent *)theEvent;
- (void)keyDown:(NSEvent *)theEvent;
- (void)keyUp:(NSEvent *)theEvent;
- (BOOL)isOpaque;
- (void)setFrame:(NSRect)frameRect;

// Register this WebView as a drag/drop target.
- (void)registerDragDrop;

// Called from BrowserWebViewDelegate::startDragging() to initiate dragging.
- (void)startDragWithDropData:(const WebDropData&)dropData
            dragOperationMask:(NSDragOperation)operationMask
                        image:(NSImage*)image
                       offset:(NSPoint)offset;

@property (nonatomic, assign) CefBrowserImpl* browser;
@property (nonatomic, assign) bool in_setfocus;

@end

#endif  // CEF_LIBCEF_BROWSER_WEBVIEW_MAC_H_
