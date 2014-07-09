// Copyright (c) 2014 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <gtk/gtk.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#undef Success  // Definition conflicts with cef_message_router.h

#include <string>

#include "cefclient/client_handler.h"
#include "include/cef_browser.h"
#include "include/cef_frame.h"

namespace {

const char kPromptTextId[] = "cef_prompt_text";

// If there's a text entry in the dialog, get the text from the first one and
// return it.
std::string GetPromptText(GtkDialog* dialog) {
  GtkWidget* widget = static_cast<GtkWidget*>(
      g_object_get_data(G_OBJECT(dialog), kPromptTextId));
  if (widget)
    return gtk_entry_get_text(GTK_ENTRY(widget));
  return std::string();
}

}  // namespace

void ClientHandler::OnAddressChange(CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefFrame> frame,
                                    const CefString& url) {
  REQUIRE_UI_THREAD();

  if (m_BrowserId == browser->GetIdentifier() && frame->IsMain()) {
      // Set the edit window text
    std::string urlStr(url);
    gtk_entry_set_text(GTK_ENTRY(m_EditHwnd), urlStr.c_str());
  }
}

void ClientHandler::OnTitleChange(CefRefPtr<CefBrowser> browser,
                                  const CefString& title) {
  REQUIRE_UI_THREAD();

  std::string titleStr(title);

  if (!browser->IsPopup()) {
    // Set the GTK parent window title.
    GtkWidget* window = gtk_widget_get_ancestor(m_MainHwnd,
        GTK_TYPE_WINDOW);
    gtk_window_set_title(GTK_WINDOW(window), titleStr.c_str());
  } else {
    // Retrieve the X11 display shared with Chromium.
    ::Display* display = cef_get_xdisplay();
    ASSERT(display);

    // Retrieve the X11 window handle for the browser.
    ::Window window = browser->GetHost()->GetWindowHandle();
    ASSERT(window != kNullWindowHandle);

    // Retrieve the atoms required by the below XChangeProperty call.
    const char* kAtoms[] = {
      "_NET_WM_NAME",
      "UTF8_STRING"
    };
    Atom atoms[2];
    int result = XInternAtoms(display, const_cast<char**>(kAtoms), 2, false,
                              atoms);
    if (!result)
      ASSERT(false);

    // Set the window title.
    XChangeProperty(display,
                    window,
                    atoms[0],
                    atoms[1],
                    8,
                    PropModeReplace,
                    reinterpret_cast<const unsigned char*>(titleStr.c_str()),
                    titleStr.size());

    // TODO(erg): This is technically wrong. So XStoreName and friends expect
    // this in Host Portable Character Encoding instead of UTF-8, which I
    // believe is Compound Text. This shouldn't matter 90% of the time since
    // this is the fallback to the UTF8 property above.
    XStoreName(display, browser->GetHost()->GetWindowHandle(),
               titleStr.c_str());
  }
}

