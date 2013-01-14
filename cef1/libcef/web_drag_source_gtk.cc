// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/web_drag_source_gtk.h"

#include <gdk/gdk.h>
#include <string>

#include "libcef/browser_impl.h"
#include "libcef/cef_context.h"
#include "libcef/drag_data_impl.h"
#include "libcef/web_drop_target_gtk.h"

#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebDragData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebPoint.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebImage.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider_gtk.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/gtk_dnd_util.h"
#include "ui/gfx/gtk_util.h"
#include "webkit/glue/webdropdata.h"
#include "webkit/glue/window_open_disposition.h"


using WebKit::WebDragOperation;
using WebKit::WebDragOperationNone;
using WebKit::WebDragOperationCopy;
using WebKit::WebDragOperationLink;
using WebKit::WebDragOperationMove;
using WebKit::WebDragOperationGeneric;
using WebKit::WebPoint;
using WebKit::WebView;
using WebKit::WebDragOperationsMask;
using ui::DragDropTypes;

namespace {

void drag_end(GtkWidget* widget, GdkDragContext* context,
              WebDragSource* user_data) {
  user_data->OnDragEnd(widget, context);
}

bool drag_failed(GtkWidget* widget, GdkDragContext* context,
                 GtkDragResult result, WebDragSource* user_data) {
  return user_data->OnDragFailed(widget, context, result);
}

void drag_data_get(GtkWidget* widget, GdkDragContext* context,
                   GtkSelectionData* selection_data, guint target_type,
                   guint time, WebDragSource* user_data) {
  user_data->OnDragDataGet(widget, context, selection_data, target_type, time);
}

}  // namespace

WebDragSource::WebDragSource(CefBrowserImpl* browser)
    : browser_(browser) {
  widget_ = gtk_invisible_new();
  // At some point we might want to listen to drag-begin
  g_signal_connect(widget_, "drag-end", G_CALLBACK(&drag_end), this);
  g_signal_connect(widget_, "drag-failed", G_CALLBACK(&drag_failed), this);
  g_signal_connect(widget_, "drag-data-get", G_CALLBACK(&drag_data_get), this);
}

WebDragSource::~WebDragSource() {
  gtk_widget_destroy(widget_);
}

void WebDragSource::StartDragging(const WebDropData& drop_data,
                                  WebDragOperationsMask mask,
                                  const WebKit::WebImage& image,
                                  const WebKit::WebPoint& image_offset) {
  drop_data_.reset(new WebDropData(drop_data));
  int targets_mask = 0;
  if (!drop_data.text.is_null() && !drop_data.text.string().empty())
    targets_mask |= ui::TEXT_PLAIN;
  if (drop_data.url.is_valid()) {
    targets_mask |= ui::TEXT_URI_LIST;
    targets_mask |= ui::CHROME_NAMED_URL;
    targets_mask |= ui::NETSCAPE_URL;
  }
  if (!drop_data.html.is_null() && !drop_data.html.string().empty())
    targets_mask |= ui::TEXT_HTML;
  GtkTargetList* tl = ui::GetTargetListFromCodeMask(targets_mask);

  GdkEvent* event = gtk_get_current_event();
  GdkDragContext* context = gtk_drag_begin(widget_, tl,
      (GdkDragAction)DragDropTypes::DragOperationToGdkDragAction(mask), 1,
      event);
  if (!image.isNull()) {
    const SkBitmap& bitmap = image.getSkBitmap();
    GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(bitmap);
    gtk_drag_set_icon_pixbuf(context, pixbuf, image_offset.x, image_offset.y);
    g_object_unref(pixbuf);
  } else {
    gtk_drag_set_icon_default(context);
  }
  gdk_event_free(event);
}

void WebDragSource::OnDragEnd(GtkWidget* widget, GdkDragContext* context) {
  gfx::Point client(0, 0);
  gfx::Point screen(0, 0);
  getView()->dragSourceEndedAt(client, screen, WebDragOperationNone);
  getView()->dragSourceSystemDragEnded();
}

bool WebDragSource::OnDragFailed(GtkWidget* widget, GdkDragContext* context,
                                 GtkDragResult result) {
  gfx::Point client(0, 0);
  gfx::Point screen(0, 0);
  getView()->dragSourceEndedAt(client, screen, WebDragOperationNone);
  return FALSE;
}

void WebDragSource::OnDragDataGet(GtkWidget* sender, GdkDragContext* context,
                                  GtkSelectionData* selection_data,
                                  guint target_type, guint time) {
  switch (target_type) {
    case ui::TEXT_PLAIN: {
      std::string utf8_text = drop_data_->text.is_null() ?
          std::string() : UTF16ToUTF8(drop_data_->text.string());
      gtk_selection_data_set_text(selection_data, utf8_text.c_str(),
                                  utf8_text.length());
      break;
    }

    case ui::TEXT_HTML: {
      std::string utf8_text = drop_data_->html.is_null() ?
          std::string() : UTF16ToUTF8(drop_data_->html.string());
      gtk_selection_data_set(selection_data,
                             ui::GetAtomForTarget(ui::TEXT_HTML),
                             8,
                             reinterpret_cast<const guchar*>(utf8_text.c_str()),
                             utf8_text.length());
      break;
    }

    case ui::TEXT_URI_LIST:
    case ui::CHROME_NAMED_URL:
    case ui::NETSCAPE_URL: {
      ui::WriteURLWithName(selection_data, drop_data_->url,
                           drop_data_->url_title, target_type);
      break;
    }

    default:
      NOTREACHED();
  }
}

WebKit::WebView* WebDragSource::getView() {
  return browser_->UIT_GetWebView();
}

