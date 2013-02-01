// Copyright (c) 2013 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/web_drop_target_gtk.h"

#include <gdk/gdk.h>
#include <string>

#include "libcef/browser_impl.h"
#include "libcef/cef_context.h"
#include "libcef/drag_data_impl.h"
#include "libcef/web_drag_utils_gtk.h"

#include "base/utf_string_conversions.h"
#include "base/bind.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebDragData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebPoint.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider_gtk.h"
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

namespace {

int GetModifierFlags(GtkWidget* widget) {
  int modifier_state = 0;
  GdkModifierType state;
  gdk_window_get_pointer(gtk_widget_get_window(widget), NULL, NULL, &state);

  if (state & GDK_SHIFT_MASK)
    modifier_state |= WebKit::WebInputEvent::ShiftKey;
  if (state & GDK_CONTROL_MASK)
    modifier_state |= WebKit::WebInputEvent::ControlKey;
  if (state & GDK_MOD1_MASK)
    modifier_state |= WebKit::WebInputEvent::AltKey;
  if (state & GDK_META_MASK)
    modifier_state |= WebKit::WebInputEvent::MetaKey;
  return modifier_state;
}

WebDragOperationsMask GetOperationsMask(GdkDragContext* context) {
  return web_drag_utils_gtk::GdkDragActionToWebDragOp(context->actions);
}

}  // namespace

WebDropTarget::WebDropTarget(CefBrowserImpl* browser)
    : browser_(browser),
      context_(NULL),
      data_requests_(0),
      is_drop_target_(false),
      sent_drag_enter_(false),
      method_factory_(this) {
  widget_ = browser->UIT_GetWebViewHost()->view_handle();
  gtk_drag_dest_set(widget_, static_cast<GtkDestDefaults>(0),
                    NULL, 0,
                    static_cast<GdkDragAction>(GDK_ACTION_COPY |
                                               GDK_ACTION_LINK |
                                               GDK_ACTION_MOVE));
  g_signal_connect(widget_, "drag-motion",
                   G_CALLBACK(OnDragMotionThunk), this);
  g_signal_connect(widget_, "drag-leave",
                   G_CALLBACK(OnDragLeaveThunk), this);
  g_signal_connect(widget_, "drag-drop",
                   G_CALLBACK(OnDragDropThunk), this);
  g_signal_connect(widget_, "drag-data-received",
                   G_CALLBACK(OnDragDataReceivedThunk), this);
  // TODO(tony): Need a drag-data-delete handler for moving content out of
  // the WebContents.  http://crbug.com/38989

  destroy_handler_ = g_signal_connect(
      widget_, "destroy", G_CALLBACK(gtk_widget_destroyed), &widget_);
}

WebDropTarget::~WebDropTarget() {
  if (widget_) {
    gtk_drag_dest_unset(widget_);
    g_signal_handler_disconnect(widget_, destroy_handler_);
  }
}

void WebDropTarget::DragLeave() {
  getView()->dragTargetDragLeave();

  drop_data_.reset();
}

WebKit::WebView* WebDropTarget::getView() {
  return browser_->UIT_GetWebView();
}

void WebDropTarget::UpdateDragStatus(WebDragOperation operation, gint time) {
  if (context_) {
    is_drop_target_ = operation != WebDragOperationNone;
    GdkDragAction action = web_drag_utils_gtk::WebDragOpToGdkDragAction(operation);
    gdk_drag_status(context_, action, time);
  }
}

gboolean WebDropTarget::OnDragMotion(GtkWidget* sender,
                                     GdkDragContext* context,
                                     gint x, gint y,
                                     guint time) {
  if (context_ != context) {
    context_ = context;
    drop_data_.reset(new WebDropData);
    is_drop_target_ = false;

    // text/plain must come before text/uri-list. This is a hack that works in
    // conjunction with OnDragDataReceived. Since some file managers populate
    // text/plain with file URLs when dragging files, we want to handle
    // text/uri-list after text/plain so that the plain text can be cleared if
    // it's a file drag.
    static int supported_targets[] = {
      ui::TEXT_PLAIN,
      ui::TEXT_URI_LIST,
      ui::TEXT_HTML,
      ui::NETSCAPE_URL,
      ui::CHROME_NAMED_URL,
      // TODO(estade): support image drags?
      ui::CUSTOM_DATA,
    };

    data_requests_ = arraysize(supported_targets);
    for (size_t i = 0; i < arraysize(supported_targets); ++i) {
      gtk_drag_get_data(widget_, context,
                        ui::GetAtomForTarget(supported_targets[i]),
                        time);
    }
  } else if (data_requests_ == 0) {
    WebDragOperation operation = getView()->dragTargetDragOver(
        ui::ClientPoint(widget_),
        ui::ScreenPoint(widget_),
        GetOperationsMask(context),
        GetModifierFlags(widget_));

    UpdateDragStatus(operation, time);
  }

  return TRUE;
}

