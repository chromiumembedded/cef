// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef_web_urlrequest.h"
#include "tests/unittests/test_handler.h"

// #define WEB_URLREQUEST_DEBUG

class TestResults {
 public:
  TestResults()
    : errorCode(0),
      contentLength(0),
      statusCode(0),
      redirectCount(0) {
  }

  int errorCode;
  size_t contentLength;
  int statusCode;
  CefString statusText;
  CefString contentLengthHeader;
  CefString allHeaders;
  CefResponse::HeaderMap headerMap;

  int redirectCount;

  TrackCallback
      got_redirect,
      got_deleted,
      got_started,
      got_headers,
      got_loading,
      got_done,
      got_progress,
      got_abort,
      got_error;
};

class BrowserTestHandler : public TestHandler {
 public:
  // Cancel at state WUR_STATE_UNSENT means that no cancellation
  // will occur since that state change is never fired.
  BrowserTestHandler(TestResults &tr,
                     cef_weburlrequest_state_t cancelAtState = WUR_STATE_UNSENT)
    : cancelAtState_(cancelAtState),
      test_results_(tr) {
  }

  virtual void RunTest() OVERRIDE {
    std::stringstream testHtml;
    testHtml <<
        "<html><body>"
        "<h1>Testing Web Url Request...</h1>"
        "</body></html>";

    AddResource("http://tests/run.html", testHtml.str(), "text/html");
    CreateBrowser("http://tests/run.html");
  }

  virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         int httpStatusCode) {
    StartTest();
  }

  void TestCompleted() {
    DestroyTest();
  }

  virtual void StartTest() = 0;

  cef_weburlrequest_state_t cancelAtState_;

  TestResults& test_results_;
};

class TestWebURLRequestClient : public CefWebURLRequestClient {
 public:
  TestWebURLRequestClient(TestResults& tr, BrowserTestHandler* browser)
    : test_results_(tr),
      cancelAtState_(WUR_STATE_UNSENT),
      browser_(browser) {
  }

  virtual ~TestWebURLRequestClient() {
      test_results_.got_deleted.yes();
  }

  bool MaybeCancelRequest(CefRefPtr<CefWebURLRequest> requester,
                          RequestState state) {
    if (cancelAtState_ == state) {
#ifdef WEB_URLREQUEST_DEBUG
      printf("  Cancelling at state %d\n", cancelAtState_);
#endif
      requester->Cancel();
      return true;
    }
    return false;
  }

  void OnStateChange(CefRefPtr<CefWebURLRequest> requester,
                     RequestState state) {
#ifdef WEB_URLREQUEST_DEBUG
    printf("OnStateChange(0x%p, %d)\n", requester.get(), state);
#endif

    if (MaybeCancelRequest(requester, state))
      return;

    switch (state) {
    case WUR_STATE_STARTED:
      test_results_.got_started.yes();
      break;
    case WUR_STATE_HEADERS_RECEIVED:
      test_results_.got_headers.yes();
      break;
    case WUR_STATE_LOADING:
      test_results_.got_loading.yes();
      break;
    case WUR_STATE_DONE:
      test_results_.got_done.yes();
      if ( contents_.length() ) {
        size_t len = contents_.length();
        test_results_.contentLength = len;
#ifdef WEB_URLREQUEST_DEBUG
        printf("Response: %lu - %s\n", len, contents_.c_str());
#endif
      }
      TestCompleted();
      break;
    case WUR_STATE_ABORT:
      test_results_.got_abort.yes();
      TestCompleted();
      break;
    default:
      break;
    }
  }

  void OnRedirect(CefRefPtr<CefWebURLRequest> requester,
                  CefRefPtr<CefRequest> request,
                  CefRefPtr<CefResponse> response) {
#ifdef WEB_URLREQUEST_DEBUG
    printf("OnRedirect(0x%p, 0x%p, 0x%p)\n",
           requester.get(), request.get(), response.get());
#endif
    test_results_.got_redirect.yes();
    ++test_results_.redirectCount;

    CefString url = request->GetURL();
#ifdef WEB_URLREQUEST_DEBUG
    printf("URL = %s\n", url.ToString().c_str());
#endif
  }

  void OnHeadersReceived(CefRefPtr<CefWebURLRequest> requester,
                         CefRefPtr<CefResponse> response) {
#ifdef WEB_URLREQUEST_DEBUG
    printf("OnHeadersReceived(0x%p, 0x%p)\n", requester.get(), response.get());
#endif

    test_results_.statusCode = response->GetStatus();
    test_results_.statusText = response->GetStatusText();
    test_results_.contentLengthHeader = response->GetHeader("Content-Length");
    response->GetHeaderMap(test_results_.headerMap);
  }

