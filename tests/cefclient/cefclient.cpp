// Copyright (c) 2010 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef.h"
#include "include/cef_runnable.h"
#include "include/cef_wrapper.h"
#include "cefclient.h"
#include "binding_test.h"
#include "download_handler.h"
#include "string_util.h"
#include "util.h"
#include <sstream>
#include <stdio.h>
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
  : m_MainHwnd(NULL),
    m_BrowserHwnd(NULL),
    m_EditHwnd(NULL),
    m_BackHwnd(NULL),
    m_ForwardHwnd(NULL),
    m_StopHwnd(NULL),
    m_ReloadHwnd(NULL),
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
  REQUIRE_UI_THREAD();

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


CefHandler::RetVal ClientHandler::HandleNavStateChange(
    CefRefPtr<CefBrowser> browser, bool canGoBack, bool canGoForward)
{
  REQUIRE_UI_THREAD();

  SetNavState(canGoBack, canGoForward);

  return RV_CONTINUE;
}

CefHandler::RetVal ClientHandler::HandleLoadStart(CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame)
{
  REQUIRE_UI_THREAD();

  if(!browser->IsPopup() && frame->IsMain())
  {
    // We've just started loading a page
    SetLoading(true);
  }
  return RV_CONTINUE;
}

CefHandler::RetVal ClientHandler::HandleLoadEnd(CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame, int httpStatusCode)
{
  REQUIRE_UI_THREAD();

  if(!browser->IsPopup() && frame->IsMain())
  {
    // We've just finished loading a page
    SetLoading(false);

    CefRefPtr<CefDOMVisitor> visitor = GetDOMVisitor(frame->GetURL());
    if(visitor.get())
      frame->VisitDOM(visitor);
  }
  return RV_CONTINUE;
}

CefHandler::RetVal ClientHandler::HandleLoadError(CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame, ErrorCode errorCode, const CefString& failedUrl,
    CefString& errorText)
{
  REQUIRE_UI_THREAD();

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
  REQUIRE_UI_THREAD();

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
  REQUIRE_UI_THREAD();

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
  REQUIRE_UI_THREAD();

  // Add the V8 bindings.
  InitBindingTest(browser, frame, object);
  
  return RV_HANDLED;
}

