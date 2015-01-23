// Copyright (c) 2014 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <gtk/gtk.h>
#include <libgen.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#undef Success  // Definition conflicts with cef_message_router.h

#include <string>

#include "cefclient/client_handler.h"
#include "include/cef_browser.h"
#include "include/cef_frame.h"

namespace client {

void ClientHandler::OnAddressChange(CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefFrame> frame,
                                    const CefString& url) {
  CEF_REQUIRE_UI_THREAD();

  if (GetBrowserId() == browser->GetIdentifier() && frame->IsMain()) {
      // Set the edit window text
    std::string urlStr(url);
    gtk_entry_set_text(GTK_ENTRY(edit_handle_), urlStr.c_str());
  }
}

void ClientHandler::OnTitleChange(CefRefPtr<CefBrowser> browser,
                                  const CefString& title) {
  CEF_REQUIRE_UI_THREAD();

  std::string titleStr(title);

  if (!browser->IsPopup()) {
    // Set the GTK parent window title.
    GtkWidget* window = gtk_widget_get_ancestor(main_handle_,
        GTK_TYPE_WINDOW);
    gtk_window_set_title(GTK_WINDOW(window), titleStr.c_str());
  } else {
    // Retrieve the X11 display shared with Chromium.
    ::Display* display = cef_get_xdisplay();
    DCHECK(display);

    // Retrieve the X11 window handle for the browser.
    ::Window window = browser->GetHost()->GetWindowHandle();
    DCHECK(window != kNullWindowHandle);

    // Retrieve the atoms required by the below XChangeProperty call.
    const char* kAtoms[] = {
      "_NET_WM_NAME",
      "UTF8_STRING"
    };
    Atom atoms[2];
    int result = XInternAtoms(display, const_cast<char**>(kAtoms), 2, false,
                              atoms);
    if (!result)
      NOTREACHED();

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

void ClientHandler::SetLoading(bool isLoading) {
  CEF_REQUIRE_UI_THREAD();

  if (isLoading)
    gtk_widget_set_sensitive(GTK_WIDGET(stop_handle_), true);
  else
    gtk_widget_set_sensitive(GTK_WIDGET(stop_handle_), false);
}

void ClientHandler::SetNavState(bool canGoBack, bool canGoForward) {
  CEF_REQUIRE_UI_THREAD();

  if (canGoBack)
    gtk_widget_set_sensitive(GTK_WIDGET(back_handle_), true);
  else
    gtk_widget_set_sensitive(GTK_WIDGET(back_handle_), false);

  if (canGoForward)
    gtk_widget_set_sensitive(GTK_WIDGET(forward_handle_), true);
  else
    gtk_widget_set_sensitive(GTK_WIDGET(forward_handle_), false);
}

}  // namespace client
