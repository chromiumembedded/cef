// Copyright (c) 2008-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/compiler_specific.h"
#include "third_party/WebKit/Source/WebCore/config.h"
MSVC_PUSH_WARNING_LEVEL(0);
#include "KeyEventCocoa.h"  // NOLINT(build/include)
MSVC_POP_WARNING();

#undef LOG
#import "libcef/webwidget_host.h"
#include "libcef/cef_thread.h"

#include "base/bind.h"
#import "base/logging.h"
#import "skia/ext/platform_canvas.h"
#import "third_party/WebKit/Source/WebKit/chromium/public/mac/WebInputEventFactory.h"
#import "third_party/WebKit/Source/WebKit/chromium/public/mac/WebScreenInfoFactory.h"
#import "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#import "third_party/WebKit/Source/WebKit/chromium/public/WebPopupMenu.h"
#import "third_party/WebKit/Source/WebKit/chromium/public/WebScreenInfo.h"
#import "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#import "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSize.h"
#import "third_party/skia/include/core/SkRegion.h"
#import "ui/gfx/rect.h"
#import "ui/gfx/size.h"
#import "webkit/glue/webkit_glue.h"

using webkit::npapi::WebPluginGeometry;
using WebKit::WebInputEvent;
using WebKit::WebInputEventFactory;
using WebKit::WebKeyboardEvent;
using WebKit::WebMouseEvent;
using WebKit::WebMouseWheelEvent;
using WebKit::WebPopupMenu;
using WebKit::WebScreenInfo;
using WebKit::WebScreenInfoFactory;
using WebKit::WebSize;
using WebKit::WebWidgetClient;

namespace {

inline SkIRect convertToSkiaRect(const gfx::Rect& r) {
  return SkIRect::MakeLTRB(r.x(), r.y(), r.right(), r.bottom());
}

inline gfx::Rect convertFromSkiaRect(const SkIRect& r) {
  return gfx::Rect(r.x(), r.y(), r.width(), r.height());
}

}  // namespace

/*static*/
WebWidgetHost* WebWidgetHost::Create(NSView* parent_view,
                                     WebWidgetClient* client,
                                     PaintDelegate* paint_delegate) {
  WebWidgetHost* host = new WebWidgetHost();

  if (!paint_delegate) {
    const NSRect bounds = [parent_view bounds];
    host->view_ = [[NSView alloc] initWithFrame:bounds];
    [parent_view addSubview:host->view_];

    host->webwidget_ = WebPopupMenu::create(client);
    host->webwidget_->resize(WebSize(NSWidth(bounds), NSHeight(bounds)));
  } else {
    host->paint_delegate_ = paint_delegate;
    host->view_ = nil;
    host->webwidget_ = WebPopupMenu::create(client);
  }

  return host;
}

WebWidgetHost::WebWidgetHost()
    : view_(NULL),
      paint_delegate_(NULL),
      webwidget_(NULL),
      canvas_w_(0),
      canvas_h_(0),
      popup_(false),
      frame_delay_(1000 / kDefaultFrameRate),
      mouse_modifiers_(0),
      painting_(false),
      layouting_(false) {
  set_painting(false);
}

WebWidgetHost::~WebWidgetHost() {
}

void WebWidgetHost::DidInvalidateRect(const gfx::Rect& damaged_rect) {
  int width, height;
  GetSize(width, height);
  const gfx::Rect client_rect(width, height);

  const gfx::Rect damaged_rect_in_client = client_rect.Intersect(damaged_rect);
  if (damaged_rect_in_client.IsEmpty())
    return;

  UpdatePaintRect(damaged_rect_in_client);

  if (view_) {
    NSRect cocoa_rect = NSRectFromCGRect(damaged_rect_in_client.ToCGRect());
    cocoa_rect.origin.y = client_rect.height() - NSMaxY(cocoa_rect);
    [view_ setNeedsDisplayInRect:cocoa_rect];
  } else {
    SchedulePaintTimer();
  }
}

