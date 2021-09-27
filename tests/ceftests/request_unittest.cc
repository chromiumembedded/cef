// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <map>

#include "include/base/cef_callback.h"
#include "include/cef_request.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/test_handler.h"
#include "tests/ceftests/test_util.h"
#include "tests/gtest/include/gtest/gtest.h"
#include "tests/shared/renderer/client_app_renderer.h"

using client::ClientAppRenderer;

// Verify Set/Get methods for CefRequest, CefPostData and CefPostDataElement.
TEST(RequestTest, SetGet) {
  // CefRequest CreateRequest
  CefRefPtr<CefRequest> request(CefRequest::Create());
  EXPECT_TRUE(request.get() != nullptr);
  EXPECT_EQ(0U, request->GetIdentifier());

  CefString url = "http://tests.com/run.html";
  CefString method = "POST";
  CefRequest::HeaderMap setHeaders, getHeaders;
  setHeaders.insert(std::make_pair("HeaderA", "ValueA"));
  setHeaders.insert(std::make_pair("HeaderB", "ValueB"));

  // CefPostData CreatePostData
  CefRefPtr<CefPostData> postData(CefPostData::Create());
  EXPECT_TRUE(postData.get() != nullptr);

  // CefPostDataElement CreatePostDataElement
  CefRefPtr<CefPostDataElement> element1(CefPostDataElement::Create());
  EXPECT_TRUE(element1.get() != nullptr);
  CefRefPtr<CefPostDataElement> element2(CefPostDataElement::Create());
  EXPECT_TRUE(element2.get() != nullptr);

  // CefPostDataElement SetToFile
  CefString file = "c:\\path\\to\\file.ext";
  element1->SetToFile(file);
  EXPECT_EQ(PDE_TYPE_FILE, element1->GetType());
  EXPECT_EQ(file, element1->GetFile());

  // CefPostDataElement SetToBytes
  char bytes[] = "Test Bytes";
  element2->SetToBytes(sizeof(bytes), bytes);
  EXPECT_EQ(PDE_TYPE_BYTES, element2->GetType());
  EXPECT_EQ(sizeof(bytes), element2->GetBytesCount());
  char bytesOut[sizeof(bytes)];
  element2->GetBytes(sizeof(bytes), bytesOut);
  EXPECT_TRUE(!memcmp(bytes, bytesOut, sizeof(bytes)));

  // CefPostData AddElement
  postData->AddElement(element1);
  postData->AddElement(element2);
  EXPECT_EQ((size_t)2, postData->GetElementCount());

  // CefPostData RemoveElement
  postData->RemoveElement(element1);
  EXPECT_EQ((size_t)1, postData->GetElementCount());

  // CefPostData RemoveElements
  postData->RemoveElements();
  EXPECT_EQ((size_t)0, postData->GetElementCount());

  postData->AddElement(element1);
  postData->AddElement(element2);
  EXPECT_EQ((size_t)2, postData->GetElementCount());
  CefPostData::ElementVector elements;
  postData->GetElements(elements);
  CefPostData::ElementVector::const_iterator it = elements.begin();
  for (size_t i = 0; it != elements.end(); ++it, ++i) {
    if (i == 0)
      TestPostDataElementEqual(element1, (*it).get());
    else if (i == 1)
      TestPostDataElementEqual(element2, (*it).get());
  }

  // CefRequest SetURL
  request->SetURL(url);
  EXPECT_EQ(url, request->GetURL());

  // CefRequest SetMethod
  request->SetMethod(method);
  EXPECT_EQ(method, request->GetMethod());

  // CefRequest SetReferrer
  CefString referrer = "http://tests.com/referrer.html";
  CefRequest::ReferrerPolicy policy = REFERRER_POLICY_ORIGIN;
  request->SetReferrer(referrer, policy);
  EXPECT_STREQ("http://tests.com/",
               request->GetReferrerURL().ToString().c_str());
  EXPECT_EQ(policy, request->GetReferrerPolicy());

  // CefRequest SetHeaderMap
  request->SetHeaderMap(setHeaders);
  request->GetHeaderMap(getHeaders);
  TestMapEqual(setHeaders, getHeaders, false);
  getHeaders.clear();

  // CefRequest SetPostData
  request->SetPostData(postData);
  TestPostDataEqual(postData, request->GetPostData());

  EXPECT_EQ(0U, request->GetIdentifier());

  request = CefRequest::Create();
  EXPECT_TRUE(request.get() != nullptr);
  EXPECT_EQ(0U, request->GetIdentifier());

  // CefRequest Set
  request->Set(url, method, postData, setHeaders);
  EXPECT_EQ(0U, request->GetIdentifier());
  EXPECT_EQ(url, request->GetURL());
  EXPECT_EQ(method, request->GetMethod());
  request->GetHeaderMap(getHeaders);
  TestMapEqual(setHeaders, getHeaders, false);
  getHeaders.clear();
  TestPostDataEqual(postData, request->GetPostData());
}

