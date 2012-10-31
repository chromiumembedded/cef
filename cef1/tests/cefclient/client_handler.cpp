// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cefclient/client_handler.h"
#include <stdio.h>
#include <sstream>
#include <string>
#include "include/cef_browser.h"
#include "include/cef_command_line.h"
#include "include/cef_frame.h"
#include "cefclient/binding_test.h"
#include "cefclient/cefclient.h"
#include "cefclient/cefclient_switches.h"
#include "cefclient/download_handler.h"
#include "cefclient/performance_test.h"
#include "cefclient/string_util.h"

ClientHandler::ClientHandler()
  : m_MainHwnd(NULL),
    m_BrowserHwnd(NULL),
    m_EditHwnd(NULL),
    m_BackHwnd(NULL),
    m_ForwardHwnd(NULL),
    m_StopHwnd(NULL),
    m_ReloadHwnd(NULL),
    m_bFormElementHasFocus(false) {
  CefRefPtr<CefCommandLine> commandLine = AppGetCommandLine();
  if (commandLine.get()) {
    if (commandLine->HasSwitch(cefclient::kUrl))
      m_StartupURL = commandLine->GetSwitchValue(cefclient::kUrl);
    else
      m_StartupURL = "http://www.google.com/";
  }
}

ClientHandler::~ClientHandler() {
}


void ClientHandler::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  REQUIRE_UI_THREAD();

  AutoLock lock_scope(this);
  if (!m_Browser.get())   {
    // We need to keep the main child window, but not popup windows
    m_Browser = browser;
    m_BrowserHwnd = browser->GetWindowHandle();
  }
}

bool ClientHandler::DoClose(CefRefPtr<CefBrowser> browser) {
  REQUIRE_UI_THREAD();

  if (m_BrowserHwnd == browser->GetWindowHandle()) {
    // Since the main window contains the browser window, we need to close
    // the parent window instead of the browser window.
    CloseMainWindow();

    // Return true here so that we can skip closing the browser window
    // in this pass. (It will be destroyed due to the call to close
    // the parent above.)
    return true;
  }

  // A popup browser window is not contained in another window, so we can let
  // these windows close by themselves.
  return false;
}

void ClientHandler::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  REQUIRE_UI_THREAD();

  if (m_BrowserHwnd == browser->GetWindowHandle()) {
    // Free the browser pointer so that the browser can be destroyed
    m_Browser = NULL;
  }
}

void ClientHandler::OnLoadStart(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame) {
  REQUIRE_UI_THREAD();

  if (m_BrowserHwnd == browser->GetWindowHandle() && frame->IsMain()) {
    // We've just started loading a page
    SetLoading(true);
  }
}

void ClientHandler::OnLoadEnd(CefRefPtr<CefBrowser> browser,
                              CefRefPtr<CefFrame> frame,
                              int httpStatusCode) {
  REQUIRE_UI_THREAD();

  if (m_BrowserHwnd == browser->GetWindowHandle() && frame->IsMain()) {
    // We've just finished loading a page
    SetLoading(false);

    CefRefPtr<CefDOMVisitor> visitor = GetDOMVisitor(frame->GetURL());
    if (visitor.get())
      frame->VisitDOM(visitor);
  }
}

bool ClientHandler::OnLoadError(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                ErrorCode errorCode,
                                const CefString& failedUrl,
                                CefString& errorText) {
  REQUIRE_UI_THREAD();

  if (errorCode == ERR_CACHE_MISS) {
    // Usually caused by navigating to a page with POST data via back or
    // forward buttons.
    errorText = "<html><head><title>Expired Form Data</title></head>"
                "<body><h1>Expired Form Data</h1>"
                "<h2>Your form request has expired. "
                "Click reload to re-submit the form data.</h2></body>"
                "</html>";
  } else {
    // All other messages.
    std::stringstream ss;
    ss <<       "<html><head><title>Load Failed</title></head>"
                "<body><h1>Load Failed</h1>"
                "<h2>Load of URL " << std::string(failedUrl) <<
                " failed with error code " << static_cast<int>(errorCode) <<
                ".</h2></body>"
                "</html>";
    errorText = ss.str();
  }

  return false;
}

