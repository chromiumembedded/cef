// Copyright (c) 2011 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web_drag_source_win.h"
#include "web_drag_utils_win.h"
#include "cef_thread.h"

#include "base/task.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPoint.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

using WebKit::WebDragOperationNone;
using WebKit::WebPoint;
using WebKit::WebView;

namespace {

static void GetCursorPositions(gfx::NativeWindow wnd, gfx::Point* client,
                               gfx::Point* screen) {
  POINT cursor_pos;
  GetCursorPos(&cursor_pos);
  screen->SetPoint(cursor_pos.x, cursor_pos.y);
  ScreenToClient(wnd, &cursor_pos);
  client->SetPoint(cursor_pos.x, cursor_pos.y);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// WebDragSource, public:

WebDragSource::WebDragSource(gfx::NativeWindow source_wnd, WebView* view)
    : ui::DragSource(),
      source_wnd_(source_wnd),
      view_(view),
      effect_(DROPEFFECT_NONE) {
}

WebDragSource::~WebDragSource() {
}

void WebDragSource::OnDragSourceCancel() {
  // Delegate to the UI thread if we do drag-and-drop in the background thread.
  if (!CefThread::CurrentlyOn(CefThread::UI)) {
    CefThread::PostTask(
        CefThread::UI, FROM_HERE,
        NewRunnableMethod(this, &WebDragSource::OnDragSourceCancel));
    return;
  }

  if (!view_)
    return;

  gfx::Point client;
  gfx::Point screen;
  GetCursorPositions(source_wnd_, &client, &screen);
  view_->dragSourceEndedAt(client, screen, WebDragOperationNone);
}

void WebDragSource::OnDragSourceDrop() {
  // On Windows, we check for drag end in IDropSource::QueryContinueDrag which
  // happens before IDropTarget::Drop is called. HTML5 requires the "dragend"
  // event to happen after the "drop" event. Since Windows calls these two
  // directly after each other we can just post a task to handle the
  // OnDragSourceDrop after the current task.
  CefThread::PostTask(
      CefThread::UI, FROM_HERE,
      NewRunnableMethod(this, &WebDragSource::DelayedOnDragSourceDrop));
}

void WebDragSource::DelayedOnDragSourceDrop() {
  if (!view_)
    return;

  gfx::Point client;
  gfx::Point screen;
  GetCursorPositions(source_wnd_, &client, &screen);
  view_->dragSourceEndedAt(client, screen,
      web_drag_utils_win::WinDragOpToWebDragOp(effect_));
}

void WebDragSource::OnDragSourceMove() {
  // Delegate to the UI thread if we do drag-and-drop in the background thread.
  if (!CefThread::CurrentlyOn(CefThread::UI)) {
    CefThread::PostTask(
        CefThread::UI, FROM_HERE,
        NewRunnableMethod(this, &WebDragSource::OnDragSourceMove));
    return;
  }

  if (!view_)
    return;

  gfx::Point client;
  gfx::Point screen;
  GetCursorPositions(source_wnd_, &client, &screen);
  view_->dragSourceMovedTo(client, screen, WebDragOperationNone);
}
