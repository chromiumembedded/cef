// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_WEB_DROP_TARGET_GTK_H_
#define CEF_LIBCEF_WEB_DROP_TARGET_GTK_H_

#include <gtk/gtk.h>
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"

class BrowserWebViewDelegate;
class CefBrowserImpl;
class WebDropData;

namespace WebKit {
class WebView;
}

class WebDropTarget : public base::RefCounted<WebDropTarget> {
 public:
  explicit WebDropTarget(CefBrowserImpl* browser);
  virtual ~WebDropTarget();

  WebKit::WebView* getView();
  BrowserWebViewDelegate* getDelegate();

  // Called by GTK callbacks
  bool OnDragMove(GtkWidget* widget, GdkDragContext* context, gint x, gint y,
                  guint time);
  void OnDragLeave(GtkWidget* widget, GdkDragContext* context, guint time);
  bool OnDragDrop(GtkWidget* widget, GdkDragContext* context, gint x, gint y,
                  guint time);
  void OnDragEnd(GtkWidget* widget, GdkDragContext* context, guint time);
  void OnDragDataReceived(GtkWidget* widget, GdkDragContext* context, gint x,
                          gint y, GtkSelectionData* data, guint info,
                          guint time);
  void DragLeave();

 private:
  CefBrowserImpl* browser_;
  scoped_ptr<WebDropData> drop_data_;
  bool entered_;
  int data_requests_;
  GdkDragContext* context_;
  base::WeakPtrFactory<WebDropTarget> method_factory_;
};

#endif  // CEF_LIBCEF_WEB_DROP_TARGET_GTK_H_

