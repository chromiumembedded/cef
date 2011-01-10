// Copyright (c) 2011 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "browser_webview_delegate.h"
#include "browser_impl.h"

#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include "base/message_loop.h"
#include "gfx/gtk_util.h"
#include "gfx/point.h"
#include "third_party/WebKit/WebKit/chromium/public/WebCursorInfo.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebRect.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/WebKit/chromium/public/WebView.h"
#include "webkit/glue/webcursor.h"
#include "webkit/glue/webdropdata.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/glue/window_open_disposition.h"
#include "webkit/plugins/npapi/gtk_plugin_container_manager.h"
#include "webkit/plugins/npapi/plugin_list.h"
#include "webkit/plugins/npapi/webplugin.h"
#include "webkit/plugins/npapi/webplugin_delegate_impl.h"

using WebKit::WebCursorInfo;
using WebKit::WebFrame;
using WebKit::WebNavigationPolicy;
using WebKit::WebPopupMenuInfo;
using WebKit::WebRect;
using WebKit::WebWidget;
using WebKit::WebView;

namespace {

enum SelectionClipboardType {
  TEXT_HTML,
  PLAIN_TEXT,
};

GdkAtom GetTextHtmlAtom() {
  GdkAtom kTextHtmlGdkAtom = gdk_atom_intern_static_string("text/html");
  return kTextHtmlGdkAtom;
}

void SelectionClipboardGetContents(GtkClipboard* clipboard,
    GtkSelectionData* selection_data, guint info, gpointer data) {
  // Ignore formats that we don't know about.
  if (info != TEXT_HTML && info != PLAIN_TEXT)
    return;

  WebView* webview = static_cast<WebView*>(data);
  WebFrame* frame = webview->focusedFrame();
  if (!frame)
    frame = webview->mainFrame();
  DCHECK(frame);

  std::string selection;
  if (TEXT_HTML == info) {
    selection = frame->selectionAsMarkup().utf8();
  } else {
    selection = frame->selectionAsText().utf8();
  }
  if (TEXT_HTML == info) {
    gtk_selection_data_set(selection_data,
                           GetTextHtmlAtom(),
                           8 /* bits per data unit, ie, char */,
                           reinterpret_cast<const guchar*>(selection.data()),
                           selection.length());
  } else {
    gtk_selection_data_set_text(selection_data, selection.data(),
        selection.length());
  }
}

}  // namespace

// WebViewClient --------------------------------------------------------------

WebWidget* BrowserWebViewDelegate::createPopupMenu(
    const WebPopupMenuInfo& info) {
  NOTREACHED();
  return NULL;
}

void BrowserWebViewDelegate::showContextMenu(
    WebKit::WebFrame* frame, const WebKit::WebContextMenuData& data) {
  NOTIMPLEMENTED();
}

// WebWidgetClient ------------------------------------------------------------

void BrowserWebViewDelegate::show(WebNavigationPolicy policy) {
  WebWidgetHost* host = GetWidgetHost();
  GtkWidget* drawing_area = host->view_handle();
  GtkWidget* window =
      gtk_widget_get_parent(gtk_widget_get_parent(drawing_area));
  gtk_widget_show_all(window);
}

void BrowserWebViewDelegate::closeWidgetSoon() {
  if (this == browser_->GetWebViewDelegate()) {
    MessageLoop::current()->PostTask(FROM_HERE, NewRunnableFunction(
        &gtk_widget_destroy, GTK_WIDGET(browser_->GetMainWndHandle())));
  } else if (this == browser_->GetPopupDelegate()) {
    browser_->UIT_ClosePopupWidget();
  }
}

void BrowserWebViewDelegate::didChangeCursor(const WebCursorInfo& cursor_info) {
  current_cursor_.InitFromCursorInfo(cursor_info);
  GdkCursorType cursor_type =
      static_cast<GdkCursorType>(current_cursor_.GetCursorType());
  GdkCursor* gdk_cursor;
  if (cursor_type == GDK_CURSOR_IS_PIXMAP) {
    // TODO(port): WebKit bug https://bugs.webkit.org/show_bug.cgi?id=16388 is
    // that calling gdk_window_set_cursor repeatedly is expensive.  We should
    // avoid it here where possible.
    gdk_cursor = current_cursor_.GetCustomCursor();
  } else {
    // Optimize the common case, where the cursor hasn't changed.
    // However, we can switch between different pixmaps, so only on the
    // non-pixmap branch.
    if (cursor_type_ == cursor_type)
      return;
    if (cursor_type == GDK_LAST_CURSOR)
      gdk_cursor = NULL;
    else
      gdk_cursor = gfx::GetCursor(cursor_type);
  }
  cursor_type_ = cursor_type;
  gdk_window_set_cursor(browser_->GetWebViewWndHandle()->window, gdk_cursor);
}