bool ClientHandler::GetDownloadHandler(CefRefPtr<CefBrowser> browser,
                                       const CefString& mimeType,
                                       const CefString& fileName,
                                       int64 contentLength,
                                       CefRefPtr<CefDownloadHandler>& handler) {
  REQUIRE_UI_THREAD();

  // Create the handler for the file download.
  handler = CreateDownloadHandler(this, fileName);

  // Close the browser window if it is a popup with no other document contents.
  if (browser->IsPopup() && !browser->HasDocument())
    browser->CloseBrowser();

  return true;
}

void ClientHandler::OnNavStateChange(CefRefPtr<CefBrowser> browser,
                                     bool canGoBack,
                                     bool canGoForward) {
  REQUIRE_UI_THREAD();

  SetNavState(canGoBack, canGoForward);
}

bool ClientHandler::OnConsoleMessage(CefRefPtr<CefBrowser> browser,
                                     const CefString& message,
                                     const CefString& source,
                                     int line) {
  REQUIRE_UI_THREAD();

  bool first_message;
  std::string logFile;

  {
    AutoLock lock_scope(this);

    first_message = m_LogFile.empty();
    if (first_message) {
      std::stringstream ss;
      ss << AppGetWorkingDirectory();
#if defined(OS_WIN)
      ss << "\\";
#else
      ss << "/";
#endif
      ss << "console.log";
      m_LogFile = ss.str();
    }
    logFile = m_LogFile;
  }

  FILE* file = fopen(logFile.c_str(), "a");
  if (file) {
    std::stringstream ss;
    ss << "Message: " << std::string(message) << "\r\nSource: " <<
        std::string(source) << "\r\nLine: " << line <<
        "\r\n-----------------------\r\n";
    fputs(ss.str().c_str(), file);
    fclose(file);

    if (first_message)
      SendNotification(NOTIFY_CONSOLE_MESSAGE);
  }

  return false;
}

void ClientHandler::OnFocusedNodeChanged(CefRefPtr<CefBrowser> browser,
                                         CefRefPtr<CefFrame> frame,
                                         CefRefPtr<CefDOMNode> node) {
  REQUIRE_UI_THREAD();

  // Set to true if a form element has focus.
  m_bFormElementHasFocus = (node.get() && node->IsFormControlElement());
}

bool ClientHandler::OnKeyEvent(CefRefPtr<CefBrowser> browser,
                               KeyEventType type,
                               int code,
                               int modifiers,
                               bool isSystemKey,
                               bool isAfterJavaScript) {
  REQUIRE_UI_THREAD();

  if (isAfterJavaScript && !m_bFormElementHasFocus && code == 0x20) {
    // Special handling for the space character if a form element does not have
    // focus.
    if (type == KEYEVENT_RAWKEYDOWN) {
      browser->GetMainFrame()->ExecuteJavaScript(
          "alert('You pressed the space bar!');", "", 0);
    }
    return true;
  }

  return false;
}

bool ClientHandler::GetPrintHeaderFooter(CefRefPtr<CefBrowser> browser,
                                         CefRefPtr<CefFrame> frame,
                                         const CefPrintInfo& printInfo,
                                         const CefString& url,
                                         const CefString& title,
                                         int currentPage,
                                         int maxPages,
                                         CefString& topLeft,
                                         CefString& topCenter,
                                         CefString& topRight,
                                         CefString& bottomLeft,
                                         CefString& bottomCenter,
                                         CefString& bottomRight) {
  REQUIRE_UI_THREAD();

  // Place the page title at top left
  topLeft = title;
  // Place the page URL at top right
  topRight = url;

  // Place "Page X of Y" at bottom center
  std::stringstream strstream;
  strstream << "Page " << currentPage << " of " << maxPages;
  bottomCenter = strstream.str();

  return false;
}

