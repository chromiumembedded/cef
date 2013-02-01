// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/web_drop_target_gtk.h"

#include <gdk/gdk.h>
#include <string>

#include "libcef/browser_impl.h"
#include "libcef/cef_context.h"
#include "libcef/drag_data_impl.h"

#include "base/utf_string_conversions.h"
#include "base/bind.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebDragData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebPoint.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider_gtk.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/gtk_dnd_util.h"
#include "ui/base/gtk/gtk_screen_util.h"
#include "webkit/glue/webdropdata.h"
#include "webkit/glue/window_open_disposition.h"

using WebKit::WebDragOperation;
using WebKit::WebDragOperationNone;
using WebKit::WebDragOperationCopy;
using WebKit::WebDragOperationLink;
using WebKit::WebDragOperationMove;
using WebKit::WebDragOperationGeneric;
using WebKit::WebDragOperationsMask;
using WebKit::WebPoint;
using WebKit::WebView;
using ui::DragDropTypes;

namespace {

/// GTK callbacks
gboolean drag_motion(GtkWidget* widget, GdkDragContext* context, gint x,
                     gint y, guint time, WebDropTarget* user_data) {
  return user_data->OnDragMove(widget, context, x, y, time);
}

void drag_leave(GtkWidget* widget, GdkDragContext* context, guint time,
                WebDropTarget* user_data) {
  user_data->OnDragLeave(widget, context, time);
}

gboolean drag_drop(GtkWidget* widget, GdkDragContext* context, gint x,
                   gint y, guint time, WebDropTarget* user_data) {
  return user_data->OnDragDrop(widget, context, x, y, time);
}

void drag_data_received(GtkWidget* widget, GdkDragContext* context,
                        gint x, gint y, GtkSelectionData* data, guint info,
                        guint time, WebDropTarget* user_data) {
  user_data->OnDragDataReceived(widget, context, x, y, data, info, time);
}

int supported_targets[] = {
  ui::TEXT_PLAIN,
  ui::TEXT_URI_LIST,
  ui::TEXT_HTML,
  ui::NETSCAPE_URL,
  ui::CHROME_NAMED_URL,
  ui::TEXT_PLAIN_NO_CHARSET,
};

WebDragOperationsMask _mask(GdkDragContext* context) {
  GdkDragAction propsed_action = context->suggested_action;
  return (WebDragOperationsMask)DragDropTypes::GdkDragActionToDragOperation(
      propsed_action);
}

}  // namespace

WebDropTarget::WebDropTarget(CefBrowserImpl* browser)
    : browser_(browser),
      data_requests_(0),
      context_(NULL),
      sent_drag_enter_(false),
      method_factory_(this) {
  GtkWidget* widget = browser->UIT_GetWebViewHost()->view_handle();
  gtk_drag_dest_set(widget, (GtkDestDefaults)0, NULL, 0,
      static_cast<GdkDragAction>(GDK_ACTION_COPY | GDK_ACTION_LINK |
                                 GDK_ACTION_MOVE));
  g_signal_connect(widget, "drag-motion", G_CALLBACK(&drag_motion), this);
  g_signal_connect(widget, "drag-leave", G_CALLBACK(&drag_leave), this);
  g_signal_connect(widget, "drag-drop", G_CALLBACK(&drag_drop), this);
  g_signal_connect(widget, "drag-data-received",
      G_CALLBACK(&drag_data_received), this);
}

WebDropTarget::~WebDropTarget() {
}

WebKit::WebView* WebDropTarget::getView() {
  return browser_->UIT_GetWebView();
}

BrowserWebViewDelegate* WebDropTarget::getDelegate() {
  return browser_->UIT_GetWebViewDelegate();
}

bool WebDropTarget::OnDragMove(GtkWidget* widget, GdkDragContext* context,
                               gint x, gint y, guint time) {
  WebView* webview = getView();
  gint widget_x, widget_y;
  WebDragOperation operation;
  gtk_widget_translate_coordinates(gtk_widget_get_toplevel(widget), widget, x,
      y, &widget_x, &widget_y);

  // Request all the data and potentially start the DnD.
  if (context_ != context) {
    drop_data_.reset(new WebDropData);
    data_requests_ = arraysize(supported_targets);
    for (size_t i = 0; i < arraysize(supported_targets); ++i) {
      gtk_drag_get_data(widget, context,
                        ui::GetAtomForTarget(supported_targets[i]),
                        time);
    }
  } else if (data_requests_ == 0) {
    operation = webview->dragTargetDragOver(WebPoint(x, y),
                                  WebPoint(widget_x, widget_y),
                                  _mask(context));
    gdk_drag_status(context,
        (GdkDragAction)DragDropTypes::DragOperationToGdkDragAction(operation),
        time);
  }
  return TRUE;
}

// Real DragLeave
void WebDropTarget::DragLeave() {
  WebView* webview = getView();
  webview->dragTargetDragLeave();
}

// GTK Sends DragDrop (immediately) after DragLeave
// So post re-post DragLeave allowing us to behave like chromium expects.
void WebDropTarget::OnDragLeave(GtkWidget* widget, GdkDragContext* context,
                                guint time) {
  context_ = NULL;
  drop_data_.reset();

  // Don't send the drag leave if we didn't send the drag enter.
  if (!sent_drag_enter_)
    return;

  // Sometimes we get a drag-leave event before getting a drag-data-received
  // event. In that case, we don't want to bother the renderer with a
  // DragLeave event.
  if (data_requests_ != 0)
    return;

  // When GTK sends us a drag-drop signal, it is shortly (and synchronously)
  // preceded by a drag-leave. The renderer doesn't like getting the signals
  // in this order so delay telling it about the drag-leave till we are sure
  // we are not getting a drop as well.
  MessageLoop::current()->PostTask(FROM_HERE,
      base::Bind(&WebDropTarget::DragLeave, method_factory_.GetWeakPtr()));
}