CefHandler::RetVal ClientHandler::HandleBeforeWindowClose(
    CefRefPtr<CefBrowser> browser)
{
  REQUIRE_UI_THREAD();

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
  REQUIRE_UI_THREAD();

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

void ClientHandler::SetButtonHwnds(CefWindowHandle backHwnd,
                                   CefWindowHandle forwardHwnd,
                                   CefWindowHandle reloadHwnd,
                                   CefWindowHandle stopHwnd)
{
  Lock();
  m_BackHwnd = backHwnd;
  m_ForwardHwnd = forwardHwnd;
  m_ReloadHwnd = reloadHwnd;
  m_StopHwnd = stopHwnd;
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

void ClientHandler::AddDOMVisitor(const std::string& path,
                                  CefRefPtr<CefDOMVisitor> visitor)
{
  AutoLock lock_scope(this);
  DOMVisitorMap::iterator it = m_DOMVisitors.find(path);
  if (it == m_DOMVisitors.end())
    m_DOMVisitors.insert(std::make_pair(path, visitor));
  else
    it->second = visitor;
}

CefRefPtr<CefDOMVisitor> ClientHandler::GetDOMVisitor(const std::string& path)
{
  AutoLock lock_scope(this);
  DOMVisitorMap::iterator it = m_DOMVisitors.find(path);
  if (it != m_DOMVisitors.end())
    return it->second;
  return NULL;
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

static void ExecuteGetSource(CefRefPtr<CefFrame> frame)
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

void RunGetSourceTest(CefRefPtr<CefBrowser> browser)
{
  // Execute the GetSource() call on the UI thread.
  CefPostTask(TID_UI,
      NewCefRunnableFunction(&ExecuteGetSource, browser->GetMainFrame()));
}

static void ExecuteGetText(CefRefPtr<CefFrame> frame)
{
  std::string text = frame->GetText();
  text = StringReplace(text, "<", "&lt;");
  text = StringReplace(text, ">", "&gt;");
  std::stringstream ss;
  ss << "<html><body>Text:" << "<pre>" << text
      << "</pre></body></html>";
  frame->LoadString(ss.str(), "http://tests/gettext");
}

void RunGetTextTest(CefRefPtr<CefBrowser> browser)
{
  // Execute the GetText() call on the UI thread.
  CefPostTask(TID_UI,
      NewCefRunnableFunction(&ExecuteGetText, browser->GetMainFrame()));
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

void RunAccelerated2DCanvasTest(CefRefPtr<CefBrowser> browser)
{
  browser->GetMainFrame()->LoadURL(
      "http://mudcu.be/labs/JS1k/BreathingGalaxies.html");
}

void RunAcceleratedLayersTest(CefRefPtr<CefBrowser> browser)
{
  browser->GetMainFrame()->LoadURL(
      "http://webkit.org/blog-files/3d-transforms/poster-circle.html");
}

void RunWebGLTest(CefRefPtr<CefBrowser> browser)
{
  browser->GetMainFrame()->LoadURL(
      "http://webglsamples.googlecode.com/hg/field/field.html");
}

void RunHTML5VideoTest(CefRefPtr<CefBrowser> browser)
{
  browser->GetMainFrame()->LoadURL(
      "http://www.youtube.com/watch?v=siOHh0uzcuY&html5=True");
}

void RunXMLHTTPRequestTest(CefRefPtr<CefBrowser> browser)
{
  browser->GetMainFrame()->LoadURL("http://tests/xmlhttprequest");
}

void RunWebURLRequestTest(CefRefPtr<CefBrowser> browser)
{
  class RequestClient : public CefThreadSafeBase<CefWebURLRequestClient>
  {
  public:
    RequestClient(CefRefPtr<CefBrowser> browser) : browser_(browser) {}

    virtual void OnStateChange(CefRefPtr<CefWebURLRequest> requester, 
                               RequestState state)
    {
      REQUIRE_UI_THREAD();
      if (state == WUR_STATE_DONE) {
        buffer_ = StringReplace(buffer_, "<", "&lt;");
        buffer_ = StringReplace(buffer_, ">", "&gt;");
        std::stringstream ss;
        ss << "<html><body>Source:<pre>" << buffer_ << "</pre></body></html>";

        browser_->GetMainFrame()->LoadString(ss.str(),
            "http://tests/weburlrequest");
      }
    }
    
    virtual void OnRedirect(CefRefPtr<CefWebURLRequest> requester, 
                            CefRefPtr<CefRequest> request, 
                            CefRefPtr<CefResponse> response)
    {
      REQUIRE_UI_THREAD();
    }
    
    virtual void OnHeadersReceived(CefRefPtr<CefWebURLRequest> requester, 
                                   CefRefPtr<CefResponse> response)
    {
      REQUIRE_UI_THREAD();
    }
    
    virtual void OnProgress(CefRefPtr<CefWebURLRequest> requester, 
                            uint64 bytesSent, uint64 totalBytesToBeSent)
    {
      REQUIRE_UI_THREAD();
    }
    
    virtual void OnData(CefRefPtr<CefWebURLRequest> requester, 
                        const void* data, int dataLength)
    {
      REQUIRE_UI_THREAD();
      buffer_.append(static_cast<const char*>(data), dataLength);
    }
    
    virtual void OnError(CefRefPtr<CefWebURLRequest> requester, 
                         ErrorCode errorCode)
    {
      REQUIRE_UI_THREAD();
      std::stringstream ss;
      ss << "Load failed with error code " << errorCode;
      browser_->GetMainFrame()->LoadString(ss.str(),
          "http://tests/weburlrequest");
    }

  protected:
    CefRefPtr<CefBrowser> browser_;
    std::string buffer_;
  };

  CefRefPtr<CefRequest> request(CefRequest::CreateRequest());
  request->SetURL("http://www.google.com");

  CefRefPtr<CefWebURLRequestClient> client(new RequestClient(browser));
  CefRefPtr<CefWebURLRequest> requester(
      CefWebURLRequest::CreateWebURLRequest(request, client));
}

void RunDOMAccessTest(CefRefPtr<CefBrowser> browser)
{
  class Listener : public CefThreadSafeBase<CefDOMEventListener>
  {
  public:
    Listener() {}
    virtual void HandleEvent(CefRefPtr<CefDOMEvent> event)
    {
      CefRefPtr<CefDOMDocument> document = event->GetDocument();
      ASSERT(document.get());
      
      std::stringstream ss;

      CefRefPtr<CefDOMNode> button = event->GetTarget();
      ASSERT(button.get());
      std::string buttonValue = button->GetElementAttribute("value");
      ss << "You clicked the " << buttonValue.c_str() << " button. ";
      
      if (document->HasSelection()) {
        std::string startName, endName;
      
        // Determine the start name by first trying to locate the "id" attribute
        // and then defaulting to the tag name.
        {
          CefRefPtr<CefDOMNode> node = document->GetSelectionStartNode();
          if (!node->IsElement())
            node = node->GetParent();
          if (node->IsElement() && node->HasElementAttribute("id"))
            startName = node->GetElementAttribute("id");
          else
            startName = node->GetName();
        }

        // Determine the end name by first trying to locate the "id" attribute
        // and then defaulting to the tag name.
        {
          CefRefPtr<CefDOMNode> node = document->GetSelectionEndNode();
          if (!node->IsElement())
            node = node->GetParent();
          if (node->IsElement() && node->HasElementAttribute("id"))
            endName = node->GetElementAttribute("id");
          else
            endName = node->GetName();
        }

        ss << "The selection is from " <<
            startName.c_str() << ":" << document->GetSelectionStartOffset() <<
            " to " <<
            endName.c_str() << ":" << document->GetSelectionEndOffset();
      } else {
        ss << "Nothing is selected.";
      }
      
      // Update the description.
      CefRefPtr<CefDOMNode> desc = document->GetElementById("description");
      ASSERT(desc.get());
      CefRefPtr<CefDOMNode> text = desc->GetFirstChild();
      ASSERT(text.get());
      ASSERT(text->IsText());
      text->SetValue(ss.str());
    }
  };

  class Visitor : public CefThreadSafeBase<CefDOMVisitor>
  {
  public:
    Visitor() {}
    virtual void Visit(CefRefPtr<CefDOMDocument> document)
    {
      // Register an click listener for the button.
      CefRefPtr<CefDOMNode> button = document->GetElementById("button");
      ASSERT(button.get());
      button->AddEventListener("click", new Listener(), false);
    }
  };

  // The DOM visitor will be called after the path is loaded.
  CefRefPtr<CefHandler> handler = browser->GetHandler();
  static_cast<ClientHandler*>(handler.get())->AddDOMVisitor(
      "http://tests/domaccess", new Visitor());

  browser->GetMainFrame()->LoadURL("http://tests/domaccess");
}

void RunDragDropTest(CefRefPtr<CefBrowser> browser)
{
  browser->GetMainFrame()->LoadURL("http://html5demos.com/drag");
}
