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
    const std::wstring& fileName)
{
  handler_->SetLastDownloadFile(fileName);
  handler_->SendNotification(NOTIFY_DOWNLOAD_COMPLETE);
}

void ClientHandler::ClientDownloadListener::NotifyDownloadError(
    const std::wstring& fileName)
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
    const std::wstring& failedUrl, std::wstring& errorText)
{
  if(errorCode == ERR_CACHE_MISS)
  {
    // Usually caused by navigating to a page with POST data via back or
    // forward buttons.
    errorText = L"<html><head><title>Expired Form Data</title></head>"
                L"<body><h1>Expired Form Data</h1>"
                L"<h2>Your form request has expired. "
                L"Click reload to re-submit the form data.</h2></body>"
                L"</html>";
  }
  else
  {
    // All other messages.
    std::wstringstream ss;
    ss <<       L"<html><head><title>Load Failed</title></head>"
                L"<body><h1>Load Failed</h1>"
                L"<h2>Load of URL " << failedUrl <<
                L"failed with error code " << static_cast<int>(errorCode) <<
                L".</h2></body>"
                L"</html>";
    errorText = ss.str();
  }
  return RV_HANDLED;
}

CefHandler::RetVal ClientHandler::HandleDownloadResponse(
    CefRefPtr<CefBrowser> browser, const std::wstring& mimeType,
    const std::wstring& fileName, int64 contentLength,
    CefRefPtr<CefDownloadHandler>& handler)
{
  // Create the handler for the file download.
  handler = CreateDownloadHandler(m_DownloadListener, fileName);
  return RV_CONTINUE;
}

CefHandler::RetVal ClientHandler::HandlePrintHeaderFooter(
    CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
    CefPrintInfo& printInfo, const std::wstring& url, const std::wstring& title,
    int currentPage, int maxPages, std::wstring& topLeft,
    std::wstring& topCenter, std::wstring& topRight, std::wstring& bottomLeft,
    std::wstring& bottomCenter, std::wstring& bottomRight)
{
  // Place the page title at top left
  topLeft = title;
  // Place the page URL at top right
  topRight = url;
  
  // Place "Page X of Y" at bottom center
  std::wstringstream strstream;
  strstream << L"Page " << currentPage << L" of " << maxPages;
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
    CefRefPtr<CefBrowser> browser, const std::wstring& message,
    const std::wstring& source, int line)
{
  Lock();
  bool first_message = m_LogFile.empty();
  if(first_message) {
    std::wstringstream ss;
    ss << AppGetWorkingDirectory();
#ifdef _WIN32
    ss << L"\\";
#else
    ss << L"/";
#endif
    ss << L"console.log";
    m_LogFile = ss.str();
  }
  std::wstring logFile = m_LogFile;
  Unlock();
  
  FILE* file = NULL;
#ifdef _WIN32
  _wfopen_s(&file, logFile.c_str(), L"a, ccs=UTF-8");
#else
  file = fopen(WStringToString(logFile).c_str(), "a");
#endif
  
  if(file) {
    std::wstringstream ss;
    ss << L"Message: " << message << L"\r\nSource: " << source <<
        L"\r\nLine: " << line << L"\r\n-----------------------\r\n";
    fputws(ss.str().c_str(), file);
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

std::wstring ClientHandler::GetLogFile()
{
  Lock();
  std::wstring str = m_LogFile;
  Unlock();
  return str;
}

void ClientHandler::SetLastDownloadFile(const std::wstring& fileName)
{
  Lock();
  m_LastDownloadFile = fileName;
  Unlock();
}

std::wstring ClientHandler::GetLastDownloadFile()
{
  std::wstring str;
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
  std::wstring source = frame->GetSource();
  source = StringReplace(source, L"<", L"&lt;");
  source = StringReplace(source, L">", L"&gt;");
  std::wstringstream ss;
  ss << L"<html><body>Source:" << L"<pre>" << source
      << L"</pre></body></html>";
  frame->LoadString(ss.str(), L"http://tests/getsource");
}

void RunGetTextTest(CefRefPtr<CefFrame> frame)
{
  // Retrieve the current page text and display.
  std::wstring text = frame->GetText();
  text = StringReplace(text, L"<", L"&lt;");
  text = StringReplace(text, L">", L"&gt;");
  std::wstringstream ss;
  ss << L"<html><body>Text:" << L"<pre>" << text
      << L"</pre></body></html>";
  frame->LoadString(ss.str(), L"http://tests/gettext");
}

void RunRequestTest(CefRefPtr<CefBrowser> browser)
{
	// Create a new request
  CefRefPtr<CefRequest> request(CefRequest::CreateRequest());

  // Set the request URL
  request->SetURL(L"http://tests/request");

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
      std::make_pair(L"X-My-Header", L"My Header Value"));
  request->SetHeaderMap(headerMap);

  // Load the request
  browser->GetMainFrame()->LoadRequest(request);
}

void RunJavaScriptExecuteTest(CefRefPtr<CefBrowser> browser)
{
  browser->GetMainFrame()->ExecuteJavaScript(
      L"alert('JavaScript execute works!');", L"about:blank", 0);
}

void RunPopupTest(CefRefPtr<CefBrowser> browser)
{
  browser->GetMainFrame()->ExecuteJavaScript(
      L"window.open('http://www.google.com');", L"about:blank", 0);
}
