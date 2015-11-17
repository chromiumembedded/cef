// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_BROWSER_INFO_H_
#define CEF_LIBCEF_BROWSER_BROWSER_INFO_H_
#pragma once

#include <set>

#include "include/internal/cef_ptr.h"

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"

class CefBrowserHostImpl;

// CefBrowserInfo is used to associate a browser ID and render view/process
// IDs with a particular CefBrowserHostImpl. Render view/process IDs may change
// during the lifetime of a single CefBrowserHostImpl.
//
// CefBrowserInfo objects are managed by CefContentBrowserClient and should not
// be created directly.
class CefBrowserInfo : public base::RefCountedThreadSafe<CefBrowserInfo> {
 public:
  class RenderIDManager {
   public:
    explicit RenderIDManager(base::Lock* lock);

    // Adds an ID pair if it doesn't already exist.
    void add_render_view_id(int render_process_id, int render_routing_id);
    void add_render_frame_id(int render_process_id, int render_routing_id);

    // Remove an ID pair if it exists.
    void remove_render_view_id(int render_process_id, int render_routing_id);
    void remove_render_frame_id(int render_process_id, int render_routing_id);

    // Returns true if this browser matches the specified ID pair.
    bool is_render_view_id_match(int render_process_id,
                                 int render_routing_id) const;
    bool is_render_frame_id_match(int render_process_id,
                                  int render_routing_id)const ;

   private:
    typedef std::set<std::pair<int, int> > RenderIdSet;

    void add_render_id(RenderIdSet* id_set,
                       int render_process_id,
                       int render_routing_id);
    void remove_render_id(RenderIdSet* id_set,
                          int render_process_id,
                          int render_routing_id);
    bool is_render_id_match(const RenderIdSet* id_set,
                            int render_process_id,
                            int render_routing_id) const;

    mutable base::Lock* lock_;

    // The below members must be protected by |lock_|.

    // Set of mapped (process_id, routing_id) pairs. Keeping this set is
    // necessary for the following reasons:
    // 1. When navigating cross-origin the new (pending) RenderViewHost will be
    //    created before the old (current) RenderViewHost is destroyed.
    // 2. When canceling and asynchronously continuing navigation of the same
    //    URL a new RenderViewHost may be created for the first (canceled)
    //    navigation and then destroyed as a result of the second (allowed)
    //    navigation.
    // 3. Out-of-process iframes have their own render IDs which must also be
    //    associated with the host browser.
    RenderIdSet render_view_id_set_;
    RenderIdSet render_frame_id_set_;
  };

  CefBrowserInfo(int browser_id, bool is_popup);

  int browser_id() const { return browser_id_; };
  bool is_popup() const { return is_popup_; }
  bool is_windowless() const { return is_windowless_; }

  void set_windowless(bool windowless);

  // Returns the render ID manager for this browser.
  RenderIDManager* render_id_manager() {
    return &render_id_manager_;
  }

  // Returns the render ID manager for guest views owned by this browser.
  RenderIDManager* guest_render_id_manager() {
    return &guest_render_id_manager_;
  }

  CefRefPtr<CefBrowserHostImpl> browser() const;
  void set_browser(CefRefPtr<CefBrowserHostImpl> browser);

 private:
  friend class base::RefCountedThreadSafe<CefBrowserInfo>;

  ~CefBrowserInfo();

  int browser_id_;
  bool is_popup_;
  bool is_windowless_;

  mutable base::Lock lock_;

  // The below members must be protected by |lock_|.

  RenderIDManager render_id_manager_;
  RenderIDManager guest_render_id_manager_;

  // May be NULL if the browser has not yet been created or if the browser has
  // been destroyed.
  CefRefPtr<CefBrowserHostImpl> browser_;

  DISALLOW_COPY_AND_ASSIGN(CefBrowserInfo);
};

#endif  // CEF_LIBCEF_BROWSER_BROWSER_INFO_H_
