// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "test_handler.h"


// Verify that CefRequest::HeaderMap objects are equal
// If |allowExtras| is true then additional header fields will be allowed in
// |map2|.
static void VerifyMapEqual(CefRequest::HeaderMap &map1,
                           CefRequest::HeaderMap &map2,
                           bool allowExtras)
{
  if(!allowExtras)
    ASSERT_EQ(map1.size(), map2.size());
  CefRequest::HeaderMap::const_iterator it1, it2;

  for(it1 = map1.begin(); it1 != map1.end(); ++it1) {
    it2 = map2.find(it1->first);
    ASSERT_TRUE(it2 != map2.end());
    ASSERT_EQ(it1->second, it2->second);
  }
}

// Verify that CefPostDataElement objects are equal
static void VerifyPostDataElementEqual(CefRefPtr<CefPostDataElement> elem1,
                                       CefRefPtr<CefPostDataElement> elem2)
{
  ASSERT_EQ(elem1->GetType(), elem2->GetType());
  switch(elem1->GetType()) {
    case PDE_TYPE_BYTES: {
      ASSERT_EQ(elem1->GetBytesCount(), elem2->GetBytesCount());
      char *buff1, *buff2;
      size_t bytesCt = elem1->GetBytesCount();
      buff1 = new char[bytesCt];
      buff2 = new char[bytesCt];
      elem1->GetBytes(bytesCt, buff1);
      elem2->GetBytes(bytesCt, buff2);
      ASSERT_TRUE(!memcmp(buff1, buff2, bytesCt));
      delete [] buff1;
      delete [] buff2;
    }  break;
    case PDE_TYPE_FILE:
      ASSERT_EQ(elem1->GetFile(), elem2->GetFile());
      break;
    default:
      break;
  }
}

// Verify that CefPostData objects are equal
static void VerifyPostDataEqual(CefRefPtr<CefPostData> postData1,
                                CefRefPtr<CefPostData> postData2)
{
  ASSERT_TRUE(!(postData1.get()) == !(postData2.get()));
  ASSERT_EQ(postData1->GetElementCount(), postData2->GetElementCount());
  
  CefPostData::ElementVector ev1, ev2;
  postData1->GetElements(ev1);
  postData1->GetElements(ev2);
  ASSERT_EQ(ev1.size(), ev2.size());

  CefPostData::ElementVector::const_iterator it1 = ev1.begin();
  CefPostData::ElementVector::const_iterator it2 = ev2.begin();
  for(; it1 != ev1.end() && it2 != ev2.end(); ++it1, ++it2)
    VerifyPostDataElementEqual((*it1), (*it2));
}

// Verify that CefRequest objects are equal
// If |allowExtras| is true then additional header fields will be allowed in
// |request2|.
static void VerifyRequestEqual(CefRefPtr<CefRequest> request1,
                               CefRefPtr<CefRequest> request2,
                               bool allowExtras)
{
  ASSERT_EQ(request1->GetURL(), request2->GetURL());
  ASSERT_EQ(request1->GetMethod(), request2->GetMethod());
  
  CefRequest::HeaderMap headers1, headers2;
  request1->GetHeaderMap(headers1);
  request2->GetHeaderMap(headers2);
  VerifyMapEqual(headers1, headers2, allowExtras);

  VerifyPostDataEqual(request1->GetPostData(), request2->GetPostData());
}

