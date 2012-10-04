// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <gtk/gtk.h>
#include <string>
#include "cefclient/client_handler.h"
#include "include/cef_browser.h"
#include "include/cef_frame.h"
#include "cefclient/resource_util.h"
#include "cefclient/string_util.h"

// ClientHandler::ClientLifeSpanHandler implementation
bool ClientHandler::OnBeforePopup(CefRefPtr<CefBrowser> parentBrowser,
                                  const CefPopupFeatures& popupFeatures,
                                  CefWindowInfo& windowInfo,
                                  const CefString& url,
                                  CefRefPtr<CefClient>& client,
                                  CefBrowserSettings& settings) {
  REQUIRE_UI_THREAD();

  return false;
}

bool ClientHandler::OnBeforeResourceLoad(CefRefPtr<CefBrowser> browser,
                                     CefRefPtr<CefRequest> request,
                                     CefString& redirectUrl,
                                     CefRefPtr<CefStreamReader>& resourceStream,
                                     CefRefPtr<CefResponse> response,
                                     int loadFlags) {
  REQUIRE_IO_THREAD();

  std::string url = request->GetURL();

  if (url == "http://tests/request") {
    // Show the request contents
    std::string dump;
    DumpRequestContents(request, dump);
    resourceStream = CefStreamReader::CreateForData(
        static_cast<void*>(const_cast<char*>(dump.c_str())),
        dump.size());
    response->SetMimeType("text/plain");
    response->SetStatus(200);
  } else if (strstr(url.c_str(), "/ps_logo2.png") != NULL) {
    // Any time we find "ps_logo2.png" in the URL substitute in our own image
    resourceStream = GetBinaryResourceReader("logo.png");
    response->SetMimeType("image/png");
    response->SetStatus(200);
  } else if (url == "http://tests/localstorage") {
    // Show the localstorage contents
    resourceStream = GetBinaryResourceReader("localstorage.html");
    response->SetMimeType("text/html");
    response->SetStatus(200);
  } else if (url == "http://tests/xmlhttprequest") {
    // Show the xmlhttprequest HTML contents
    resourceStream = GetBinaryResourceReader("xmlhttprequest.html");
    response->SetMimeType("text/html");
    response->SetStatus(200);
  } else if (url == "http://tests/domaccess") {
    // Show the domaccess HTML contents
    resourceStream = GetBinaryResourceReader("domaccess.html");
    response->SetMimeType("text/html");
    response->SetStatus(200);
  }

  return false;
}

void ClientHandler::OnAddressChange(CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefFrame> frame,
                                    const CefString& url) {
  REQUIRE_UI_THREAD();

  if (m_BrowserHwnd == browser->GetWindowHandle() && frame->IsMain()) {
      // Set the edit window text
    std::string urlStr(url);
      gtk_entry_set_text(GTK_ENTRY(m_EditHwnd), urlStr.c_str());
  }
}

void ClientHandler::OnTitleChange(CefRefPtr<CefBrowser> browser,
                                  const CefString& title) {
  REQUIRE_UI_THREAD();

  GtkWidget* window =
      gtk_widget_get_ancestor(GTK_WIDGET(browser->GetWindowHandle()),
                              GTK_TYPE_WINDOW);
  std::string titleStr(title);
  gtk_window_set_title(GTK_WINDOW(window), titleStr.c_str());
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

void ClientHandler::CloseMainWindow() {
  // TODO(port): Close main window.
}