void ClientHandler::OnContextCreated(CefRefPtr<CefBrowser> browser,
                                     CefRefPtr<CefFrame> frame,
                                     CefRefPtr<CefV8Context> context) {
  REQUIRE_UI_THREAD();

  CefRefPtr<CefV8Value> object = context->GetGlobal();

  // Add the V8 bindings.
  InitBindingTest(browser, frame, object);

  std::string url = frame->GetURL();
  if (url == performance_test::kTestUrl)
    performance_test::InitTest(browser, frame, object);
}

bool ClientHandler::OnDragStart(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefDragData> dragData,
                                DragOperationsMask mask) {
  REQUIRE_UI_THREAD();

  // Forbid dragging of image files.
  if (dragData->IsFile()) {
    std::string fileName = dragData->GetFileName();
    if (fileName.find(".png") != std::string::npos ||
        fileName.find(".jpg") != std::string::npos ||
        fileName.find(".gif") != std::string::npos)
      return true;
  }

  return false;
}

bool ClientHandler::OnDragEnter(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefDragData> dragData,
                                DragOperationsMask mask) {
  REQUIRE_UI_THREAD();

  // Forbid dragging of link URLs.
  if (dragData->IsLink())
    return true;

  return false;
}

bool ClientHandler::OnBeforeScriptExtensionLoad(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    const CefString& extensionName) {
  return false;
}

void ClientHandler::OnRequestGeolocationPermission(
      CefRefPtr<CefBrowser> browser,
      const CefString& requesting_url,
      int request_id,
      CefRefPtr<CefGeolocationCallback> callback) {
  // Allow geolocation access from all websites.
  callback->Continue(true);
}

void ClientHandler::NotifyDownloadComplete(const CefString& fileName) {
  SetLastDownloadFile(fileName);
  SendNotification(NOTIFY_DOWNLOAD_COMPLETE);
}

void ClientHandler::NotifyDownloadError(const CefString& fileName) {
  SetLastDownloadFile(fileName);
  SendNotification(NOTIFY_DOWNLOAD_ERROR);
}

void ClientHandler::SetMainHwnd(CefWindowHandle hwnd) {
  AutoLock lock_scope(this);
  m_MainHwnd = hwnd;
}

void ClientHandler::SetEditHwnd(CefWindowHandle hwnd) {
  AutoLock lock_scope(this);
  m_EditHwnd = hwnd;
}

void ClientHandler::SetButtonHwnds(CefWindowHandle backHwnd,
                                   CefWindowHandle forwardHwnd,
                                   CefWindowHandle reloadHwnd,
                                   CefWindowHandle stopHwnd) {
  AutoLock lock_scope(this);
  m_BackHwnd = backHwnd;
  m_ForwardHwnd = forwardHwnd;
  m_ReloadHwnd = reloadHwnd;
  m_StopHwnd = stopHwnd;
}

std::string ClientHandler::GetLogFile() {
  AutoLock lock_scope(this);
  return m_LogFile;
}

void ClientHandler::SetLastDownloadFile(const std::string& fileName) {
  AutoLock lock_scope(this);
  m_LastDownloadFile = fileName;
}

std::string ClientHandler::GetLastDownloadFile() {
  AutoLock lock_scope(this);
  return m_LastDownloadFile;
}

void ClientHandler::AddDOMVisitor(const std::string& path,
                                  CefRefPtr<CefDOMVisitor> visitor) {
  AutoLock lock_scope(this);
  DOMVisitorMap::iterator it = m_DOMVisitors.find(path);
  if (it == m_DOMVisitors.end())
    m_DOMVisitors.insert(std::make_pair(path, visitor));
  else
    it->second = visitor;
}

CefRefPtr<CefDOMVisitor> ClientHandler::GetDOMVisitor(const std::string& path) {
  AutoLock lock_scope(this);
  DOMVisitorMap::iterator it = m_DOMVisitors.find(path);
  if (it != m_DOMVisitors.end())
    return it->second;
  return NULL;
}
