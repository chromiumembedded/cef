// Copyright (c) 2008-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#import "libcef/webwidget_host.h"
#import "base/logging.h"
#import "skia/ext/platform_canvas.h"
#import "third_party/WebKit/Source/WebKit/chromium/public/mac/WebInputEventFactory.h"
#import "third_party/WebKit/Source/WebKit/chromium/public/mac/WebScreenInfoFactory.h"
#import "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#import "third_party/WebKit/Source/WebKit/chromium/public/WebPopupMenu.h"
#import "third_party/WebKit/Source/WebKit/chromium/public/WebScreenInfo.h"
#import "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSize.h"
#import "third_party/skia/include/core/SkRegion.h"
#import "ui/gfx/rect.h"
#import "ui/gfx/size.h"
#import "webkit/glue/webkit_glue.h"

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

  const NSRect bounds = [parent_view bounds];
  host->view_ = [[NSView alloc] initWithFrame:bounds];
  [parent_view addSubview:host->view_];

  host->painting_  = false;
  host->layouting_ = false;
  host->webwidget_ = WebPopupMenu::create(client);
  host->webwidget_->resize(WebSize(NSWidth(bounds), NSHeight(bounds)));
  return host;
}

WebWidgetHost::WebWidgetHost()
    : view_(NULL),
      paint_delegate_(NULL),
      webwidget_(NULL),
      canvas_w_(0),
      canvas_h_(0),
      popup_(false),
      has_update_task_(false),
      has_invalidate_task_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  set_painting(false);
}

WebWidgetHost::~WebWidgetHost() {
}

void WebWidgetHost::DidInvalidateRect(const gfx::Rect& damaged_rect) {
  const gfx::Rect client_rect(NSRectToCGRect([view_ bounds]));
  const gfx::Rect damaged_rect_in_client = client_rect.Intersect(damaged_rect);

  if (!damaged_rect_in_client.IsEmpty()) {
    UpdatePaintRect(damaged_rect_in_client);
    NSRect cocoa_rect = NSRectFromCGRect(damaged_rect_in_client.ToCGRect());
    cocoa_rect.origin.y = client_rect.height() - NSMaxY(cocoa_rect);
    [view_ setNeedsDisplayInRect:cocoa_rect];
  }
}

void WebWidgetHost::DidScrollRect(int dx, int dy, const gfx::Rect& clip_rect) {
  DCHECK(dx || dy);

  const gfx::Rect client_rect(NSRectToCGRect([view_ bounds]));
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
  if (![view_ canDraw] || painting_ || layouting_ || Dx >= w || Dy >= h) {
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
  gfx::Rect client_rect(NSRectToCGRect([view_ bounds]));

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
    for (; !iterator.done(); iterator.next())
      PaintRect(convertFromSkiaRect(iterator.rect()));

    // If any more rectangles were made dirty during the paint operation, make
    // sure they are copied to the window buffer, by including the paint region.
    // If nothing needs additional painting, this is a no-op.
    update_rgn.op(paint_rgn_, SkRegion::kUnion_Op);
  }

  // Set the context back to our view and copy the bitmap that we just painted
  // into to the view. Only the regions that were updated are copied.
  [NSGraphicsContext restoreGraphicsState];
  NSGraphicsContext* view_context = [NSGraphicsContext currentContext];
  CGContextRef context = static_cast<CGContextRef>([view_context graphicsPort]);

  SkRegion::Cliperator iterator(update_rgn, convertToSkiaRect(client_rect));
  for (; !iterator.done(); iterator.next()) {
    const SkIRect& r = iterator.rect();
    CGRect copy_rect = { { r.x(), r.y() }, { r.width(), r.height() } };
    const float x = r.x();
    const float y = client_rect.height() - r.bottom();
    skia::DrawToNativeContext(canvas_.get(), context, x, y, &copy_rect);
  }

  // Used with scheduled invalidation to maintain a consistent frame rate.
  paint_last_call_ = base::TimeTicks::Now();
  if (has_invalidate_task_)
    has_invalidate_task_ = false;
}

void WebWidgetHost::Invalidate() {
  [view_ setNeedsDisplay:YES];
}

void WebWidgetHost::SetTooltipText(const CefString& tooltip_text) {
  // TODO(port): Implement this method as part of tooltip support.
}

void WebWidgetHost::InvalidateRect(const gfx::Rect& rect) {
  // TODO(port): Implement this method as part of off-screen rendering support.
  NOTIMPLEMENTED();
}

bool WebWidgetHost::GetImage(int width, int height, void* rgba_buffer) {
  if (!canvas_.get())
    return false;

  // TODO(port): Implement this method as part of off-screen rendering support.
  NOTIMPLEMENTED();
  return false;
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

  if (!rect.IsEmpty()) {
    set_painting(true);
    webwidget_->paint(webkit_glue::ToWebCanvas(canvas_.get()), rect);
    set_painting(false);
  }
}

void WebWidgetHost::SendKeyEvent(cef_key_type_t type, int key, int modifiers,
                                 bool sysChar, bool imeChar) {
  // TODO(port): Implement this method as part of off-screen rendering support.
  NOTIMPLEMENTED();
}

void WebWidgetHost::SendMouseClickEvent(int x, int y,
                                        cef_mouse_button_type_t type,
                                        bool mouseUp, int clickCount) {
  // TODO(port): Implement this method as part of off-screen rendering support.
  NOTIMPLEMENTED();
}

void WebWidgetHost::SendMouseMoveEvent(int x, int y, bool mouseLeave) {
  // TODO(port): Implement this method as part of off-screen rendering support.
  NOTIMPLEMENTED();
}

void WebWidgetHost::SendMouseWheelEvent(int x, int y, int delta) {
  // TODO(port): Implement this method as part of off-screen rendering support.
  NOTIMPLEMENTED();
}

void WebWidgetHost::SendFocusEvent(bool setFocus) {
  // TODO(port): Implement this method as part of off-screen rendering support.
  NOTIMPLEMENTED();
}

void WebWidgetHost::SendCaptureLostEvent() {
  // TODO(port): Implement this method as part of off-screen rendering support.
  NOTIMPLEMENTED();
}

void WebWidgetHost::EnsureTooltip() {
  // TODO(port): Implement this method as part of tooltip support.
}

void WebWidgetHost::ResetTooltip() {
  // TODO(port): Implement this method as part of tooltip support.
}