void WebWidgetHost::DidScrollRect(int dx, int dy, const gfx::Rect& clip_rect) {
  DCHECK(dx || dy);

  int width, height;
  GetSize(width, height);
  const gfx::Rect client_rect(width, height);
  gfx::Rect rect = clip_rect.Intersect(client_rect);

  const int x = rect.x();
  const int y = rect.y();
  const int r = rect.right();
  const int b = rect.bottom();
  const int w = rect.width();
  const int h = rect.height();
  const int Dx = ABS(dx);
  const int Dy = ABS(dy);

  // If we're in a state right now where we cannot draw into the view then just
  // mark the scrolling rect as dirty and it will be completely redrawn instead.
  // The Paint() method can end up calling this method indirectly if the view
  // needs to be laid out; calling scrollRect:by: in this situation leads to
  // unwanted behavior. Finally, scrolling the rectangle by more than the size
  // of the view means we can just invalidate the entire scroll rect.
  if (!view_ || [view_ canDraw] || painting_ || layouting_ || Dx >= w ||
      Dy >= h) {
    DidInvalidateRect(clip_rect);
    return;
  }

  // The scrolling rect must not scroll outside the clip rect because that will
  // clobber the scrollbars. As a result we shorten the rectangle a bit from the
  // leading side of the scroll (could be either horizontally or vertically).
  rect = gfx::Rect(dx>=0? x: x + Dx, dy>=0? y: y + Dy, w - Dx, h - Dy);

  // Convert the scroll rectangle to the view's coordinate system and perform
  // the scroll directly without invalidating the view. In theory this could
  // cause some kind of performance issue since we're not coalescing redraw
  // events. In practice, however, we get much smoother scrolling of big views
  // since just copying the pixels within the window is much faster than
  // redrawing them.
  NSRect cocoa_rect = NSRectFromCGRect(rect.ToCGRect());
  cocoa_rect.origin.y = client_rect.height() - NSMaxY(cocoa_rect);
  [view_ scrollRect:cocoa_rect by:NSMakeSize(dx, -dy)];

  // Repaint the rectangle that was revealed when scrolling the given rectangle.
  // The invalidated area will be painted before the view contents are flushed
  // to the screen buffer.
  rect = gfx::Rect(dx>=0? x: r - Dx, dy>=0? y: b - Dy,
                   dx>0? Dx: w, dy>0? Dy: h);
  DidInvalidateRect(rect);

  // If any part of the scrolled rect was marked as dirty make sure to redraw
  // it in the new scrolled-to location. Otherwise we can end up with artifacts
  // for overlapping elements. If there are multiple dirty regions in the scroll
  // rect the rectangle union of those regions will be redrawn.
  SkRegion moved_paint_rgn(paint_rgn_);
  moved_paint_rgn.translate(dx, dy);
  moved_paint_rgn.op(convertToSkiaRect(client_rect), SkRegion::kIntersect_Op);
  DidInvalidateRect(convertFromSkiaRect(moved_paint_rgn.getBounds()));
}