  void OnData(CefRefPtr<CefWebURLRequest> requester, const void *data,
              int dataLength) {
#ifdef WEB_URLREQUEST_DEBUG
    printf("OnData(0x%p, 0x%p, %d)\n", requester.get(), data, dataLength);
#endif

    // Add data to buffer, create if not already
    contents_.append(static_cast<const char*>(data), dataLength);
  }

  void TestCompleted() {
    browser_->TestCompleted();
    browser_ = NULL;
    requester_ = NULL;
    Release();
  }

  void OnProgress(CefRefPtr<CefWebURLRequest> requester,
                  uint64 bytesSent,
                  uint64 totalBytesToBeSent) {
#ifdef WEB_URLREQUEST_DEBUG
    printf("OnProgress(0x%p, %d, %d)\n", requester.get(),
        (unsigned int)bytesSent, (unsigned int)totalBytesToBeSent);
#endif
    test_results_.got_progress.yes();
  }

  void OnError(CefRefPtr<CefWebURLRequest> requester,
               CefWebURLRequestClient::ErrorCode errorCode) {
#ifdef WEB_URLREQUEST_DEBUG
    printf("Error(0x%p, %d)\n", requester.get(), errorCode);
#endif
    test_results_.errorCode = static_cast<int>(errorCode);
    test_results_.got_error.yes();
    TestCompleted();
  }

  bool Run(CefRefPtr<CefRequest> req, RequestState
           cancelAtState = WUR_STATE_UNSENT) {
    if ( requester_.get() )
      return false;

    cancelAtState_ = cancelAtState;
    request_ = req;

    // Keep ourselves alive... blanced in TestCompleted() when done.
    AddRef();

    requester_ = CefWebURLRequest::CreateWebURLRequest(request_, this);
#ifdef WEB_URLREQUEST_DEBUG
    printf("Created requester at address 0x%p\n", requester_.get());
#endif

    return requester_.get() != NULL;
  }

 protected:
  TestResults& test_results_;
  RequestState cancelAtState_;

  CefRefPtr<BrowserTestHandler> browser_;
  CefRefPtr<CefWebURLRequest> requester_;
  CefRefPtr<CefRequest> request_;
  std::string contents_;

  IMPLEMENT_REFCOUNTING(TestWebURLRequestClient);
};

TEST(WebURLRequestTest, GET) {
  class BrowserForTest : public BrowserTestHandler {
   public:
    explicit BrowserForTest(TestResults &tr) : BrowserTestHandler(tr) { }

    void StartTest() {
      CefRefPtr<CefRequest> req;
      CefRefPtr<CefPostData> postdata;
      CefRequest::HeaderMap headers;

      req = CefRequest::CreateRequest();

      CefString url(
          "http://search.twitter.com/search.json?result_type=popular&q=webkit");
      CefString method("GET");

      req->Set(url, method, postdata, headers);

      CefRefPtr<TestWebURLRequestClient> handler =
          new TestWebURLRequestClient(test_results_, this);

      req->SetFlags((CefRequest::RequestFlags)(
          WUR_FLAG_SKIP_CACHE |
          WUR_FLAG_REPORT_LOAD_TIMING |
          WUR_FLAG_REPORT_RAW_HEADERS |
          WUR_FLAG_REPORT_UPLOAD_PROGRESS));

      ASSERT_TRUE(handler->Run(req));
    }
  };

  TestResults tr;
  CefRefPtr<BrowserTestHandler> browser = new BrowserForTest(tr);
  browser->ExecuteTest();

  EXPECT_TRUE(tr.got_started);
  EXPECT_TRUE(tr.got_headers);
  EXPECT_TRUE(tr.got_loading);
  EXPECT_TRUE(tr.got_done);
  EXPECT_TRUE(tr.got_deleted);
  EXPECT_FALSE(tr.got_abort);
  EXPECT_FALSE(tr.got_error);
  EXPECT_FALSE(tr.got_redirect);
  EXPECT_FALSE(tr.got_progress);
  EXPECT_GT(tr.contentLength, static_cast<size_t>(0));
  EXPECT_EQ(200, tr.statusCode);
}

