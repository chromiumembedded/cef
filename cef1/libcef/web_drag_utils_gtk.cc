// Copyright (c) 2013 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/web_drag_utils_gtk.h"

using WebKit::WebDragOperationsMask;
using WebKit::WebDragOperation;
using WebKit::WebDragOperationNone;
using WebKit::WebDragOperationCopy;
using WebKit::WebDragOperationLink;
using WebKit::WebDragOperationMove;

namespace web_drag_utils_gtk {

// From content/browser/web_contents/drag_utils_gtk.cc.

GdkDragAction WebDragOpToGdkDragAction(WebDragOperationsMask op) {
  GdkDragAction action = static_cast<GdkDragAction>(0);
  if (op & WebDragOperationCopy)
    action = static_cast<GdkDragAction>(action | GDK_ACTION_COPY);
  if (op & WebDragOperationLink)
    action = static_cast<GdkDragAction>(action | GDK_ACTION_LINK);
  if (op & WebDragOperationMove)
    action = static_cast<GdkDragAction>(action | GDK_ACTION_MOVE);
  return action;
}

WebDragOperationsMask GdkDragActionToWebDragOp(GdkDragAction action) {
  WebDragOperationsMask op = WebDragOperationNone;
  if (action & GDK_ACTION_COPY)
    op = static_cast<WebDragOperationsMask>(op | WebDragOperationCopy);
  if (action & GDK_ACTION_LINK)
    op = static_cast<WebDragOperationsMask>(op | WebDragOperationLink);
  if (action & GDK_ACTION_MOVE)
    op = static_cast<WebDragOperationsMask>(op | WebDragOperationMove);
  return op;
}

}  // namespace web_drag_utils_gtk