// Verify Set/Get methods for CefRequest, CefPostData and CefPostDataElement.
TEST(RequestTest, SetGet)
{
  // CefRequest CreateRequest
  CefRefPtr<CefRequest> request(CefRequest::CreateRequest());
  ASSERT_TRUE(request.get() != NULL);

  CefString url = "http://tests/run.html";
  CefString method = "POST";
  CefRequest::HeaderMap setHeaders, getHeaders;
  setHeaders.insert(std::make_pair("HeaderA", "ValueA"));
  setHeaders.insert(std::make_pair("HeaderB", "ValueB"));

  // CefPostData CreatePostData
  CefRefPtr<CefPostData> postData(CefPostData::CreatePostData());
  ASSERT_TRUE(postData.get() != NULL);

  // CefPostDataElement CreatePostDataElement
  CefRefPtr<CefPostDataElement> element1(
      CefPostDataElement::CreatePostDataElement());
  ASSERT_TRUE(element1.get() != NULL);
  CefRefPtr<CefPostDataElement> element2(
      CefPostDataElement::CreatePostDataElement());
  ASSERT_TRUE(element2.get() != NULL);
  
  // CefPostDataElement SetToFile
  CefString file = "c:\\path\\to\\file.ext";
  element1->SetToFile(file);
  ASSERT_EQ(PDE_TYPE_FILE, element1->GetType());
  ASSERT_EQ(file, element1->GetFile());

  // CefPostDataElement SetToBytes
  char bytes[] = "Test Bytes";
  element2->SetToBytes(sizeof(bytes), bytes);
  ASSERT_EQ(PDE_TYPE_BYTES, element2->GetType());
  ASSERT_EQ(sizeof(bytes), element2->GetBytesCount());
  char bytesOut[sizeof(bytes)];
  element2->GetBytes(sizeof(bytes), bytesOut);
  ASSERT_TRUE(!memcmp(bytes, bytesOut, sizeof(bytes)));

  // CefPostData AddElement
  postData->AddElement(element1);
  postData->AddElement(element2);
  ASSERT_EQ((size_t)2, postData->GetElementCount());

  // CefPostData RemoveElement
  postData->RemoveElement(element1);
  ASSERT_EQ((size_t)1, postData->GetElementCount());

  // CefPostData RemoveElements
  postData->RemoveElements();
  ASSERT_EQ((size_t)0, postData->GetElementCount());

  postData->AddElement(element1);
  postData->AddElement(element2);
  ASSERT_EQ((size_t)2, postData->GetElementCount());
  CefPostData::ElementVector elements;
  postData->GetElements(elements);
  CefPostData::ElementVector::const_iterator it = elements.begin();
  for(size_t i = 0; it != elements.end(); ++it, ++i) {
    if(i == 0)
      VerifyPostDataElementEqual(element1, (*it).get());
    else if(i == 1)
      VerifyPostDataElementEqual(element2, (*it).get());
  }
  
  // CefRequest SetURL
  request->SetURL(url);
  ASSERT_EQ(url, request->GetURL());

  // CefRequest SetMethod
  request->SetMethod(method);
  ASSERT_EQ(method, request->GetMethod());

  // CefRequest SetHeaderMap
  request->SetHeaderMap(setHeaders);
  request->GetHeaderMap(getHeaders);
  VerifyMapEqual(setHeaders, getHeaders, false);
  getHeaders.clear();

  // CefRequest SetPostData
  request->SetPostData(postData);
  VerifyPostDataEqual(postData, request->GetPostData());

  request = CefRequest::CreateRequest();
  ASSERT_TRUE(request.get() != NULL);

  // CefRequest Set
  request->Set(url, method, postData, setHeaders);
  ASSERT_EQ(url, request->GetURL());
  ASSERT_EQ(method, request->GetMethod());
  request->GetHeaderMap(getHeaders);
  VerifyMapEqual(setHeaders, getHeaders, false);
  getHeaders.clear();
  VerifyPostDataEqual(postData, request->GetPostData());
}

static void CreateRequest(CefRefPtr<CefRequest>& request)
{
  request = CefRequest::CreateRequest();
  ASSERT_TRUE(request.get() != NULL);

  request->SetURL("http://tests/run.html");
  request->SetMethod("POST");

  CefRequest::HeaderMap headers;
  headers.insert(std::make_pair("HeaderA", "ValueA"));
  headers.insert(std::make_pair("HeaderB", "ValueB"));
  request->SetHeaderMap(headers);

  CefRefPtr<CefPostData> postData(CefPostData::CreatePostData());
  ASSERT_TRUE(postData.get() != NULL);

  CefRefPtr<CefPostDataElement> element1(
      CefPostDataElement::CreatePostDataElement());
  ASSERT_TRUE(element1.get() != NULL);
  element1->SetToFile("c:\\path\\to\\file.ext");
  postData->AddElement(element1);
  
  CefRefPtr<CefPostDataElement> element2(
      CefPostDataElement::CreatePostDataElement());
  ASSERT_TRUE(element2.get() != NULL);
  char bytes[] = "Test Bytes";
  element2->SetToBytes(sizeof(bytes), bytes);
  postData->AddElement(element2);
  
  request->SetPostData(postData);
}

bool g_RequestSendRecvTestHandlerHandleBeforeBrowseCalled;
bool g_RequestSendRecvTestHandlerHandleBeforeResourceLoadCalled;

class RequestSendRecvTestHandler : public TestHandler
{
public:
  RequestSendRecvTestHandler() {}

  virtual void RunTest() OVERRIDE
  {
    // Create the test request
    CreateRequest(request_);

    // Create the browser
    CreateBrowser(CefString());
  }

  virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) OVERRIDE
  {
    TestHandler::OnAfterCreated(browser);

    // Load the test request
    browser->GetMainFrame()->LoadRequest(request_);
  }

  virtual bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                              CefRefPtr<CefFrame> frame,
                              CefRefPtr<CefRequest> request,
                              NavType navType,
                              bool isRedirect) OVERRIDE
  {
    g_RequestSendRecvTestHandlerHandleBeforeBrowseCalled = true;
    
    // Verify that the request is the same
    VerifyRequestEqual(request_, request, true);
    
    return false;
  }

  virtual bool OnBeforeResourceLoad(CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefRequest> request,
                                    CefString& redirectUrl,
                                    CefRefPtr<CefStreamReader>& resourceStream,
                                    CefRefPtr<CefResponse> response,
                                    int loadFlags) OVERRIDE
  {
    g_RequestSendRecvTestHandlerHandleBeforeResourceLoadCalled = true;
    
    // Verify that the request is the same
    VerifyRequestEqual(request_, request, true);

    // No results
    return true;
  }

  virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         int httpStatusCode) OVERRIDE
  {
    if(!browser->IsPopup() && frame->IsMain())
      DestroyTest();
  }

  CefRefPtr<CefRequest> request_;
};

