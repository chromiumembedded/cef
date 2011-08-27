// Copyright (c) 2008-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "webwidget_host.h"

#include "base/logging.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/mac/WebInputEventFactory.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/mac/WebScreenInfoFactory.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPopupMenu.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScreenInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSize.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "webkit/glue/webkit_glue.h"

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

  NSRect content_rect = [parent_view frame];
  host->view_ = [[NSView alloc] initWithFrame:content_rect];
  [parent_view addSubview:host->view_];

  host->webwidget_ = WebPopupMenu::create(client);
  host->webwidget_->resize(WebSize(content_rect.size.width,
                                   content_rect.size.height));
  return host;
}

void WebWidgetHost::DidInvalidateRect(const gfx::Rect& damaged_rect) {
#ifndef NDEBUG
  DLOG_IF(WARNING, painting_) << "unexpected invalidation while painting";
#endif

  // If this invalidate overlaps with a pending scroll, then we have to
  // downgrade to invalidating the scroll rect.
  if (damaged_rect.Intersects(scroll_rect_)) {
    paint_rect_ = paint_rect_.Union(scroll_rect_);
    ResetScrollRect();
  }
  paint_rect_ = paint_rect_.Union(damaged_rect);

  NSRect r = NSRectFromCGRect(damaged_rect.ToCGRect());
  // flip to cocoa coordinates
  r.origin.y = [view_ frame].size.height - r.size.height - r.origin.y;
  [view_ setNeedsDisplayInRect:r];
}

void WebWidgetHost::DidScrollRect(int dx, int dy, const gfx::Rect& clip_rect) {
  DCHECK(dx || dy);

  // If we already have a pending scroll operation or if this scroll operation
  // intersects the existing paint region, then just failover to invalidating.
  if (!scroll_rect_.IsEmpty() || paint_rect_.Intersects(clip_rect)) {
    paint_rect_ = paint_rect_.Union(scroll_rect_);
    ResetScrollRect();
    paint_rect_ = paint_rect_.Union(clip_rect);
  }

  // We will perform scrolling lazily, when requested to actually paint.
  scroll_rect_ = clip_rect;
  scroll_dx_ = dx;
  scroll_dy_ = dy;

  NSRect r = NSRectFromCGRect(clip_rect.ToCGRect());
  // flip to cocoa coordinates
  r.origin.y = [view_ frame].size.height - r.size.height - r.origin.y;
  [view_ setNeedsDisplayInRect:r];
}

void WebWidgetHost::ScheduleComposite() {
  if (!webwidget_)
    return;
  WebSize size = webwidget_->size();
  NSRect r = NSMakeRect(0, 0, size.width, size.height);
  [view_ setNeedsDisplayInRect:r];
}

WebWidgetHost::WebWidgetHost()
    : view_(NULL),
      paint_delegate_(NULL),
      webwidget_(NULL),
      popup_(false),
      scroll_dx_(0),
      scroll_dy_(0),
      update_task_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(factory_(this)) {
  set_painting(false);
}

WebWidgetHost::~WebWidgetHost() {
}

void WebWidgetHost::Paint() {
  gfx::Rect client_rect(NSRectToCGRect([view_ frame]));
  NSGraphicsContext* view_context = [NSGraphicsContext currentContext];
  CGContextRef context = static_cast<CGContextRef>([view_context graphicsPort]);

  // Allocate a canvas if necessary
  if (!canvas_.get()) {
    ResetScrollRect();
    paint_rect_ = client_rect;
    canvas_.reset(new skia::PlatformCanvas(
        paint_rect_.width(), paint_rect_.height(), true));
  }

  // make sure webkit draws into our bitmap, not the window
  CGContextRef bitmap_context =
      skia::GetBitmapContext(skia::GetTopDevice(*canvas_));
  [NSGraphicsContext setCurrentContext:
      [NSGraphicsContext graphicsContextWithGraphicsPort:bitmap_context
                                                 flipped:YES]];

#ifdef WEBWIDGET_HAS_ANIMATE_CHANGES
  webwidget_->animate(0.0);
#else
   webwidget_->animate();
#endif

  // This may result in more invalidation
  webwidget_->layout();

  // Scroll the canvas if necessary
  scroll_rect_ = client_rect.Intersect(scroll_rect_);
  if (!scroll_rect_.IsEmpty()) {
    // add to invalidate rect, since there's no equivalent of ScrollDC.
    paint_rect_ = paint_rect_.Union(scroll_rect_);
  }
  ResetScrollRect();

  // Paint the canvas if necessary.  Allow painting to generate extra rects the
  // first time we call it.  This is necessary because some WebCore rendering
  // objects update their layout only when painted.
  for (int i = 0; i < 2; ++i) {
    paint_rect_ = client_rect.Intersect(paint_rect_);
    if (!paint_rect_.IsEmpty()) {
      gfx::Rect rect(paint_rect_);
      paint_rect_ = gfx::Rect();

//      DLOG_IF(WARNING, i == 1) << "painting caused additional invalidations";
      PaintRect(rect);
    }
  }
  DCHECK(paint_rect_.IsEmpty());

  // set the context back to our window
  [NSGraphicsContext setCurrentContext: view_context];

  // Paint to the screen
  if ([view_ lockFocusIfCanDraw]) {
    int bitmap_height = CGBitmapContextGetHeight(bitmap_context);
    int bitmap_width = CGBitmapContextGetWidth(bitmap_context);
    CGRect bitmap_rect = { { 0, 0 },
                           { bitmap_width, bitmap_height } };
    skia::DrawToNativeContext(canvas_.get(), context, 0,
        client_rect.height() - bitmap_height, &bitmap_rect);

    [view_ unlockFocus];
  }
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
  webwidget_->handleInputEvent(keyboard_event);
  if ([event type] == NSKeyDown &&
      !([event modifierFlags] & NSNumericPadKeyMask)) {
    // Send a Char event here to emulate the keyboard events. Do not send a
    // Char event for arrow keys (NSNumericPadKeyMask modifier will be set).
    // TODO(hbono): Bug 20852 <http://crbug.com/20852> implement the
    // NSTextInput protocol and remove this code.
    keyboard_event.type = WebInputEvent::Char;
    webwidget_->handleInputEvent(keyboard_event);
  }
}

void WebWidgetHost::SetFocus(bool enable) {
  webwidget_->setFocus(enable);
}

void WebWidgetHost::ResetScrollRect() {
  scroll_rect_ = gfx::Rect();
  scroll_dx_ = 0;
  scroll_dy_ = 0;
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
