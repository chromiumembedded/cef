// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "browser_webview_mac.h"
#import "browser_impl.h"
#import "cef_context.h"
#import "web_drag_source_mac.h"
#import "web_drop_target_mac.h"
#import "webwidget_host.h"

#import "base/memory/scoped_ptr.h"
#import "base/string_util.h"
#import "base/sys_string_conversions.h"
#import "third_party/WebKit/Source/WebKit/chromium/public/mac/WebInputEventFactory.h"
#import "third_party/WebKit/Source/WebKit/chromium/public/mac/WebSubstringUtil.h"
#import "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#import "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#import "third_party/mozilla/NSPasteboard+Utils.h"
#import "third_party/skia/include/core/SkRegion.h"
#import "ui/gfx/rect.h"

using WebKit::WebColor;
using WebKit::WebCompositionUnderline;
using WebKit::WebInputEvent;
using WebKit::WebInputEventFactory;
using WebKit::WebKeyboardEvent;
using WebKit::WebPoint;
using WebKit::WebRect;
using WebKit::WebString;
using WebKit::WebSubstringUtil;

// This code is copied from 
// content/browser/renderer_host/render_widget_host_mac
namespace {
WebColor WebColorFromNSColor(NSColor *color) {
  CGFloat r, g, b, a;
  [color getRed:&r green:&g blue:&b alpha:&a];

  return
      std::max(0, std::min(static_cast<int>(lroundf(255.0f * a)), 255)) << 24 |
      std::max(0, std::min(static_cast<int>(lroundf(255.0f * r)), 255)) << 16 |
      std::max(0, std::min(static_cast<int>(lroundf(255.0f * g)), 255)) << 8  |
      std::max(0, std::min(static_cast<int>(lroundf(255.0f * b)), 255));
}

// Extract underline information from an attributed string. Mostly copied from
// third_party/WebKit/Source/WebKit/mac/WebView/WebHTMLView.mm
void ExtractUnderlines(
    NSAttributedString* string,
    std::vector<WebCompositionUnderline>* underlines) {
  int length = [[string string] length];
  int i = 0;
  while (i < length) {
    NSRange range;
    NSDictionary* attrs = [string attributesAtIndex:i
                              longestEffectiveRange:&range
                                            inRange:NSMakeRange(i, length - i)];
    if (NSNumber *style = [attrs objectForKey:NSUnderlineStyleAttributeName]) {
      WebColor color = SK_ColorBLACK;
      if (NSColor *colorAttr =
          [attrs objectForKey:NSUnderlineColorAttributeName]) {
        color = WebColorFromNSColor(
            [colorAttr colorUsingColorSpaceName:NSDeviceRGBColorSpace]);
      }
      underlines->push_back(WebCompositionUnderline(
          range.location, NSMaxRange(range), color, [style intValue] > 1));
    }
    i = range.location + range.length;
  }
}
} // namespace

@implementation BrowserWebView

@synthesize browser = browser_;
@synthesize in_setfocus = is_in_setfocus_;

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
#ifndef NDEBUG
  CGContextRef context = reinterpret_cast<CGContextRef>(
      [[NSGraphicsContext currentContext] graphicsPort]);
  CGContextSetRGBFillColor(context, 1, 0, 1, 1);
  CGContextFillRect(context, NSRectToCGRect(rect));
