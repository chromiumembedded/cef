// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_WEB_DRAG_UTILS_WIN_H_
#define CEF_LIBCEF_WEB_DRAG_UTILS_WIN_H_
#pragma once

#include <windows.h>
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDragOperation.h"

namespace web_drag_utils_win {

WebKit::WebDragOperation WinDragOpToWebDragOp(DWORD effect);
WebKit::WebDragOperationsMask WinDragOpMaskToWebDragOpMask(DWORD effects);

DWORD WebDragOpToWinDragOp(WebKit::WebDragOperation op);
DWORD WebDragOpMaskToWinDragOpMask(WebKit::WebDragOperationsMask ops);

}  // namespace web_drag_utils_win

#endif  // CEF_LIBCEF_WEB_DRAG_UTILS_WIN_H_