WebRect BrowserWebViewDelegate::windowRect() {
  WebWidgetHost* host = GetWidgetHost();
  GtkWidget* drawing_area = host->view_handle();
  GtkWidget* vbox = gtk_widget_get_parent(drawing_area);
  GtkWidget* window = gtk_widget_get_parent(vbox);

  gint x, y;
  gtk_window_get_position(GTK_WINDOW(window), &x, &y);
  x += vbox->allocation.x + drawing_area->allocation.x;
  y += vbox->allocation.y + drawing_area->allocation.y;

  return WebRect(x, y,
                 drawing_area->allocation.width,
                 drawing_area->allocation.height);
}

void BrowserWebViewDelegate::setWindowRect(const WebRect& rect) {
  if (this == browser_->GetWebViewDelegate()) {
    // TODO(port): Set the window rectangle.
  } else if (this == browser_->GetPopupDelegate()) {
    WebWidgetHost* host = GetWidgetHost();
    GtkWidget* drawing_area = host->view_handle();
    GtkWidget* window =
        gtk_widget_get_parent(gtk_widget_get_parent(drawing_area));
    gtk_window_resize(GTK_WINDOW(window), rect.width, rect.height);
    gtk_window_move(GTK_WINDOW(window), rect.x, rect.y);
  }
}

WebRect BrowserWebViewDelegate::rootWindowRect() {
  if (WebWidgetHost* host = GetWidgetHost()) {
    // We are being asked for the x/y and width/height of the entire browser
    // window.  This means the x/y is the distance from the corner of the
    // screen, and the width/height is the size of the entire browser window.
    // For example, this is used to implement window.screenX and window.screenY.
    GtkWidget* drawing_area = host->view_handle();
    GtkWidget* window =
        gtk_widget_get_parent(gtk_widget_get_parent(drawing_area));
    gint x, y, width, height;
    gtk_window_get_position(GTK_WINDOW(window), &x, &y);
    gtk_window_get_size(GTK_WINDOW(window), &width, &height);
    return WebRect(x, y, width, height);
  }
  return WebRect();
}

WebRect BrowserWebViewDelegate::windowResizerRect() {
  // Not necessary on Linux.
  return WebRect();
}

void BrowserWebViewDelegate::runModal() {
  NOTIMPLEMENTED();
}

// WebPluginPageDelegate ------------------------------------------------------

webkit::npapi::WebPluginDelegate* BrowserWebViewDelegate::CreatePluginDelegate(
    const FilePath& path,
    const std::string& mime_type) {
  // TODO(evanm): we probably shouldn't be doing this mapping to X ids at
  // this level.
  GdkNativeWindow plugin_parent =
      GDK_WINDOW_XWINDOW(browser_->GetWebViewHost()->view_handle()->window);

  return webkit::npapi::WebPluginDelegateImpl::Create(path, mime_type,
      plugin_parent);
}

void BrowserWebViewDelegate::CreatedPluginWindow(
    gfx::PluginWindowHandle id) {
  browser_->GetWebViewHost()->CreatePluginContainer(id);
}

void BrowserWebViewDelegate::WillDestroyPluginWindow(
    gfx::PluginWindowHandle id) {
  browser_->GetWebViewHost()->DestroyPluginContainer(id);
}

void BrowserWebViewDelegate::DidMovePlugin(
    const webkit::npapi::WebPluginGeometry& move) {
  WebWidgetHost* host = GetWidgetHost();
  webkit::npapi::GtkPluginContainerManager* plugin_container_manager =
      static_cast<WebViewHost*>(host)->plugin_container_manager();
  plugin_container_manager->MovePluginContainer(move);
}

// Protected methods ----------------------------------------------------------

void BrowserWebViewDelegate::ShowJavaScriptAlert(
    WebKit::WebFrame* webframe, const CefString& message) {
  NOTIMPLEMENTED();
}

bool BrowserWebViewDelegate::ShowJavaScriptConfirm(
    WebKit::WebFrame* webframe, const CefString& message) {
  NOTIMPLEMENTED();
  return false;
}

bool BrowserWebViewDelegate::ShowJavaScriptPrompt(
    WebKit::WebFrame* webframe, const CefString& message,
    const CefString& default_value, CefString* result) {
  NOTIMPLEMENTED();
  return false;
}

// Called to show the file chooser dialog.
bool BrowserWebViewDelegate::ShowFileChooser(std::vector<FilePath>& file_names,
                                             const bool multi_select,
                                             const WebKit::WebString& title,
                                             const FilePath& default_file) {
  NOTIMPLEMENTED();
  return false;
}