void WebWidgetHost::Paint(SkRegion& update_rgn) {
  int width, height;
  GetSize(width, height);
  gfx::Rect client_rect(width, height);

  SkRegion damaged_rgn;
  if (!view_ && !redraw_rect_.IsEmpty()) {
    // At a minimum we need to send the delegate the rectangle that was
    // requested by calling CefBrowser::InvalidateRect().
    damaged_rgn.setRect(convertToSkiaRect(redraw_rect_));
    redraw_rect_ = gfx::Rect();
  }

  if (view_) {
    // Union the rectangle that WebKit think needs repainting with the rectangle
    // of the view that must be painted now. In most situations this will not
    // affect the painted rectangle. In some situations we could end up re-
    // painting areas that are already in the canvas and only dirty in the view
    // itself. However, if we don't do this we can get artifacts when scrolling
    // because contents of the canvas are no longer correct after scrolling only
    // in the view.
    paint_rgn_.op(update_rgn, SkRegion::kUnion_Op);

    // When we are not using accelerated compositing the canvas area is allowed
    // to differ in size from the client by a certain number of pixels (128 in
    // this case). When accelerated compositing is in effect the size must match
    // exactly.
    const int extra_w = (webwidget_->isAcceleratedCompositingActive()? 0: 128);
    const int extra_h = (webwidget_->isAcceleratedCompositingActive()? 0: 128);
    const int min_w = client_rect.width();
    const int min_h = client_rect.height();
    const int max_w = client_rect.width()  + extra_w * 2;
    const int max_h = client_rect.height() + extra_h * 2;

    const bool too_small = (canvas_w_ < min_w || canvas_h_ < min_h);
    const bool too_large = (canvas_w_ > max_w || canvas_h_ > max_h);

    if (!canvas_.get() || too_small || too_large) {
      canvas_w_ = client_rect.width()  + extra_w;
      canvas_h_ = client_rect.height() + extra_h;
      canvas_.reset(new skia::PlatformCanvas(canvas_w_, canvas_h_, true));
      paint_rgn_.setRect(convertToSkiaRect(client_rect));
    }
  } else if (!canvas_.get() || canvas_w_ != client_rect.width() ||
             canvas_h_ != client_rect.height()) {
    paint_rgn_.setRect(convertToSkiaRect(client_rect));

    // The canvas must be the exact size of the client area.
    canvas_w_ = client_rect.width();
    canvas_h_ = client_rect.height();
    canvas_.reset(new skia::PlatformCanvas(canvas_w_, canvas_h_, true));
  }

  webwidget_->animate(0.0);

  // Layout any views that have not been laid out yet. The layout may result in
  // more invalidation. Keep track of the fact that we are laying out views,
  // because this will sometimes cause ScrollRect to be called and we don't want
  // to try to scrollRect:by: then.
  layouting_ = true;
  webwidget_->layout();
  layouting_ = false;

  // Draw into the graphics context of the canvas instead of the view's context.
  // The view's context is pushed onto the context stack to be restored below.
  CGContextRef bitmap = skia::GetBitmapContext(skia::GetTopDevice(*canvas_));
  NSGraphicsContext* paint_context =
      [NSGraphicsContext graphicsContextWithGraphicsPort:bitmap flipped:YES];
  [NSGraphicsContext saveGraphicsState];
  [NSGraphicsContext setCurrentContext:paint_context];

  // Paint the canvas if necessary. The painting operation can cause additional
  // regions to be invalidated because some elements are laid out the first time
  // they are painted.
  while (!paint_rgn_.isEmpty()) {
    SkRegion draw_rgn;
    draw_rgn.swap(paint_rgn_);

    SkRegion::Cliperator iterator(draw_rgn, convertToSkiaRect(client_rect));
    for (; !iterator.done(); iterator.next()) {
      const SkIRect& r = iterator.rect();
      PaintRect(convertFromSkiaRect(r));

      if (!view_)
        damaged_rgn.op(r, SkRegion::kUnion_Op);
    }

    if (view_) {
      // If any more rectangles were made dirty during the paint operation, make
      // sure they are copied to the window buffer, by including the paint
      // region. If nothing needs additional painting, this is a no-op.
      update_rgn.op(paint_rgn_, SkRegion::kUnion_Op);
    }
  }
  
  if (!view_ && plugin_map_.size() > 0) {
    // Flash seems to stop calling NPN_InvalidateRect, which means we stop
    // painting. If we've got a plugin make sure we paint its rect each time.

    PluginMap::const_iterator it = plugin_map_.begin();
    for (; it != plugin_map_.end(); ++it) {
      if (it->second.visible &&
          client_rect.Intersects(it->second.window_rect)) {
        const WebPluginGeometry* geom = &it->second;
        damaged_rgn.op(convertToSkiaRect(geom->window_rect),
                       SkRegion::kUnion_Op);
      }
    }
  }

  if (view_) {
    // Set the context back to our view and copy the bitmap that we just painted
    // into to the view. Only the regions that were updated are copied.
    [NSGraphicsContext restoreGraphicsState];
    NSGraphicsContext* view_context = [NSGraphicsContext currentContext];
    CGContextRef context =
        static_cast<CGContextRef>([view_context graphicsPort]);

    SkRegion::Cliperator iterator(update_rgn, convertToSkiaRect(client_rect));
    for (; !iterator.done(); iterator.next()) {
      const SkIRect& r = iterator.rect();
      CGRect copy_rect = { { r.x(), r.y() }, { r.width(), r.height() } };
      const float x = r.x();
      const float y = client_rect.height() - r.bottom();
      skia::DrawToNativeContext(canvas_.get(), context, x, y, &copy_rect);
    }
  } else {
    if (damaged_rgn.isEmpty())
      return;

    // Paint to the delegate.
    DCHECK(paint_delegate_);
    const SkBitmap& bitmap = canvas_->getDevice()->accessBitmap(false);
    DCHECK(bitmap.config() == SkBitmap::kARGB_8888_Config);
    const void* pixels = bitmap.getPixels();

    std::vector<CefRect> damaged_rects;
    SkRegion::Cliperator iterator(damaged_rgn, convertToSkiaRect(client_rect));
    for (; !iterator.done(); iterator.next()) {
      const SkIRect& r = iterator.rect();
      damaged_rects.push_back(
          CefRect(r.left(), r.top(), r.width(), r.height()));
    }

    paint_delegate_->Paint(popup_, damaged_rects, pixels);
  }
}

