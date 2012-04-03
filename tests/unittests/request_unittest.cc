// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef_request.h"
#include "tests/unittests/test_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Verify that CefRequest::HeaderMap objects are equal
// If |allowExtras| is true then additional header fields will be allowed in
// |map2|.
void VerifyMapEqual(CefRequest::HeaderMap &map1,
                    CefRequest::HeaderMap &map2,
                    bool allowExtras) {
  if (!allowExtras)
    ASSERT_EQ(map1.size(), map2.size());
  CefRequest::HeaderMap::const_iterator it1, it2;

  for (it1 = map1.begin(); it1 != map1.end(); ++it1) {
    it2 = map2.find(it1->first);
    ASSERT_TRUE(it2 != map2.end());
    ASSERT_EQ(it1->second, it2->second);
  }
}

// Verify that CefPostDataElement objects are equal
void VerifyPostDataElementEqual(CefRefPtr<CefPostDataElement> elem1,
                                CefRefPtr<CefPostDataElement> elem2) {
  ASSERT_EQ(elem1->GetType(), elem2->GetType());
  switch (elem1->GetType()) {
    case PDE_TYPE_BYTES: {
      ASSERT_EQ(elem1->GetBytesCount(), elem2->GetBytesCount());
      size_t bytesCt = elem1->GetBytesCount();
      char* buff1 = new char[bytesCt];
      char* buff2 = new char[bytesCt];
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
void VerifyPostDataEqual(CefRefPtr<CefPostData> postData1,
                         CefRefPtr<CefPostData> postData2) {
  ASSERT_TRUE(!(postData1.get()) == !(postData2.get()));
  ASSERT_EQ(postData1->GetElementCount(), postData2->GetElementCount());

  CefPostData::ElementVector ev1, ev2;
  postData1->GetElements(ev1);
  postData1->GetElements(ev2);
  ASSERT_EQ(ev1.size(), ev2.size());

  CefPostData::ElementVector::const_iterator it1 = ev1.begin();
  CefPostData::ElementVector::const_iterator it2 = ev2.begin();
  for (; it1 != ev1.end() && it2 != ev2.end(); ++it1, ++it2)
    VerifyPostDataElementEqual((*it1), (*it2));
}

// Verify that CefRequest objects are equal
// If |allowExtras| is true then additional header fields will be allowed in
// |request2|.
void VerifyRequestEqual(CefRefPtr<CefRequest> request1,
                        CefRefPtr<CefRequest> request2,
                        bool allowExtras) {
  ASSERT_EQ(request1->GetURL(), request2->GetURL());
  ASSERT_EQ(request1->GetMethod(), request2->GetMethod());

  CefRequest::HeaderMap headers1, headers2;
  request1->GetHeaderMap(headers1);
  request2->GetHeaderMap(headers2);
  VerifyMapEqual(headers1, headers2, allowExtras);

  VerifyPostDataEqual(request1->GetPostData(), request2->GetPostData());
}

}  // namespace

// Verify Set/Get methods for CefRequest, CefPostData and CefPostDataElement.
TEST(RequestTest, SetGet) {
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
  for (size_t i = 0; it != elements.end(); ++it, ++i) {
    if (i == 0)
      VerifyPostDataElementEqual(element1, (*it).get());
    else if (i == 1)
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

namespace {

void CreateRequest(CefRefPtr<CefRequest>& request) {
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
  char bytes[] = "Test Bytes";
  element1->SetToBytes(sizeof(bytes), bytes);
  postData->AddElement(element1);

  request->SetPostData(postData);
}

class RequestSendRecvTestHandler : public TestHandler {
 public:
  RequestSendRecvTestHandler() {}

  virtual void RunTest() OVERRIDE {
    // Create the test request
    CreateRequest(request_);

    // Create the browser
    CreateBrowser("about:blank");
  }

  virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) OVERRIDE {
    TestHandler::OnAfterCreated(browser);

    // Load the test request
    browser->GetMainFrame()->LoadRequest(request_);
  }

  virtual bool OnBeforeResourceLoad(CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefFrame> frame,
                                    CefRefPtr<CefRequest> request) OVERRIDE {
    // Verify that the request is the same
    VerifyRequestEqual(request_, request, true);

    got_before_resource_load_.yes();

    return false;
  }

  virtual CefRefPtr<CefResourceHandler> GetResourceHandler(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request) OVERRIDE {
    // Verify that the request is the same
    VerifyRequestEqual(request_, request, true);

    got_resource_handler_.yes();

    DestroyTest();

    // No results
    return NULL;
  }

  CefRefPtr<CefRequest> request_;

  TrackCallback got_before_resource_load_;
  TrackCallback got_resource_handler_;
};

}  // namespace

// Verify send and recieve
TEST(RequestTest, SendRecv) {
  CefRefPtr<RequestSendRecvTestHandler> handler =
      new RequestSendRecvTestHandler();
  handler->ExecuteTest();

  ASSERT_TRUE(handler->got_before_resource_load_);
  ASSERT_TRUE(handler->got_resource_handler_);
}
