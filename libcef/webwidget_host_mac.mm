// Copyright (c) 2008-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#import "webwidget_host.h"
#import "base/logging.h"
#import "skia/ext/platform_canvas.h"
#import "third_party/WebKit/Source/WebKit/chromium/public/mac/WebInputEventFactory.h"
#import "third_party/WebKit/Source/WebKit/chromium/public/mac/WebScreenInfoFactory.h"
#import "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#import "third_party/WebKit/Source/WebKit/chromium/public/WebPopupMenu.h"
#import "third_party/WebKit/Source/WebKit/chromium/public/WebScreenInfo.h"
#import "third_party/WebKit/Source/WebKit/chromium/public/WebSize.h"
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

void WebWidgetHost::DidInvalidateRect(const gfx::Rect& damaged_rect) {
  const gfx::Rect client_rect(NSRectToCGRect([view_ bounds]));
  paint_rect_ = paint_rect_.Union(client_rect.Intersect(damaged_rect));

  if (!paint_rect_.IsEmpty()) {
    NSRect r = NSRectFromCGRect(damaged_rect.ToCGRect());
    r.origin.y = NSHeight([view_ frame]) - NSMaxY(r);
    [view_ setNeedsDisplayInRect:r];
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

  // If we're in a state right now in which we cannot draw into the view, just
  // mark the scrolling rect as dirty, and it will be completely redrawn instead.
  // The Paint() method can end up calling this method indirectly, if the view
  // needs to be laid out. Calling scrollRect:by: in this situation leads to
  // unwanted behavior. Finally, scrolling the rectangle by more than the size
  // of the view means we can just invalidate the entire scroll rect.
  if (![view_ canDraw] || painting_ || layouting_ || ABS(dx) >= w ||
      ABS(dy) >= h) {
    DidInvalidateRect(clip_rect);
    return;
  }

  // The scrolling rect must not scroll outside the clip rect, because that will
  // clobber the scrollbars; so the rectangle is shortened a bit from the
  // leading side of the scroll (could be either horizontally or vertically).
  if (dx > 0)
    rect = gfx::Rect(x, y, w - dx, h);
  else if (dx < 0)
    rect = gfx::Rect(x - dx, y, w + dx, h);
  else if (dy > 0)
    rect = gfx::Rect(x, y, w, h - dy);
  else if (dy < 0) 
    rect = gfx::Rect(x, y - dy, w, h + dy);

  // Convert scroll rectangle to the view's coordinate system, and perform the
  // scroll directly, without invalidating the view. In theory this could cause
  // some kind of performance issue, since we're not coalescing redraw events,
  // but in practice we get much smoother scrolling of big views, since just
  // copying the pixels within the window is much faster than redrawing them.
  NSRect cocoa_rect = NSRectFromCGRect(rect.ToCGRect());
  cocoa_rect.origin.y = NSHeight([view_ bounds]) - NSMaxY(cocoa_rect);
  [view_ scrollRect:cocoa_rect by:NSMakeSize(dx, -dy)];

  const gfx::Rect saved_paint_rect = paint_rect_;
  if (![view_ lockFocusIfCanDraw])
    return;

  // Repaint the rectangle that was revealed when scrolling the given rectangle.
  // We don't want to invalidate the rectangle, because that would cause the
  // invalidated area to be L-shaped, because of this narrow area, and the
  // scrollbar area that also could be invalidated, and the union of those two
  // rectangles is pretty much the entire view area, and we would not save any
  // work by doing the scrollRect: call above.
  if (dx > 0)
    paint_rect_ = gfx::Rect(x, y, dx, h);
  else if (dx < 0)
    paint_rect_ = gfx::Rect(r + dx, y, -dx, h);
  else if (dy > 0)
    paint_rect_ = gfx::Rect(x, y, w, dy);
  else if (dy < 0)
    paint_rect_ = gfx::Rect(x, b + dy, w, -dy);

  paint_rect_ = paint_rect_.Intersect(client_rect);
  if (!paint_rect_.IsEmpty())
    Paint();

  // If any part of the scrolled rect was marked as dirty, make sure to redraw
  // it in the new scrolled-to location. Otherwise we can end up with artifacts
  // for overlapping elements.
  gfx::Rect moved_paint_rect = saved_paint_rect.Intersect(clip_rect);
  if (!moved_paint_rect.IsEmpty()) {
    moved_paint_rect.Offset(dx, dy);
    paint_rect_ = moved_paint_rect;
    paint_rect_ = paint_rect_.Intersect(client_rect);
    if (!paint_rect_.IsEmpty()) Paint();
  }
  
  [view_ unlockFocus];
  paint_rect_ = saved_paint_rect;
}

void WebWidgetHost::ScheduleComposite() {
  if (!webwidget_)
    return;
  const WebSize size = webwidget_->size();
  [view_ setNeedsDisplayInRect:NSMakeRect(0, 0, size.width, size.height)];
}

WebWidgetHost::WebWidgetHost()
    : view_(NULL),
      paint_delegate_(NULL),
      webwidget_(NULL),
      canvas_w_(0),
      canvas_h_(0),
      popup_(false),
      has_update_task_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  set_painting(false);
}

