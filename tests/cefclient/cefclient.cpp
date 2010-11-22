// Copyright (c) 2010 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef.h"
#include "include/cef_wrapper.h"
#include "cefclient.h"
#include "binding_test.h"
#include "download_handler.h"
#include "string_util.h"
#include "util.h"
#include <sstream>
#include <string>


// ClientHandler::ClientDownloadListener implementation

void ClientHandler::ClientDownloadListener::NotifyDownloadComplete(
    const CefString& fileName)
{
  handler_->SetLastDownloadFile(fileName);
  handler_->SendNotification(NOTIFY_DOWNLOAD_COMPLETE);
}

void ClientHandler::ClientDownloadListener::NotifyDownloadError(
    const CefString& fileName)
{
  handler_->SetLastDownloadFile(fileName);
  handler_->SendNotification(NOTIFY_DOWNLOAD_ERROR);
}


// ClientHandler implementation

ClientHandler::ClientHandler()
  : m_MainHwnd(NULL), m_BrowserHwnd(NULL), m_EditHwnd(NULL), m_bLoading(false),
    m_bCanGoBack(false), m_bCanGoForward(false),
    ALLOW_THIS_IN_INITIALIZER_LIST(
        m_DownloadListener(new ClientDownloadListener(this)))
{
}

ClientHandler::~ClientHandler()
{
}

CefHandler::RetVal ClientHandler::HandleAfterCreated(
    CefRefPtr<CefBrowser> browser)
{
  Lock();
  if(!browser->IsPopup())
  {
    // We need to keep the main child window, but not popup windows
    m_Browser = browser;
    m_BrowserHwnd = browser->GetWindowHandle();
  }
  Unlock();
  return RV_CONTINUE;
}

CefHandler::RetVal ClientHandler::HandleLoadStart(CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame)
{
  if(!browser->IsPopup() && !frame.get())
  {
    Lock();
    // We've just started loading a page
    m_bLoading = true;
    m_bCanGoBack = false;
    m_bCanGoForward = false;
    Unlock();
  }
  return RV_CONTINUE;
}

CefHandler::RetVal ClientHandler::HandleLoadEnd(CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame)
{
  if(!browser->IsPopup() && !frame.get())
  {
    Lock();
    // We've just finished loading a page
    m_bLoading = false;
    m_bCanGoBack = browser->CanGoBack();
    m_bCanGoForward = browser->CanGoForward();
    Unlock();
  }
  return RV_CONTINUE;
}