TEST(RequestTest, SetGetHeaderByName) {
  CefRefPtr<CefRequest> request(CefRequest::Create());
  EXPECT_TRUE(request.get() != nullptr);

  CefRequest::HeaderMap headers, expectedHeaders;

  request->SetHeaderByName("HeaderA", "ValueA", false);
  request->SetHeaderByName("HeaderB", "ValueB", false);

  expectedHeaders.insert(std::make_pair("HeaderA", "ValueA"));
  expectedHeaders.insert(std::make_pair("HeaderB", "ValueB"));

  // Case insensitive retrieval.
  EXPECT_STREQ("ValueA",
               request->GetHeaderByName("headera").ToString().c_str());
  EXPECT_STREQ("ValueB",
               request->GetHeaderByName("headerb").ToString().c_str());
  EXPECT_STREQ("", request->GetHeaderByName("noexist").ToString().c_str());

  request->GetHeaderMap(headers);
  TestMapEqual(expectedHeaders, headers, false);

  // Replace an existing value.
  request->SetHeaderByName("HeaderA", "ValueANew", true);

  expectedHeaders.clear();
  expectedHeaders.insert(std::make_pair("HeaderA", "ValueANew"));
  expectedHeaders.insert(std::make_pair("HeaderB", "ValueB"));

  // Case insensitive retrieval.
  EXPECT_STREQ("ValueANew",
               request->GetHeaderByName("headerA").ToString().c_str());

  request->GetHeaderMap(headers);
  TestMapEqual(expectedHeaders, headers, false);

  // Header with multiple values.
  expectedHeaders.clear();
  expectedHeaders.insert(std::make_pair("HeaderA", "ValueA1"));
  expectedHeaders.insert(std::make_pair("HeaderA", "ValueA2"));
  expectedHeaders.insert(std::make_pair("HeaderB", "ValueB"));
  request->SetHeaderMap(expectedHeaders);

  // When there are multiple values only the first is returned.
  EXPECT_STREQ("ValueA1",
               request->GetHeaderByName("headera").ToString().c_str());

  // Don't overwrite the value.
  request->SetHeaderByName("HeaderA", "ValueANew", false);

  request->GetHeaderMap(headers);
  TestMapEqual(expectedHeaders, headers, false);

  // Overwrite the value (remove the duplicates).
  request->SetHeaderByName("HeaderA", "ValueANew", true);

  expectedHeaders.clear();
  expectedHeaders.insert(std::make_pair("HeaderA", "ValueANew"));
  expectedHeaders.insert(std::make_pair("HeaderB", "ValueB"));

  request->GetHeaderMap(headers);
  TestMapEqual(expectedHeaders, headers, false);
}

namespace {

const char kTestUrl[] = "http://tests.com/run.html";

void CreateRequest(CefRefPtr<CefRequest>& request) {
  request = CefRequest::Create();
  EXPECT_TRUE(request.get() != nullptr);

  request->SetURL(kTestUrl);
  request->SetMethod("POST");

  request->SetReferrer("http://tests.com/main.html", REFERRER_POLICY_DEFAULT);

  CefRequest::HeaderMap headers;
  headers.insert(std::make_pair("HeaderA", "ValueA"));
  headers.insert(std::make_pair("HeaderB", "ValueB"));
  request->SetHeaderMap(headers);

  CefRefPtr<CefPostData> postData(CefPostData::Create());
  EXPECT_TRUE(postData.get() != nullptr);

  CefRefPtr<CefPostDataElement> element1(CefPostDataElement::Create());
  EXPECT_TRUE(element1.get() != nullptr);
  char bytes[] = "Test Bytes";
  element1->SetToBytes(sizeof(bytes), bytes);
  postData->AddElement(element1);

  request->SetPostData(postData);
}

class RequestSendRecvTestHandler : public TestHandler {
 public:
  RequestSendRecvTestHandler() : response_length_(0), request_id_(0U) {}