WebWidgetHost::~WebWidgetHost() {
}

void WebWidgetHost::Paint() {
  gfx::Rect client_rect(NSRectToCGRect([view_ bounds]));
  gfx::Rect update_rect;
  
  // When we are not using accelerated compositing, the canvas area is allowed
  // to differ in size from the client with a certain number of pixels (128 in
  // this case). When accelerated compositing is in effect, the size must match
  // exactly.
  if (!webwidget_->isAcceleratedCompositingActive()) {
    const int kCanvasGrowSize = 128;

    if (!canvas_.get() ||
        canvas_w_ < client_rect.width() ||
        canvas_h_ < client_rect.height() ||
        canvas_w_ > client_rect.width() + kCanvasGrowSize * 2 ||
        canvas_h_ > client_rect.height() + kCanvasGrowSize * 2) {
      canvas_w_ = client_rect.width() + kCanvasGrowSize;
      canvas_h_ = client_rect.height() + kCanvasGrowSize;
      canvas_.reset(new skia::PlatformCanvas(canvas_w_, canvas_h_, true));
      paint_rect_ = client_rect;
    }
  } else if(!canvas_.get() || canvas_w_ != client_rect.width() ||
            canvas_h_ != client_rect.height()) {
    canvas_w_ = client_rect.width();
    canvas_h_ = client_rect.height();
    canvas_.reset(new skia::PlatformCanvas(canvas_w_, canvas_h_, true));
    paint_rect_ = client_rect;
  }

  // Animate the view and layout any views that have not been laid out yet. The
  // latter may result in more invalidation. Keep track of the fact that we are
  // laying out views, because this will sometimes cause ScrollRect to be called
  // and we don't want to try to scrollRect:by: then.
  layouting_ = true;
  webwidget_->animate(0.0);
  webwidget_->layout();
  layouting_ = false;

  // Draw into the graphics context of the canvas instead of the view's context.
  // The view's context is pushed onto the context stack, to be restored below.
  CGContextRef bitmap = skia::GetBitmapContext(skia::GetTopDevice(*canvas_));
  NSGraphicsContext* paint_context =
      [NSGraphicsContext graphicsContextWithGraphicsPort:bitmap flipped:YES];
  [NSGraphicsContext saveGraphicsState];
  [NSGraphicsContext setCurrentContext:paint_context];

  // Paint the canvas if necessary. Allow painting to generate extra rects the
  // first time we call it. This is necessary because some WebCore rendering
  // objects update their layout only when painted.
  for (int i = 0; i < 2; ++i) {
    paint_rect_ = client_rect.Intersect(paint_rect_);
    if (!paint_rect_.IsEmpty()) {
      gfx::Rect rect(paint_rect_);
      update_rect = (i == 0? rect: update_rect.Union(rect));
      paint_rect_ = gfx::Rect();
      PaintRect(rect);
    }
  }

  // Set the context back to our view and copy the bitmap that we just painted
  // into to the view. Only the region that was update is copied.
  [NSGraphicsContext restoreGraphicsState];
  NSGraphicsContext* view_context = [NSGraphicsContext currentContext];
  CGContextRef context = static_cast<CGContextRef>([view_context graphicsPort]);
  CGRect bitmap_rect = { { update_rect.x(), update_rect.y() },
                         { update_rect.width(), update_rect.height() } };
  skia::DrawToNativeContext(canvas_.get(), context, update_rect.x(),
      client_rect.height() - update_rect.bottom(), &bitmap_rect);
}