CefHandler::RetVal ClientHandler::HandleLoadError(CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame, ErrorCode errorCode,
    const CefString& failedUrl, CefString& errorText)
{
  if(errorCode == ERR_CACHE_MISS)
  {
    // Usually caused by navigating to a page with POST data via back or
    // forward buttons.
    errorText = "<html><head><title>Expired Form Data</title></head>"
                "<body><h1>Expired Form Data</h1>"
                "<h2>Your form request has expired. "
                "Click reload to re-submit the form data.</h2></body>"
                "</html>";
  }
  else
  {
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
  return RV_HANDLED;
}

CefHandler::RetVal ClientHandler::HandleDownloadResponse(
    CefRefPtr<CefBrowser> browser, const CefString& mimeType,
    const CefString& fileName, int64 contentLength,
    CefRefPtr<CefDownloadHandler>& handler)
{
  // Create the handler for the file download.
  handler = CreateDownloadHandler(m_DownloadListener, fileName);
  return RV_CONTINUE;
}

CefHandler::RetVal ClientHandler::HandlePrintHeaderFooter(
    CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
    CefPrintInfo& printInfo, const CefString& url, const CefString& title,
    int currentPage, int maxPages, CefString& topLeft,
    CefString& topCenter, CefString& topRight, CefString& bottomLeft,
    CefString& bottomCenter, CefString& bottomRight)
{
  // Place the page title at top left
  topLeft = title;
  // Place the page URL at top right
  topRight = url;
  
  // Place "Page X of Y" at bottom center
  std::stringstream strstream;
  strstream << "Page " << currentPage << " of " << maxPages;
  bottomCenter = strstream.str();

  return RV_CONTINUE;
}

CefHandler::RetVal ClientHandler::HandleJSBinding(CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Value> object)
{
  // Add the V8 bindings.
  InitBindingTest(browser, frame, object);
  
  return RV_HANDLED;
}

CefHandler::RetVal ClientHandler::HandleBeforeWindowClose(
    CefRefPtr<CefBrowser> browser)
{
  if(m_BrowserHwnd == browser->GetWindowHandle())
  {
    // Free the browser pointer so that the browser can be destroyed
    m_Browser = NULL;
  }
  return RV_CONTINUE;
}

CefHandler::RetVal ClientHandler::HandleConsoleMessage(
    CefRefPtr<CefBrowser> browser, const CefString& message,
    const CefString& source, int line)
{
  Lock();
  bool first_message = m_LogFile.empty();
  if(first_message) {
    std::stringstream ss;
    ss << AppGetWorkingDirectory();
#ifdef _WIN32
    ss << "\\";
#else
    ss << "/";
#endif
    ss << "console.log";
    m_LogFile = ss.str();
  }
  std::string logFile = m_LogFile;
  Unlock();
  
  FILE* file = fopen(logFile.c_str(), "a");
  if(file) {
    std::stringstream ss;
    ss << "Message: " << std::string(message) << "\r\nSource: " <<
        std::string(source) << "\r\nLine: " << line <<
        "\r\n-----------------------\r\n";
    fputs(ss.str().c_str(), file);
    fclose(file);

    if(first_message)
      SendNotification(NOTIFY_CONSOLE_MESSAGE);
  }

  return RV_HANDLED;
}

void ClientHandler::GetNavState(bool &isLoading, bool &canGoBack,
                                bool &canGoForward)
{
  Lock();
  isLoading = m_bLoading;
  canGoBack = m_bCanGoBack;
  canGoForward = m_bCanGoForward;
  Unlock();
}

void ClientHandler::SetMainHwnd(CefWindowHandle hwnd)
{
  Lock();
  m_MainHwnd = hwnd;
  Unlock();
}

void ClientHandler::SetEditHwnd(CefWindowHandle hwnd)
{
  Lock();
  m_EditHwnd = hwnd;
  Unlock();
}

std::string ClientHandler::GetLogFile()
{
  Lock();
  std::string str = m_LogFile;
  Unlock();
  return str;
}

void ClientHandler::SetLastDownloadFile(const std::string& fileName)
{
  Lock();
  m_LastDownloadFile = fileName;
  Unlock();
}

std::string ClientHandler::GetLastDownloadFile()
{
  std::string str;
  Lock();
  str = m_LastDownloadFile;
  Unlock();
  return str;
}


// Global functions

CefRefPtr<ClientHandler> g_handler;

CefRefPtr<CefBrowser> AppGetBrowser()
{
  if(!g_handler.get())
    return NULL;
  return g_handler->GetBrowser();
}

CefWindowHandle AppGetMainHwnd()
{
  if(!g_handler.get())
    return NULL;
  return g_handler->GetMainHwnd();
}

void RunGetSourceTest(CefRefPtr<CefFrame> frame)
{
  // Retrieve the current page source and display.
  std::string source = frame->GetSource();
  source = StringReplace(source, "<", "&lt;");
  source = StringReplace(source, ">", "&gt;");
  std::stringstream ss;
  ss << "<html><body>Source:" << "<pre>" << source
      << "</pre></body></html>";
  frame->LoadString(ss.str(), "http://tests/getsource");
}

void RunGetTextTest(CefRefPtr<CefFrame> frame)
{
  // Retrieve the current page text and display.
  std::string text = frame->GetText();
  text = StringReplace(text, "<", "&lt;");
  text = StringReplace(text, ">", "&gt;");
  std::stringstream ss;
  ss << "<html><body>Text:" << "<pre>" << text
      << "</pre></body></html>";
  frame->LoadString(ss.str(), "http://tests/gettext");
}

void RunRequestTest(CefRefPtr<CefBrowser> browser)
{
	// Create a new request
  CefRefPtr<CefRequest> request(CefRequest::CreateRequest());

  // Set the request URL
  request->SetURL("http://tests/request");

  // Add post data to the request.  The correct method and content-
  // type headers will be set by CEF.
  CefRefPtr<CefPostDataElement> postDataElement(
      CefPostDataElement::CreatePostDataElement());
  std::string data = "arg1=val1&arg2=val2";
  postDataElement->SetToBytes(data.length(), data.c_str());
  CefRefPtr<CefPostData> postData(CefPostData::CreatePostData());
  postData->AddElement(postDataElement);
  request->SetPostData(postData);

  // Add a custom header
  CefRequest::HeaderMap headerMap;
  headerMap.insert(
      std::make_pair("X-My-Header", "My Header Value"));
  request->SetHeaderMap(headerMap);

  // Load the request
  browser->GetMainFrame()->LoadRequest(request);
}

void RunJavaScriptExecuteTest(CefRefPtr<CefBrowser> browser)
{
  browser->GetMainFrame()->ExecuteJavaScript(
      "alert('JavaScript execute works!');", "about:blank", 0);
}

void RunPopupTest(CefRefPtr<CefBrowser> browser)
{
  browser->GetMainFrame()->ExecuteJavaScript(
      "window.open('http://www.google.com');", "about:blank", 0);
}

void RunLocalStorageTest(CefRefPtr<CefBrowser> browser)
{
  browser->GetMainFrame()->LoadURL("http://tests/localstorage");
}