bool ClientHandler::OnJSDialog(CefRefPtr<CefBrowser> browser,
                               const CefString& origin_url,
                               const CefString& accept_lang,
                               JSDialogType dialog_type,
                               const CefString& message_text,
                               const CefString& default_prompt_text,
                               CefRefPtr<CefJSDialogCallback> callback,
                               bool& suppress_message) {
  GtkButtonsType buttons = GTK_BUTTONS_NONE;
  GtkMessageType gtk_message_type = GTK_MESSAGE_OTHER;
  std::string title;

  switch (dialog_type) {
    case JSDIALOGTYPE_ALERT:
      buttons = GTK_BUTTONS_NONE;
      gtk_message_type = GTK_MESSAGE_WARNING;
      title = "JavaScript Alert";
      break;

    case JSDIALOGTYPE_CONFIRM:
      buttons = GTK_BUTTONS_CANCEL;
      gtk_message_type = GTK_MESSAGE_QUESTION;
      title = "JavaScript Confirm";
      break;

    case JSDIALOGTYPE_PROMPT:
      buttons = GTK_BUTTONS_CANCEL;
      gtk_message_type = GTK_MESSAGE_QUESTION;
      title = "JavaScript Prompt";
      break;
  }

  js_dialog_callback_ = callback;

  if (!origin_url.empty()) {
    title += " - ";
    title += origin_url.ToString();
  }

  GtkWidget* window =
      gtk_widget_get_ancestor(GTK_WIDGET(GetMainHwnd()), GTK_TYPE_WINDOW);
  gtk_dialog_ = gtk_message_dialog_new(GTK_WINDOW(window),
                                       GTK_DIALOG_MODAL,
                                       gtk_message_type,
                                       buttons,
                                       "%s",
                                       message_text.ToString().c_str());
  g_signal_connect(gtk_dialog_,
                   "delete-event",
                   G_CALLBACK(gtk_widget_hide_on_delete),
                   NULL);

  gtk_window_set_title(GTK_WINDOW(gtk_dialog_), title.c_str());

  GtkWidget* ok_button = gtk_dialog_add_button(GTK_DIALOG(gtk_dialog_),
                                               GTK_STOCK_OK,
                                               GTK_RESPONSE_OK);

  if (dialog_type != JSDIALOGTYPE_PROMPT)
    gtk_widget_grab_focus(ok_button);

  if (dialog_type == JSDIALOGTYPE_PROMPT) {
    GtkWidget* content_area =
        gtk_dialog_get_content_area(GTK_DIALOG(gtk_dialog_));
    GtkWidget* text_box = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(text_box),
                       default_prompt_text.ToString().c_str());
    gtk_box_pack_start(GTK_BOX(content_area), text_box, TRUE, TRUE, 0);
    g_object_set_data(G_OBJECT(gtk_dialog_), kPromptTextId, text_box);
    gtk_entry_set_activates_default(GTK_ENTRY(text_box), TRUE);
  }

  gtk_dialog_set_default_response(GTK_DIALOG(gtk_dialog_), GTK_RESPONSE_OK);
  g_signal_connect(gtk_dialog_, "response", G_CALLBACK(OnDialogResponse), this);
  gtk_widget_show_all(GTK_WIDGET(gtk_dialog_));

  return true;
}

bool ClientHandler::OnBeforeUnloadDialog(
    CefRefPtr<CefBrowser> browser,
    const CefString& message_text,
    bool is_reload,
    CefRefPtr<CefJSDialogCallback> callback) {
  const std::string& new_message_text =
      message_text.ToString() + "\n\nIs it OK to leave/reload this page?";
  bool suppress_message = false;

  return OnJSDialog(browser, CefString(), CefString(), JSDIALOGTYPE_CONFIRM,
                    new_message_text, CefString(), callback, suppress_message);
}

void ClientHandler::OnResetDialogState(CefRefPtr<CefBrowser> browser) {
  if (!gtk_dialog_)
    return;
  gtk_widget_destroy(gtk_dialog_);
  gtk_dialog_ = NULL;
  js_dialog_callback_ = NULL;
}

// static
void ClientHandler::OnDialogResponse(GtkDialog* dialog,
                                     gint response_id,
                                     ClientHandler* handler) {
  ASSERT(dialog == GTK_DIALOG(handler->gtk_dialog_));
  switch (response_id) {
    case GTK_RESPONSE_OK:
      handler->js_dialog_callback_->Continue(true, GetPromptText(dialog));
      break;
    case GTK_RESPONSE_CANCEL:
    case GTK_RESPONSE_DELETE_EVENT:
      handler->js_dialog_callback_->Continue(false, CefString());
      break;
    default:
      ASSERT(false);  // Not reached
  }

  handler->OnResetDialogState(NULL);
}

void ClientHandler::SendNotification(NotificationType type) {
  // TODO(port): Implement this method.
}

void ClientHandler::SetLoading(bool isLoading) {
  if (isLoading)
    gtk_widget_set_sensitive(GTK_WIDGET(m_StopHwnd), true);
  else
    gtk_widget_set_sensitive(GTK_WIDGET(m_StopHwnd), false);
}

void ClientHandler::SetNavState(bool canGoBack, bool canGoForward) {
  if (canGoBack)
    gtk_widget_set_sensitive(GTK_WIDGET(m_BackHwnd), true);
  else
    gtk_widget_set_sensitive(GTK_WIDGET(m_BackHwnd), false);

  if (canGoForward)
    gtk_widget_set_sensitive(GTK_WIDGET(m_ForwardHwnd), true);
  else
    gtk_widget_set_sensitive(GTK_WIDGET(m_ForwardHwnd), false);
}

std::string ClientHandler::GetDownloadPath(const std::string& file_name) {
  return std::string();
}

