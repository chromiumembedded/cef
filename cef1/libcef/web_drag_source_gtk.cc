// Copyright (c) 2013 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/web_drag_source_gtk.h"

#include <gdk/gdk.h>
#include <string>

#include "libcef/browser_impl.h"
#include "libcef/cef_context.h"
#include "libcef/drag_data_impl.h"
#include "libcef/drag_download_file.h"
#include "libcef/drag_download_util.h"
#include "libcef/web_drag_utils_gtk.h"
#include "libcef/web_drop_target_gtk.h"

#include "base/nix/mime_util_xdg.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "net/base/file_stream.h"
#include "net/base/net_util.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebDragData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebPoint.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebImage.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/dragdrop/os_exchange_data_provider_gtk.h"
#include "ui/base/dragdrop/gtk_dnd_util.h"
#include "ui/base/gtk/gtk_screen_util.h"
#include "ui/gfx/gtk_util.h"
#include "webkit/glue/webdropdata.h"
#include "webkit/glue/window_open_disposition.h"

using WebKit::WebDragOperation;
using WebKit::WebDragOperationsMask;
using WebKit::WebDragOperationNone;
using WebKit::WebDragOperationCopy;
using WebKit::WebDragOperationLink;
using WebKit::WebDragOperationMove;
using WebKit::WebDragOperationGeneric;
using WebKit::WebPoint;
using WebKit::WebView;

WebDragSource::WebDragSource(CefBrowserImpl* browser)
    : browser_(browser),
      drag_pixbuf_(NULL),
      drag_failed_(false),
      drag_widget_(gtk_invisible_new()),
      drag_context_(NULL),
      drag_icon_(gtk_window_new(GTK_WINDOW_POPUP)) {
  signals_.Connect(drag_widget_, "drag-failed",
                   G_CALLBACK(OnDragFailedThunk), this);
  signals_.Connect(drag_widget_, "drag-begin",
                   G_CALLBACK(OnDragBeginThunk), this);
  signals_.Connect(drag_widget_, "drag-end",
                   G_CALLBACK(OnDragEndThunk), this);
  signals_.Connect(drag_widget_, "drag-data-get",
                   G_CALLBACK(OnDragDataGetThunk), this);

  signals_.Connect(drag_icon_, "expose-event",
                   G_CALLBACK(OnDragIconExposeThunk), this);
}

WebDragSource::~WebDragSource() {
  // Break the current drag, if any.
  if (drop_data_.get()) {
    gtk_grab_add(drag_widget_);
    gtk_grab_remove(drag_widget_);
    MessageLoopForUI::current()->RemoveObserver(this);
    drop_data_.reset();
  }

  gtk_widget_destroy(drag_widget_);
  gtk_widget_destroy(drag_icon_);
}

void WebDragSource::StartDragging(const WebDropData& drop_data,
                                  WebDragOperationsMask allowed_ops,
                                  GdkEventButton* last_mouse_down,
                                  const SkBitmap& image,
                                  const gfx::Vector2d& image_offset) {
  // Guard against re-starting before previous drag completed.
  if (drag_context_) {
    NOTREACHED();
    getView()->dragSourceSystemDragEnded();
    return;
  }

  int targets_mask = 0;

  if (!drop_data.text.string().empty())
    targets_mask |= ui::TEXT_PLAIN;
  if (drop_data.url.is_valid()) {
    targets_mask |= ui::TEXT_URI_LIST;
    targets_mask |= ui::CHROME_NAMED_URL;
    targets_mask |= ui::NETSCAPE_URL;
  }
  if (!drop_data.html.string().empty())
    targets_mask |= ui::TEXT_HTML;
  if (!drop_data.file_contents.empty())
    targets_mask |= ui::CHROME_WEBDROP_FILE_CONTENTS;
  if (!drop_data.download_metadata.empty() &&
      drag_download_util::ParseDownloadMetadata(drop_data.download_metadata,
                                                &wide_download_mime_type_,
                                                &download_file_name_,
                                                &download_url_)) {
    targets_mask |= ui::DIRECT_SAVE_FILE;
  }
  if (!drop_data.custom_data.empty())
    targets_mask |= ui::CUSTOM_DATA;

  // NOTE: Begin a drag even if no targets present. Otherwise, things like
  // draggable list elements will not work.

  drop_data_.reset(new WebDropData(drop_data));

  // The image we get from WebKit makes heavy use of alpha-shading. This looks
  // bad on non-compositing WMs. Fall back to the default drag icon.
  if (!image.isNull() && ui::IsScreenComposited())
    drag_pixbuf_ = gfx::GdkPixbufFromSkBitmap(image);
  image_offset_ = image_offset;

  GtkTargetList* list = ui::GetTargetListFromCodeMask(targets_mask);
  if (targets_mask & ui::CHROME_WEBDROP_FILE_CONTENTS) {
    // Looking up the mime type can hit the disk.  http://crbug.com/84896
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    drag_file_mime_type_ = gdk_atom_intern(
        base::nix::GetDataMimeType(drop_data.file_contents).c_str(), FALSE);
    gtk_target_list_add(list, drag_file_mime_type_,
                        0, ui::CHROME_WEBDROP_FILE_CONTENTS);
  }

  drag_failed_ = false;

  // If we don't pass an event, GDK won't know what event time to start grabbing
  // mouse events. Technically it's the mouse motion event and not the mouse
  // down event that causes the drag, but there's no reliable way to know
  // *which* motion event initiated the drag, so this will have to do.
  // TODO(estade): This can sometimes be very far off, e.g. if the user clicks
  // and holds and doesn't start dragging for a long time. I doubt it matters
  // much, but we should probably look into the possibility of getting the
  // initiating event from webkit.
  GdkDragAction action =
      web_drag_utils_gtk::WebDragOpToGdkDragAction(allowed_ops);
  drag_context_ = gtk_drag_begin(drag_widget_, list, action,
      1,  // Drags are always initiated by the left button.
      reinterpret_cast<GdkEvent*>(last_mouse_down));
  // The drag adds a ref; let it own the list.
  gtk_target_list_unref(list);

  // Sometimes the drag fails to start; |context| will be NULL and we won't
  // get a drag-end signal.
  if (!drag_context_) {
    drag_failed_ = true;
    drop_data_.reset();
    getView()->dragSourceSystemDragEnded();
    return;
  }

  MessageLoopForUI::current()->AddObserver(this);
}

