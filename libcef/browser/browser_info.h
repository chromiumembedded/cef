// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_BROWSER_INFO_H_
#define CEF_LIBCEF_BROWSER_BROWSER_INFO_H_
#pragma once

#include <set>
#include <unordered_map>

#include "include/internal/cef_ptr.h"
#include "libcef/common/values_impl.h"

#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/values.h"

namespace content {
class RenderFrameHost;
}

class CefBrowserHostImpl;
class CefFrameHostImpl;

// CefBrowserInfo is used to associate a browser ID and render view/process
// IDs with a particular CefBrowserHostImpl. Render view/process IDs may change
// during the lifetime of a single CefBrowserHostImpl.
//
// CefBrowserInfo objects are managed by CefBrowserInfoManager and should not be
// created directly.
class CefBrowserInfo : public base::RefCountedThreadSafe<CefBrowserInfo> {
 public:
  CefBrowserInfo(int browser_id,
                 bool is_popup,
                 bool is_windowless,
                 CefRefPtr<CefDictionaryValue> extra_info);

  int browser_id() const { return browser_id_; }
  bool is_popup() const { return is_popup_; }
  bool is_windowless() const { return is_windowless_; }
  CefRefPtr<CefDictionaryValue> extra_info() const { return extra_info_; }

  // May return NULL if the browser has not yet been created or if the browser
  // has been destroyed.
  CefRefPtr<CefBrowserHostImpl> browser() const;

  // Set or clear the browser. Called from the CefBrowserHostImpl constructor
  // (to set) and DestroyBrowser (to clear).
  void SetBrowser(CefRefPtr<CefBrowserHostImpl> browser);

  // Ensure that a frame record exists for |host|. Called for the main frame
  // when the RenderView is created, or for a sub-frame when the associated
  // RenderFrame is created in the renderer process.
  // Called from CefBrowserHostImpl::RenderFrameCreated (is_guest_view = false)
  // or CefMimeHandlerViewGuestDelegate::OnGuestAttached (is_guest_view = true).
  void MaybeCreateFrame(content::RenderFrameHost* host, bool is_guest_view);

  // Remove the frame record for |host|. Called for the main frame when the
  // RenderView is destroyed, or for a sub-frame when the associated RenderFrame
  // is destroyed in the renderer process.
  // Called from CefBrowserHostImpl::FrameDeleted or
  // CefMimeHandlerViewGuestDelegate::OnGuestDetached.
  void RemoveFrame(content::RenderFrameHost* host);

  // Returns the main frame object. This object will remain valid until the
  // browser is destroyed even though the indentifier may change with cross-
  // origin navigations. Furthermore, calling LoadURL on this object will always
  // behave as expected because the call is routed through the browser's
  // NavigationController.
  CefRefPtr<CefFrameHostImpl> GetMainFrame();

  // Creates a temporary sub-frame object for situations during navigation or
  // resource loading where a RFH does not yet exist. If |parent_frame_id|
  // is invalid the current main frame will be specified as the parent.
  // Temporary frame objects are not tracked but will be implicitly detached
  // on browser destruction.
  CefRefPtr<CefFrameHostImpl> CreateTempSubFrame(int64_t parent_frame_id);

  // Returns the frame object matching the specified host or nullptr if no match
  // is found. Nullptr will also be returned if a guest view match is found
  // because we don't create frame objects for guest views. If |is_guest_view|
  // is non-nullptr it will be set to true in this case. Must be called on the
  // UI thread.
  CefRefPtr<CefFrameHostImpl> GetFrameForHost(
      const content::RenderFrameHost* host,
      bool* is_guest_view = nullptr) const;

  // Returns the frame object matching the specified IDs or nullptr if no match
  // is found. Nullptr will also be returned if a guest view match is found
  // because we don't create frame objects for guest views. If |is_guest_view|
  // is non-nullptr it will be set to true in this case. Safe to call from any
  // thread.
  CefRefPtr<CefFrameHostImpl> GetFrameForRoute(
      int32_t render_process_id,
      int32_t render_routing_id,
      bool* is_guest_view = nullptr) const;

  // Returns the frame object matching the specified ID or nullptr if no match
  // is found. Nullptr will also be returned if a guest view match is found
  // because we don't create frame objects for guest views. If |is_guest_view|
  // is non-nullptr it will be set to true in this case. Safe to call from any
  // thread.
  CefRefPtr<CefFrameHostImpl> GetFrameForId(
      int64_t frame_id,
      bool* is_guest_view = nullptr) const;

  // Returns the frame object matching the specified ID or nullptr if no match
  // is found. Nullptr will also be returned if a guest view match is found
  // because we don't create frame objects for guest views. If |is_guest_view|
  // is non-nullptr it will be set to true in this case. Safe to call from any
  // thread.
  CefRefPtr<CefFrameHostImpl> GetFrameForFrameTreeNode(
      int frame_tree_node_id,
      bool* is_guest_view = nullptr) const;

  // Returns all non-speculative frame objects that currently exist. Guest views
  // will be excluded because they don't have a frame object. Safe to call from
  // any thread.
  typedef std::set<CefRefPtr<CefFrameHostImpl>> FrameHostList;
  FrameHostList GetAllFrames() const;

 private:
  friend class base::RefCountedThreadSafe<CefBrowserInfo>;

  virtual ~CefBrowserInfo();

  struct FrameInfo {
    ~FrameInfo();

    content::RenderFrameHost* host_;
    int64_t frame_id_;  // Combination of render_process_id + render_routing_id.
    int frame_tree_node_id_;
    bool is_guest_view_;
    bool is_main_frame_;
    bool is_speculative_;
    CefRefPtr<CefFrameHostImpl> frame_;
  };

  void MaybeUpdateFrameTreeNodeIdMap(FrameInfo* info);

  CefRefPtr<CefFrameHostImpl> GetFrameForFrameTreeNodeInternal(
      int frame_tree_node_id,
      bool* is_guest_view = nullptr) const;

  void RemoveAllFrames();

  int browser_id_;
  bool is_popup_;
  bool is_windowless_;
  CefRefPtr<CefDictionaryValue> extra_info_;

  mutable base::Lock lock_;

  // The below members must be protected by |lock_|.

  CefRefPtr<CefBrowserHostImpl> browser_;

  // Owner of FrameInfo structs.
  typedef std::set<std::unique_ptr<FrameInfo>, base::UniquePtrComparator>
      FrameInfoSet;
  FrameInfoSet frame_info_set_;

  // Map a frame ID (e.g. MakeFrameId(process_id, routing_id)) to one frame.
  typedef std::unordered_map<int64_t, FrameInfo*> FrameIDMap;
  FrameIDMap frame_id_map_;

  // Map a frame_tree_node_id to one frame.
  typedef std::unordered_map<int, FrameInfo*> FrameTreeNodeIDMap;
  FrameTreeNodeIDMap frame_tree_node_id_map_;

  // The current main frame.
  CefRefPtr<CefFrameHostImpl> main_frame_;

  DISALLOW_COPY_AND_ASSIGN(CefBrowserInfo);
};

#endif  // CEF_LIBCEF_BROWSER_BROWSER_INFO_H_
