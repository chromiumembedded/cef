// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/browser_info.h"
#include "ipc/ipc_message.h"

CefBrowserInfo::CefBrowserInfo(int browser_id, bool is_popup)
    : browser_id_(browser_id),
      is_popup_(is_popup),
      render_process_id_(MSG_ROUTING_NONE),
      render_view_id_(MSG_ROUTING_NONE) {
  DCHECK_GT(browser_id, 0);
}

CefBrowserInfo::~CefBrowserInfo() {
}

void CefBrowserInfo::set_render_ids(
    int render_process_id, int render_view_id) {
  base::AutoLock lock_scope(lock_);
  render_process_id_ = render_process_id;
  render_view_id_ = render_view_id;
}

bool CefBrowserInfo::is_render_id_match(
    int render_process_id, int render_view_id) {
  base::AutoLock lock_scope(lock_);
  if (render_process_id != render_process_id_)
    return false;
  return (render_view_id == 0 || render_view_id == render_view_id_);
}

CefRefPtr<CefBrowserHostImpl> CefBrowserInfo::browser() {
  base::AutoLock lock_scope(lock_);
  return browser_;
}

void CefBrowserInfo::set_browser(CefRefPtr<CefBrowserHostImpl> browser) {
  base::AutoLock lock_scope(lock_);
  browser_ = browser;
}