bool WebDropTarget::OnDragDrop(GtkWidget* widget, GdkDragContext* context,
                               gint x, gint y, guint time) {
  method_factory_.InvalidateWeakPtrs();

  gint widget_x, widget_y;
  gtk_widget_translate_coordinates(gtk_widget_get_toplevel(widget), widget, x,
      y, &widget_x, &widget_y);
  browser_->set_is_dropping(true);

  if (browser_->UIT_GetWebView()) {
    browser_->UIT_GetWebView()->dragTargetDrop(
        WebPoint(x, y),
        WebPoint(widget_x, widget_y));
  }
  browser_->set_is_dropping(false);
  context_ = NULL;
  gtk_drag_finish(context, TRUE, FALSE, time);

  return TRUE;
}


void WebDropTarget::OnDragDataReceived(GtkWidget* widget,
                                       GdkDragContext* context, gint x, gint y,
                                       GtkSelectionData* data, guint info,
                                       guint time) {
  WebDragOperation operation;
  data_requests_--;

  // If the source can't provide us with valid data for a requested target,
  // data->data will be NULL.
  if (data->data && data->length > 0) {
    if (data->target == ui::GetAtomForTarget(ui::TEXT_PLAIN) ||
        data->target == ui::GetAtomForTarget(ui::TEXT_PLAIN_NO_CHARSET)) {
      guchar* text = gtk_selection_data_get_text(data);
      if (text) {
        drop_data_->text =
            NullableString16(UTF8ToUTF16((const char*)text), false);
        g_free(text);
      }
    } else if (data->target == ui::GetAtomForTarget(ui::TEXT_URI_LIST)) {
      gchar** uris = gtk_selection_data_get_uris(data);
      if (uris) {
        drop_data_->url = GURL();
        for (gchar** uri_iter = uris; *uri_iter; uri_iter++) {
          // Most file managers populate text/uri-list with file URLs when
          // dragging files. To avoid exposing file system paths to web content,
          // file URLs are never set as the URL content for the drop.
          // TODO(estade): Can the filenames have a non-UTF8 encoding?
          GURL url(*uri_iter);
          FilePath file_path;
          if (url.SchemeIs("file") &&
              net::FileURLToFilePath(url, &file_path)) {
            drop_data_->filenames.push_back(
                WebDropData::FileInfo(UTF8ToUTF16(file_path.value()), 
                                      string16()));
            // This is a hack. Some file managers also populate text/plain with
            // a file URL when dragging files, so we clear it to avoid exposing
            // it to the web content.
            // drop_data_->text = NullableString16(true);
          } else if (!drop_data_->url.is_valid()) {
            // Also set the first non-file URL as the URL content for the drop.
            drop_data_->url = url;
          }
        }
        g_strfreev(uris);
      }
    } else if (data->target == ui::GetAtomForTarget(ui::TEXT_HTML)) {
      drop_data_->html = NullableString16(
          UTF8ToUTF16(std::string(reinterpret_cast<char*>(data->data),
                                  data->length)), false);
      // We leave the base URL empty.
    } else if (data->target == ui::GetAtomForTarget(ui::NETSCAPE_URL)) {
      std::string netscape_url(reinterpret_cast<char*>(data->data),
                               data->length);
      size_t split = netscape_url.find_first_of('\n');
      if (split != std::string::npos) {
        drop_data_->url = GURL(netscape_url.substr(0, split));
        if (split < netscape_url.size() - 1)
          drop_data_->url_title = UTF8ToUTF16(netscape_url.substr(split + 1));
      }
    } else if (data->target == ui::GetAtomForTarget(ui::CHROME_NAMED_URL)) {
      ui::ExtractNamedURL(data, &drop_data_->url, &drop_data_->url_title);
    }
  }

  if (data_requests_ == 0) {
    CefRefPtr<CefClient> client = browser_->GetClient();
    if (client.get()) {
      CefRefPtr<CefDragHandler> handler = client->GetDragHandler();
      if (handler.get()) {
        CefRefPtr<CefDragData> data(new CefDragDataImpl(*drop_data_.get()));
        if (handler->OnDragEnter(browser_, data,
                (cef_drag_operations_mask_t)_mask(context))) {
          operation = WebDragOperationNone;
          sent_drag_enter_ = false;
          gdk_drag_status(context,
              (GdkDragAction)DragDropTypes::DragOperationToGdkDragAction(
                  operation),
              time);
          return;
        }
      }
    }
    gint widget_x, widget_y;
    gtk_widget_translate_coordinates(gtk_widget_get_toplevel(widget), widget,
        x, y, &widget_x, &widget_y);
    WebView* webview = getView();
    sent_drag_enter_ = true;
    operation = webview->dragTargetDragEnter(drop_data_->ToDragData(),
                                   WebPoint(x, y),
                                   WebPoint(widget_x, widget_y),
                                   _mask(context));
    gdk_drag_status(context,
        (GdkDragAction)DragDropTypes::DragOperationToGdkDragAction(operation),
        time);
    context_ = context;
  }
}
