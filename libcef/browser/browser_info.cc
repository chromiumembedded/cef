// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/browser_info.h"

#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/thread_util.h"

#include "ipc/ipc_message.h"

// CefBrowserInfo::RenderIDManager

CefBrowserInfo::RenderIDManager::RenderIDManager(base::Lock* lock)
  : lock_(lock) {
  DCHECK(lock);
}

void CefBrowserInfo::RenderIDManager::add_render_view_id(
    int render_process_id, int render_routing_id) {
  add_render_id(&render_view_id_set_, render_process_id, render_routing_id);
}

void CefBrowserInfo::RenderIDManager::add_render_frame_id(
    int render_process_id, int render_routing_id) {
  add_render_id(&render_frame_id_set_, render_process_id, render_routing_id);
}

void CefBrowserInfo::RenderIDManager::remove_render_view_id(
    int render_process_id, int render_routing_id) {
  remove_render_id(&render_view_id_set_, render_process_id, render_routing_id);
}

void CefBrowserInfo::RenderIDManager::remove_render_frame_id(
    int render_process_id, int render_routing_id) {
  remove_render_id(&render_frame_id_set_, render_process_id, render_routing_id);
}

bool CefBrowserInfo::RenderIDManager::is_render_view_id_match(
    int render_process_id, int render_routing_id) const {
  return is_render_id_match(&render_view_id_set_,
                            render_process_id,
                            render_routing_id);
}

bool CefBrowserInfo::RenderIDManager::is_render_frame_id_match(
    int render_process_id, int render_routing_id) const {
  return is_render_id_match(&render_frame_id_set_,
                            render_process_id,
                            render_routing_id);
}

void CefBrowserInfo::RenderIDManager::add_render_id(RenderIdSet* id_set,
                                                    int render_process_id,
                                                    int render_routing_id) {
  DCHECK_GT(render_process_id, 0);
  DCHECK_GT(render_routing_id, 0);

  base::AutoLock lock_scope(*lock_);

  if (!id_set->empty()) {
    RenderIdSet::const_iterator it =
        id_set->find(std::make_pair(render_process_id, render_routing_id));
    if (it != id_set->end())
      return;
  }

  id_set->insert(std::make_pair(render_process_id, render_routing_id));
}

void CefBrowserInfo::RenderIDManager::remove_render_id(RenderIdSet* id_set,
                                                       int render_process_id,
                                                       int render_routing_id) {
  DCHECK_GT(render_process_id, 0);
  DCHECK_GT(render_routing_id, 0);

  base::AutoLock lock_scope(*lock_);

  DCHECK(!id_set->empty());
  if (id_set->empty())
    return;

  bool erased = id_set->erase(
      std::make_pair(render_process_id, render_routing_id)) != 0;
  DCHECK(erased);
}

bool CefBrowserInfo::RenderIDManager::is_render_id_match(
    const RenderIdSet* id_set,
    int render_process_id,
    int render_routing_id) const {
  base::AutoLock lock_scope(*lock_);

  if (id_set->empty())
    return false;

  RenderIdSet::const_iterator it =
      id_set->find(std::make_pair(render_process_id, render_routing_id));
  return (it != id_set->end());
}


// CefBrowserInfo

CefBrowserInfo::CefBrowserInfo(int browser_id, bool is_popup)
    : browser_id_(browser_id),
      is_popup_(is_popup),
      is_windowless_(false),
      render_id_manager_(&lock_),
      guest_render_id_manager_(&lock_) {
  DCHECK_GT(browser_id, 0);
}

CefBrowserInfo::~CefBrowserInfo() {
}

void CefBrowserInfo::set_windowless(bool windowless) {
  is_windowless_ = windowless;
}

CefRefPtr<CefBrowserHostImpl> CefBrowserInfo::browser() const {
  base::AutoLock lock_scope(lock_);
  return browser_;
}

void CefBrowserInfo::set_browser(CefRefPtr<CefBrowserHostImpl> browser) {
  base::AutoLock lock_scope(lock_);
  browser_ = browser;
}
