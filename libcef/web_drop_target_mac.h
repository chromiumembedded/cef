// Copyright (c) 2011 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/string16.h"

class GURL;
struct WebDropData;
@class BrowserWebView;
class WebViewHost;

// A class that handles tracking and event processing for a drag and drop
// over the content area. Assumes something else initiates the drag, this is
// only for processing during a drag.

@interface WebDropTarget : NSObject {
 @private
  // Our associated WebView. Weak reference.
  BrowserWebView* view_;

  // Keep track of the WebViewHost we're dragging over.  If it changes during a
  // drag, we need to re-send the DragEnter message.
  WebViewHost* current_wvh_;

  // True if the drag has been canceled.
  bool canceled_;
}

// |view| is the WebView representing this browser window, used to communicate
// drag&drop messages to WebCore and handle navigation on a successful drop
// (if necessary).
- (id)initWithWebView:(BrowserWebView*)view;

// Messages to send during the tracking of a drag, ususally upon receiving
// calls from the view system. Communicates the drag messages to WebCore.
- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)info
                              view:(NSView*)view;
- (void)draggingExited:(id<NSDraggingInfo>)info;
- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)info
                              view:(NSView*)view;
- (BOOL)performDragOperation:(id<NSDraggingInfo>)info
                              view:(NSView*)view;

@end

// Public use only for unit tests.
@interface WebDropTarget(Testing)
// Given |data|, which should not be nil, fill it in using the contents of the
// given pasteboard.
- (void)populateWebDropData:(WebDropData*)data
             fromPasteboard:(NSPasteboard*)pboard;
// Given a point in window coordinates and a view in that window, return a
// flipped point in the coordinate system of |view|.
- (NSPoint)flipWindowPointToView:(const NSPoint&)windowPoint
                            view:(NSView*)view;
// Given a point in window coordinates and a view in that window, return a
// flipped point in screen coordinates.
- (NSPoint)flipWindowPointToScreen:(const NSPoint&)windowPoint
                              view:(NSView*)view;
@end