  void RunTest() override {
    // Create the test request.
    CreateRequest(request_);

    const std::string& resource = "<html><body>SendRecv Test</body></html>";
    response_length_ = static_cast<int64>(resource.size());
    AddResource(kTestUrl, resource, "text/html");

    // Create the browser.
    CreateBrowser("about:blank");

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    TestHandler::OnAfterCreated(browser);

    // Load the test request.
    browser->GetMainFrame()->LoadRequest(request_);
  }

  cef_return_value_t OnBeforeResourceLoad(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request,
      CefRefPtr<CefCallback> callback) override {
    EXPECT_IO_THREAD();

    request_id_ = request->GetIdentifier();
    DCHECK_GT(request_id_, 0U);

    TestRequest(request);
    EXPECT_FALSE(request->IsReadOnly());

    got_before_resource_load_.yes();

    return RV_CONTINUE;
  }

  CefRefPtr<CefResourceHandler> GetResourceHandler(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request) override {
    EXPECT_IO_THREAD();

    TestRequest(request);
    EXPECT_TRUE(request->IsReadOnly());

    got_resource_handler_.yes();

    return TestHandler::GetResourceHandler(browser, frame, request);
  }

  bool OnResourceResponse(CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefFrame> frame,
                          CefRefPtr<CefRequest> request,
                          CefRefPtr<CefResponse> response) override {
    EXPECT_IO_THREAD();

    TestRequest(request);
    EXPECT_FALSE(request->IsReadOnly());
    TestResponse(response);
    EXPECT_TRUE(response->IsReadOnly());

    got_resource_response_.yes();

    return false;
  }

  CefRefPtr<CefResponseFilter> GetResourceResponseFilter(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request,
      CefRefPtr<CefResponse> response) override {
    EXPECT_IO_THREAD();

    TestRequest(request);
    EXPECT_TRUE(request->IsReadOnly());
    TestResponse(response);
    EXPECT_TRUE(response->IsReadOnly());

    got_resource_response_filter_.yes();
    return nullptr;
  }

  void OnResourceLoadComplete(CefRefPtr<CefBrowser> browser,
                              CefRefPtr<CefFrame> frame,
                              CefRefPtr<CefRequest> request,
                              CefRefPtr<CefResponse> response,
                              URLRequestStatus status,
                              int64 received_content_length) override {
    EXPECT_IO_THREAD();

    if (IsChromeRuntimeEnabled() && request->GetResourceType() == RT_FAVICON) {
      // Ignore favicon requests.
      return;
    }

    TestRequest(request);
    EXPECT_TRUE(request->IsReadOnly());
    TestResponse(response);
    EXPECT_TRUE(response->IsReadOnly());
    EXPECT_EQ(UR_SUCCESS, status);
    EXPECT_EQ(response_length_, received_content_length);

    got_resource_load_complete_.yes();

    DestroyTest();
  }

 private:
  void TestRequest(CefRefPtr<CefRequest> request) {
    TestRequestEqual(request_, request, true);
    EXPECT_EQ(request_id_, request->GetIdentifier());
    EXPECT_EQ(RT_MAIN_FRAME, request->GetResourceType());
    EXPECT_EQ(TT_FORM_SUBMIT, request->GetTransitionType());
  }

  void TestResponse(CefRefPtr<CefResponse> response) {
    EXPECT_EQ(200, response->GetStatus());
    EXPECT_STREQ("OK", response->GetStatusText().ToString().c_str());
    EXPECT_STREQ("text/html", response->GetMimeType().ToString().c_str());
  }

  void DestroyTest() override {
    EXPECT_TRUE(got_before_resource_load_);
    EXPECT_TRUE(got_resource_handler_);
    EXPECT_TRUE(got_resource_response_);
    EXPECT_TRUE(got_resource_response_filter_);
    EXPECT_TRUE(got_resource_load_complete_);

    TestHandler::DestroyTest();
  }

  CefRefPtr<CefRequest> request_;
  int64 response_length_;
  uint64 request_id_;

