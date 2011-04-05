// Copyright (c) 2011 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _BROWSER_DRAG_DELEGATE_WIN_H
#define _BROWSER_DRAG_DELEGATE_WIN_H
#pragma once

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/platform_thread.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDragOperation.h"
#include "ui/base/dragdrop/os_exchange_data_provider_win.h"
#include "ui/gfx/point.h"

class BrowserWebViewDelegate;
class DragDropThread;
class WebDragSource;
struct WebDropData;

// Windows-specific drag-and-drop handling.
// If we are dragging a virtual file out of the browser, we use a background
// thread to do the drag-and-drop because we do not want to run nested
// message loop in the UI thread. For all other cases, the drag-and-drop happens
// in the UI thread.
class BrowserDragDelegate
    : public ui::DataObjectImpl::Observer,
      public base::RefCountedThreadSafe<BrowserDragDelegate> {
 public:
  explicit BrowserDragDelegate(BrowserWebViewDelegate* view);
  virtual ~BrowserDragDelegate();

  // Called on UI thread.
  void StartDragging(const WebDropData& drop_data,
                     WebKit::WebDragOperationsMask ops,
                     const SkBitmap& image,
                     const gfx::Point& image_offset);
  void CancelDrag();

  // DataObjectImpl::Observer implementation.
  // Called on drag-and-drop thread.
  virtual void OnWaitForData();
  virtual void OnDataObjectDisposed();

 private:
  // Called on either UI thread or drag-and-drop thread.
  void PrepareDragForDownload(const WebDropData& drop_data,
                              ui::OSExchangeData* data,
                              const GURL& page_url,
                              const std::string& page_encoding);
  void PrepareDragForFileContents(const WebDropData& drop_data,
                                  ui::OSExchangeData* data);
  void PrepareDragForUrl(const WebDropData& drop_data,
                         ui::OSExchangeData* data);
  void DoDragging(const WebDropData& drop_data,
                  WebKit::WebDragOperationsMask ops,
                  const GURL& page_url,
                  const std::string& page_encoding,
                  const SkBitmap& image,
                  const gfx::Point& image_offset);

  // Called on drag-and-drop thread.
  void StartBackgroundDragging(const WebDropData& drop_data,
                               WebKit::WebDragOperationsMask ops,
                               const GURL& page_url,
                               const std::string& page_encoding,
                               const SkBitmap& image,
                               const gfx::Point& image_offset);
  // Called on UI thread.
  void EndDragging(bool restore_suspended_state);
  void CloseThread();

  // For debug check only. Access only on drag-and-drop thread.
  base::PlatformThreadId drag_drop_thread_id_;

  // All the member variables below are accessed on UI thread.

  // Keep track of the BrowserWebViewDelegate it is associated with.
  BrowserWebViewDelegate* view_;

  // |drag_source_| is our callback interface passed to the system when we
  // want to initiate a drag and drop operation.  We use it to tell if a
  // drag operation is happening.
  scoped_refptr<WebDragSource> drag_source_;

  // The thread used by the drag-out download. This is because we want to avoid
  // running nested message loop in main UI thread.
  scoped_ptr<DragDropThread> drag_drop_thread_;

  // The flag to guard that EndDragging is not called twice.
  bool drag_ended_;

  // Keep track of the old suspended state of the drop target.
  bool old_drop_target_suspended_state_;

  DISALLOW_COPY_AND_ASSIGN(BrowserDragDelegate);
};


#endif  // _BROWSER_DRAG_DELEGATE_WIN_H