void WebWidgetHost::SetTooltipText(const CefString& tooltip_text)
{
  // TODO(port): Implement this method as part of tooltip support.
}

void WebWidgetHost::InvalidateRect(const gfx::Rect& rect)
{
  // TODO(port): Implement this method as part of off-screen rendering support.
  NOTIMPLEMENTED();
}

bool WebWidgetHost::GetImage(int width, int height, void* rgba_buffer)
{
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
  if ([event type] == NSKeyDown &&
      !([event modifierFlags] & NSNumericPadKeyMask)) {
    // Send a Char event here to emulate the keyboard events. Do not send a
    // Char event for arrow keys (NSNumericPadKeyMask modifier will be set).
    // TODO(hbono): Bug 20852 <http://crbug.com/20852> implement the
    // NSTextInput protocol and remove this code.
    keyboard_event.type = WebInputEvent::Char;
    last_key_event_ = keyboard_event;
    webwidget_->handleInputEvent(keyboard_event);
  }
}

void WebWidgetHost::SetFocus(bool enable) {
  webwidget_->setFocus(enable);
}

void WebWidgetHost::PaintRect(const gfx::Rect& rect) {
#ifndef NDEBUG
  DCHECK(!painting_);
#endif
  DCHECK(canvas_.get());

  set_painting(true);
  webwidget_->paint(webkit_glue::ToWebCanvas(canvas_.get()), rect);
  set_painting(false);
}

void WebWidgetHost::SendKeyEvent(cef_key_type_t type, int key, int modifiers,
                                 bool sysChar, bool imeChar)
{
  // TODO(port): Implement this method as part of off-screen rendering support.
  NOTIMPLEMENTED();
}

void WebWidgetHost::SendMouseClickEvent(int x, int y,
                                        cef_mouse_button_type_t type,
                                        bool mouseUp, int clickCount)
{
  // TODO(port): Implement this method as part of off-screen rendering support.
  NOTIMPLEMENTED();
}

void WebWidgetHost::SendMouseMoveEvent(int x, int y, bool mouseLeave)
{
  // TODO(port): Implement this method as part of off-screen rendering support.
  NOTIMPLEMENTED();
}

void WebWidgetHost::SendMouseWheelEvent(int x, int y, int delta)
{
  // TODO(port): Implement this method as part of off-screen rendering support.
  NOTIMPLEMENTED();
}

void WebWidgetHost::SendFocusEvent(bool setFocus)
{
  // TODO(port): Implement this method as part of off-screen rendering support.
  NOTIMPLEMENTED();
}

void WebWidgetHost::SendCaptureLostEvent()
{
  // TODO(port): Implement this method as part of off-screen rendering support.
  NOTIMPLEMENTED();
}

void WebWidgetHost::EnsureTooltip()
{
  // TODO(port): Implement this method as part of tooltip support.
}

void WebWidgetHost::ResetTooltip()
{
  // TODO(port): Implement this method as part of tooltip support.
}
