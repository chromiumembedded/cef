// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/osr/text_input_client_osr_mac.h"

#include "libcef/browser/browser_host_impl.h"

#include "base/numerics/safe_conversions.h"
#include "base/strings/sys_string_conversions.h"
#import "content/browser/renderer_host/render_widget_host_view_mac_editcommand_helper.h"
#import "content/browser/renderer_host/text_input_client_mac.h"
#include "content/common/input_messages.h"

namespace {

// TODO(suzhe): Upstream this function.
blink::WebColor WebColorFromNSColor(NSColor *color) {
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
void ExtractUnderlines(NSAttributedString* string,
    std::vector<blink::WebCompositionUnderline>* underlines) {
  int length = [[string string] length];
  int i = 0;
  while (i < length) {
    NSRange range;
    NSDictionary* attrs = [string attributesAtIndex:i
        longestEffectiveRange:&range
        inRange:NSMakeRange(i, length - i)];
    NSNumber *style = [attrs objectForKey: NSUnderlineStyleAttributeName];
    if (style) {
      blink::WebColor color = SK_ColorBLACK;
      if (NSColor *colorAttr =
          [attrs objectForKey:NSUnderlineColorAttributeName]) {
        color = WebColorFromNSColor(
            [colorAttr colorUsingColorSpaceName:NSDeviceRGBColorSpace]);
      }
      underlines->push_back(blink::WebCompositionUnderline(
          range.location, NSMaxRange(range), color, [style intValue] > 1, 0));
    }
    i = range.location + range.length;
  }
}

}  // namespace

extern "C" {
  extern NSString* NSTextInputReplacementRangeAttributeName;
}

@implementation CefTextInputClientOSRMac

@synthesize selectedRange = selectedRange_;
@synthesize handlingKeyDown = handlingKeyDown_;

- (id)initWithRenderWidgetHostViewOSR:(CefRenderWidgetHostViewOSR*)rwhv {
  self = [super init];
  renderWidgetHostView_ = rwhv;

  return self;
}

- (NSArray*)validAttributesForMarkedText {
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

- (NSRange)markedRange {
  return hasMarkedText_ ?
      renderWidgetHostView_->composition_range().ToNSRange() :
      NSMakeRange(NSNotFound, 0);
}

- (BOOL)hasMarkedText {
  return hasMarkedText_;
}

- (void)insertText:(id)aString replacementRange:(NSRange)replacementRange {
  BOOL isAttributedString = [aString isKindOfClass:[NSAttributedString class]];
  NSString* im_text = isAttributedString ? [aString string] : aString;
  if (handlingKeyDown_) {
    textToBeInserted_.append(base::SysNSStringToUTF16(im_text));
  } else {
    gfx::Range replacement_range(replacementRange);

    renderWidgetHostView_->render_widget_host()->ImeConfirmComposition(
        base::SysNSStringToUTF16(im_text), replacement_range, false);
  }

  // Inserting text will delete all marked text automatically.
  hasMarkedText_ = NO;
}

- (void)doCommandBySelector:(SEL)aSelector {
  // An input method calls this function to dispatch an editing command to be
  // handled by this view.
  if (aSelector == @selector(noop:))
    return;
  std::string command([content::RenderWidgetHostViewMacEditCommandHelper::
                      CommandNameForSelector(aSelector) UTF8String]);

  // If this method is called when handling a key down event, then we need to
  // handle the command in the key event handler. Otherwise we can just handle
  // it here.
  if (handlingKeyDown_) {
    hasEditCommands_ = YES;
    // We ignore commands that insert characters, because this was causing
    // strange behavior (e.g. tab always inserted a tab rather than moving to
    // the next field on the page).
    if (!base::StartsWith(command, "insert", base::CompareCase::SENSITIVE))
      editCommands_.push_back(content::EditCommand(command, ""));
  } else {
    renderWidgetHostView_->render_widget_host()->Send(
        new InputMsg_ExecuteEditCommand(
            renderWidgetHostView_->render_widget_host()->GetRoutingID(),
            command, ""));
  }
}
    
- (void)setMarkedText:(id)aString selectedRange:(NSRange)newSelRange
                      replacementRange:(NSRange)replacementRange {
  // An input method updates the composition string.
  // We send the given text and range to the renderer so it can update the
  // composition node of WebKit.

  BOOL isAttributedString = [aString isKindOfClass:[NSAttributedString class]];
  NSString* im_text = isAttributedString ? [aString string] : aString;
  int length = [im_text length];

  // |markedRange_| will get set on a callback from ImeSetComposition().
  selectedRange_ = newSelRange;
  markedText_ = base::SysNSStringToUTF16(im_text);
  hasMarkedText_ = (length > 0);
  underlines_.clear();

  if (isAttributedString) {
    ExtractUnderlines(aString, &underlines_);
  } else {
    // Use a thin black underline by default.
    underlines_.push_back(blink::WebCompositionUnderline(0, length,
        SK_ColorBLACK, false, 0));
  }

  // If we are handling a key down event, then SetComposition() will be
  // called in keyEvent: method.
  // Input methods of Mac use setMarkedText calls with an empty text to cancel
  // an ongoing composition. So, we should check whether or not the given text
  // is empty to update the input method state. (Our input method backend can
  // automatically cancels an ongoing composition when we send an empty text.
  // So, it is OK to send an empty text to the renderer.)
  if (!handlingKeyDown_) {
    renderWidgetHostView_->render_widget_host()->ImeSetComposition(
        markedText_, underlines_, newSelRange.location,
        NSMaxRange(newSelRange));
  }
}

- (void)unmarkText {
  // Delete the composition node of the renderer and finish an ongoing
  // composition.
  // It seems an input method calls the setMarkedText method and set an empty
  // text when it cancels an ongoing composition, i.e. I have never seen an
  // input method calls this method.
  hasMarkedText_ = NO;
  markedText_.clear();
  underlines_.clear();

  // If we are handling a key down event, then ConfirmComposition() will be
  // called in keyEvent: method.
  if (!handlingKeyDown_) {
    renderWidgetHostView_->render_widget_host()->ImeConfirmComposition(
        base::string16(), gfx::Range::InvalidRange(), false);
  } else {
    unmarkTextCalled_ = YES;
  }
}

- (NSAttributedString *)attributedSubstringForProposedRange:(NSRange)range
    actualRange:(NSRangePointer)actualRange {
  if (actualRange)
    *actualRange = range;

  const gfx::Range requested_range(range);
  if (requested_range.is_reversed())
    return nil;

  gfx::Range expected_range;
  const base::string16* expected_text;

  if (!renderWidgetHostView_->composition_range().is_empty()) {
    expected_text = &markedText_;
    expected_range = renderWidgetHostView_->composition_range();
  } else {
    expected_text = &renderWidgetHostView_->selection_text();
    size_t offset = renderWidgetHostView_->selection_text_offset();
    expected_range = gfx::Range(offset, offset + expected_text->size());
  }

  if (!expected_range.Contains(requested_range))
    return nil;

  // Gets the raw bytes to avoid unnecessary string copies for generating
  // NSString.
  const base::char16* bytes =
      &(*expected_text)[requested_range.start() - expected_range.start()];
  // Avoid integer overflow.
  base::CheckedNumeric<size_t> requested_len = requested_range.length();
  requested_len *= sizeof(base::char16);
  NSUInteger bytes_len = base::strict_cast<NSUInteger, size_t>(
      requested_len.ValueOrDefault(0));
  base::scoped_nsobject<NSString> ns_string(
      [[NSString alloc] initWithBytes:bytes
                               length:bytes_len
                             encoding:NSUTF16LittleEndianStringEncoding]);
  return [[[NSAttributedString alloc] initWithString:ns_string] autorelease];
}

- (NSRect)firstViewRectForCharacterRange:(NSRange)theRange
    actualRange:(NSRangePointer)actualRange {
  NSRect rect;
  gfx::Rect gfxRect;
  gfx::Range range(theRange);
  gfx::Range actual_range;
  if (!renderWidgetHostView_->GetCachedFirstRectForCharacterRange(range,
      &gfxRect, &actual_range)) {
    rect = content::TextInputClientMac::GetInstance()->
        GetFirstRectForRange(renderWidgetHostView_->GetRenderWidgetHost(),
                             range.ToNSRange());

    if (actualRange)
      *actualRange = range.ToNSRange();
  } else {
    rect = NSRectFromCGRect(gfxRect.ToCGRect());
  }

  return rect;
}

- (NSRect) screenRectFromViewRect:(NSRect)rect {
  NSRect screenRect;

  int screenX, screenY;
  renderWidgetHostView_->browser_impl()->GetClient()->GetRenderHandler()->
      GetScreenPoint(renderWidgetHostView_->browser_impl()->GetBrowser(),
                     rect.origin.x, rect.origin.y, screenX, screenY);
  screenRect.origin = NSMakePoint(screenX, screenY);
  screenRect.size = rect.size;

  return screenRect;
}

- (NSRect)firstRectForCharacterRange:(NSRange)theRange
                         actualRange:(NSRangePointer)actualRange {
  NSRect rect = [self firstViewRectForCharacterRange:theRange
                    actualRange:actualRange];

  // Convert into screen coordinates for return.
  rect = [self screenRectFromViewRect:rect];

  if (rect.origin.y >= rect.size.height)
    rect.origin.y -= rect.size.height;
  else
    rect.origin.y = 0;

  return rect;
}

- (NSUInteger)characterIndexForPoint:(NSPoint)thePoint {
  // |thePoint| is in screen coordinates, but needs to be converted to WebKit
  // coordinates (upper left origin). Scroll offsets will be taken care of in
  // the renderer.

  CefRect view_rect;
  renderWidgetHostView_->browser_impl()->GetClient()->GetRenderHandler()->
      GetViewRect(renderWidgetHostView_->browser_impl()->GetBrowser(),
                  view_rect);

  thePoint.x -= view_rect.x;
  thePoint.y -= view_rect.y;
  thePoint.y = view_rect.height - thePoint.y;

  NSUInteger index = content::TextInputClientMac::GetInstance()->
      GetCharacterIndexAtPoint(renderWidgetHostView_->GetRenderWidgetHost(),
                               gfx::Point(thePoint.x, thePoint.y));
  return index;
}

- (void)HandleKeyEventBeforeTextInputClient:(NSEvent*)keyEvent {
  DCHECK([keyEvent type] == NSKeyDown);
  // Don't call this method recursively.
  DCHECK(!handlingKeyDown_);

  oldHasMarkedText_ = hasMarkedText_;
  handlingKeyDown_ = YES;

  // These variables might be set when handling the keyboard event.
  // Clear them here so that we can know whether they have changed afterwards.
  textToBeInserted_.clear();
  markedText_.clear();
  underlines_.clear();
  unmarkTextCalled_ = NO;
  hasEditCommands_ = NO;
  editCommands_.clear();
}

- (void)HandleKeyEventAfterTextInputClient:(NSEvent*)keyEvent {
  handlingKeyDown_ = NO;

  // Then send keypress and/or composition related events.
  // If there was a marked text or the text to be inserted is longer than 1
  // character, then we send the text by calling ConfirmComposition().
  // Otherwise, if the text to be inserted only contains 1 character, then we
  // can just send a keypress event which is fabricated by changing the type of
  // the keydown event, so that we can retain all necessary informations, such
  // as unmodifiedText, etc. And we need to set event.skip_in_browser to true to
  // prevent the browser from handling it again.
  // Note that, |textToBeInserted_| is a UTF-16 string, but it's fine to only
  // handle BMP characters here, as we can always insert non-BMP characters as
  // text.

  if (!hasMarkedText_ && !oldHasMarkedText_ &&
      textToBeInserted_.length() <= 1) {
    content::NativeWebKeyboardEvent event(keyEvent);
    if (textToBeInserted_.length() == 1) {
      event.type = blink::WebInputEvent::Type::Char;
      event.text[0] = textToBeInserted_[0];
      event.text[1] = 0;
    }
    renderWidgetHostView_->SendKeyEvent(event);
  }

  BOOL textInserted = NO;
  if (textToBeInserted_.length() >
    ((hasMarkedText_ || oldHasMarkedText_) ? 0u : 1u)) {
    renderWidgetHostView_->render_widget_host()->ImeConfirmComposition(
       textToBeInserted_, gfx::Range::InvalidRange(), false);
    textToBeInserted_ = YES;
  }

  // Updates or cancels the composition. If some text has been inserted, then
  // we don't need to cancel the composition explicitly.
  if (hasMarkedText_ && markedText_.length()) {
    // Sends the updated marked text to the renderer so it can update the
    // composition node in WebKit.
    // When marked text is available, |selectedRange_| will be the range being
    // selected inside the marked text.
    renderWidgetHostView_->render_widget_host()->ImeSetComposition(
        markedText_, underlines_, selectedRange_.location,
        NSMaxRange(selectedRange_));
  } else if (oldHasMarkedText_ && !hasMarkedText_ && !textInserted) {
    if (unmarkTextCalled_) {
      renderWidgetHostView_->render_widget_host()->ImeConfirmComposition(
          base::string16(), gfx::Range::InvalidRange(), false);
    } else {
      renderWidgetHostView_->render_widget_host()->ImeCancelComposition();
    }
  }
}

- (void)cancelComposition {
  if (!hasMarkedText_)
    return;

  // Cancel the ongoing composition. [NSInputManager markedTextAbandoned:]
  // doesn't call any NSTextInput functions, such as setMarkedText or
  // insertText. So, we need to send an IPC message to a renderer so it can
  // delete the composition node.
  // TODO(erikchen): NSInputManager is deprecated since OSX 10.6. Switch to
  // NSTextInputContext. http://www.crbug.com/479010.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  NSInputManager *currentInputManager = [NSInputManager currentInputManager];
  [currentInputManager markedTextAbandoned:self];
#pragma clang diagnostic pop

  hasMarkedText_ = NO;
  // Should not call [self unmarkText] here, because it'll send unnecessary
  // cancel composition IPC message to the renderer.
}

@end