void WebDropTarget::OnDragDataReceived(
    GtkWidget* sender, GdkDragContext* context, gint x, gint y,
    GtkSelectionData* data, guint info, guint time) {
  // We might get the data from an old get_data() request that we no longer
  // care about.
  if (context != context_)
    return;

  data_requests_--;

  // Decode the data.
  gint data_length = gtk_selection_data_get_length(data);
  const guchar* raw_data = gtk_selection_data_get_data(data);
  GdkAtom target = gtk_selection_data_get_target(data);
  if (raw_data && data_length > 0) {
    // If the source can't provide us with valid data for a requested target,
    // raw_data will be NULL.
    if (target == ui::GetAtomForTarget(ui::TEXT_PLAIN)) {
      guchar* text = gtk_selection_data_get_text(data);
      if (text) {
        drop_data_->text = NullableString16(
            UTF8ToUTF16(std::string(reinterpret_cast<const char*>(text))),
            false);
        g_free(text);
      }
    } else if (target == ui::GetAtomForTarget(ui::TEXT_URI_LIST)) {
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
            drop_data_->text = NullableString16(true);
          } else if (!drop_data_->url.is_valid()) {
            // Also set the first non-file URL as the URL content for the drop.
            drop_data_->url = url;
          }
        }
        g_strfreev(uris);
      }
    } else if (target == ui::GetAtomForTarget(ui::TEXT_HTML)) {
      // TODO(estade): Can the html have a non-UTF8 encoding?
      drop_data_->html = NullableString16(
          UTF8ToUTF16(std::string(reinterpret_cast<const char*>(raw_data),
                                  data_length)),
          false);
      // We leave the base URL empty.
    } else if (target == ui::GetAtomForTarget(ui::NETSCAPE_URL)) {
      std::string netscape_url(reinterpret_cast<const char*>(raw_data),
                               data_length);
      size_t split = netscape_url.find_first_of('\n');
      if (split != std::string::npos) {
        drop_data_->url = GURL(netscape_url.substr(0, split));
        if (split < netscape_url.size() - 1)
          drop_data_->url_title = UTF8ToUTF16(netscape_url.substr(split + 1));
      }
    } else if (target == ui::GetAtomForTarget(ui::CHROME_NAMED_URL)) {
      ui::ExtractNamedURL(data, &drop_data_->url, &drop_data_->url_title);
    } else if (target == ui::GetAtomForTarget(ui::CUSTOM_DATA)) {
      ui::ReadCustomDataIntoMap(
          raw_data, data_length, &drop_data_->custom_data);
    }
  }

  if (data_requests_ == 0) {
    WebDragOperation operation = WebDragOperationNone;
    bool handled = false;

    CefRefPtr<CefClient> client = browser_->GetClient();
    if (client.get()) {
      CefRefPtr<CefDragHandler> handler = client->GetDragHandler();
      if (handler.get()) {
        CefRefPtr<CefDragData> data(new CefDragDataImpl(*drop_data_.get()));
        handled = handler->OnDragEnter(
            browser_, data,
            static_cast<cef_drag_operations_mask_t>(
                GetOperationsMask(context)));
      }
    }

    sent_drag_enter_ = !handled;
    if (!handled) {
      // Tell the renderer about the drag.
      // |x| and |y| are seemingly arbitrary at this point.
      operation = getView()->dragTargetDragEnter(
          drop_data_->ToDragData(),
          ui::ClientPoint(widget_),
          ui::ScreenPoint(widget_),
          GetOperationsMask(context),
          GetModifierFlags(widget_));
    }

    UpdateDragStatus(operation, time);
  }
}

// The drag has left our widget; forward this information to the renderer.
void WebDropTarget::OnDragLeave(GtkWidget* sender, GdkDragContext* context,
                                guint time) {
  // Set |context_| to NULL to make sure we will recognize the next DragMotion
  // as an enter.
  context_ = NULL;

  // Don't send the drag leave event if we didn't send a drag enter event.
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

// Called by GTK when the user releases the mouse, executing a drop.
gboolean WebDropTarget::OnDragDrop(GtkWidget* sender, GdkDragContext* context,
                                   gint x, gint y, guint time) {
  // Cancel that drag leave!
  method_factory_.InvalidateWeakPtrs();

  browser_->set_is_dropping(true);

  if (getView()) {
    getView()->dragTargetDrop(ui::ClientPoint(widget_),
                              ui::ScreenPoint(widget_),
                              GetModifierFlags(widget_));
  }

  browser_->set_is_dropping(false);

  // The second parameter is just an educated guess as to whether or not the
  // drag succeeded, but at least we will get the drag-end animation right
  // sometimes.
  gtk_drag_finish(context, is_drop_target_, FALSE, time);

  return TRUE;
}

