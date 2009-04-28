// Copyright (c) 2008 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "precompiled_libcef.h"
#include "browser_drop_delegate.h"

#include "third_party/WebKit/WebKit/chromium/public/WebDragData.h"
#include "third_party/WebKit/WebKit/chromium/public/WebPoint.h"
#include "webkit/glue/webdropdata.h"
#include "webkit/glue/webview.h"

using WebKit::WebPoint;

// BaseDropTarget methods ----------------------------------------------------

DWORD BrowserDropDelegate::OnDragEnter(IDataObject* data_object,
                                    DWORD key_state,
                                    POINT cursor_position,
                                    DWORD effect) {
  WebDropData drop_data;
  WebDropData::PopulateWebDropData(data_object, &drop_data);

  POINT client_pt = cursor_position;
  ScreenToClient(GetHWND(), &client_pt);
  bool valid = webview_->DragTargetDragEnter(
      drop_data.ToDragData(), drop_data.identity,
      WebPoint(client_pt.x, client_pt.y),
      WebPoint(cursor_position.x, cursor_position.y));
  return valid ? DROPEFFECT_COPY : DROPEFFECT_NONE;
}

DWORD BrowserDropDelegate::OnDragOver(IDataObject* data_object,
                                   DWORD key_state,
                                   POINT cursor_position,
                                   DWORD effect) {
  POINT client_pt = cursor_position;
  ScreenToClient(GetHWND(), &client_pt);
  bool valid = webview_->DragTargetDragOver(
      WebPoint(client_pt.x, client_pt.y),
      WebPoint(cursor_position.x, cursor_position.y));
  return valid ? DROPEFFECT_COPY : DROPEFFECT_NONE;
}

void BrowserDropDelegate::OnDragLeave(IDataObject* data_object) {
  webview_->DragTargetDragLeave();
}

DWORD BrowserDropDelegate::OnDrop(IDataObject* data_object,
                               DWORD key_state,
                               POINT cursor_position,
                               DWORD effect) {
  POINT client_pt = cursor_position;
  ScreenToClient(GetHWND(), &client_pt);
  webview_->DragTargetDrop(
      WebPoint(client_pt.x, client_pt.y),
      WebPoint(cursor_position.x, cursor_position.y));

  // webkit win port always returns DROPEFFECT_NONE
  return DROPEFFECT_NONE;
}

