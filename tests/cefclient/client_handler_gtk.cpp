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
    ASSERT(result);

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