void WebWidgetHost::SetTooltipText(const CefString& tooltip_text) {
  // TODO(port): Implement this method as part of tooltip support.
}

bool WebWidgetHost::GetImage(int width, int height, void* rgba_buffer) {
  if (!canvas_.get())
    return false;

  const SkBitmap& bitmap = canvas_->getDevice()->accessBitmap(false);
  DCHECK(bitmap.config() == SkBitmap::kARGB_8888_Config);

  if (width == canvas_->getDevice()->width() &&
      height == canvas_->getDevice()->height()) {
    // The specified width and height values are the same as the canvas size.
    // Return the existing canvas contents.
    const void* pixels = bitmap.getPixels();
    memcpy(rgba_buffer, pixels, width * height * 4);
    return true;
  }

  // Create a new canvas of the requested size.
  scoped_ptr<skia::PlatformCanvas> new_canvas(
      new skia::PlatformCanvas(width, height, true));

  new_canvas->writePixels(bitmap, 0, 0);
  const SkBitmap& new_bitmap = new_canvas->getDevice()->accessBitmap(false);
  DCHECK(new_bitmap.config() == SkBitmap::kARGB_8888_Config);

  // Return the new canvas contents.
  const void* pixels = new_bitmap.getPixels();
  memcpy(rgba_buffer, pixels, width * height * 4);
  return true;
}

WebScreenInfo WebWidgetHost::GetScreenInfo() {
  return WebScreenInfoFactory::screenInfo(view_);
}

void WebWidgetHost::Resize(const gfx::Rect& rect) {
  SetSize(rect.width(), rect.height());
}

void WebWidgetHost::MouseEvent(NSEvent *event) {
  const WebMouseEvent& web_event = WebInputEventFactory::mouseEvent(
      event, view_);
  webwidget_->handleInputEvent(web_event);
}

void WebWidgetHost::WheelEvent(NSEvent *event) {
  webwidget_->handleInputEvent(
      WebInputEventFactory::mouseWheelEvent(event, view_));
}

void WebWidgetHost::KeyEvent(NSEvent *event) {
  WebKeyboardEvent keyboard_event(WebInputEventFactory::keyboardEvent(event));
  last_key_event_ = keyboard_event;
  webwidget_->handleInputEvent(keyboard_event);

  if ([event type] != NSKeyDown)
    return;

  if ([event modifierFlags] & (NSNumericPadKeyMask | NSFunctionKeyMask)) {
    // Don't send a Char event for non-char keys like arrows, function keys and
    // clear.
    switch ([event keyCode]) {
      case 81: // =
      case 75: // /
      case 67: // *
      case 78: // -
      case 69: // +
      case 76: // Enter
      case 65: // .
      case 82: // 0
      case 83: // 1
      case 84: // 2
      case 85: // 3
      case 86: // 4
      case 87: // 5
      case 88: // 6
      case 89: // 7
      case 91: // 8
      case 92: // 9
        break;
      default:
        return;
    }
  }

  // Send a Char event here to emulate the keyboard events. 
  // TODO(hbono): Bug 20852 <http://crbug.com/20852> implement the NSTextInput
  // protocol and remove this code.
  keyboard_event.type = WebInputEvent::Char;
  last_key_event_ = keyboard_event;
  webwidget_->handleInputEvent(keyboard_event);
}

