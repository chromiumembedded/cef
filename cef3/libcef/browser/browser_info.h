// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_BROWSER_INFO_H_
#define CEF_LIBCEF_BROWSER_BROWSER_INFO_H_
#pragma once

#include "libcef/browser/browser_host_impl.h"
#include "base/memory/ref_counted.h"

// CefBrowserInfo is used to associate a browser ID and render view/process
// IDs with a particular CefBrowserHostImpl. Render view/process IDs may change
// during the lifetime of a single CefBrowserHostImpl.
//
// CefBrowserInfo objects are managed by CefContentBrowserClient and should not
// be created directly.
class CefBrowserInfo : public base::RefCountedThreadSafe<CefBrowserInfo> {
 public:
  CefBrowserInfo(int browser_id, bool is_popup);
  virtual ~CefBrowserInfo();

  int browser_id() const { return browser_id_; };
  bool is_popup() const { return is_popup_; }
  
  void set_render_ids(int render_process_id, int render_view_id);

  // Returns true if this browser matches the specified ID values. If
  // |render_view_id| is 0 any browser with the specified |render_process_id|
  // will match.
  bool is_render_id_match(int render_process_id, int render_view_id);

  CefRefPtr<CefBrowserHostImpl> browser();
  void set_browser(CefRefPtr<CefBrowserHostImpl> browser);

 private:
  int browser_id_;
  bool is_popup_;

  base::Lock lock_;

  // The below members must be protected by |lock_|.
  int render_process_id_;
  int render_view_id_;

  // May be NULL if the browser has not yet been created or if the browser has
  // been destroyed.
  CefRefPtr<CefBrowserHostImpl> browser_;

  DISALLOW_COPY_AND_ASSIGN(CefBrowserInfo);
};

#endif  // CEF_LIBCEF_BROWSER_BROWSER_INFO_H_
