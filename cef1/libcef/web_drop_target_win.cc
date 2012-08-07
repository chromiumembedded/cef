// Copyright (c) 2011 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/web_drop_target_win.h"

#include <windows.h>
#include <shlobj.h>

#include "libcef/browser_impl.h"
#include "libcef/cef_context.h"
#include "libcef/drag_data_impl.h"
#include "libcef/web_drag_utils_win.h"

#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebDragData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebPoint.h"
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
using WebKit::WebView;

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

WebDropTarget::WebDropTarget(CefBrowserImpl* browser)
    : ui::DropTarget(browser->UIT_GetWebViewWndHandle()),
      browser_(browser),
      current_wvh_(NULL),
      drag_cursor_(WebDragOperationNone),
      canceled_(false) {
}

WebDropTarget::~WebDropTarget() {
}

DWORD WebDropTarget::OnDragEnter(IDataObject* data_object,
                                 DWORD key_state,
                                 POINT cursor_position,
                                 DWORD effects) {
  current_wvh_ = _Context->current_webviewhost();
  DCHECK(current_wvh_);

  // TODO(tc): PopulateWebDropData can be slow depending on what is in the
  // IDataObject.  Maybe we can do this in a background thread.
  WebDropData drop_data;
  WebDropData::PopulateWebDropData(data_object, &drop_data);

  // Clear the fields that are currently unused when dragging into WebKit.
  // Remove these lines once PopulateWebDropData() is updated not to set them.
  // See crbug.com/112255.
  if (!drop_data.file_contents.empty())
    drop_data.file_contents.clear();
  if (!drop_data.file_description_filename.empty())
    drop_data.file_description_filename.clear();

  if (drop_data.url.is_empty())
    ui::OSExchangeDataProviderWin::GetPlainTextURL(data_object, &drop_data.url);

  WebKit::WebDragOperationsMask mask =
      web_drag_utils_win::WinDragOpMaskToWebDragOpMask(effects);

  canceled_ = false;

  CefRefPtr<CefClient> client = browser_->GetClient();
  if (client.get()) {
    CefRefPtr<CefDragHandler> handler = client->GetDragHandler();
    if (handler.get()) {
      CefRefPtr<CefDragData> data(new CefDragDataImpl(drop_data));
      if (handler->OnDragEnter(browser_, data,
              static_cast<CefDragHandler::DragOperationsMask>(mask))) {
        canceled_ = true;
        return DROPEFFECT_NONE;
      }
    }
  }

  drag_cursor_ = WebDragOperationNone;

  POINT client_pt = cursor_position;
  ScreenToClient(GetHWND(), &client_pt);
  WebDragOperation operation;
  if (browser_->UIT_GetWebView()) {
    operation = browser_->UIT_GetWebView()->dragTargetDragEnter(
        drop_data.ToDragData(),
        WebPoint(client_pt.x, client_pt.y),
        WebPoint(cursor_position.x, cursor_position.y),
        mask,
        0);
  } else {
    operation = WebDragOperationNone;
  }

  return web_drag_utils_win::WebDragOpToWinDragOp(operation);
}

DWORD WebDropTarget::OnDragOver(IDataObject* data_object,
                                DWORD key_state,
                                POINT cursor_position,
                                DWORD effects) {
  DCHECK(current_wvh_);
  if (current_wvh_ != _Context->current_webviewhost())
    OnDragEnter(data_object, key_state, cursor_position, effects);

  if (canceled_)
    return DROPEFFECT_NONE;

  POINT client_pt = cursor_position;
  ScreenToClient(GetHWND(), &client_pt);
  WebDragOperation operation;
  if (browser_->UIT_GetWebView()) {
    operation = browser_->UIT_GetWebView()->dragTargetDragOver(
        WebPoint(client_pt.x, client_pt.y),
        WebPoint(cursor_position.x, cursor_position.y),
        web_drag_utils_win::WinDragOpMaskToWebDragOpMask(effects),
        0);
  } else {
    operation = WebDragOperationNone;
  }

  return web_drag_utils_win::WebDragOpToWinDragOp(operation);
}

void WebDropTarget::OnDragLeave(IDataObject* data_object) {
  DCHECK(current_wvh_);
  if (current_wvh_ != _Context->current_webviewhost())
    return;

  if (canceled_)
    return;

  if (browser_->UIT_GetWebView())
    browser_->UIT_GetWebView()->dragTargetDragLeave();
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

  browser_->set_is_dropping(true);
  if (browser_->UIT_GetWebView()) {
    browser_->UIT_GetWebView()->dragTargetDrop(
        WebPoint(client_pt.x, client_pt.y),
        WebPoint(cursor_position.x, cursor_position.y),
        0);
  }
  browser_->set_is_dropping(false);

  current_wvh_ = NULL;

  // This isn't always correct, but at least it's a close approximation.
  // For now, we always map a move to a copy to prevent potential data loss.
  DWORD drop_effect = web_drag_utils_win::WebDragOpToWinDragOp(drag_cursor_);
  return drop_effect != DROPEFFECT_MOVE ? drop_effect : DROPEFFECT_COPY;
}
