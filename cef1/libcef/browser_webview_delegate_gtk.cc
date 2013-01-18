// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser_webview_delegate.h"
#include "libcef/browser_impl.h"
#include "libcef/drag_data_impl.h"
#include "libcef/web_drop_target_gtk.h"
#include "libcef/web_drag_source_gtk.h"

#include "base/message_loop.h"
#include "base/string_util.h"
#include "net/base/net_errors.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebContextMenuData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCursorInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPopupMenu.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebDragData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebImage.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebPoint.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/gtk_util.h"
#include "ui/gfx/point.h"
#include "webkit/glue/webdropdata.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/window_open_disposition.h"
#include "webkit/plugins/npapi/plugin_list.h"
#include "webkit/plugins/npapi/webplugin.h"
#include "webkit/plugins/npapi/webplugin_delegate_impl.h"

#include <gdk/gdkx.h>  // NOLINT(build/include_order)
#include <gtk/gtk.h>  // NOLINT(build/include_order)


using webkit::npapi::WebPluginDelegateImpl;
using WebKit::WebContextMenuData;
using WebKit::WebCursorInfo;
using WebKit::WebDragData;
using WebKit::WebDragOperationsMask;
using WebKit::WebExternalPopupMenu;
using WebKit::WebExternalPopupMenuClient;
using WebKit::WebFrame;
using WebKit::WebImage;
using WebKit::WebNavigationPolicy;
using WebKit::WebPoint;
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
  if (TEXT_HTML == info)
    selection = frame->selectionAsMarkup().utf8();
  else
    selection = frame->selectionAsText().utf8();

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

void ShowJSAlertDialog(GtkWidget* window,
                       const gchar* title,
                       const gchar* message) {
  // Create the widgets.
  GtkWidget* dialog = gtk_dialog_new_with_buttons(
      title,
      GTK_WINDOW(window),
      GtkDialogFlags(GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL),
      GTK_STOCK_OK,
      GTK_RESPONSE_NONE,
      NULL);
  GtkWidget* content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget* label = gtk_label_new(message);

  // Add the label and show everything we've added to the dialog.
  gtk_container_add(GTK_CONTAINER(content_area), label);

  gtk_widget_show_all(dialog);

  gtk_dialog_run(GTK_DIALOG(dialog));

  gtk_widget_destroy(dialog);
}

bool ShowJSConfirmDialog(GtkWidget* window,
                         const gchar* title,
                         const gchar* message) {
  // Create the widgets.
  GtkWidget* dialog = gtk_dialog_new_with_buttons(
      title,
      GTK_WINDOW(window),
      GtkDialogFlags(GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL),
      GTK_STOCK_NO,
      GTK_RESPONSE_NO,
      GTK_STOCK_YES,
      GTK_RESPONSE_YES,
      NULL);
  GtkWidget* content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget* label = gtk_label_new(message);

  // Add the label and show everything we've added to the dialog.
  gtk_container_add(GTK_CONTAINER(content_area), label);
  gtk_widget_show_all(dialog);

  gint result = gtk_dialog_run(GTK_DIALOG(dialog));

  gtk_widget_destroy(dialog);

  return (result == GTK_RESPONSE_YES);
}

bool ShowJSPromptDialog(GtkWidget* window,
                        const gchar* title,
                        const gchar* message,
                        const gchar* default_val,
                        std::string* return_val) {
  // Create the widgets.
  GtkWidget* dialog = gtk_dialog_new_with_buttons(
      title,
      GTK_WINDOW(window),
      GtkDialogFlags(GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL),
      GTK_STOCK_CANCEL,
      GTK_RESPONSE_CANCEL,
      GTK_STOCK_OK,
      GTK_RESPONSE_OK,
      NULL);
  GtkWidget* content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  GtkWidget* label = gtk_label_new(message);
  GtkWidget* entry = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(entry), default_val);

  // Add the label and entry and show everything we've added to the dialog.
  gtk_container_add(GTK_CONTAINER(content_area), label);
  gtk_container_add(GTK_CONTAINER(content_area), entry);
  gtk_widget_show_all(dialog);

  gint result = gtk_dialog_run(GTK_DIALOG(dialog));
  if (result == GTK_RESPONSE_OK)
    *return_val = std::string(gtk_entry_get_text(GTK_ENTRY(entry)));

  gtk_widget_destroy(dialog);

  return (result == GTK_RESPONSE_OK);
}