// Verify send and recieve
TEST(RequestTest, SendRecv)
{
  g_RequestSendRecvTestHandlerHandleBeforeBrowseCalled = false;
  g_RequestSendRecvTestHandlerHandleBeforeResourceLoadCalled = false;

  CefRefPtr<RequestSendRecvTestHandler> handler =
      new RequestSendRecvTestHandler();
  handler->ExecuteTest();

  ASSERT_TRUE(g_RequestSendRecvTestHandlerHandleBeforeBrowseCalled);
  ASSERT_TRUE(g_RequestSendRecvTestHandlerHandleBeforeResourceLoadCalled);
}

// Enable this test if you have applied the patches for issue #42.
#if 0

bool g_RequestHistoryNavTestDidLoadRequest;
bool g_RequestHistoryNavTestDidReloadRequest;

class RequestHistoryNavTestHandler : public TestHandler
{
public:
  RequestHistoryNavTestHandler() : navigated_(false) {}

  virtual void RunTest()
  {
    // Create the test request
    CreateRequest(request_);

    // Add the resource that we will navigate to/from
    AddResource("http://tests/goto.html", "<html>To</html>", "text/html");

    // Create the browser
    CreateBrowser(CefString());
  }

  virtual RetVal HandleAfterCreated(CefRefPtr<CefBrowser> browser)
  {
    TestHandler::HandleAfterCreated(browser);

    // Load the test request
    browser->GetMainFrame()->LoadRequest(request_);
    return RV_CONTINUE;
  }

  virtual RetVal HandleBeforeBrowse(CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefFrame> frame,
                                    CefRefPtr<CefRequest> request,
                                    NavType navType, bool isRedirect)
  {
    CefString url = request->GetURL();
    if(url == "http://tests/run.html")
    {
      // Verify that the request is the same
      VerifyRequestEqual(request_, request, true);
    }
    
    return RV_CONTINUE;
  }

  virtual RetVal HandleBeforeResourceLoad(CefRefPtr<CefBrowser> browser,
                                          CefRefPtr<CefRequest> request,
                                          CefString& redirectUrl,
                                          CefRefPtr<CefStreamReader>& resourceStream,
                                          CefString& mimeType,
                                          int loadFlags)
  {
    CefString url = request->GetURL();
    if(url == "http://tests/run.html")
    {
      // Verify that the request is the same
      VerifyRequestEqual(request_, request, true);

      if(!navigated_)
      {
        // Loading the request for the 1st time
        g_RequestHistoryNavTestDidLoadRequest = true;
      }
      else
      {
        // Re-loading the request
        g_RequestHistoryNavTestDidReloadRequest = true;
      }

      // Return dummy results
      std::string output = "<html>Request</html>";
      resourceStream = CefStreamReader::CreateForData((void*)output.c_str(),
          output.length());
      mimeType = "text/html";
      return RV_CONTINUE;
    }
    else
    {
      // Pass to the default handler to return the to/from page
      return TestHandler::HandleBeforeResourceLoad(browser, request,
          redirectUrl, resourceStream, mimeType, loadFlags);
    }
  }

  virtual RetVal HandleLoadEnd(CefRefPtr<CefBrowser> browser,
                               CefRefPtr<CefFrame> frame,
                               int httpStatusCode)
  {
    if(!browser->IsPopup() && frame->IsMain())
    {
      CefString url = browser->GetMainFrame()->GetURL();
      if(url == "http://tests/run.html")
      {
        if(!navigated_)
        {
          // First resource load, go to the next page
          navigated_ = true;
          browser->GetMainFrame()->LoadURL("http://tests/goto.html");
        }
        else
        {
          // Resource re-load, end the test
          DestroyTest();
        }
      }
      else
      {
        // To/from page load, go back the the request page
        browser->GoBack();
      }
    }
    return RV_CONTINUE;
  }

  CefRefPtr<CefRequest> request_;
  bool navigated_;
};

// Verify history navigation
// This test will only pass if the patches for issue #42 are applied.
TEST(RequestTest, HistoryNav)
{
  g_RequestHistoryNavTestDidLoadRequest = false;
  g_RequestHistoryNavTestDidReloadRequest = false;

  CefRefPtr<RequestHistoryNavTestHandler> handler =
      new RequestHistoryNavTestHandler();
  handler->ExecuteTest();

  ASSERT_TRUE(g_RequestHistoryNavTestDidLoadRequest);
  ASSERT_TRUE(g_RequestHistoryNavTestDidReloadRequest);
}

#endif // 0
