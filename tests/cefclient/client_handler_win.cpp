// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cefclient/client_handler.h"
#include <string>
#include "include/cef_browser.h"
#include "include/cef_frame.h"
#include "include/cef_stream.h"
#include "include/wrapper/cef_stream_resource_handler.h"
#include "cefclient/resource.h"
#include "cefclient/resource_util.h"
#include "cefclient/string_util.h"

bool ClientHandler::OnBeforePopup(CefRefPtr<CefBrowser> parentBrowser,
                                  const CefPopupFeatures& popupFeatures,
                                  CefWindowInfo& windowInfo,
                                  const CefString& url,
                                  CefRefPtr<CefClient>& client,
                                  CefBrowserSettings& settings) {
  REQUIRE_UI_THREAD();

  return false;
}

CefRefPtr<CefResourceHandler> ClientHandler::GetResourceHandler(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request) {
  std::string url = request->GetURL();
  if (url == "http://tests/request") {
    // Show the request contents
    std::string dump;
    DumpRequestContents(request, dump);
    CefRefPtr<CefStreamReader> stream =
        CefStreamReader::CreateForData(
            static_cast<void*>(const_cast<char*>(dump.c_str())),
            dump.size());
    ASSERT(stream.get());
    return new CefStreamResourceHandler("text/plain", stream);
  } else if (url == "http://tests/localstorage") {
    // Show the localstorage contents
    CefRefPtr<CefStreamReader> stream =
        GetBinaryResourceReader(IDS_LOCALSTORAGE);
    ASSERT(stream.get());
    return new CefStreamResourceHandler("text/html", stream);
  } else if (url == "http://tests/xmlhttprequest") {
    // Show the xmlhttprequest contents
    CefRefPtr<CefStreamReader> stream =
       GetBinaryResourceReader(IDS_XMLHTTPREQUEST);
    ASSERT(stream.get());
    return new CefStreamResourceHandler("text/html", stream);
  }

  return NULL;
}

void ClientHandler::OnAddressChange(CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefFrame> frame,
                                    const CefString& url) {
  REQUIRE_UI_THREAD();

  if (m_BrowserId == browser->GetIdentifier() && frame->IsMain())   {
    // Set the edit window text
    SetWindowText(m_EditHwnd, std::wstring(url).c_str());
  }
}

void ClientHandler::OnTitleChange(CefRefPtr<CefBrowser> browser,
                                  const CefString& title) {
  REQUIRE_UI_THREAD();

  // Set the frame window title bar
  CefWindowHandle hwnd = browser->GetHost()->GetWindowHandle();
  if (m_BrowserId == browser->GetIdentifier())   {
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
