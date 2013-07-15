// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/browser_info.h"
#include "ipc/ipc_message.h"

CefBrowserInfo::CefBrowserInfo(int browser_id, bool is_popup)
    : browser_id_(browser_id),
      is_popup_(is_popup),
      is_window_rendering_disabled_(false) {
  DCHECK_GT(browser_id, 0);
}

CefBrowserInfo::~CefBrowserInfo() {
}

void CefBrowserInfo::set_window_rendering_disabled(bool disabled) {
  is_window_rendering_disabled_ = disabled;
}

void CefBrowserInfo::add_render_id(
    int render_process_id, int render_view_id) {
  DCHECK_GT(render_process_id, 0);
  DCHECK_GT(render_view_id, 0);

  base::AutoLock lock_scope(lock_);

  if (!render_id_set_.empty()) {
    RenderIdSet::const_iterator it =
        render_id_set_.find(std::make_pair(render_process_id, render_view_id));
    if (it != render_id_set_.end())
      return;
  }

  render_id_set_.insert(std::make_pair(render_process_id, render_view_id));
}

void CefBrowserInfo::remove_render_id(
    int render_process_id, int render_view_id) {
  DCHECK_GT(render_process_id, 0);
  DCHECK_GT(render_view_id, 0);

  base::AutoLock lock_scope(lock_);

  DCHECK(!render_id_set_.empty());
  if (render_id_set_.empty())
    return;

  RenderIdSet::iterator it =
      render_id_set_.find(std::make_pair(render_process_id, render_view_id));
  DCHECK(it != render_id_set_.end());
  if (it != render_id_set_.end())
    render_id_set_.erase(it);
}

bool CefBrowserInfo::is_render_id_match(
    int render_process_id, int render_view_id) {
  base::AutoLock lock_scope(lock_);

  if (render_id_set_.empty())
    return false;

  RenderIdSet::const_iterator it =
      render_id_set_.find(std::make_pair(render_process_id, render_view_id));
  return (it != render_id_set_.end());
}

CefRefPtr<CefBrowserHostImpl> CefBrowserInfo::browser() {
  base::AutoLock lock_scope(lock_);
  return browser_;
}

void CefBrowserInfo::set_browser(CefRefPtr<CefBrowserHostImpl> browser) {
  base::AutoLock lock_scope(lock_);
  browser_ = browser;
}
