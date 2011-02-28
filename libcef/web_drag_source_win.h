// Copyright (c) 2011 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _WEB_DRAG_SOURCE_WIN_H
#define _WEB_DRAG_SOURCE_WIN_H
#pragma once

#include "base/basictypes.h"
#include "ui/base/dragdrop/drag_source.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/point.h"

namespace WebKit {
class WebView;
};

// An IDropSource implementation for a WebView. Handles notifications sent
// by an active drag-drop operation as the user mouses over other drop targets
// on their system. This object tells Windows whether or not the drag should
// continue, and supplies the appropriate cursors.
class WebDragSource : public ui::DragSource {
 public:
  // Create a new DragSource for a given HWND and WebView.
 WebDragSource(gfx::NativeWindow source_wnd, WebKit::WebView* view);
  virtual ~WebDragSource();

  void set_effect(DWORD effect) { effect_ = effect; }

 protected:
  // ui::DragSource
  virtual void OnDragSourceCancel();
  virtual void OnDragSourceDrop();
  virtual void OnDragSourceMove();

 private:
  // Cannot construct thusly.
  WebDragSource();

  // OnDragSourceDrop schedules its main work to be done after IDropTarget::Drop
  // by posting a task to this function.
  void DelayedOnDragSourceDrop();

  // Keep a reference to the window so we can translate the cursor position.
  gfx::NativeWindow source_wnd_;

  // We use this as a channel to the web view to tell it about various drag
  // drop events that it needs to know about (such as when a drag operation it
  // initiated terminates).
  WebKit::WebView* view_;

  DWORD effect_;

  DISALLOW_COPY_AND_ASSIGN(WebDragSource);
};

#endif  // _WEB_DRAG_SOURCE_WIN_H