WebKit::WebView* WebDragSource::getView() {
  return browser_->UIT_GetWebView();
}

GtkWidget* WebDragSource::getNativeView() {
  return browser_->UIT_GetWebViewHost()->view_handle();
}

void WebDragSource::WillProcessEvent(GdkEvent* event) {
  // No-op.
}

void WebDragSource::DidProcessEvent(GdkEvent* event) {
  if (event->type != GDK_MOTION_NOTIFY)
    return;

  GdkEventMotion* event_motion = reinterpret_cast<GdkEventMotion*>(event);
  gfx::Point client = ui::ClientPoint(getNativeView());
  gfx::Point screen(static_cast<int>(event_motion->x_root),
                    static_cast<int>(event_motion->y_root));

  getView()->dragSourceMovedTo(client, screen, WebDragOperationNone);
}

void WebDragSource::OnDragDataGet(GtkWidget* sender,
                                  GdkDragContext* context,
                                  GtkSelectionData* selection_data,
                                  guint target_type,
                                  guint time) {
  const int kBitsPerByte = 8;

  switch (target_type) {
    case ui::TEXT_PLAIN: {
      std::string utf8_text = UTF16ToUTF8(drop_data_->text.string());
      gtk_selection_data_set_text(selection_data, utf8_text.c_str(),
                                  utf8_text.length());
      break;
    }

    case ui::TEXT_HTML: {
      // TODO(estade): change relative links to be absolute using
      // |html_base_url|.
      std::string utf8_text = UTF16ToUTF8(drop_data_->html.string());
      gtk_selection_data_set(selection_data,
                             ui::GetAtomForTarget(ui::TEXT_HTML),
                             kBitsPerByte,
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

    case ui::CHROME_WEBDROP_FILE_CONTENTS: {
      gtk_selection_data_set(
          selection_data,
          drag_file_mime_type_, kBitsPerByte,
          reinterpret_cast<const guchar*>(drop_data_->file_contents.data()),
          drop_data_->file_contents.length());
      break;
    }

    case ui::DIRECT_SAVE_FILE: {
      char status_code = 'E';

      // Retrieves the full file path (in file URL format) provided by the
      // drop target by reading from the source window's XdndDirectSave0
      // property.
      gint file_url_len = 0;
      guchar* file_url_value = NULL;
      if (gdk_property_get(context->source_window,
                           ui::GetAtomForTarget(ui::DIRECT_SAVE_FILE),
                           ui::GetAtomForTarget(ui::TEXT_PLAIN_NO_CHARSET),
                           0,
                           1024,
                           FALSE,
                           NULL,
                           NULL,
                           &file_url_len,
                           &file_url_value) &&
          file_url_value) {
        // Convert from the file url to the file path.
        GURL file_url(std::string(reinterpret_cast<char*>(file_url_value),
                                  file_url_len));
        g_free(file_url_value);
        FilePath file_path;
        if (net::FileURLToFilePath(file_url, &file_path)) {
          // Open the file as a stream.
          scoped_ptr<net::FileStream> file_stream(
              drag_download_util::CreateFileStreamForDrop(&file_path));
          if (file_stream.get()) {
            WebView* webview = getView();
            const GURL& page_url = webview->mainFrame()->document().url();
            const std::string& page_encoding =
                webview->mainFrame()->document().encoding().utf8();

            // Start downloading the file to the stream.
            scoped_refptr<DragDownloadFile> drag_file_downloader =
                new DragDownloadFile(
                    file_path,
                    file_stream.Pass(),
                    download_url_,
                    page_url,
                    page_encoding,
                    browser_->UIT_GetWebViewDelegate());
            drag_file_downloader->Start(
                new drag_download_util::PromiseFileFinalizer(
                    drag_file_downloader));

            // Set the status code to success.
            status_code = 'S';
          }
        }

        // Return the status code to the file manager.
        gtk_selection_data_set(selection_data,
                               gtk_selection_data_get_target(selection_data),
                               kBitsPerByte,
                               reinterpret_cast<guchar*>(&status_code),
                               1);
      }
      break;
    }

    case ui::CUSTOM_DATA: {
      Pickle custom_data;
      ui::WriteCustomDataToPickle(drop_data_->custom_data, &custom_data);
      gtk_selection_data_set(
          selection_data,
          ui::GetAtomForTarget(ui::CUSTOM_DATA),
          kBitsPerByte,
          reinterpret_cast<const guchar*>(custom_data.data()),
          custom_data.size());
      break;
    }

    default:
      NOTREACHED();
  }
}

gboolean WebDragSource::OnDragFailed(GtkWidget* sender,
                                     GdkDragContext* context,
                                     GtkDragResult result) {
  drag_failed_ = true;

  GtkWidget* native_view = getNativeView();
  gfx::Point client = ui::ClientPoint(native_view);
  gfx::Point screen = ui::ScreenPoint(native_view);

  getView()->dragSourceEndedAt(client, screen, WebDragOperationNone);

  // Let the native failure animation run.
  return FALSE;
}

void WebDragSource::OnDragBegin(GtkWidget* sender,
                                GdkDragContext* drag_context) {
  if (!download_url_.is_empty()) {
    // Generate the file name based on both mime type and proposed file name.
    std::string default_name = "download";
    FilePath generated_download_file_name =
        net::GenerateFileName(download_url_,
                              std::string(),
                              std::string(),
                              download_file_name_.value(),
                              UTF16ToUTF8(wide_download_mime_type_),
                              default_name);

    // Pass the file name to the drop target by setting the source window's
    // XdndDirectSave0 property.
    gdk_property_change(drag_context->source_window,
                        ui::GetAtomForTarget(ui::DIRECT_SAVE_FILE),
                        ui::GetAtomForTarget(ui::TEXT_PLAIN_NO_CHARSET),
                        8,
                        GDK_PROP_MODE_REPLACE,
                        reinterpret_cast<const guchar*>(
                            generated_download_file_name.value().c_str()),
                        generated_download_file_name.value().length());
  }

  if (drag_pixbuf_) {
    gtk_widget_set_size_request(drag_icon_,
                                gdk_pixbuf_get_width(drag_pixbuf_),
                                gdk_pixbuf_get_height(drag_pixbuf_));

    // We only need to do this once.
    if (!gtk_widget_get_realized(drag_icon_)) {
      GdkScreen* screen = gtk_widget_get_screen(drag_icon_);
      GdkColormap* rgba = gdk_screen_get_rgba_colormap(screen);
      if (rgba)
        gtk_widget_set_colormap(drag_icon_, rgba);
    }

    gtk_drag_set_icon_widget(drag_context, drag_icon_,
                             image_offset_.x(), image_offset_.y());
  }
}

void WebDragSource::OnDragEnd(GtkWidget* sender,
                              GdkDragContext* drag_context) {
  if (drag_pixbuf_) {
    g_object_unref(drag_pixbuf_);
    drag_pixbuf_ = NULL;
  }

  MessageLoopForUI::current()->RemoveObserver(this);

  if (!download_url_.is_empty()) {
    gdk_property_delete(drag_context->source_window,
                        ui::GetAtomForTarget(ui::DIRECT_SAVE_FILE));
  }

  if (!drag_failed_) {
    GtkWidget* native_view = getNativeView();
    gfx::Point client = ui::ClientPoint(native_view);
    gfx::Point screen = ui::ScreenPoint(native_view);

    getView()->dragSourceEndedAt(client, screen,
        web_drag_utils_gtk::GdkDragActionToWebDragOp(drag_context->action));
  }

  getView()->dragSourceSystemDragEnded();

  drop_data_.reset();
  drag_context_ = NULL;
}

gboolean WebDragSource::OnDragIconExpose(GtkWidget* sender,
                                         GdkEventExpose* event) {
  cairo_t* cr = gdk_cairo_create(event->window);
  gdk_cairo_rectangle(cr, &event->area);
  cairo_clip(cr);
  cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
  gdk_cairo_set_source_pixbuf(cr, drag_pixbuf_, 0, 0);
  cairo_paint(cr);
  cairo_destroy(cr);

  return TRUE;
}

