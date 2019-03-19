// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/browser_info.h"

#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/values_impl.h"

#include "ipc/ipc_message.h"

// CefBrowserInfo::RenderIDManager

CefBrowserInfo::RenderIDManager::RenderIDManager(base::Lock* lock)
    : lock_(lock) {
  DCHECK(lock);
}

void CefBrowserInfo::RenderIDManager::add_render_frame_id(
    int render_process_id,
    int render_routing_id) {
  DCHECK_GT(render_process_id, 0);
  DCHECK_GT(render_routing_id, 0);

  base::AutoLock lock_scope(*lock_);

  if (!render_frame_id_set_.empty()) {
    RenderIdSet::const_iterator it = render_frame_id_set_.find(
        std::make_pair(render_process_id, render_routing_id));
    DCHECK(it == render_frame_id_set_.end());
    if (it != render_frame_id_set_.end())
      return;
  }

  render_frame_id_set_.insert(
      std::make_pair(render_process_id, render_routing_id));
}

void CefBrowserInfo::RenderIDManager::remove_render_frame_id(
    int render_process_id,
    int render_routing_id) {
  DCHECK_GT(render_process_id, 0);
  DCHECK_GT(render_routing_id, 0);

  base::AutoLock lock_scope(*lock_);

  DCHECK(!render_frame_id_set_.empty());
  if (render_frame_id_set_.empty())
    return;

  bool erased = render_frame_id_set_.erase(
                    std::make_pair(render_process_id, render_routing_id)) != 0;
  DCHECK(erased);
}

bool CefBrowserInfo::RenderIDManager::is_render_frame_id_match(
    int render_process_id,
    int render_routing_id) const {
  base::AutoLock lock_scope(*lock_);

  if (render_frame_id_set_.empty())
    return false;

  RenderIdSet::const_iterator it = render_frame_id_set_.find(
      std::make_pair(render_process_id, render_routing_id));
  return (it != render_frame_id_set_.end());
}

// CefBrowserInfo::FrameTreeNodeIDManager

CefBrowserInfo::FrameTreeNodeIDManager::FrameTreeNodeIDManager(base::Lock* lock)
    : lock_(lock) {
  DCHECK(lock);
}

void CefBrowserInfo::FrameTreeNodeIDManager::add_frame_tree_node_id(
    int frame_tree_node_id) {
  DCHECK_GE(frame_tree_node_id, 0);

  base::AutoLock lock_scope(*lock_);

  if (!frame_tree_node_id_set_.empty()) {
    FrameTreeNodeIdSet::const_iterator it =
        frame_tree_node_id_set_.find(frame_tree_node_id);
    DCHECK(it == frame_tree_node_id_set_.end());
    if (it != frame_tree_node_id_set_.end())
      return;
  }

  frame_tree_node_id_set_.insert(frame_tree_node_id);
}

void CefBrowserInfo::FrameTreeNodeIDManager::remove_frame_tree_node_id(
    int frame_tree_node_id) {
  DCHECK_GE(frame_tree_node_id, 0);

  base::AutoLock lock_scope(*lock_);

  DCHECK(!frame_tree_node_id_set_.empty());
  if (frame_tree_node_id_set_.empty())
    return;

  bool erased = frame_tree_node_id_set_.erase(frame_tree_node_id) != 0;
  DCHECK(erased);
}

bool CefBrowserInfo::FrameTreeNodeIDManager::is_frame_tree_node_id_match(
    int frame_tree_node_id) const {
  base::AutoLock lock_scope(*lock_);

  if (frame_tree_node_id_set_.empty())
    return false;

  FrameTreeNodeIdSet::const_iterator it =
      frame_tree_node_id_set_.find(frame_tree_node_id);
  return (it != frame_tree_node_id_set_.end());
}

// CefBrowserInfo

CefBrowserInfo::CefBrowserInfo(int browser_id,
                               bool is_popup,
                               CefRefPtr<CefDictionaryValue> extra_info)
    : browser_id_(browser_id),
      is_popup_(is_popup),
      is_windowless_(false),
      render_id_manager_(&lock_),
      guest_render_id_manager_(&lock_),
      frame_tree_node_id_manager_(&lock_),
      extra_info_(extra_info) {
  DCHECK_GT(browser_id, 0);
}

CefBrowserInfo::~CefBrowserInfo() {}

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