TEST(WebURLRequestTest, POST) {
  class BrowserForTest : public BrowserTestHandler {
   public:
    explicit BrowserForTest(TestResults &tr) : BrowserTestHandler(tr) { }

    void StartTest() {
      CefRefPtr<CefRequest> req;
      CefRefPtr<CefPostData> postdata;
      CefRefPtr<CefPostDataElement> postitem;
      CefRequest::HeaderMap headers;

      headers.insert(std::make_pair("Content-Type",
                                    "application/x-www-form-urlencoded"));

      req = CefRequest::CreateRequest();

      CefString url("http://pastebin.com/api_public.php");
      CefString method("POST");

      postdata = CefPostData::CreatePostData();
      postitem = CefPostDataElement::CreatePostDataElement();

      static char posttext[] =
          "paste_name=CEF%20Test%20Post&paste_code=testing a post call."
          "&paste_expire_date=10M";

      postitem->SetToBytes(strlen(posttext), posttext);
      postdata->AddElement(postitem);

      req->Set(url, method, postdata, headers);

      CefRefPtr<TestWebURLRequestClient> handler =
          new TestWebURLRequestClient(test_results_, this);

      req->SetFlags((CefRequest::RequestFlags)(
          WUR_FLAG_SKIP_CACHE |
          WUR_FLAG_REPORT_LOAD_TIMING |
          WUR_FLAG_REPORT_RAW_HEADERS |
          WUR_FLAG_REPORT_UPLOAD_PROGRESS));

      ASSERT_TRUE(handler->Run(req));
    }
  };

  TestResults tr;
  CefRefPtr<BrowserTestHandler> browser = new BrowserForTest(tr);
  browser->ExecuteTest();

  EXPECT_TRUE(tr.got_started);
  EXPECT_TRUE(tr.got_headers);
  EXPECT_TRUE(tr.got_loading);
  EXPECT_TRUE(tr.got_done);
  EXPECT_TRUE(tr.got_deleted);
  EXPECT_FALSE(tr.got_redirect);
  EXPECT_TRUE(tr.got_progress);
  EXPECT_FALSE(tr.got_error);
  EXPECT_FALSE(tr.got_abort);
  EXPECT_GT(tr.contentLength, static_cast<size_t>(0));
  EXPECT_EQ(200, tr.statusCode);
}

TEST(WebURLRequestTest, BADHOST) {
  class BrowserForTest : public BrowserTestHandler {
   public:
    explicit BrowserForTest(TestResults &tr) : BrowserTestHandler(tr) { }

    void StartTest() {
      CefRefPtr<CefRequest> req;
      CefRefPtr<CefPostData> postdata;
      CefRefPtr<CefPostDataElement> postitem;
      CefRequest::HeaderMap headers;

      req = CefRequest::CreateRequest();

      CefString url("http://this.host.does.not.exist/not/really/here");
      CefString method("GET");

      req->Set(url, method, postdata, headers);

      CefRefPtr<TestWebURLRequestClient> handler =
          new TestWebURLRequestClient(test_results_, this);

      req->SetFlags((CefRequest::RequestFlags)(
          WUR_FLAG_SKIP_CACHE |
          WUR_FLAG_REPORT_LOAD_TIMING |
          WUR_FLAG_REPORT_RAW_HEADERS |
          WUR_FLAG_REPORT_UPLOAD_PROGRESS));

      ASSERT_TRUE(handler->Run(req));
    }
  };

  TestResults tr;
  CefRefPtr<BrowserTestHandler> browser = new BrowserForTest(tr);
  browser->ExecuteTest();

  // NOTE: THIS TEST WILL FAIL IF YOUR ISP REDIRECTS YOU TO
  // THEIR SEARCH PAGE ON NXDOMAIN ERRORS.
  EXPECT_TRUE(tr.got_started);
  EXPECT_FALSE(tr.got_headers);
  EXPECT_FALSE(tr.got_loading);
  EXPECT_FALSE(tr.got_done);
  EXPECT_TRUE(tr.got_deleted);
  EXPECT_FALSE(tr.got_redirect);
  EXPECT_FALSE(tr.got_progress);
  EXPECT_FALSE(tr.got_abort);
  EXPECT_TRUE(tr.got_error);
  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, tr.errorCode);
  EXPECT_EQ(static_cast<size_t>(0), tr.contentLength);
  EXPECT_EQ(0, tr.statusCode);
}

#define COUNTOF_(ar) (sizeof(ar)/sizeof(ar[0]))

