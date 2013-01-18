// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cefclient/client_handler.h"
#include <string>
#include "include/cef_browser.h"
#include "include/cef_frame.h"
#include "cefclient/performance_test.h"
#include "cefclient/resource.h"
#include "cefclient/resource_util.h"
#include "cefclient/string_util.h"

#ifdef TEST_REDIRECT_POPUP_URLS
#include "cefclient/client_popup_handler.h"
#endif

bool ClientHandler::OnBeforePopup(CefRefPtr<CefBrowser> parentBrowser,
                                  const CefPopupFeatures& popupFeatures,
                                  CefWindowInfo& windowInfo,
                                  const CefString& url,
                                  CefRefPtr<CefClient>& client,
                                  CefBrowserSettings& settings) {
  REQUIRE_UI_THREAD();

#ifdef TEST_REDIRECT_POPUP_URLS
  std::string urlStr = url;
  if (urlStr.find("chrome-devtools:") == std::string::npos) {
    // Show all popup windows excluding DevTools in the current window.
    windowInfo.m_dwStyle &= ~WS_VISIBLE;
    client = new ClientPopupHandler(m_Browser);
  }
#endif  // TEST_REDIRECT_POPUP_URLS

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
    resourceStream = GetBinaryResourceReader(IDS_LOGO);
    response->SetMimeType("image/png");
    response->SetStatus(200);
  } else if (url == "http://tests/uiapp") {
    // Show the uiapp contents
    resourceStream = GetBinaryResourceReader(IDS_UIPLUGIN);
    response->SetMimeType("text/html");
    response->SetStatus(200);
  } else if (url == "http://tests/osrapp") {
    // Show the osrapp contents
    resourceStream = GetBinaryResourceReader(IDS_OSRPLUGIN);
    response->SetMimeType("text/html");
    response->SetStatus(200);
  } else if (url == "http://tests/localstorage") {
    // Show the localstorage contents
    resourceStream = GetBinaryResourceReader(IDS_LOCALSTORAGE);
    response->SetMimeType("text/html");
    response->SetStatus(200);
  } else if (url == "http://tests/xmlhttprequest") {
    // Show the xmlhttprequest HTML contents
    resourceStream = GetBinaryResourceReader(IDS_XMLHTTPREQUEST);
    response->SetMimeType("text/html");
    response->SetStatus(200);
  } else if (url == "http://tests/domaccess") {
    // Show the domaccess HTML contents
    resourceStream = GetBinaryResourceReader(IDS_DOMACCESS);
    response->SetMimeType("text/html");
    response->SetStatus(200);
  } else if (strstr(url.c_str(), "/logoball.png") != NULL) {
    // Load the "logoball.png" image resource.
    resourceStream = GetBinaryResourceReader(IDS_LOGOBALL);
    response->SetMimeType("image/png");
    response->SetStatus(200);
  } else if (url == "http://tests/modalmain") {
    resourceStream = GetBinaryResourceReader(IDS_MODALMAIN);
    response->SetMimeType("text/html");
    response->SetStatus(200);
  } else if (url == "http://tests/modaldialog") {
    resourceStream = GetBinaryResourceReader(IDS_MODALDIALOG);
    response->SetMimeType("text/html");
    response->SetStatus(200);
  } else if (url == "http://tests/transparency") {
    resourceStream = GetBinaryResourceReader(IDS_TRANSPARENCY);
    response->SetMimeType("text/html");
    response->SetStatus(200);
  } else if (url == "http://tests/plugin") {
    std::string html =
        "<html><body>\n"
        "Client Plugin loaded by Mime Type:<br>\n"
        "<embed type=\"application/x-client-plugin\" width=600 height=40>\n"
        "<br><br>Client Plugin loaded by File Extension:<br>\n"
        "<embed src=\"test.xcp\" width=600 height=40>\n"
        // Add some extra space below the plugin to allow scrolling.
        "<div style=\"height:1000px;\">&nbsp;</div>\n"
        "</body></html>";

    resourceStream = CefStreamReader::CreateForData(
        static_cast<void*>(const_cast<char*>(html.c_str())),
        html.size());
    response->SetMimeType("text/html");
    response->SetStatus(200);
  } else if (url == performance_test::kTestUrl) {
    resourceStream = GetBinaryResourceReader(IDS_PERFORMANCE);
    response->SetMimeType("text/html");
    response->SetStatus(200);
  } else if (url == "http://tests/dialogs") {
    // Show the dialogs HTML contents
    resourceStream = GetBinaryResourceReader(IDS_DIALOGS);
    response->SetMimeType("text/html");
    response->SetStatus(200);
  }

  return false;
}

void ClientHandler::OnAddressChange(CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefFrame> frame,
                                    const CefString& url) {
  REQUIRE_UI_THREAD();

  if (m_BrowserHwnd == browser->GetWindowHandle() && frame->IsMain())   {
    // Set the edit window text
    SetWindowText(m_EditHwnd, std::wstring(url).c_str());
  }
}

void ClientHandler::OnTitleChange(CefRefPtr<CefBrowser> browser,
                                  const CefString& title) {
  REQUIRE_UI_THREAD();

  // Set the frame window title bar
  CefWindowHandle hwnd = browser->GetWindowHandle();
  if (m_BrowserHwnd == hwnd)   {
    // The frame window will be the parent of the browser window
    hwnd = GetParent(hwnd);
  }
  SetWindowText(hwnd, std::wstring(title).c_str());
}

void ClientHandler::SendNotification(NotificationType type) {
  UINT id;
  switch (type) {
  case NOTIFY_CONSOLE_MESSAGE:
    id = ID_WARN_CONSOLEMESSAGE;
    break;
  case NOTIFY_DOWNLOAD_COMPLETE:
    id = ID_WARN_DOWNLOADCOMPLETE;
    break;
  case NOTIFY_DOWNLOAD_ERROR:
    id = ID_WARN_DOWNLOADERROR;
    break;
  default:
    return;
  }
  PostMessage(m_MainHwnd, WM_COMMAND, id, 0);
}

void ClientHandler::SetLoading(bool isLoading) {
  ASSERT(m_EditHwnd != NULL && m_ReloadHwnd != NULL && m_StopHwnd != NULL);
  EnableWindow(m_EditHwnd, TRUE);
  EnableWindow(m_ReloadHwnd, !isLoading);
  EnableWindow(m_StopHwnd, isLoading);
}

void ClientHandler::SetNavState(bool canGoBack, bool canGoForward) {
  ASSERT(m_BackHwnd != NULL && m_ForwardHwnd != NULL);
  EnableWindow(m_BackHwnd, canGoBack);
  EnableWindow(m_ForwardHwnd, canGoForward);
}

void ClientHandler::CloseMainWindow() {
  ::PostMessage(m_MainHwnd, WM_CLOSE, 0, 0);
}
