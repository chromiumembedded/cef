// Copyright (c) 2013 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_WEB_DRAG_UTILS_GTK_H_
#define CEF_LIBCEF_WEB_DRAG_UTILS_GTK_H_
#pragma once

#include <gdk/gdk.h>
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDragOperation.h"

namespace web_drag_utils_gtk {

// Convenience methods for converting between web drag operations and the GDK
// equivalent.
GdkDragAction WebDragOpToGdkDragAction(WebKit::WebDragOperationsMask op);
WebKit::WebDragOperationsMask GdkDragActionToWebDragOp(GdkDragAction action);

}  // namespace web_drag_utils_gtk

#endif  // CEF_LIBCEF_WEB_DRAG_UTILS_GTK_H_