bool ShowJSFileChooser(GtkWidget* window, FilePath* path) {
  // Create the widgets.
  GtkWidget* dialog = gtk_file_chooser_dialog_new(
      "Open File",
      GTK_WINDOW(window),
      GTK_FILE_CHOOSER_ACTION_OPEN,
      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
      GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
      NULL);

  gtk_widget_show_all(dialog);

  int result = gtk_dialog_run(GTK_DIALOG(dialog));
  if (result == GTK_RESPONSE_ACCEPT)
    *path = FilePath(gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog)));

  gtk_widget_destroy(dialog);

  return (result == GTK_RESPONSE_ACCEPT);
}

std::string GetDialogLabel(WebKit::WebFrame* webframe,
                           const std::string& label) {
  const GURL& url = webframe->document().url();
  std::string urlStr;
  if (!url.is_empty())
    urlStr = url.host();
  std::string labelStr = label;
  if (!urlStr.empty())
    labelStr += " - " + urlStr;
  return labelStr;
}

}  // namespace

// WebViewClient ---------------------------------------------------------------

WebKit::WebExternalPopupMenu* BrowserWebViewDelegate::createExternalPopupMenu(
      const WebKit::WebPopupMenuInfo& info,
      WebKit::WebExternalPopupMenuClient* client) {
  NOTIMPLEMENTED();
  return NULL;
}

static gboolean MenuItemHandle(GtkWidget* menu_item, gpointer data) {
  if (!data)
    return FALSE;

  BrowserWebViewDelegate* webViewDelegate =
      reinterpret_cast<BrowserWebViewDelegate*>(data);
  int id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(menu_item), "menu_id"));

  webViewDelegate->HandleContextMenu(id);

  return FALSE;
}

static GtkWidget* MenuItemCreate(GtkWidget* parent_menu, const char* name,
    int id, bool is_enabled, BrowserWebViewDelegate* webViewDelegate) {
  GtkWidget* menu_item = gtk_menu_item_new_with_label(name);

  g_object_set_data(G_OBJECT(menu_item), "menu_id",
      reinterpret_cast<gpointer>(id));
  g_signal_connect(G_OBJECT(menu_item), "activate", G_CALLBACK(MenuItemHandle),
      (gpointer)webViewDelegate);
  gtk_menu_shell_append(GTK_MENU_SHELL(parent_menu), menu_item);
  gtk_widget_set_sensitive(menu_item, is_enabled);

  gtk_widget_show(menu_item);

  return menu_item;
}

static GtkWidget* MenuItemCreateSeperator(GtkWidget* parent_menu) {
  GtkWidget* menu_item = gtk_menu_item_new();

  gtk_menu_shell_append(GTK_MENU_SHELL(parent_menu), menu_item);
  gtk_widget_show(menu_item);

  return menu_item;
}

