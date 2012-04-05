// Copyright (c) 2011 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_WEB_DROP_TARGET_WIN_H_
#define CEF_LIBCEF_WEB_DROP_TARGET_WIN_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDragOperation.h"
#include "ui/base/dragdrop/drop_target.h"

class CefBrowserImpl;
class WebViewHost;

// A helper object that provides drop capabilities to a WebView. The
// DropTarget handles drags that enter the region of the WebView by
// passing on the events to the renderer.
class WebDropTarget : public ui::DropTarget {
 public:
  // Create a new WebDropTarget associating it with the given HWND and
  // WebView.
  explicit WebDropTarget(CefBrowserImpl* browser);
  virtual ~WebDropTarget();

  void set_drag_cursor(WebKit::WebDragOperation op) {
    drag_cursor_ = op;
  }

 protected:
  virtual DWORD OnDragEnter(IDataObject* data_object,
                            DWORD key_state,
                            POINT cursor_position,
                            DWORD effect);

  virtual DWORD OnDragOver(IDataObject* data_object,
                           DWORD key_state,
                           POINT cursor_position,
                           DWORD effect);

  virtual void OnDragLeave(IDataObject* data_object);

  virtual DWORD OnDrop(IDataObject* data_object,
                       DWORD key_state,
                       POINT cursor_position,
                       DWORD effect);

 private:
  // Our associated CefBrowserImpl.
  CefBrowserImpl* browser_;

  // We keep track of the web view host we're dragging over.  If it changes
  // during a drag, we need to re-send the DragEnter message.  WARNING:
  // this pointer should never be dereferenced.  We only use it for comparing
  // pointers.
  WebViewHost* current_wvh_;

  // Used to determine what cursor we should display when dragging over web
  // content area.  This can be updated async during a drag operation.
  WebKit::WebDragOperation drag_cursor_;

  // True if the drag has been canceled.
  bool canceled_;

  DISALLOW_COPY_AND_ASSIGN(WebDropTarget);
};

#endif  // CEF_LIBCEF_WEB_DROP_TARGET_WIN_H_