TEST(WebURLRequestTest, CANCEL) {
  class BrowserForTest : public BrowserTestHandler {
   public:
    BrowserForTest(TestResults &tr, cef_weburlrequest_state_t cancelAtState)
      : BrowserTestHandler(tr, cancelAtState) {
    }

    void StartTest() {
      CefRefPtr<CefRequest> req;
      CefRefPtr<CefPostData> postdata;
      CefRefPtr<CefPostDataElement> postitem;
      CefRequest::HeaderMap headers;

      req = CefRequest::CreateRequest();

      CefString url(
          "http://search.twitter.com/search.json?result_type=popular&q=webkit");
      CefString method("GET");

      req->Set(url, method, postdata, headers);

      CefRefPtr<TestWebURLRequestClient> handler =
          new TestWebURLRequestClient(test_results_, this);

      req->SetFlags((CefRequest::RequestFlags)(
          WUR_FLAG_SKIP_CACHE |
          WUR_FLAG_REPORT_LOAD_TIMING |
          WUR_FLAG_REPORT_RAW_HEADERS |
          WUR_FLAG_REPORT_UPLOAD_PROGRESS));

      ASSERT_TRUE(handler->Run(req, cancelAtState_));
    }
  };

  cef_weburlrequest_state_t cancelAt[] = {
    WUR_STATE_STARTED,
    WUR_STATE_HEADERS_RECEIVED
  };

  for (unsigned int i = 0; i < COUNTOF_(cancelAt); ++i) {
    TestResults tr;
    CefRefPtr<BrowserTestHandler> browser = new BrowserForTest(tr, cancelAt[i]);
    browser->ExecuteTest();
    EXPECT_TRUE(tr.got_abort) << "i = " << i;
    EXPECT_TRUE(tr.got_deleted);
  }
}

// Enable this test if you have installed the php test page.
#if 0

TEST(WebURLRequestTest, REDIRECT) {
  /* // PHP Script for a local server to test this:
     // You can run a zwamp server on windows to run this.
     // http://sourceforge.net/projects/zwamp/
  
<?php
$max  = isset($_GET['max'])  ? $_GET['max']  : 2;
$step = isset($_GET['step']) ? $_GET['step'] : 1;

if ($step < $max)
{
    $url = $_SERVER['PHP_SELF'];
    ++$step;
    header( $_SERVER["SERVER_PROTOCOL"] . " 301 Permanently moved");
    header("Location: $url?max=$max&step=$step", true, 301);
}
else
{
    header("Content: text/plain");
    echo "Redirect completed after $max times.";
}
?>
  */


  class BrowserForTest : public BrowserTestHandler {
  public:
    explicit BrowserForTest(TestResults &tr) : BrowserTestHandler(tr) { }

    void StartTest() {
      CefRefPtr<CefRequest> req;
      CefRefPtr<CefPostData> postdata;
      CefRefPtr<CefPostDataElement> postitem;
      CefRequest::HeaderMap headers;

      req = CefRequest::CreateRequest();

      CefString url("http://localhost/cef/redirect.php?max=4");
      CefString method("GET");

      req->Set(url, method, postdata, headers);

      CefRefPtr<TestWebURLRequestClient> handler =
          new TestWebURLRequestClient(test_results_, this);

      req->SetFlags((CefRequest::RequestFlags)(
          WUR_FLAG_SKIP_CACHE |
          WUR_FLAG_REPORT_LOAD_TIMING |
          WUR_FLAG_REPORT_RAW_HEADERS |
          WUR_FLAG_REPORT_UPLOAD_PROGRESS));

      ASSERT_TRUE(handler->Run(req, cancelAtState_));
    }
  };

  TestResults tr;
  CefRefPtr<BrowserForTest> browser = new BrowserForTest(tr);
  browser->ExecuteTest();
  EXPECT_TRUE(tr.got_started);
  EXPECT_TRUE(tr.got_headers);
  EXPECT_TRUE(tr.got_loading);
  EXPECT_TRUE(tr.got_done);
  EXPECT_TRUE(tr.got_deleted);
  EXPECT_TRUE(tr.got_redirect);
  EXPECT_FALSE(tr.got_progress);
  EXPECT_FALSE(tr.got_error);
  EXPECT_FALSE(tr.got_abort);
  EXPECT_GT(tr.contentLength, static_cast<size_t>(0));
  EXPECT_EQ(200, tr.statusCode);
  EXPECT_EQ(3, tr.redirectCount);
}

#endif  // 0