void BrowserWebViewDelegate::showContextMenu(WebKit::WebFrame* frame,
    const WebKit::WebContextMenuData& data) {
  GtkWidget* menu = NULL;
  GdkPoint mouse_pt = { data.mousePosition.x, data.mousePosition.y };

  int edit_flags = 0;
  int type_flags = 0;

  // Make sure events can be pumped while the menu is up.
  MessageLoop::ScopedNestableTaskAllower allow(MessageLoop::current());

  // Give the client a chance to handle the menu.
  if (OnBeforeMenu(data, mouse_pt.x, mouse_pt.y, edit_flags, type_flags))
    return;

  CefRefPtr<CefClient> client = browser_->GetClient();
  CefRefPtr<CefMenuHandler> handler;
  if (client.get())
    handler = client->GetMenuHandler();

  // Build the correct default context menu
  if (type_flags &  MENUTYPE_EDITABLE) {
    menu = gtk_menu_new();
    MenuItemCreate(menu, "Undo", MENU_ID_UNDO,
        !!(edit_flags & MENU_CAN_UNDO), this);
    MenuItemCreate(menu, "Redo", MENU_ID_REDO,
        !!(edit_flags & MENU_CAN_REDO), this);
    MenuItemCreate(menu, "Cut", MENU_ID_CUT,
        !!(edit_flags & MENU_CAN_CUT), this);
    MenuItemCreate(menu, "Copy", MENU_ID_COPY,
        !!(edit_flags & MENU_CAN_COPY), this);
    MenuItemCreate(menu, "Paste", MENU_ID_PASTE,
        !!(edit_flags & MENU_CAN_PASTE), this);
    MenuItemCreate(menu, "Delete", MENU_ID_DELETE,
        !!(edit_flags & MENU_CAN_DELETE), this);
    MenuItemCreateSeperator(menu);
    MenuItemCreate(menu, "Select All", MENU_ID_SELECTALL,
        !!(edit_flags & MENU_CAN_SELECT_ALL), this);
  } else if (type_flags & MENUTYPE_SELECTION) {
    menu = gtk_menu_new();
    MenuItemCreate(menu, "Copy", MENU_ID_COPY,
        !!(edit_flags & MENU_CAN_COPY), this);
  } else if (type_flags & (MENUTYPE_PAGE | MENUTYPE_FRAME)) {
    menu = gtk_menu_new();
    MenuItemCreate(menu, "Back", MENU_ID_NAV_BACK,
        !!(edit_flags & MENU_CAN_GO_BACK), this);
    MenuItemCreate(menu, "Forward", MENU_ID_NAV_FORWARD,
        !!(edit_flags & MENU_CAN_GO_FORWARD), this);
    MenuItemCreateSeperator(menu);
    // TODO(port): Enable the print item when supported.
    // MenuItemCreate(menu, "Print", MENU_ID_PRINT, true, this);
    MenuItemCreate(menu, "View Source", MENU_ID_VIEWSOURCE, true, this);
  }

  if (menu) {
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 3,
                   gtk_get_current_event_time());
  }
}

// WebWidgetClient ------------------------------------------------------------

