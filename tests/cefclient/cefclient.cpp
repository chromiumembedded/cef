// Copyright (c) 2010 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef.h"
#include "include/cef_runnable.h"
#include "include/cef_wrapper.h"
#include "cefclient.h"
#include "client_handler.h"
#include "binding_test.h"
#include "string_util.h"
#include "util.h"
#include <sstream>
#include <stdio.h>
#include <string>


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
  class RequestClient : public CefWebURLRequestClient
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

    IMPLEMENT_REFCOUNTING(CefWebURLRequestClient);
  };

  CefRefPtr<CefRequest> request(CefRequest::CreateRequest());
  request->SetURL("http://www.google.com");

  CefRefPtr<CefWebURLRequestClient> client(new RequestClient(browser));
  CefRefPtr<CefWebURLRequest> requester(
      CefWebURLRequest::CreateWebURLRequest(request, client));
}

void RunDOMAccessTest(CefRefPtr<CefBrowser> browser)
{
  class Listener : public CefDOMEventListener
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

    IMPLEMENT_REFCOUNTING(Listener);
  };

  class Visitor : public CefDOMVisitor
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

    IMPLEMENT_REFCOUNTING(Visitor);
  };

  // The DOM visitor will be called after the path is loaded.
  CefRefPtr<CefClient> client = browser->GetClient();
  static_cast<ClientHandler*>(client.get())->AddDOMVisitor(
      "http://tests/domaccess", new Visitor());

  browser->GetMainFrame()->LoadURL("http://tests/domaccess");
}

void RunDragDropTest(CefRefPtr<CefBrowser> browser)
{
  browser->GetMainFrame()->LoadURL("http://html5demos.com/drag");
}

void RunModalDialogTest(CefRefPtr<CefBrowser> browser)
{
  browser->GetMainFrame()->LoadURL("http://tests/modalmain");
}
