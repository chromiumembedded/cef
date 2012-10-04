// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_WEB_DRAG_SOURCE_GTK_H_
#define CEF_LIBCEF_WEB_DRAG_SOURCE_GTK_H_

#include <gtk/gtk.h>
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDragOperation.h"

class CefBrowserImpl;
class WebDropData;

namespace WebKit {
class WebImage;
class WebPoint;
class WebView;
}

class WebDragSource : public base::RefCounted<WebDragSource> {
 public:
  explicit WebDragSource(CefBrowserImpl* browser);
  virtual ~WebDragSource();

  void StartDragging(const WebDropData& drop_data,
                     WebKit::WebDragOperationsMask mask,
                     const WebKit::WebImage& image,
                     const WebKit::WebPoint& image_offset);

  void OnDragEnd(GtkWidget* widget, GdkDragContext* context);
  bool OnDragFailed(GtkWidget* widget, GdkDragContext* context,
                    GtkDragResult result);
  void OnDragDataGet(GtkWidget* sender, GdkDragContext* context,
                     GtkSelectionData* selection_data, guint target_type,
                     guint time);

 private:
  WebKit::WebView* getView();
  scoped_ptr<WebDropData> drop_data_;
  CefBrowserImpl* browser_;
  GtkWidget* widget_;
};

#endif  // CEF_LIBCEF_WEB_DRAG_SOURCE_GTK_H_