void BrowserWebViewDelegate::show(WebNavigationPolicy policy) {
  WebWidgetHost* host = GetWidgetHost();
  GtkWidget* drawing_area = host->view_handle();
  GtkWidget* window =
      gtk_widget_get_parent(gtk_widget_get_parent(drawing_area));
  gtk_widget_show_all(window);
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
  gdk_window_set_cursor(browser_->UIT_GetWebViewWndHandle()->window,
                        gdk_cursor);
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
  if (this == browser_->UIT_GetWebViewDelegate()) {
    // TODO(port): Set the window rectangle.
  } else if (this == browser_->UIT_GetPopupDelegate()) {
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
    GtkWidget* window = gtk_widget_get_ancestor(GTK_WIDGET(host->view_handle()),
                                                GTK_TYPE_WINDOW);

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

void BrowserWebViewDelegate::startDragging(
    WebFrame* frame,
    const WebDragData& data,
    WebDragOperationsMask mask,
    const WebImage& image,
    const WebPoint& image_offset) {
  if (browser_->settings().drag_drop_disabled) {
    browser_->UIT_GetWebView()->dragSourceSystemDragEnded();
    return;
  }

  WebDropData drop_data(data);

  CefRefPtr<CefClient> client = browser_->GetClient();
  if (client.get()) {
    CefRefPtr<CefDragHandler> handler = client->GetDragHandler();
    if (handler.get()) {
      CefRefPtr<CefDragData> data(new CefDragDataImpl(drop_data));
      if (handler->OnDragStart(browser_, data,
              static_cast<CefDragHandler::DragOperationsMask>(mask))) {
        browser_->UIT_GetWebView()->dragSourceSystemDragEnded();
        return;
      }
    }
  }
  drag_source_ = new WebDragSource(browser_);
  drag_source_->StartDragging(drop_data, mask, image, image_offset);
}

void BrowserWebViewDelegate::runModal() {
  NOTIMPLEMENTED();
}

// WebPluginPageDelegate ------------------------------------------------------

webkit::npapi::WebPluginDelegate* BrowserWebViewDelegate::CreatePluginDelegate(
    const FilePath& path,
    const std::string& mime_type) {
  return webkit::npapi::WebPluginDelegateImpl::Create(path, mime_type);
}

void BrowserWebViewDelegate::CreatedPluginWindow(
    gfx::PluginWindowHandle id) {
  browser_->UIT_GetWebViewHost()->CreatePluginContainer(id);
}

void BrowserWebViewDelegate::WillDestroyPluginWindow(
    gfx::PluginWindowHandle id) {
  browser_->UIT_GetWebViewHost()->DestroyPluginContainer(id);
}

void BrowserWebViewDelegate::DidMovePlugin(
    const webkit::npapi::WebPluginGeometry& move) {
  WebWidgetHost* host = GetWidgetHost();
  webkit::npapi::GtkPluginContainerManager* plugin_container_manager =
      static_cast<WebViewHost*>(host)->plugin_container_manager();
  plugin_container_manager->MovePluginContainer(move);
}

void BrowserWebViewDelegate::HandleContextMenu(int selected_id) {
  if (selected_id != 0) {
    CefRefPtr<CefClient> client = browser_->GetClient();
    CefRefPtr<CefMenuHandler> handler;
    if (client.get())
      handler = client->GetMenuHandler();

    // An action was chosen
    cef_menu_id_t menuId = static_cast<cef_menu_id_t>(selected_id);
    bool handled = false;
    if (handler.get()) {
      // Ask the handler if it wants to handle the action
      handled = handler->OnMenuAction(browser_, menuId);
    }

    if (!handled) {
      // Execute the action
      browser_->UIT_HandleAction(menuId, browser_->GetFocusedFrame());
    }
  }
}

// Private methods ------------------------------------------------------------

void BrowserWebViewDelegate::RegisterDragDrop() {
  DCHECK(!drop_target_);
  drop_target_ = new WebDropTarget(browser_);
}

// Protected methods ----------------------------------------------------------

void BrowserWebViewDelegate::ShowJavaScriptAlert(
    WebKit::WebFrame* webframe, const CefString& message) {
  std::string messageStr(message);
  std::string labelStr(GetDialogLabel(webframe, "JavaScript Alert"));

  gfx::NativeView view = browser_->UIT_GetMainWndHandle();
  GtkWidget* window = gtk_widget_get_toplevel(GTK_WIDGET(view));

  ShowJSAlertDialog(window,
                    static_cast<const gchar*>(labelStr.c_str()),
                    static_cast<const gchar*>(messageStr.c_str()));
}

bool BrowserWebViewDelegate::ShowJavaScriptConfirm(
    WebKit::WebFrame* webframe, const CefString& message) {
  std::string messageStr(message);
  std::string labelStr(GetDialogLabel(webframe, "JavaScript Confirm"));

  gfx::NativeView view = browser_->UIT_GetMainWndHandle();
  GtkWidget* window = gtk_widget_get_toplevel(GTK_WIDGET(view));

  return ShowJSConfirmDialog(
      window,
      static_cast<const gchar*>(labelStr.c_str()),
      static_cast<const gchar*>(messageStr.c_str()));
}

bool BrowserWebViewDelegate::ShowJavaScriptPrompt(
    WebKit::WebFrame* webframe, const CefString& message,
    const CefString& default_value, CefString* result) {
  std::string messageStr(message);
  std::string defaultStr(default_value);
  std::string labelStr(GetDialogLabel(webframe, "JavaScript Prompt"));

  gfx::NativeView view = browser_->UIT_GetMainWndHandle();
  GtkWidget* window = gtk_widget_get_toplevel(GTK_WIDGET(view));

  std::string return_val;
  bool resp = ShowJSPromptDialog(
      window,
      static_cast<const gchar*>(labelStr.c_str()),
      static_cast<const gchar*>(messageStr.c_str()),
      static_cast<const gchar*>(defaultStr.c_str()),
      &return_val);
  if (resp)
    *result = return_val;

  return resp;
}

bool BrowserWebViewDelegate::ShowFileChooser(
    std::vector<FilePath>& file_names,
    bool multi_select,
    const WebKit::WebString& title,
    const FilePath& default_file,
    const std::vector<std::string>& accept_mime_types) {
  gfx::NativeView view = browser_->UIT_GetMainWndHandle();
  GtkWidget* window = gtk_widget_get_toplevel(GTK_WIDGET(view));

  FilePath file_name;
  bool resp = ShowJSFileChooser(window, &file_name);
  if (resp)
     file_names.push_back(file_name);

  return resp;
}