#endif

  if (browser_ && browser_->UIT_GetWebView()) {
    NSInteger count;
    const NSRect *rects;
    [self getRectsBeingDrawn:&rects count:&count];

    SkRegion update_rgn;
    for (int i = 0; i < count; i++) {
      const NSRect r = rects[i];
      const float min_x = NSMinX(r);
      const float max_x = NSMaxX(r);
      const float min_y = NSHeight([self bounds]) - NSMaxY(r);
      const float max_y = NSHeight([self bounds]) - NSMinY(r);
      update_rgn.op(min_x, min_y, max_x, max_y, SkRegion::kUnion_Op);
    }

    browser_->UIT_GetWebViewHost()->Paint(update_rgn);
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

// This code is mostly copied and adapted from
// content/browser/renderer_host/render_widget_host_mac
- (void)keyDown:(NSEvent *)theEvent {
  // Records the current marked text state, so that we can know if the marked
  // text was deleted or not after handling the key down event.
  BOOL oldHasMarkedText = hasMarkedText_;

  // We check if the marked text has one or less characters and a delete key is
  // pressed. In such cases, we want to cancel IME composition and delete the
  // marked character, so we dispatch the event directly to WebKit.
  if (hasMarkedText_ && underlines_.size() <= 1) {
    // Check for backspace or delete.
    if ([theEvent keyCode] == 0x33 || [theEvent keyCode] == 0x75)
      browser_->UIT_GetWebViewHost()->KeyEvent(theEvent);
  }

  textToBeInserted_.clear();
  markedText_.clear();
  underlines_.clear();
  unmarkTextCalled_ = NO;

  handlingKeyDown_ = YES;
  [self interpretKeyEvents:[NSArray arrayWithObject:theEvent]];
  handlingKeyDown_ = NO;

  // Only send a corresponding key press event if there is no marked text.
  // We also handle keys like backspace or delete, where the length
  // of the text to be inserted is 0.
  if (!hasMarkedText_ && !oldHasMarkedText &&
      !textToBeInserted_.length() <= 1) {
    if (textToBeInserted_.length() == 1) {
      // If a single character was inserted, then we just send it as a keypress
      // event.
      WebKeyboardEvent keyboard_event(
          WebInputEventFactory::keyboardEvent(theEvent));
      keyboard_event.type = WebInputEvent::Char;
      keyboard_event.text[0] = textToBeInserted_[0];
      keyboard_event.text[1] = 0;
      browser_->UIT_GetWebView()->handleInputEvent(keyboard_event);
    } else {
      browser_->UIT_GetWebViewHost()->KeyEvent(theEvent);
    }
  }

  // Calling KeyEvent() could have destroyed the widget.
  // We perform a sanity check and return if the widget is NULL.
  if (!browser_ || !browser_->UIT_GetWebView())
    return;

  BOOL textInserted = NO;
  if (textToBeInserted_.length() >
      ((hasMarkedText_ || oldHasMarkedText) ? 0u : 1u)) {
    browser_->UIT_GetWebView()->confirmComposition(textToBeInserted_);
    textInserted = YES;
  }

  if (hasMarkedText_ && markedText_.length()) {
    browser_->UIT_GetWebView()->setComposition(markedText_, underlines_,
                                               selectedRange_.location,
                                               NSMaxRange(selectedRange_));
  } else if (oldHasMarkedText && !hasMarkedText_ && !textInserted) {
    if (unmarkTextCalled_) {
      browser_->UIT_GetWebView()->confirmComposition();
    } else {
      // Simulating a cancelComposition
      browser_->UIT_GetWebView()->setComposition(EmptyString16(), underlines_,
                                                 0, 0);
    }
  }
}

- (void)keyUp:(NSEvent *)theEvent {
  if (browser_ && browser_->UIT_GetWebView())
    browser_->UIT_GetWebViewHost()->KeyEvent(theEvent);
}

- (void)flagsChanged:(NSEvent *)theEvent {
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
    if (!is_in_setfocus_) {
      CefRefPtr<CefClient> client = browser_->GetClient();
      if (client.get()) {
        CefRefPtr<CefFocusHandler> handler = client->GetFocusHandler();
        if (handler.get() &&
            handler->OnSetFocus(browser_, FOCUS_SOURCE_SYSTEM)) {
          return NO;
        }
      }
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
    const NSRect bounds = [self bounds];
    browser_->UIT_GetWebViewHost()->Resize(gfx::Rect(NSRectToCGRect(bounds)));
  }
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

// NSTextInputClient methods

// This code is mostly copied and adapted from
// content/browser/renderer_host/render_widget_host_mac
extern "C" {
  extern NSString *NSTextInputReplacementRangeAttributeName;
}

- (NSArray *)validAttributesForMarkedText {
  if (!validAttributesForMarkedText_) {
    validAttributesForMarkedText_.reset([[NSArray alloc] initWithObjects:
        NSUnderlineStyleAttributeName,
        NSUnderlineColorAttributeName,
        NSMarkedClauseSegmentAttributeName,
        NSTextInputReplacementRangeAttributeName,
        nil]);
  }
  return validAttributesForMarkedText_.get();
}

- (NSUInteger)characterIndexForPoint:(NSPoint)thePoint {
  DCHECK([self window]);
  // |thePoint| is in screen coordinates, but needs to be converted to WebKit
  // coordinates (upper left origin). Scroll offsets will be taken care of in
  // the renderer.
  thePoint = [[self window] convertScreenToBase:thePoint];
  thePoint = [self convertPoint:thePoint fromView:nil];
  thePoint.y = NSHeight([self frame]) - thePoint.y;

  WebPoint point(thePoint.x, thePoint.y);
  return (NSUInteger)browser_->UIT_GetWebView()->focusedFrame()->
      characterIndexForPoint(point);
}

- (NSRect)firstRectForCharacterRange:(NSRange)theRange
                         actualRange:(NSRangePointer) actualRange {
  if (actualRange)
    *actualRange = theRange;

  if (!browser_  || !browser_->UIT_GetWebView() ||
      !browser_->UIT_GetWebView()->focusedFrame()) {
    return NSMakeRect(0, 0, 0, 0);
  }

  WebRect webRect;
  browser_->UIT_GetWebView()->focusedFrame()->firstRectForCharacterRange(
      theRange.location, theRange.length, webRect);
  NSRect rect = NSMakeRect(webRect.x, webRect.y, webRect.width, webRect.height);

  // The returned rectangle is in WebKit coordinates (upper left origin), so
  // flip the coordinate system and then convert it into screen coordinates for
  // return.
  NSRect viewFrame = [self frame];
  rect.origin.y = NSHeight(viewFrame) - rect.origin.y;
  rect.origin.y -= rect.size.height;
  rect = [self convertRectToBase:rect];
  rect.origin = [[self window] convertBaseToScreen:rect.origin];
  return rect;
}

- (NSAttributedString *)attributedSubstringForProposedRange:(NSRange)theRange
    actualRange:(NSRangePointer) actualRange {
  if (actualRange)
    *actualRange = theRange;

  if (!browser_ || !browser_->UIT_GetWebView() ||
      !browser_->UIT_GetWebView()->focusedFrame()) {
    return nil;
  }

  return WebSubstringUtil::attributedSubstringInRange(
      browser_->UIT_GetWebView()->focusedFrame(),
      theRange.location,
      theRange.length);
}

- (void)doCommandBySelector:(SEL)selector {
}

- (NSRange)markedRange {
  return hasMarkedText_ ? markedRange_ : NSMakeRange(NSNotFound, 0);
}

- (NSRange)selectedRange {
  return selectedRange_;
}

- (NSInteger)conversationIdentifier {
  return reinterpret_cast<NSInteger>(self);
}

- (BOOL)hasMarkedText {
  return hasMarkedText_;
}

- (void)unmarkText {
  hasMarkedText_ = NO;
  markedText_.clear();
  underlines_.clear();

  if (!handlingKeyDown_) {
    if (browser_ && browser_->UIT_GetWebView())
      browser_->UIT_GetWebView()->confirmComposition();
  } else {
    unmarkTextCalled_ = YES;
  }
}

- (void)setMarkedText:(id)string selectedRange:(NSRange)newSelRange
    replacementRange: (NSRange) replacementRange {
  // An input method updates the composition string.
  // We send the given text and range to the renderer so it can update the
  // composition node of WebKit.
  BOOL isAttributedString = [string isKindOfClass:[NSAttributedString class]];
  NSString* im_text = isAttributedString ? [string string] : string;
  int length = [im_text length];

  // |markedRange_| will get set on a callback from ImeSetComposition().
  selectedRange_ = newSelRange;
  markedText_ = base::SysNSStringToUTF16(im_text);
  hasMarkedText_ = (length > 0);

  underlines_.clear();
  if (isAttributedString) {
    ExtractUnderlines(string, &underlines_);
  } else {
    // Use a thin black underline by default.
    underlines_.push_back(
        WebCompositionUnderline(0, length, SK_ColorBLACK, false));
  }

  // If we are handling a key down event, then SetComposition() will be
  // called in keyEvent: method.
  // Input methods of Mac use setMarkedText calls with an empty text to cancel
  // an ongoing composition. So, we should check whether or not the given text
  // is empty to update the input method state. (Our input method backend can
  // automatically cancels an ongoing composition when we send an empty text.
  // So, it is OK to send an empty text to the renderer.)
  if (!handlingKeyDown_) {
    if (browser_ && browser_->UIT_GetWebView()) {
      const WebString markedText(markedText_);
      browser_->UIT_GetWebView()->setComposition(markedText,
          underlines_,
          newSelRange.location,
          NSMaxRange(newSelRange));
    }
  }
}

- (void)insertText:(id)string replacementRange: (NSRange) replacementRange {
  BOOL isAttributedString = [string isKindOfClass:[NSAttributedString class]];
  NSString* im_text = isAttributedString ? [string string] : string;
  if (handlingKeyDown_) {
    textToBeInserted_.append(base::SysNSStringToUTF16(im_text));
  } else {
    browser_->UIT_GetWebViewHost()->webwidget()->confirmComposition(
        base::SysNSStringToUTF16(im_text));
  }

  // Inserting text will delete all marked text automatically.
  hasMarkedText_ = NO;
}

@end
