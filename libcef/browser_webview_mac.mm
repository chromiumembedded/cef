// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "browser_webview_mac.h"

#import <Cocoa/Cocoa.h>

#include "browser_impl.h"
#include "cef_context.h"
#import "web_drag_source_mac.h"
#import "web_drop_target_mac.h"
#include "webwidget_host.h"

#include "base/memory/scoped_ptr.h"
#import "third_party/mozilla/NSPasteboard+Utils.h"
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
  if (browser_)
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
    CefRefPtr<CefClient> client = browser_->GetClient();
    if (client.get()) {
      CefRefPtr<CefFocusHandler> handler = client->GetFocusHandler();
      if (handler.get() && handler->OnSetFocus(browser_, false))
        return NO;
    }

    browser_->UIT_GetWebViewHost()->SetFocus(YES);
    return [super becomeFirstResponder];
  }

  return NO;
}

- (BOOL)resignFirstResponder {
  if (browser_ && browser_->UIT_GetWebView()) {
    browser_->UIT_GetWebViewHost()->SetFocus(NO);
    return [super resignFirstResponder];
  }

  return NO;
}

- (void)setFrame:(NSRect)frameRect {
  [super setFrame:frameRect];
  if (browser_ && browser_->UIT_GetWebView()) {
    browser_->UIT_GetWebViewHost()->Resize(
        gfx::Rect(NSRectToCGRect(frameRect)));
  }
  [self setNeedsDisplay:YES];
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

- (void)registerDragDrop {
  dropTarget_.reset([[WebDropTarget alloc] initWithWebView:self]);

  // Register the view to handle the appropriate drag types.
  NSArray* types = [NSArray arrayWithObjects:NSStringPboardType,
                    NSHTMLPboardType, NSURLPboardType, nil];
  [self registerForDraggedTypes:types]; 
}

- (void)startDragWithDropData:(const WebDropData&)dropData
            dragOperationMask:(NSDragOperation)operationMask
                        image:(NSImage*)image
                       offset:(NSPoint)offset {
  dragSource_.reset([[WebDragSource alloc]
                     initWithWebView:self
                     dropData:&dropData
                     image:image
                     offset:offset
                     pasteboard:[NSPasteboard pasteboardWithName:NSDragPboard]
                     dragOperationMask:operationMask]);
  [dragSource_ startDrag];
}

// NSPasteboardOwner methods

- (void)pasteboard:(NSPasteboard*)sender provideDataForType:(NSString*)type {
  [dragSource_ lazyWriteToPasteboard:sender
                             forType:type];
}

// NSDraggingSource methods

// Returns what kind of drag operations are available. This is a required
// method for NSDraggingSource.
- (NSDragOperation)draggingSourceOperationMaskForLocal:(BOOL)isLocal {
  if (dragSource_.get())
    return [dragSource_ draggingSourceOperationMaskForLocal:isLocal];
  // No web drag source - this is the case for dragging a file from the
  // downloads manager. Default to copy operation. Note: It is desirable to
  // allow the user to either move or copy, but this requires additional
  // plumbing to update the download item's path once its moved.
  return NSDragOperationCopy;
}

// Called when a drag initiated in our view ends.
- (void)draggedImage:(NSImage*)anImage
             endedAt:(NSPoint)screenPoint
           operation:(NSDragOperation)operation {
  [dragSource_ endDragAt:screenPoint operation:operation];

  // Might as well throw out this object now.
  dragSource_.reset();
}

// Called when a drag initiated in our view moves.
- (void)draggedImage:(NSImage*)draggedImage movedTo:(NSPoint)screenPoint {
  [dragSource_ moveDragTo:screenPoint];
}

// Called when we're informed where a file should be dropped.
- (NSArray*)namesOfPromisedFilesDroppedAtDestination:(NSURL*)dropDest {
  if (![dropDest isFileURL])
    return nil;

  NSString* file_name = [dragSource_ dragPromisedFileTo:[dropDest path]];
  if (!file_name)
    return nil;

  return [NSArray arrayWithObject:file_name];
}

// NSDraggingDestination methods

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender {
  return [dropTarget_ draggingEntered:sender view:self];
}

- (void)draggingExited:(id<NSDraggingInfo>)sender {
  [dropTarget_ draggingExited:sender];
}

- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)sender {
  return [dropTarget_ draggingUpdated:sender view:self];
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender {
  return [dropTarget_ performDragOperation:sender view:self];
}

@end