void WebWidgetHost::SetFocus(bool enable) {
  webwidget_->setFocus(enable);
}

void WebWidgetHost::PaintRect(const gfx::Rect& rect) {
#ifndef NDEBUG
  DCHECK(!painting_);
#endif
  DCHECK(canvas_.get());

  if (rect.IsEmpty())
    return;

  if (!popup() && ((WebKit::WebView*)webwidget_)->isTransparent()) {
    // When using transparency mode clear the rectangle before painting.
    SkPaint clearpaint;
    clearpaint.setARGB(0, 0, 0, 0);
    clearpaint.setXfermodeMode(SkXfermode::kClear_Mode);

    SkRect skrc;
    skrc.set(rect.x(), rect.y(), rect.right(), rect.bottom());
    canvas_->drawRect(skrc, clearpaint);
  }

  set_painting(true);
  webwidget_->paint(webkit_glue::ToWebCanvas(canvas_.get()), rect);
  set_painting(false);
}

void WebWidgetHost::SendKeyEvent(cef_key_type_t type,
                                 const cef_key_info_t& keyInfo,
                                 int modifiers) {
  WebKeyboardEvent event;

  switch (type) {
    case KT_KEYUP:
    default:
      event.type = WebInputEvent::KeyUp;
      break;
    case KT_KEYDOWN:
      event.type = WebInputEvent::RawKeyDown;
      break;
    case KT_CHAR:
      event.type = WebInputEvent::Char;
      break;
  }
  event.timeStampSeconds = TickCount();	

  if (modifiers & KEY_SHIFT)
    event.modifiers |= WebInputEvent::ShiftKey;
  if (modifiers & KEY_ALT)
    event.modifiers |= WebInputEvent::AltKey;
  if (modifiers & KEY_CTRL)
    event.modifiers |= WebInputEvent::ControlKey;
  if (modifiers & KEY_META)
    event.modifiers |= WebInputEvent::MetaKey;
  if (modifiers & KEY_KEYPAD)
    event.modifiers |= WebInputEvent::IsKeyPad;

  // There are several kinds of characters for which we produce key code from
  // char code:
  // 1. Roman letters. Windows keyboard layouts affect both virtual key codes
  //    and character codes for these, so e.g. 'A' gets the same keyCode on
  //    QWERTY, AZERTY or Dvorak layouts.
  // 2. Keys for which there is no known Mac virtual key codes, like
  //    PrintScreen.
  // 3. Certain punctuation keys. On Windows, these are also remapped depending
  //    on current keyboard layout, but see comment in
  //    windowsKeyCodeForCharCode().

  if (type == KT_KEYUP || type == KT_KEYDOWN) {
    if (keyInfo.character != 0) {
      // Cmd switches Roman letters for Dvorak-QWERTY layout, so try modified
      // characters first.
      event.windowsKeyCode =
          WebCore::windowsKeyCodeForCharCode(keyInfo.character);
    }
    if (event.windowsKeyCode == 0 && keyInfo.characterNoModifiers != 0) {
      // Ctrl+A on an AZERTY keyboard would get VK_Q keyCode if we relied on
      // keyInfo.keyCode below.
      event.windowsKeyCode =
          WebCore::windowsKeyCodeForCharCode(keyInfo.characterNoModifiers);
    }
  }

  if (event.windowsKeyCode == 0) {
    // Map Mac virtual key code directly to Windows one for any keys not handled
    // above. E.g. the key next to Caps Lock has the same Event.keyCode on U.S.
    // keyboard ('A') and on Russian keyboard (CYRILLIC LETTER EF).
    event.windowsKeyCode = WebCore::windowsKeyCodeForKeyCode(keyInfo.keyCode);
  }

  event.nativeKeyCode = keyInfo.keyCode;

  int textChar = keyInfo.character;
  int unmodifiedChar = keyInfo.characterNoModifiers;

  // Always use 13 for Enter/Return -- we don't want to use AppKit's
  // different character for Enter.
  if (event.windowsKeyCode == '\r') {
    textChar = '\r';
    unmodifiedChar = '\r';
  }

  // The adjustments below are only needed in backward compatibility mode,
  // but we cannot tell what mode we are in from here.

  // Turn 0x7F into 8, because backspace needs to always be 8.
  if (textChar == '\x7F')
    textChar = '\x8';
  if (unmodifiedChar == '\x7F')
    unmodifiedChar = '\x8';
  // Always use 9 for tab -- we don't want to use AppKit's different character
  // for shift-tab.
  if (event.windowsKeyCode == 9) {
    textChar = '\x9';
    unmodifiedChar = '\x9';
  }

  event.text[0] = textChar;
  event.unmodifiedText[0] = unmodifiedChar;

  event.setKeyIdentifierFromWindowsKeyCode();

  event.isSystemKey = !!(modifiers & KEY_META);

  last_key_event_ = event;

  webwidget_->handleInputEvent(event);
}