  TrackCallback got_before_resource_load_;
  TrackCallback got_resource_handler_;
  TrackCallback got_resource_response_;
  TrackCallback got_resource_response_filter_;
  TrackCallback got_resource_load_complete_;

  IMPLEMENT_REFCOUNTING(RequestSendRecvTestHandler);
};

}  // namespace

// Verify send and recieve
TEST(RequestTest, SendRecv) {
  CefRefPtr<RequestSendRecvTestHandler> handler =
      new RequestSendRecvTestHandler();
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

namespace {

const char kTypeTestOrigin[] = "http://tests-requesttt.com/";
const cef_transition_type_t kTransitionExplicitLoad =
    static_cast<cef_transition_type_t>(TT_EXPLICIT | TT_DIRECT_LOAD_FLAG);

static struct TypeExpected {
  const char* file;
  bool navigation;  // True if this expectation represents a navigation.
  cef_transition_type_t transition_type;
  cef_resource_type_t resource_type;
  int expected_count;
} g_type_expected[] = {
    // Initial main frame load due to browser creation.
    {"main.html", true, kTransitionExplicitLoad, RT_MAIN_FRAME, 1},

    // Sub frame load.
    {"sub.html", true, TT_AUTO_SUBFRAME, RT_SUB_FRAME, 1},

    // Stylesheet load.
    {"style.css", false, TT_LINK, RT_STYLESHEET, 1},

    // Script load.
    {"script.js", false, TT_LINK, RT_SCRIPT, 1},

    // Image load.
    {"image.png", false, TT_LINK, RT_IMAGE, 1},

    // Font load.
    {"font.ttf", false, TT_LINK, RT_FONT_RESOURCE, 1},

    // XHR load.
    {"xhr.html", false, TT_LINK, RT_XHR, 1},
};

class TypeExpectations {
 public:
  explicit TypeExpectations(bool navigation) : navigation_(navigation) {
    // Build the map of relevant requests.
    for (int i = 0;
         i < static_cast<int>(sizeof(g_type_expected) / sizeof(TypeExpected));
         ++i) {
      if (navigation_ && g_type_expected[i].navigation != navigation_)
        continue;

      request_count_.insert(std::make_pair(i, 0));
    }
  }

  // Notify that a request has been received. Returns true if the request is
  // something we care about.
  bool GotRequest(CefRefPtr<CefRequest> request) {
    const std::string& url = request->GetURL();
    if (url.find(kTypeTestOrigin) != 0)
      return false;

    const std::string& file = url.substr(sizeof(kTypeTestOrigin) - 1);
    cef_transition_type_t transition_type = request->GetTransitionType();
    cef_resource_type_t resource_type = request->GetResourceType();

    const int index = GetExpectedIndex(file, transition_type, resource_type);
    EXPECT_GE(index, 0) << "File: " << file.c_str()
                        << "; Navigation: " << navigation_
                        << "; Transition Type: " << transition_type
                        << "; Resource Type: " << resource_type;

    RequestCount::iterator it = request_count_.find(index);
    EXPECT_TRUE(it != request_count_.end());

    const int actual_count = ++it->second;
    const int expected_count = g_type_expected[index].expected_count;
    EXPECT_LE(actual_count, expected_count)
        << "File: " << file.c_str() << "; Navigation: " << navigation_
        << "; Transition Type: " << transition_type
        << "; Resource Type: " << resource_type;

    return true;
  }

  // Test if all expectations have been met.
  bool IsDone(bool assert) {
    for (int i = 0;
         i < static_cast<int>(sizeof(g_type_expected) / sizeof(TypeExpected));
         ++i) {
      if (navigation_ && g_type_expected[i].navigation != navigation_)
        continue;

      RequestCount::const_iterator it = request_count_.find(i);
      EXPECT_TRUE(it != request_count_.end());
      if (it->second != g_type_expected[i].expected_count) {
        if (assert) {
          EXPECT_EQ(g_type_expected[i].expected_count, it->second)
              << "File: " << g_type_expected[i].file
              << "; Navigation: " << navigation_
              << "; Transition Type: " << g_type_expected[i].transition_type
              << "; Resource Type: " << g_type_expected[i].resource_type;
        }
        return false;
      }
    }
    return true;
  }

 private:
  // Returns the index for the specified navigation.
  int GetExpectedIndex(const std::string& file,
                       cef_transition_type_t transition_type,
                       cef_resource_type_t resource_type) {
    for (int i = 0;
         i < static_cast<int>(sizeof(g_type_expected) / sizeof(TypeExpected));
         ++i) {
      if (g_type_expected[i].file == file &&
          (!navigation_ || g_type_expected[i].navigation == navigation_) &&
          g_type_expected[i].transition_type == transition_type &&
          g_type_expected[i].resource_type == resource_type) {
        return i;
      }
    }
    return -1;
  }

  bool navigation_;

  // Map of TypeExpected index to actual request count.
  typedef std::map<int, int> RequestCount;
  RequestCount request_count_;
};

// Browser side.
class TypeTestHandler : public TestHandler {
 public:
  TypeTestHandler()
      : browse_expectations_(true),
        load_expectations_(false),
        get_expectations_(false),
        completed_browser_side_(false),
        destroyed_(false) {}

  void RunTest() override {
    AddResource(std::string(kTypeTestOrigin) + "main.html",
                "<html>"
                "<head>"
                "<link rel=\"stylesheet\" href=\"style.css\" type=\"text/css\">"
                "<script type=\"text/javascript\" src=\"script.js\"></script>"
                "</head>"
                "<body><p>Main</p>"
                "<script>xhr = new XMLHttpRequest();"
                "xhr.open('GET', 'xhr.html', false);"
                "xhr.send();</script>"
                "<iframe src=\"sub.html\"></iframe>"
                "<img src=\"image.png\">"
                "</body></html>",
                "text/html");
    AddResource(std::string(kTypeTestOrigin) + "sub.html", "<html>Sub</html>",
                "text/html");
    AddResource(std::string(kTypeTestOrigin) + "style.css",
                "@font-face {"
                "  font-family: custom_font;"
                "  src: url('font.ttf');"
                "}"
                "p {"
                "  font-family: custom_font;"
                "}",
                "text/css");
    AddResource(std::string(kTypeTestOrigin) + "script.js", "<!-- -->",
                "text/javascript");
    AddResource(std::string(kTypeTestOrigin) + "image.png", "<!-- -->",
                "image/png");
    AddResource(std::string(kTypeTestOrigin) + "font.ttf", "<!-- -->",
                "font/ttf");
    AddResource(std::string(kTypeTestOrigin) + "xhr.html", "<html>XHR</html>",
                "text/html");
    AddResource(std::string(kTypeTestOrigin) + "fetch.html",
                "<html>Fetch</html>", "text/html");

    CreateBrowser(std::string(kTypeTestOrigin) + "main.html");

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                      CefRefPtr<CefFrame> frame,
                      CefRefPtr<CefRequest> request,
                      bool user_gesture,
                      bool is_redirect) override {
    browse_expectations_.GotRequest(request);

    return false;
  }

  cef_return_value_t OnBeforeResourceLoad(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request,
      CefRefPtr<CefCallback> callback) override {
    load_expectations_.GotRequest(request);

    return RV_CONTINUE;
  }

  CefRefPtr<CefResourceHandler> GetResourceHandler(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request) override {
    if (get_expectations_.GotRequest(request) &&
        get_expectations_.IsDone(false)) {
      completed_browser_side_ = true;
      // Destroy the test on the UI thread.
      CefPostTask(TID_UI, base::BindOnce(&TypeTestHandler::DestroyTest, this));
    }

    return TestHandler::GetResourceHandler(browser, frame, request);
  }

 private:
  void DestroyTest() override {
    if (destroyed_)
      return;
    destroyed_ = true;

    // Verify test expectations.
    EXPECT_TRUE(completed_browser_side_);
    EXPECT_TRUE(browse_expectations_.IsDone(true));
    EXPECT_TRUE(load_expectations_.IsDone(true));
    EXPECT_TRUE(get_expectations_.IsDone(true));

    TestHandler::DestroyTest();
  }

  TypeExpectations browse_expectations_;
  TypeExpectations load_expectations_;
  TypeExpectations get_expectations_;

  bool completed_browser_side_;
  bool destroyed_;

  IMPLEMENT_REFCOUNTING(TypeTestHandler);
};

}  // namespace

// Verify the order of navigation-related callbacks.
TEST(RequestTest, ResourceAndTransitionType) {
  CefRefPtr<TypeTestHandler> handler = new TypeTestHandler();
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}
