// Copyright (c) 2011 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web_drop_target_win.h"
#include "cef_context.h"
#include "web_drag_utils_win.h"

#include <windows.h>
#include <shlobj.h>

#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDragData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPoint.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/base/clipboard/clipboard_util_win.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider_win.h"
#include "webkit/glue/webdropdata.h"
#include "webkit/glue/window_open_disposition.h"

using WebKit::WebDragOperation;
using WebKit::WebDragOperationNone;
using WebKit::WebDragOperationCopy;
using WebKit::WebDragOperationLink;
using WebKit::WebDragOperationMove;
using WebKit::WebDragOperationGeneric;
using WebKit::WebPoint;

namespace {

// A helper method for getting the preferred drop effect.
DWORD GetPreferredDropEffect(DWORD effect) {
  if (effect & DROPEFFECT_COPY)
    return DROPEFFECT_COPY;
  if (effect & DROPEFFECT_LINK)
    return DROPEFFECT_LINK;
  if (effect & DROPEFFECT_MOVE)
    return DROPEFFECT_MOVE;
  return DROPEFFECT_NONE;
}

}  // namespace

WebDropTarget::WebDropTarget(HWND source_hwnd, WebKit::WebView* view)
    : ui::DropTarget(source_hwnd),
      view_(view),
      current_wvh_(NULL),
      drag_cursor_(WebDragOperationNone) {
}

WebDropTarget::~WebDropTarget() {
}

DWORD WebDropTarget::OnDragEnter(IDataObject* data_object,
                                 DWORD key_state,
                                 POINT cursor_position,
                                 DWORD effects) {
  current_wvh_ = _Context->current_webviewhost();

  // TODO(tc): PopulateWebDropData can be slow depending on what is in the
  // IDataObject.  Maybe we can do this in a background thread.
  WebDropData drop_data;
  WebDropData::PopulateWebDropData(data_object, &drop_data);

  if (drop_data.url.is_empty())
    ui::OSExchangeDataProviderWin::GetPlainTextURL(data_object, &drop_data.url);

  drag_cursor_ = WebDragOperationNone;

  POINT client_pt = cursor_position;
  ScreenToClient(GetHWND(), &client_pt);
  WebDragOperation operation = view_->dragTargetDragEnter(
      drop_data.ToDragData(),
      WebPoint(client_pt.x, client_pt.y),
      WebPoint(cursor_position.x, cursor_position.y),
      web_drag_utils_win::WinDragOpMaskToWebDragOpMask(effects));

  return web_drag_utils_win::WebDragOpToWinDragOp(operation);
}

DWORD WebDropTarget::OnDragOver(IDataObject* data_object,
                                DWORD key_state,
                                POINT cursor_position,
                                DWORD effects) {
  DCHECK(current_wvh_);
  if (current_wvh_ != _Context->current_webviewhost())
    OnDragEnter(data_object, key_state, cursor_position, effects);

  POINT client_pt = cursor_position;
  ScreenToClient(GetHWND(), &client_pt);
  WebDragOperation operation = view_->dragTargetDragOver(
      WebPoint(client_pt.x, client_pt.y),
      WebPoint(cursor_position.x, cursor_position.y),
      web_drag_utils_win::WinDragOpMaskToWebDragOpMask(effects));

  return web_drag_utils_win::WebDragOpToWinDragOp(operation);
}

void WebDropTarget::OnDragLeave(IDataObject* data_object) {
  DCHECK(current_wvh_);
  if (current_wvh_ != _Context->current_webviewhost())
    return;

  view_->dragTargetDragLeave();
}

DWORD WebDropTarget::OnDrop(IDataObject* data_object,
                            DWORD key_state,
                            POINT cursor_position,
                            DWORD effect) {
  DCHECK(current_wvh_);
  if (current_wvh_ != _Context->current_webviewhost())
    OnDragEnter(data_object, key_state, cursor_position, effect);

  POINT client_pt = cursor_position;
  ScreenToClient(GetHWND(), &client_pt);
  view_->dragTargetDrop(
      WebPoint(client_pt.x, client_pt.y),
      WebPoint(cursor_position.x, cursor_position.y));

  current_wvh_ = NULL;

  // This isn't always correct, but at least it's a close approximation.
  // For now, we always map a move to a copy to prevent potential data loss.
  DWORD drop_effect = web_drag_utils_win::WebDragOpToWinDragOp(drag_cursor_);
  return drop_effect != DROPEFFECT_MOVE ? drop_effect : DROPEFFECT_COPY;
}