void WebWidgetHost::SendMouseClickEvent(int x, int y,
                                        cef_mouse_button_type_t type,
                                        bool mouseUp, int clickCount) {
  WebMouseEvent event;

  switch(type) {
    case MBT_LEFT:
      event.button = WebMouseEvent::ButtonLeft;
      event.modifiers |= WebInputEvent::LeftButtonDown;
      break;
    case MBT_MIDDLE:
      event.button = WebMouseEvent::ButtonMiddle;
      event.modifiers |= WebInputEvent::MiddleButtonDown;
      break;
    case MBT_RIGHT:
      event.button = WebMouseEvent::ButtonRight;
      event.modifiers |= WebInputEvent::RightButtonDown;
      break;
  }

  if (mouseUp)
    event.type = WebInputEvent::MouseUp;
  else
    event.type = WebInputEvent::MouseDown;

  event.clickCount = clickCount;
  event.timeStampSeconds = TickCount();	
  event.x = x;
  event.y = y;
  event.windowX = event.x;
  event.windowY = event.y;

  if (mouseUp) {
    mouse_button_down_ = WebMouseEvent::ButtonNone;
    mouse_modifiers_ &= ~event.modifiers;
  } else {
    mouse_modifiers_ |= event.modifiers;
    mouse_button_down_ = event.button;
  }
  event.modifiers = mouse_modifiers_;

  webwidget_->handleInputEvent(event);
}

void WebWidgetHost::SendMouseMoveEvent(int x, int y, bool mouseLeave) {
  WebMouseEvent event;

  event.type = WebInputEvent::MouseMove;
  event.timeStampSeconds = TickCount();
  event.button = mouse_button_down_;
  event.x = x;
  event.y = y;
  event.windowX = event.x;
  event.windowY = event.y;
  event.modifiers = mouse_modifiers_;

  webwidget_->handleInputEvent(event);
}

void WebWidgetHost::SendMouseWheelEvent(int x, int y, int deltaX, int deltaY) {
  WebMouseWheelEvent event;

  // Conversion between wheel delta amounts and number of pixels to scroll.
  static const double scrollbarPixelsPerCocoaTick = 40.0;

  event.type = WebInputEvent::MouseWheel;
  event.timeStampSeconds = TickCount();
  event.button = WebMouseEvent::ButtonNone;
  event.deltaX = static_cast<float>(deltaX);
  event.deltaY = static_cast<float>(deltaY);
  event.wheelTicksX = static_cast<float>(deltaX/scrollbarPixelsPerCocoaTick);
  event.wheelTicksY = static_cast<float>(deltaY/scrollbarPixelsPerCocoaTick);
  event.hasPreciseScrollingDeltas = true;
  event.x = x;
  event.y = y;
  event.windowX = event.x;
  event.windowY = event.y;

  webwidget_->handleInputEvent(event);
}

void WebWidgetHost::SendFocusEvent(bool setFocus) {
  SetFocus(setFocus);
}

void WebWidgetHost::SendCaptureLostEvent() {
}

void WebWidgetHost::EnsureTooltip() {
  // TODO(port): Implement this method as part of tooltip support.
}

void WebWidgetHost::ResetTooltip() {
  // TODO(port): Implement this method as part of tooltip support.
}
