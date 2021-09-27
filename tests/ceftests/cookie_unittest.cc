// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "include/base/cef_callback.h"
#include "include/base/cef_logging.h"
#include "include/base/cef_ref_counted.h"
#include "include/cef_cookie.h"
#include "include/cef_request_context_handler.h"
#include "include/cef_scheme.h"
#include "include/cef_server.h"
#include "include/cef_waitable_event.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/routing_test_handler.h"
#include "tests/ceftests/test_handler.h"
#include "tests/ceftests/test_server.h"
#include "tests/ceftests/test_suite.h"
#include "tests/ceftests/test_util.h"
#include "tests/gtest/include/gtest/gtest.h"

namespace {

const char* kTestUrl = "http://www.test.com/path/to/cookietest/foo.html";
const char* kTestDomain = "www.test.com";
const char* kTestPath = "/path/to/cookietest";

const int kIgnoreNumDeleted = -2;

typedef std::vector<CefCookie> CookieVector;

class TestCompletionCallback : public CefCompletionCallback {
 public:
  explicit TestCompletionCallback(CefRefPtr<CefWaitableEvent> event)
      : event_(event) {}

  void OnComplete() override {
    EXPECT_TRUE(CefCurrentlyOn(TID_UI));
    event_->Signal();
  }

 private:
  CefRefPtr<CefWaitableEvent> event_;

  IMPLEMENT_REFCOUNTING(TestCompletionCallback);
  DISALLOW_COPY_AND_ASSIGN(TestCompletionCallback);
};

class TestSetCookieCallback : public CefSetCookieCallback {
 public:
  TestSetCookieCallback(bool expected_success,
                        CefRefPtr<CefWaitableEvent> event)
      : expected_success_(expected_success), event_(event) {}

  void OnComplete(bool success) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_UI));
    EXPECT_EQ(expected_success_, success);
    event_->Signal();
  }

 private:
  bool expected_success_;
  CefRefPtr<CefWaitableEvent> event_;

  IMPLEMENT_REFCOUNTING(TestSetCookieCallback);
  DISALLOW_COPY_AND_ASSIGN(TestSetCookieCallback);
};

class TestDeleteCookiesCallback : public CefDeleteCookiesCallback {
 public:
  TestDeleteCookiesCallback(int expected_num_deleted,
                            CefRefPtr<CefWaitableEvent> event)
      : expected_num_deleted_(expected_num_deleted), event_(event) {}

  void OnComplete(int num_deleted) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_UI));
    if (expected_num_deleted_ != kIgnoreNumDeleted) {
      EXPECT_EQ(expected_num_deleted_, num_deleted);
    }
    event_->Signal();
  }

 private:
  int expected_num_deleted_;
  CefRefPtr<CefWaitableEvent> event_;

  IMPLEMENT_REFCOUNTING(TestDeleteCookiesCallback);
  DISALLOW_COPY_AND_ASSIGN(TestDeleteCookiesCallback);
};

class TestVisitor : public CefCookieVisitor {
 public:
  TestVisitor(CookieVector* cookies,
              bool deleteCookies,
              base::OnceClosure callback)
      : cookies_(cookies),
        delete_cookies_(deleteCookies),
        callback_(std::move(callback)) {
    EXPECT_TRUE(cookies_);
    EXPECT_FALSE(callback_.is_null());
  }
  ~TestVisitor() override { std::move(callback_).Run(); }

  bool Visit(const CefCookie& cookie,
             int count,
             int total,
             bool& deleteCookie) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_UI));
    cookies_->push_back(cookie);
    if (delete_cookies_)
      deleteCookie = true;
    return true;
  }

 private:
  CookieVector* cookies_;
  bool delete_cookies_;
  base::OnceClosure callback_;

  IMPLEMENT_REFCOUNTING(TestVisitor);
};

// Set the cookies.
void SetCookies(CefRefPtr<CefCookieManager> manager,
                const CefString& url,
                const CookieVector& cookies,
                bool expected_success,
                CefRefPtr<CefWaitableEvent> event) {
  CookieVector::const_iterator it = cookies.begin();
  for (; it != cookies.end(); ++it) {
    EXPECT_TRUE(manager->SetCookie(
        url, *it, new TestSetCookieCallback(expected_success, event)));
    event->Wait();
  }
}

// Delete the cookie.
void DeleteCookies(CefRefPtr<CefCookieManager> manager,
                   const CefString& url,
                   const CefString& cookie_name,
                   int expected_num_deleted,
                   CefRefPtr<CefWaitableEvent> event) {
  EXPECT_TRUE(manager->DeleteCookies(
      url, cookie_name,
      new TestDeleteCookiesCallback(expected_num_deleted, event)));
  event->Wait();
}

// Create a test cookie. If |withDomain| is true a domain cookie will be
// created, otherwise a host cookie will be created.
void CreateCookie(CefRefPtr<CefCookieManager> manager,
                  CefCookie& cookie,
                  bool withDomain,
                  bool sessionCookie,
                  CefRefPtr<CefWaitableEvent> event) {
  CefString(&cookie.name).FromASCII("my_cookie");
  CefString(&cookie.value).FromASCII("My Value");
  if (withDomain)
    CefString(&cookie.domain).FromASCII(kTestDomain);
  CefString(&cookie.path).FromASCII(kTestPath);
  if (!sessionCookie) {
    cookie.has_expires = true;
    cookie.expires.year = 2200;
    cookie.expires.month = 4;
    cookie.expires.day_of_week = 5;
    cookie.expires.day_of_month = 11;
  }

  CookieVector cookies;
  cookies.push_back(cookie);

  SetCookies(manager, kTestUrl, cookies, true, event);
}

// Visit URL cookies. Execute |callback| on completion.
void VisitUrlCookies(CefRefPtr<CefCookieManager> manager,
                     const CefString& url,
                     bool includeHttpOnly,
                     CookieVector& cookies,
                     bool deleteCookies,
                     base::OnceClosure callback) {
  EXPECT_TRUE(manager->VisitUrlCookies(
      url, includeHttpOnly,
      new TestVisitor(&cookies, deleteCookies, std::move(callback))));
}

// Visit URL cookies. Block on |event|.
void VisitUrlCookies(CefRefPtr<CefCookieManager> manager,
                     const CefString& url,
                     bool includeHttpOnly,
                     CookieVector& cookies,
                     bool deleteCookies,
                     CefRefPtr<CefWaitableEvent> event) {
  VisitUrlCookies(manager, url, includeHttpOnly, cookies, deleteCookies,
                  base::BindOnce(&CefWaitableEvent::Signal, event));
  event->Wait();
}

// Visit all cookies. Execute |callback| on completion.
void VisitAllCookies(CefRefPtr<CefCookieManager> manager,
                     CookieVector& cookies,
                     bool deleteCookies,
                     base::OnceClosure callback) {
  EXPECT_TRUE(manager->VisitAllCookies(
      new TestVisitor(&cookies, deleteCookies, std::move(callback))));
}

// Visit all cookies. Block on |event|.
void VisitAllCookies(CefRefPtr<CefCookieManager> manager,
                     CookieVector& cookies,
                     bool deleteCookies,
                     CefRefPtr<CefWaitableEvent> event) {
  VisitAllCookies(manager, cookies, deleteCookies,
                  base::BindOnce(&CefWaitableEvent::Signal, event));
  event->Wait();
}

// Retrieve the test cookie. If |withDomain| is true check that the cookie
// is a domain cookie, otherwise a host cookie. if |deleteCookies| is true
// the cookie will be deleted when it's retrieved.
void GetCookie(CefRefPtr<CefCookieManager> manager,
               const CefCookie& cookie,
               bool withDomain,
               CefRefPtr<CefWaitableEvent> event,
               bool deleteCookies) {
  CookieVector cookies;

  // Get the cookie and delete it.
  VisitUrlCookies(manager, kTestUrl, false, cookies, deleteCookies, event);

  EXPECT_EQ(1U, cookies.size());
  if (cookies.size() != 1U)
    return;

  const CefCookie& cookie_read = cookies[0];
  EXPECT_EQ(CefString(&cookie_read.name), "my_cookie");
  EXPECT_EQ(CefString(&cookie_read.value), "My Value");
  if (withDomain)
    EXPECT_EQ(CefString(&cookie_read.domain), ".www.test.com");
  else
    EXPECT_EQ(CefString(&cookie_read.domain), kTestDomain);
  EXPECT_EQ(CefString(&cookie_read.path), kTestPath);
  EXPECT_EQ(cookie.has_expires, cookie_read.has_expires);
  EXPECT_EQ(cookie.expires.year, cookie_read.expires.year);
  EXPECT_EQ(cookie.expires.month, cookie_read.expires.month);
  EXPECT_EQ(cookie.expires.day_of_week, cookie_read.expires.day_of_week);
  EXPECT_EQ(cookie.expires.day_of_month, cookie_read.expires.day_of_month);
  EXPECT_EQ(cookie.expires.hour, cookie_read.expires.hour);
  EXPECT_EQ(cookie.expires.minute, cookie_read.expires.minute);
  EXPECT_EQ(cookie.expires.second, cookie_read.expires.second);
  EXPECT_EQ(cookie.expires.millisecond, cookie_read.expires.millisecond);
  EXPECT_EQ(cookie.same_site, cookie_read.same_site);
  EXPECT_EQ(cookie.priority, cookie_read.priority);
}

// Verify that no cookies exist. If |withUrl| is true it will only check for
// cookies matching the URL.
void VerifyNoCookies(CefRefPtr<CefCookieManager> manager,
                     CefRefPtr<CefWaitableEvent> event,
                     bool withUrl) {
  CookieVector cookies;

  // Verify that the cookie has been deleted.
  if (withUrl) {
    VisitUrlCookies(manager, kTestUrl, false, cookies, false, event);
  } else {
    VisitAllCookies(manager, cookies, false, event);
  }

  EXPECT_EQ(0U, cookies.size());
}

// Delete all system cookies.
void DeleteAllCookies(CefRefPtr<CefCookieManager> manager,
                      CefRefPtr<CefWaitableEvent> event) {
  DeleteCookies(manager, CefString(), CefString(), kIgnoreNumDeleted, event);
}

void TestDomainCookie(CefRefPtr<CefCookieManager> manager,
                      CefRefPtr<CefWaitableEvent> event) {
  CefCookie cookie;

  // Create a domain cookie.
  CreateCookie(manager, cookie, true, false, event);

  // Retrieve, verify and delete the domain cookie.
  GetCookie(manager, cookie, true, event, true);

  // Verify that the cookie was deleted.
  VerifyNoCookies(manager, event, true);
}

void TestHostCookie(CefRefPtr<CefCookieManager> manager,
                    CefRefPtr<CefWaitableEvent> event) {
  CefCookie cookie;

  // Create a host cookie.
  CreateCookie(manager, cookie, false, false, event);

  // Retrieve, verify and delete the host cookie.
  GetCookie(manager, cookie, false, event, true);

  // Verify that the cookie was deleted.
  VerifyNoCookies(manager, event, true);
}

void TestInvalidCookie(CefRefPtr<CefCookieManager> manager,
                       CefRefPtr<CefWaitableEvent> event) {
  CookieVector cookies;

  CefCookie cookie;
  const char* kUrl = "http://www.xyz.com";
  CefString(&cookie.name).FromASCII("invalid1");
  CefString(&cookie.value).FromASCII("invalid1");
  CefString(&cookie.domain).FromASCII(".zyx.com");  // domain mismatch

  cookies.push_back(cookie);

  // No cookies will be set due to non canonical cookie
  SetCookies(manager, kUrl, cookies, false, event);
}

void TestMultipleCookies(CefRefPtr<CefCookieManager> manager,
                         CefRefPtr<CefWaitableEvent> event) {
  std::stringstream ss;
  int i;

  CookieVector cookies;

  const int kNumCookies = 4;

  // Create the cookies.
  for (i = 0; i < kNumCookies; i++) {
    CefCookie cookie;

    ss << "my_cookie" << i;
    CefString(&cookie.name).FromASCII(ss.str().c_str());
    ss.str("");
    ss << "My Value " << i;
    CefString(&cookie.value).FromASCII(ss.str().c_str());
    ss.str("");

    cookies.push_back(cookie);
  }

  // Set the cookies.
  SetCookies(manager, kTestUrl, cookies, true, event);
  cookies.clear();

  // Get the cookies without deleting them.
  VisitUrlCookies(manager, kTestUrl, false, cookies, false, event);

  EXPECT_EQ((CookieVector::size_type)kNumCookies, cookies.size());

  CookieVector::const_iterator it = cookies.begin();
  for (i = 0; it != cookies.end(); ++it, ++i) {
    const CefCookie& cookie = *it;

    ss << "my_cookie" << i;
    EXPECT_EQ(CefString(&cookie.name), ss.str());
    ss.str("");
    ss << "My Value " << i;
    EXPECT_EQ(CefString(&cookie.value), ss.str());
    ss.str("");
  }

  cookies.clear();

  // Delete the 2nd cookie.
  DeleteCookies(manager, kTestUrl, CefString("my_cookie1"), 1, event);

  // Verify that the cookie has been deleted.
  VisitUrlCookies(manager, kTestUrl, false, cookies, false, event);

  EXPECT_EQ(3U, cookies.size());
  if (cookies.size() != 3U)
    return;

  EXPECT_EQ(CefString(&cookies[0].name), "my_cookie0");
  EXPECT_EQ(CefString(&cookies[1].name), "my_cookie2");
  EXPECT_EQ(CefString(&cookies[2].name), "my_cookie3");

  cookies.clear();

  // Delete the rest of the cookies.
  DeleteCookies(manager, kTestUrl, CefString(), 3, event);

  // Verify that the cookies have been deleted.
  VisitUrlCookies(manager, kTestUrl, false, cookies, false, event);

  EXPECT_EQ(0U, cookies.size());

  // Create the cookies.
  for (i = 0; i < kNumCookies; i++) {
    CefCookie cookie;

    ss << "my_cookie" << i;
    CefString(&cookie.name).FromASCII(ss.str().c_str());
    ss.str("");
    ss << "My Value " << i;
    CefString(&cookie.value).FromASCII(ss.str().c_str());
    ss.str("");

    cookies.push_back(cookie);
  }

  // Delete all of the cookies using the visitor.
  VisitUrlCookies(manager, kTestUrl, false, cookies, true, event);

  cookies.clear();

  // Verify that the cookies have been deleted.
  VisitUrlCookies(manager, kTestUrl, false, cookies, false, event);

  EXPECT_EQ(0U, cookies.size());
}

void TestAllCookies(CefRefPtr<CefCookieManager> manager,
                    CefRefPtr<CefWaitableEvent> event) {
  CookieVector cookies;

  // Delete all system cookies just in case something is left over from a
  // different test.
  DeleteAllCookies(manager, event);

  // Verify that all system cookies have been deleted.
  VisitAllCookies(manager, cookies, false, event);

  EXPECT_EQ(0U, cookies.size());

  // Create cookies with 2 separate hosts.
  CefCookie cookie1;
  const char* kUrl1 = "http://www.foo.com";
  CefString(&cookie1.name).FromASCII("my_cookie1");
  CefString(&cookie1.value).FromASCII("My Value 1");

  cookies.push_back(cookie1);
  SetCookies(manager, kUrl1, cookies, true, event);
  cookies.clear();

  CefCookie cookie2;
  const char* kUrl2 = "http://www.bar.com";
  CefString(&cookie2.name).FromASCII("my_cookie2");
  CefString(&cookie2.value).FromASCII("My Value 2");

  cookies.push_back(cookie2);
  SetCookies(manager, kUrl2, cookies, true, event);
  cookies.clear();

  // Verify that all system cookies can be retrieved.
  VisitAllCookies(manager, cookies, false, event);

  EXPECT_EQ(2U, cookies.size());
  if (cookies.size() != 2U)
    return;

  EXPECT_EQ(CefString(&cookies[0].name), "my_cookie1");
  EXPECT_EQ(CefString(&cookies[0].value), "My Value 1");
  EXPECT_EQ(CefString(&cookies[0].domain), "www.foo.com");
  EXPECT_EQ(CefString(&cookies[1].name), "my_cookie2");
  EXPECT_EQ(CefString(&cookies[1].value), "My Value 2");
  EXPECT_EQ(CefString(&cookies[1].domain), "www.bar.com");
  cookies.clear();

  // Verify that the cookies can be retrieved separately.
  VisitUrlCookies(manager, kUrl1, false, cookies, false, event);

  EXPECT_EQ(1U, cookies.size());
  if (cookies.size() != 1U)
    return;

  EXPECT_EQ(CefString(&cookies[0].name), "my_cookie1");
  EXPECT_EQ(CefString(&cookies[0].value), "My Value 1");
  EXPECT_EQ(CefString(&cookies[0].domain), "www.foo.com");
  cookies.clear();

  VisitUrlCookies(manager, kUrl2, false, cookies, false, event);

  EXPECT_EQ(1U, cookies.size());
  if (cookies.size() != 1U)
    return;

  EXPECT_EQ(CefString(&cookies[0].name), "my_cookie2");
  EXPECT_EQ(CefString(&cookies[0].value), "My Value 2");
  EXPECT_EQ(CefString(&cookies[0].domain), "www.bar.com");
  cookies.clear();

  // Delete all of the system cookies.
  DeleteAllCookies(manager, event);

  // Verify that all system cookies have been deleted.
  VerifyNoCookies(manager, event, false);
}

}  // namespace

// Test creation of a invalid cookie.
TEST(CookieTest, BasicInvalidCookie) {
  CefRefPtr<CefWaitableEvent> event =
      CefWaitableEvent::CreateWaitableEvent(true, false);

  CefRefPtr<CefCookieManager> manager =
      CefCookieManager::GetGlobalManager(new TestCompletionCallback(event));
  event->Wait();
  EXPECT_TRUE(manager.get());

  TestInvalidCookie(manager, event);
}

// Test creation of a domain cookie.
TEST(CookieTest, BasicDomainCookie) {
  CefRefPtr<CefWaitableEvent> event =
      CefWaitableEvent::CreateWaitableEvent(true, false);

  CefRefPtr<CefCookieManager> manager =
      CefCookieManager::GetGlobalManager(new TestCompletionCallback(event));
  event->Wait();
  EXPECT_TRUE(manager.get());

  TestDomainCookie(manager, event);
}

// Test creation of a host cookie.
TEST(CookieTest, BasicHostCookie) {
  CefRefPtr<CefWaitableEvent> event =
      CefWaitableEvent::CreateWaitableEvent(true, false);

  CefRefPtr<CefCookieManager> manager =
      CefCookieManager::GetGlobalManager(new TestCompletionCallback(event));
  event->Wait();
  EXPECT_TRUE(manager.get());

  TestHostCookie(manager, event);
}

// Test creation of multiple cookies.
TEST(CookieTest, BasicMultipleCookies) {
  CefRefPtr<CefWaitableEvent> event =
      CefWaitableEvent::CreateWaitableEvent(true, false);

  CefRefPtr<CefCookieManager> manager =
      CefCookieManager::GetGlobalManager(new TestCompletionCallback(event));
  event->Wait();
  EXPECT_TRUE(manager.get());

  TestMultipleCookies(manager, event);
}

TEST(CookieTest, BasicAllCookies) {
  CefRefPtr<CefWaitableEvent> event =
      CefWaitableEvent::CreateWaitableEvent(true, false);

  CefRefPtr<CefCookieManager> manager =
      CefCookieManager::GetGlobalManager(new TestCompletionCallback(event));
  event->Wait();
  EXPECT_TRUE(manager.get());

  TestAllCookies(manager, event);
}

namespace {

const char* kCookieJSUrl1 = "http://tests/cookie1.html";
const char* kCookieJSUrl2 = "http://tests/cookie2.html";

class CookieTestJSHandler : public TestHandler {
 public:
  CookieTestJSHandler() {}

  void RunTest() override {
    std::string page =
        "<html><head>"
        "<script>"
        "document.cookie='name1=value1;"
        // Invalid date should not cause a crash (see issue #2927).
        " expires=Tue, 07 Nov 94276 07:58:05 GMT'"
        "</script>"
        "</head><body>COOKIE TEST1</body></html>";
    AddResource(kCookieJSUrl1, page, "text/html");

    page =
        "<html><head>"
        "<script>"
        "document.cookie='name2=value2';"
        "</script>"
        "</head><body>COOKIE TEST2</body></html>";
    AddResource(kCookieJSUrl2, page, "text/html");

    // Create the request context that will use an in-memory cache.
    CefRequestContextSettings settings;
    CefRefPtr<CefRequestContext> request_context =
        CefRequestContext::CreateContext(settings, nullptr);
    manager_ = request_context->GetCookieManager(nullptr);

    // Create the browser.
    CreateBrowser(kCookieJSUrl1, request_context);

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  // Go to the next URL.
  void LoadNextURL(CefRefPtr<CefFrame> frame) {
    if (!CefCurrentlyOn(TID_UI)) {
      CefPostTask(TID_UI, base::BindOnce(&CookieTestJSHandler::LoadNextURL,
                                         this, frame));
      return;
    }

    frame->LoadURL(kCookieJSUrl2);
  }

  void CompleteTest() {
    if (!CefCurrentlyOn(TID_UI)) {
      CefPostTask(TID_UI,
                  base::BindOnce(&CookieTestJSHandler::CompleteTest, this));
      return;
    }

    DestroyTest();
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    std::string url = frame->GetURL();
    if (url == kCookieJSUrl1) {
      got_load_end1_.yes();
      VerifyCookie(
          manager_, url, "name1", "value1", true, &got_cookie1_,
          base::BindOnce(&CookieTestJSHandler::LoadNextURL, this, frame));
    } else {
      got_load_end2_.yes();
      VerifyCookie(manager_, url, "name2", "value2", true, &got_cookie2_,
                   base::BindOnce(&CookieTestJSHandler::CompleteTest, this));
    }
  }

  // Verify that the cookie was set successfully.
  void VerifyCookie(CefRefPtr<CefCookieManager> manager,
                    const std::string& url,
                    const std::string& name,
                    const std::string& value,
                    bool deleteCookie,
                    TrackCallback* callback,
                    base::OnceClosure continue_callback) {
    // Get the cookie.
    EXPECT_TRUE(cookies_.empty());
    VisitUrlCookies(
        manager, url, false, cookies_, deleteCookie,
        base::BindOnce(&CookieTestJSHandler::VerifyCookieComplete, this, name,
                       value, callback, std::move(continue_callback)));
  }

  void VerifyCookieComplete(const std::string& name,
                            const std::string& value,
                            TrackCallback* callback,
                            base::OnceClosure continue_callback) {
    if (cookies_.size() == 1U && CefString(&cookies_[0].name) == name &&
        CefString(&cookies_[0].value) == value) {
      callback->yes();
    }

    cookies_.clear();
    std::move(continue_callback).Run();
  }

  CefRefPtr<CefCookieManager> manager_;

  CookieVector cookies_;

  TrackCallback got_load_end1_;
  TrackCallback got_load_end2_;
  TrackCallback got_cookie1_;
  TrackCallback got_cookie2_;

  IMPLEMENT_REFCOUNTING(CookieTestJSHandler);
};

}  // namespace

// Verify use of multiple cookie managers vis JS.
TEST(CookieTest, GetCookieManagerJS) {
  CefRefPtr<CookieTestJSHandler> handler = new CookieTestJSHandler();
  handler->ExecuteTest();

  EXPECT_TRUE(handler->got_load_end1_);
  EXPECT_TRUE(handler->got_load_end2_);
  EXPECT_TRUE(handler->got_cookie1_);
  EXPECT_TRUE(handler->got_cookie2_);

  ReleaseAndWaitForDestructor(handler);
}

namespace {

const char kCustomCookieScheme[] = "ccustom";

class CompletionCallback : public CefCompletionCallback {
 public:
  explicit CompletionCallback(base::OnceClosure callback)
      : callback_(std::move(callback)) {}

  void OnComplete() override { std::move(callback_).Run(); }

 private:
  base::OnceClosure callback_;
  IMPLEMENT_REFCOUNTING(CompletionCallback);
};

class CookieTestSchemeHandler : public TestHandler {
 public:
  class SchemeHandler : public CefResourceHandler {
   public:
    explicit SchemeHandler(CookieTestSchemeHandler* handler)
        : handler_(handler), offset_(0) {}

    bool Open(CefRefPtr<CefRequest> request,
              bool& handle_request,
              CefRefPtr<CefCallback> callback) override {
      EXPECT_FALSE(CefCurrentlyOn(TID_UI) || CefCurrentlyOn(TID_IO));

      std::string url = request->GetURL();
      if (url == handler_->url1_) {
        content_ = "<html><body>COOKIE TEST1</body></html>";
        cookie_ = "name1=value1";
        handler_->got_process_request1_.yes();
      } else if (url == handler_->url2_) {
        content_ = "<html><body>COOKIE TEST2</body></html>";
        cookie_ = "name2=value2";
        handler_->got_process_request2_.yes();
      } else if (url == handler_->url3_) {
        content_ = "<html><body>COOKIE TEST3</body></html>";
        handler_->got_process_request3_.yes();

        // Verify that the cookie was passed in.
        CefRequest::HeaderMap headerMap;
        request->GetHeaderMap(headerMap);
        CefRequest::HeaderMap::iterator it = headerMap.find("Cookie");
        if (it != headerMap.end() && it->second == "name2=value2")
          handler_->got_process_request_cookie_.yes();
      }

      // Continue immediately.
      handle_request = true;
      return true;
    }

    void GetResponseHeaders(CefRefPtr<CefResponse> response,
                            int64& response_length,
                            CefString& redirectUrl) override {
      response_length = content_.size();

      response->SetStatus(200);
      response->SetMimeType("text/html");

      if (!cookie_.empty()) {
        CefResponse::HeaderMap headerMap;
        response->GetHeaderMap(headerMap);
        headerMap.insert(std::make_pair("Set-Cookie", cookie_));
        response->SetHeaderMap(headerMap);
      }
    }

    bool Read(void* data_out,
              int bytes_to_read,
              int& bytes_read,
              CefRefPtr<CefResourceReadCallback> callback) override {
      EXPECT_FALSE(CefCurrentlyOn(TID_UI) || CefCurrentlyOn(TID_IO));

      bool has_data = false;
      bytes_read = 0;

      size_t size = content_.size();
      if (offset_ < size) {
        int transfer_size =
            std::min(bytes_to_read, static_cast<int>(size - offset_));
        memcpy(data_out, content_.c_str() + offset_, transfer_size);
        offset_ += transfer_size;

        bytes_read = transfer_size;
        has_data = true;
      }

      return has_data;
    }

    void Cancel() override {}

   private:
    CookieTestSchemeHandler* handler_;
    std::string content_;
    size_t offset_;
    std::string cookie_;

    IMPLEMENT_REFCOUNTING(SchemeHandler);
    DISALLOW_COPY_AND_ASSIGN(SchemeHandler);
  };

  class SchemeHandlerFactory : public CefSchemeHandlerFactory {
   public:
    explicit SchemeHandlerFactory(CookieTestSchemeHandler* handler)
        : handler_(handler) {}

    CefRefPtr<CefResourceHandler> Create(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        const CefString& scheme_name,
        CefRefPtr<CefRequest> request) override {
      std::string url = request->GetURL();
      if (url == handler_->url3_) {
        // Verify that the cookie was not passed in.
        CefRequest::HeaderMap headerMap;
        request->GetHeaderMap(headerMap);
        CefRequest::HeaderMap::iterator it = headerMap.find("Cookie");
        if (it != headerMap.end() && it->second == "name2=value2")
          handler_->got_create_cookie_.yes();
      }

      return new SchemeHandler(handler_);
    }

   private:
    CookieTestSchemeHandler* handler_;

    IMPLEMENT_REFCOUNTING(SchemeHandlerFactory);
    DISALLOW_COPY_AND_ASSIGN(SchemeHandlerFactory);
  };

  CookieTestSchemeHandler(const std::string& scheme,
                          bool use_global,
                          bool block_cookies = false)
      : scheme_(scheme),
        use_global_(use_global),
        block_cookies_(block_cookies) {
    url1_ = scheme + "://cookie-tests/cookie1.html";
    url2_ = scheme + "://cookie-tests/cookie2.html";
    url3_ = scheme + "://cookie-tests/cookie3.html";
  }

  void RunTest() override {
    if (use_global_) {
      request_context_ = CefRequestContext::GetGlobalContext();
    } else {
      // Create the request context that will use an in-memory cache.
      CefRequestContextSettings settings;

      if (scheme_ == kCustomCookieScheme || block_cookies_) {
        if (!block_cookies_) {
          CefString(&settings.cookieable_schemes_list) = kCustomCookieScheme;
        } else {
          settings.cookieable_schemes_exclude_defaults = true;
        }
      }

      request_context_ = CefRequestContext::CreateContext(settings, nullptr);
    }

    // Register the scheme handler.
    request_context_->RegisterSchemeHandlerFactory(
        scheme_, "cookie-tests", new SchemeHandlerFactory(this));

    manager_ = request_context_->GetCookieManager(nullptr);

    // Create the browser.
    CreateBrowser(url1_, request_context_);

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  // Go to the next URL.
  void LoadNextURL(CefRefPtr<CefFrame> frame, const std::string& url) {
    if (!CefCurrentlyOn(TID_UI)) {
      CefPostTask(TID_UI, base::BindOnce(&CookieTestSchemeHandler::LoadNextURL,
                                         this, frame, url));
      return;
    }

    frame->LoadURL(url);
  }

  void CompleteTest(CefRefPtr<CefBrowser> browser) {
    if (!CefCurrentlyOn(TID_UI)) {
      CefPostTask(TID_UI, base::BindOnce(&CookieTestSchemeHandler::CompleteTest,
                                         this, browser));
      return;
    }

    // Unregister the scheme handler.
    browser->GetHost()->GetRequestContext()->RegisterSchemeHandlerFactory(
        scheme_, "cookie-tests", nullptr);

    DestroyTest();
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    std::string url = frame->GetURL();
    if (url == url1_) {
      got_load_end1_.yes();
      VerifyCookie(manager_, url, "name1", "value1", true, &got_cookie1_,
                   base::BindOnce(&CookieTestSchemeHandler::LoadNextURL, this,
                                  frame, url2_));
    } else if (url == url2_) {
      got_load_end2_.yes();
      VerifyCookie(manager_, url, "name2", "value2", false, &got_cookie2_,
                   base::BindOnce(&CookieTestSchemeHandler::LoadNextURL, this,
                                  frame, url3_));
    } else {
      got_load_end3_.yes();
      VerifyCookie(manager_, url, "name2", "value2", true, &got_cookie3_,
                   base::BindOnce(&CookieTestSchemeHandler::CompleteTest, this,
                                  browser));
    }
  }

  void DestroyTest() override {
    EXPECT_TRUE(got_process_request1_);
    EXPECT_TRUE(got_process_request2_);
    EXPECT_TRUE(got_process_request3_);
    EXPECT_TRUE(got_load_end1_);
    EXPECT_TRUE(got_load_end2_);
    EXPECT_TRUE(got_load_end3_);

    if (block_cookies_) {
      EXPECT_FALSE(got_create_cookie_);
      EXPECT_FALSE(got_process_request_cookie_);
      EXPECT_FALSE(got_cookie1_);
      EXPECT_FALSE(got_cookie2_);
      EXPECT_FALSE(got_cookie3_);
    } else {
      EXPECT_TRUE(got_create_cookie_);
      EXPECT_TRUE(got_process_request_cookie_);
      EXPECT_TRUE(got_cookie1_);
      EXPECT_TRUE(got_cookie2_);
      EXPECT_TRUE(got_cookie3_);
    }

    // Unregister the scheme handler.
    request_context_->RegisterSchemeHandlerFactory(scheme_, "cookie-tests",
                                                   nullptr);
    request_context_ = nullptr;

    TestHandler::DestroyTest();
  }

  // Verify that the cookie was set successfully.
  void VerifyCookie(CefRefPtr<CefCookieManager> manager,
                    const std::string& url,
                    const std::string& name,
                    const std::string& value,
                    bool deleteCookie,
                    TrackCallback* callback,
                    base::OnceClosure continue_callback) {
    // Get the cookie.
    EXPECT_TRUE(cookies_.empty());
    VisitUrlCookies(
        manager, url, false, cookies_, deleteCookie,
        base::BindOnce(&CookieTestSchemeHandler::VerifyCookieComplete, this,
                       name, value, callback, std::move(continue_callback)));
  }

  void VerifyCookieComplete(const std::string& name,
                            const std::string& value,
                            TrackCallback* callback,
                            base::OnceClosure continue_callback) {
    if (cookies_.size() == 1U && CefString(&cookies_[0].name) == name &&
        CefString(&cookies_[0].value) == value) {
      callback->yes();
    }

    cookies_.clear();
    std::move(continue_callback).Run();
  }

  const std::string scheme_;
  const bool use_global_;
  const bool block_cookies_;
  std::string url1_;
  std::string url2_;
  std::string url3_;

  CefRefPtr<CefRequestContext> request_context_;
  CefRefPtr<CefCookieManager> manager_;

  CookieVector cookies_;

  TrackCallback got_process_request1_;
  TrackCallback got_process_request2_;
  TrackCallback got_process_request3_;
  TrackCallback got_create_cookie_;
  TrackCallback got_process_request_cookie_;
  TrackCallback got_load_end1_;
  TrackCallback got_load_end2_;
  TrackCallback got_load_end3_;
  TrackCallback got_cookie1_;
  TrackCallback got_cookie2_;
  TrackCallback got_cookie3_;

  IMPLEMENT_REFCOUNTING(CookieTestSchemeHandler);
};

}  // namespace

// Verify use of the global cookie manager with HTTP.
TEST(CookieTest, GetCookieManagerHttpGlobal) {
  CefRefPtr<CookieTestSchemeHandler> handler =
      new CookieTestSchemeHandler("http", true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Verify use of an in-memory cookie manager with HTTP.
TEST(CookieTest, GetCookieManagerHttpInMemory) {
  CefRefPtr<CookieTestSchemeHandler> handler =
      new CookieTestSchemeHandler("http", false);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Verify use of an in-memory cookie manager with HTTP to block all cookies.
TEST(CookieTest, GetCookieManagerHttpInMemoryBlocked) {
  CefRefPtr<CookieTestSchemeHandler> handler =
      new CookieTestSchemeHandler("http", false, true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Verify use of the global cookie manager with a custom scheme.
TEST(CookieTest, GetCookieManagerCustomGlobal) {
  CefRefPtr<CookieTestSchemeHandler> handler =
      new CookieTestSchemeHandler(kCustomCookieScheme, true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Verify use of an in-memory cookie manager with a custom scheme.
TEST(CookieTest, GetCookieManagerCustomInMemory) {
  CefRefPtr<CookieTestSchemeHandler> handler =
      new CookieTestSchemeHandler(kCustomCookieScheme, false);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

namespace {

const char kCookieAccessScheme[] = "http";
const char kCookieAccessDomain[] = "test-cookies.com";
const char* kCookieAccessServerAddress = test_server::kServerAddress;
const uint16 kCookieAccessServerPort = test_server::kServerPort;

std::string GetCookieAccessOrigin(const std::string& scheme,
                                  bool server_backend) {
  std::stringstream ss;
  if (server_backend) {
    ss << scheme << "://" << kCookieAccessServerAddress << ":"
       << kCookieAccessServerPort;
  } else {
    ss << scheme << "://" << kCookieAccessDomain;
  }
  return ss.str();
}

std::string GetCookieAccessUrl1(const std::string& scheme,
                                bool server_backend) {
  return GetCookieAccessOrigin(scheme, server_backend) + "/cookie1.html";
}

std::string GetCookieAccessUrl2(const std::string& scheme,
                                bool server_backend) {
  return GetCookieAccessOrigin(scheme, server_backend) + "/cookie2.html";
}

void TestCookieString(const std::string& cookie_str,
                      int& cookie_js_ct,
                      int& cookie_net_ct) {
  if (cookie_str.find("name_js=value_js") != std::string::npos) {
    cookie_js_ct++;
  }
  if (cookie_str.find("name_net=value_net") != std::string::npos) {
    cookie_net_ct++;
  }
}

struct CookieAccessData {
  CefRefPtr<CefResponse> response;
  std::string response_data;

  int request_ct_ = 0;
  int cookie_js_ct_ = 0;
  int cookie_net_ct_ = 0;
};

class CookieAccessResponseHandler {
 public:
  CookieAccessResponseHandler() {}
  virtual void AddResponse(const std::string& url, CookieAccessData* data) = 0;

 protected:
  virtual ~CookieAccessResponseHandler() {}
};

std::string GetHeaderValue(const CefServer::HeaderMap& header_map,
                           const std::string& header_name) {
  CefServer::HeaderMap::const_iterator it = header_map.find(header_name);
  if (it != header_map.end())
    return it->second;
  return std::string();
}

// Serves request responses.
class CookieAccessSchemeHandler : public CefResourceHandler {
 public:
  explicit CookieAccessSchemeHandler(CookieAccessData* data)
      : data_(data), offset_(0) {}

  bool Open(CefRefPtr<CefRequest> request,
            bool& handle_request,
            CefRefPtr<CefCallback> callback) override {
    EXPECT_FALSE(CefCurrentlyOn(TID_UI) || CefCurrentlyOn(TID_IO));

    CefRequest::HeaderMap headerMap;
    request->GetHeaderMap(headerMap);
    const std::string& cookie_str = GetHeaderValue(headerMap, "Cookie");
    TestCookieString(cookie_str, data_->cookie_js_ct_, data_->cookie_net_ct_);

    // Continue immediately.
    handle_request = true;
    return true;
  }

  void GetResponseHeaders(CefRefPtr<CefResponse> response,
                          int64& response_length,
                          CefString& redirectUrl) override {
    EXPECT_IO_THREAD();

    response->SetStatus(data_->response->GetStatus());
    response->SetStatusText(data_->response->GetStatusText());
    response->SetMimeType(data_->response->GetMimeType());

    CefResponse::HeaderMap headerMap;
    data_->response->GetHeaderMap(headerMap);
    response->SetHeaderMap(headerMap);

    response_length = data_->response_data.length();
  }

  bool Read(void* data_out,
            int bytes_to_read,
            int& bytes_read,
            CefRefPtr<CefResourceReadCallback> callback) override {
    EXPECT_FALSE(CefCurrentlyOn(TID_UI) || CefCurrentlyOn(TID_IO));

    bool has_data = false;
    bytes_read = 0;

    size_t size = data_->response_data.length();
    if (offset_ < size) {
      int transfer_size =
          std::min(bytes_to_read, static_cast<int>(size - offset_));
      memcpy(data_out, data_->response_data.c_str() + offset_, transfer_size);
      offset_ += transfer_size;

      bytes_read = transfer_size;
      has_data = true;
    }

    return has_data;
  }

  void Cancel() override { EXPECT_IO_THREAD(); }

 private:
  static void TestCookie(const CefCookie& cookie,
                         TrackCallback& got_cookie_js,
                         TrackCallback& got_cookie_net) {
    const std::string& cookie_name = CefString(&cookie.name);
    const std::string& cookie_val = CefString(&cookie.value);
    if (cookie_name == "name_js") {
      EXPECT_STREQ("value_js", cookie_val.c_str());
      got_cookie_js.yes();
    } else if (cookie_name == "name_net") {
      EXPECT_STREQ("value_net", cookie_val.c_str());
      got_cookie_net.yes();
    } else {
      ADD_FAILURE() << "Unexpected cookie: " << cookie_name;
    }
  }

  // |data_| is not owned by this object.
  CookieAccessData* data_;

  size_t offset_;

  IMPLEMENT_REFCOUNTING(CookieAccessSchemeHandler);
  DISALLOW_COPY_AND_ASSIGN(CookieAccessSchemeHandler);
};

class CookieAccessSchemeHandlerFactory : public CefSchemeHandlerFactory,
                                         public CookieAccessResponseHandler {
 public:
  CookieAccessSchemeHandlerFactory() {}

  CefRefPtr<CefResourceHandler> Create(CefRefPtr<CefBrowser> browser,
                                       CefRefPtr<CefFrame> frame,
                                       const CefString& scheme_name,
                                       CefRefPtr<CefRequest> request) override {
    EXPECT_IO_THREAD();
    const std::string& url = request->GetURL();
    ResponseDataMap::const_iterator it = data_map_.find(url);
    if (it != data_map_.end()) {
      it->second->request_ct_++;

      return new CookieAccessSchemeHandler(it->second);
    }

    // Unknown test.
    if (!IgnoreURL(url)) {
      ADD_FAILURE() << "Unexpected url: " << url;
    }
    return nullptr;
  }

  void AddResponse(const std::string& url, CookieAccessData* data) override {
    data_map_.insert(std::make_pair(url, data));
  }

  void Shutdown(base::OnceClosure complete_callback) {
    if (!CefCurrentlyOn(TID_IO)) {
      CefPostTask(TID_IO, base::BindOnce(std::move(complete_callback)));
      return;
    }

    std::move(complete_callback).Run();
  }

 private:
  // Map of URL to Data.
  typedef std::map<std::string, CookieAccessData*> ResponseDataMap;
  ResponseDataMap data_map_;

  IMPLEMENT_REFCOUNTING(CookieAccessSchemeHandlerFactory);
};

// HTTP server handler.
class CookieAccessServerHandler : public test_server::ObserverHelper,
                                  public CookieAccessResponseHandler {
 public:
  CookieAccessServerHandler()
      : initialized_(false),
        expected_http_request_ct_(-1),
        actual_http_request_ct_(0) {}

  virtual ~CookieAccessServerHandler() { RunCompleteCallback(); }

  // Must be called before CreateServer().
  void AddResponse(const std::string& url, CookieAccessData* data) override {
    EXPECT_FALSE(initialized_);
    data_map_.insert(std::make_pair(url, data));
  }

  // Must be called before CreateServer().
  void SetExpectedRequestCount(int count) {
    EXPECT_FALSE(initialized_);
    expected_http_request_ct_ = count;
  }

  // |complete_callback| will be executed on the UI thread after the server is
  // started.
  void CreateServer(base::OnceClosure complete_callback) {
    EXPECT_UI_THREAD();

    if (expected_http_request_ct_ < 0) {
      // Default to the assumption of one request per registered URL.
      SetExpectedRequestCount(static_cast<int>(data_map_.size()));
    }

    EXPECT_FALSE(initialized_);
    initialized_ = true;

    EXPECT_TRUE(complete_callback_.is_null());
    complete_callback_ = std::move(complete_callback);

    Initialize();
  }

  // Results in a call to VerifyResults() and eventual execution of the
  // |complete_callback| on the UI thread via CookieAccessServerHandler
  // destruction.
  void ShutdownServer(base::OnceClosure complete_callback) {
    EXPECT_UI_THREAD();

    EXPECT_TRUE(complete_callback_.is_null());
    complete_callback_ = std::move(complete_callback);

    Shutdown();
  }

  void OnInitialized(const std::string& server_origin) override {
    EXPECT_UI_THREAD();
    EXPECT_STREQ(server_origin.c_str(),
                 GetCookieAccessOrigin(kCookieAccessScheme, true).c_str());

    EXPECT_FALSE(got_server_created_);
    got_server_created_.yes();

    RunCompleteCallback();
  }

  void OnShutdown() override {
    EXPECT_UI_THREAD();

    EXPECT_FALSE(got_server_destroyed_);
    got_server_destroyed_.yes();

    VerifyResults();

    delete this;
  }

  bool OnHttpRequest(CefRefPtr<CefServer> server,
                     int connection_id,
                     const CefString& client_address,
                     CefRefPtr<CefRequest> request) override {
    EXPECT_UI_THREAD();
    EXPECT_FALSE(client_address.empty());

    // Log the requests for better error reporting.
    request_log_ += request->GetMethod().ToString() + " " +
                    request->GetURL().ToString() + "\n";

    HandleRequest(server, connection_id, request);

    actual_http_request_ct_++;

    return true;
  }

 private:
  void VerifyResults() {
    EXPECT_TRUE(got_server_created_);
    EXPECT_TRUE(got_server_destroyed_);
    EXPECT_EQ(expected_http_request_ct_, actual_http_request_ct_)
        << request_log_;
  }

  void HandleRequest(CefRefPtr<CefServer> server,
                     int connection_id,
                     CefRefPtr<CefRequest> request) {
    const std::string& url = request->GetURL();
    ResponseDataMap::const_iterator it = data_map_.find(url);
    if (it != data_map_.end()) {
      it->second->request_ct_++;

      CefRequest::HeaderMap headerMap;
      request->GetHeaderMap(headerMap);
      const std::string& cookie_str = GetHeaderValue(headerMap, "cookie");
      TestCookieString(cookie_str, it->second->cookie_js_ct_,
                       it->second->cookie_net_ct_);

      SendResponse(server, connection_id, it->second->response,
                   it->second->response_data);
    } else {
      // Unknown test.
      if (!IgnoreURL(url)) {
        ADD_FAILURE() << "Unexpected url: " << url;
      }
      server->SendHttp500Response(connection_id, "Unknown test");
    }
  }

  static void SendResponse(CefRefPtr<CefServer> server,
                           int connection_id,
                           CefRefPtr<CefResponse> response,
                           const std::string& response_data) {
    // Execute on the server thread because some methods require it.
    CefRefPtr<CefTaskRunner> task_runner = server->GetTaskRunner();
    if (!task_runner->BelongsToCurrentThread()) {
      task_runner->PostTask(CefCreateClosureTask(
          base::BindOnce(CookieAccessServerHandler::SendResponse, server,
                         connection_id, response, response_data)));
      return;
    }

    int response_code = response->GetStatus();
    const CefString& content_type = response->GetMimeType();
    int64 content_length = static_cast<int64>(response_data.size());

    CefResponse::HeaderMap extra_headers;
    response->GetHeaderMap(extra_headers);

    server->SendHttpResponse(connection_id, response_code, content_type,
                             content_length, extra_headers);

    if (content_length != 0) {
      server->SendRawData(connection_id, response_data.data(),
                          response_data.size());
      server->CloseConnection(connection_id);
    }

    // The connection should be closed.
    EXPECT_FALSE(server->IsValidConnection(connection_id));
  }

  void RunCompleteCallback() {
    EXPECT_UI_THREAD();

    EXPECT_FALSE(complete_callback_.is_null());
    std::move(complete_callback_).Run();
  }

  // Map of URL to Data.
  typedef std::map<std::string, CookieAccessData*> ResponseDataMap;
  ResponseDataMap data_map_;

  bool initialized_;

  // Only accessed on the UI thread.
  base::OnceClosure complete_callback_;

  // After initialization the below members are only accessed on the server
  // thread.

  TrackCallback got_server_created_;
  TrackCallback got_server_destroyed_;

  int expected_http_request_ct_;
  int actual_http_request_ct_;

  std::string request_log_;

  DISALLOW_COPY_AND_ASSIGN(CookieAccessServerHandler);
};

class CookieAccessTestHandler : public RoutingTestHandler,
                                public CefCookieAccessFilter {
 public:
  enum TestMode {
    ALLOW = 0,
    BLOCK_READ = 1 << 0,
    BLOCK_WRITE = 1 << 1,
    BLOCK_READ_WRITE = BLOCK_READ | BLOCK_WRITE,
    ALLOW_NO_FILTER = 1 << 2,

    // Block all cookies using CefRequestContextSettings. Can only be used with
    // a non-global request context because it's too late (during test
    // execution) to call this method on the global context.
    BLOCK_ALL_COOKIES = 1 << 3,

    // Return nullptr from GetResourceRequestHandler. Can only be used in
    // combination with the SERVER or SCHEME_HANDLER backend (the
    // RESOURCE_HANDLER backend would not be called).
    ALLOW_NO_HANDLER = 1 << 4,
  };

  enum TestBackend {
    // Test an HTTP server backend.
    SERVER,

    // Test a custom scheme handler backend.
    SCHEME_HANDLER,

    // Test that GetResourceHandler behaves the same as a custom scheme handler.
    RESOURCE_HANDLER,
  };

  CookieAccessTestHandler(TestMode test_mode,
                          TestBackend test_backend,
                          bool custom_scheme,
                          bool use_global)
      : test_mode_(test_mode),
        test_backend_(test_backend),
        scheme_(custom_scheme ? kCustomCookieScheme : kCookieAccessScheme),
        use_global_(use_global) {
    if (test_mode_ == BLOCK_ALL_COOKIES)
      CHECK(!use_global_);
    else if (test_mode_ == ALLOW_NO_HANDLER)
      CHECK_NE(RESOURCE_HANDLER, test_backend_);
    if (test_backend_ == SERVER)
      CHECK(!custom_scheme);
  }

  void RunTest() override {
    if (use_global_) {
      context_ = CefRequestContext::GetGlobalContext();
    } else {
      // Create the request context that will use an in-memory cache.
      CefRequestContextSettings settings;

      const bool block_cookies = (test_mode_ == BLOCK_ALL_COOKIES);
      if (scheme_ == kCustomCookieScheme || block_cookies) {
        if (!block_cookies) {
          CefString(&settings.cookieable_schemes_list) = kCustomCookieScheme;
        } else {
          settings.cookieable_schemes_exclude_defaults = true;
        }
      }

      context_ = CefRequestContext::CreateContext(settings, nullptr);
    }

    SetTestTimeout();

    cookie_manager_ = context_->GetCookieManager(nullptr);
    RunTestSetupContinue();
  }

  void DestroyTest() override {
    if (!CefCurrentlyOn(TID_UI)) {
      CefPostTask(TID_UI,
                  base::BindOnce(&CookieAccessTestHandler::DestroyTest, this));
      return;
    }

    cookie_manager_ = nullptr;
    context_ = nullptr;

    // Got both network requests.
    EXPECT_EQ(1, data1_.request_ct_);
    EXPECT_EQ(1, data2_.request_ct_);

    if (test_mode_ == ALLOW_NO_FILTER || test_mode_ == ALLOW_NO_HANDLER) {
      EXPECT_EQ(0, can_save_cookie1_ct_);
      EXPECT_EQ(0, can_send_cookie2_ct_);
    } else {
      if (test_mode_ == BLOCK_ALL_COOKIES) {
        // Never send any cookies.
        EXPECT_EQ(0, can_send_cookie2_ct_);
        EXPECT_EQ(0, can_save_cookie1_ct_);
      } else if (test_mode_ & BLOCK_WRITE) {
        // Get 1 calls to CanSendCookie for the 2nd network request due to the
        // JS cookie (network cookie is blocked).
        EXPECT_EQ(1, can_send_cookie2_ct_);
        // Get 1 call to CanSaveCookie for the 1st network request due to the
        // network cookie.
        EXPECT_EQ(1, can_save_cookie1_ct_);
      } else {
        // Get 2 calls to CanSendCookie for the 2nd network request due to the
        // network cookie + JS cookie.
        EXPECT_EQ(2, can_send_cookie2_ct_);
        // Get 1 call to CanSaveCookie for the 1st network request due to the
        // network cookie.
        EXPECT_EQ(1, can_save_cookie1_ct_);
      }
    }

    if (test_mode_ == BLOCK_ALL_COOKIES) {
      // Never get the JS cookie via JS.
      EXPECT_EQ(0, cookie_js1_ct_);
      EXPECT_EQ(0, cookie_js2_ct_);
      EXPECT_EQ(0, cookie_js3_ct_);
    } else {
      // Always get the JS cookie via JS.
      EXPECT_EQ(1, cookie_js1_ct_);
      EXPECT_EQ(1, cookie_js2_ct_);
      EXPECT_EQ(1, cookie_js3_ct_);
    }

    // Only get the net cookie via JS if cookie write was allowed.
    if ((test_mode_ & BLOCK_WRITE) || test_mode_ == BLOCK_ALL_COOKIES) {
      EXPECT_EQ(0, cookie_net1_ct_);
      EXPECT_EQ(0, cookie_net2_ct_);
      EXPECT_EQ(0, cookie_net3_ct_);
    } else {
      EXPECT_EQ(1, cookie_net1_ct_);
      EXPECT_EQ(1, cookie_net2_ct_);
      EXPECT_EQ(1, cookie_net3_ct_);
    }

    // No cookies sent for the 1st network request.
    EXPECT_EQ(0, data1_.cookie_js_ct_);
    EXPECT_EQ(0, data1_.cookie_net_ct_);

    // 2nd network request...
    if ((test_mode_ & BLOCK_READ) || test_mode_ == BLOCK_ALL_COOKIES) {
      // No cookies sent if reading was blocked.
      EXPECT_EQ(0, data2_.cookie_js_ct_);
      EXPECT_EQ(0, data2_.cookie_net_ct_);
    } else if (test_mode_ & BLOCK_WRITE) {
      // Only JS cookie sent if writing was blocked.
      EXPECT_EQ(1, data2_.cookie_js_ct_);
      EXPECT_EQ(0, data2_.cookie_net_ct_);
    } else {
      // All cookies sent.
      EXPECT_EQ(1, data2_.cookie_js_ct_);
      EXPECT_EQ(1, data2_.cookie_net_ct_);
    }

    TestHandler::DestroyTest();
  }

  CefRefPtr<CefCookieAccessFilter> GetCookieAccessFilter(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request) override {
    EXPECT_IO_THREAD();

    if (test_mode_ == ALLOW_NO_FILTER)
      return nullptr;

    return this;
  }

  CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request,
      bool is_navigation,
      bool is_download,
      const CefString& request_initiator,
      bool& disable_default_handling) override {
    if (test_mode_ == ALLOW_NO_HANDLER)
      return nullptr;

    return this;
  }

  CefRefPtr<CefResourceHandler> GetResourceHandler(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request) override {
    if (test_backend_ == RESOURCE_HANDLER && scheme_factory_) {
      return scheme_factory_->Create(browser, frame, scheme_, request);
    }

    return nullptr;
  }

  bool CanSendCookie(CefRefPtr<CefBrowser> browser,
                     CefRefPtr<CefFrame> frame,
                     CefRefPtr<CefRequest> request,
                     const CefCookie& cookie) override {
    EXPECT_IO_THREAD();

    const std::string& url = request->GetURL();
    if (url == GetCookieAccessUrl2(scheme_, test_backend_ == SERVER)) {
      can_send_cookie2_ct_++;
    } else if (!IgnoreURL(url)) {
      ADD_FAILURE() << "Unexpected url: " << url;
    }

    return !(test_mode_ & BLOCK_READ);
  }

  bool CanSaveCookie(CefRefPtr<CefBrowser> browser,
                     CefRefPtr<CefFrame> frame,
                     CefRefPtr<CefRequest> request,
                     CefRefPtr<CefResponse> response,
                     const CefCookie& cookie) override {
    EXPECT_IO_THREAD();

    // Expecting the network cookie only.
    EXPECT_STREQ("name_net", CefString(&cookie.name).ToString().c_str());
    EXPECT_STREQ("value_net", CefString(&cookie.value).ToString().c_str());

    const std::string& url = request->GetURL();
    if (url == GetCookieAccessUrl1(scheme_, test_backend_ == SERVER)) {
      can_save_cookie1_ct_++;
    } else if (!IgnoreURL(url)) {
      ADD_FAILURE() << "Unexpected url: " << url;
    }

    return !(test_mode_ & BLOCK_WRITE);
  }

  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64 query_id,
               const CefString& request,
               bool persistent,
               CefRefPtr<Callback> callback) override {
    const std::string& url = frame->GetURL();
    const std::string& cookie_str = request.ToString();
    if (url == GetCookieAccessUrl1(scheme_, test_backend_ == SERVER)) {
      TestCookieString(cookie_str, cookie_js1_ct_, cookie_net1_ct_);
      browser->GetMainFrame()->LoadURL(
          GetCookieAccessUrl2(scheme_, test_backend_ == SERVER));
    } else if (url == GetCookieAccessUrl2(scheme_, test_backend_ == SERVER)) {
      TestCookieString(cookie_str, cookie_js2_ct_, cookie_net2_ct_);
      FinishTest();
    } else {
      ADD_FAILURE() << "Unexpected url: " << url;
    }
    return true;
  }

 private:
  void AddResponses(CookieAccessResponseHandler* handler) {
    // 1st request sets a cookie via net response headers and JS, then retrieves
    // the cookies via JS.
    {
      data1_.response = CefResponse::Create();
      data1_.response->SetMimeType("text/html");
      data1_.response->SetStatus(200);
      data1_.response->SetStatusText("OK");

      CefResponse::HeaderMap headerMap;
      data1_.response->GetHeaderMap(headerMap);
      headerMap.insert(std::make_pair("Set-Cookie", "name_net=value_net"));
      data1_.response->SetHeaderMap(headerMap);

      data1_.response_data =
          "<html><head>"
          "<script>"
          "document.cookie='name_js=value_js';"
          "window.testQuery({request:document.cookie});"
          "</script>"
          "</head><body>COOKIE ACCESS TEST 1</body></html>";

      handler->AddResponse(
          GetCookieAccessUrl1(scheme_, test_backend_ == SERVER), &data1_);
    }

    // 2nd request retrieves the cookies via JS.
    {
      data2_.response = CefResponse::Create();
      data2_.response->SetMimeType("text/html");
      data2_.response->SetStatus(200);
      data2_.response->SetStatusText("OK");

      data2_.response_data =
          "<html><head>"
          "<script>"
          "window.testQuery({request:document.cookie});"
          "</script>"
          "</head><body>COOKIE ACCESS TEST 2</body></html>";

      handler->AddResponse(
          GetCookieAccessUrl2(scheme_, test_backend_ == SERVER), &data2_);
    }
  }

  void RunTestSetupContinue() {
    CefPostTask(
        TID_UI,
        base::BindOnce(
            &CookieAccessTestHandler::StartBackend, this,
            base::BindOnce(&CookieAccessTestHandler::RunTestContinue, this)));
  }

  void StartBackend(base::OnceClosure complete_callback) {
    if (test_backend_ == SERVER) {
      StartServer(std::move(complete_callback));
    } else {
      StartSchemeHandler(std::move(complete_callback));
    }
  }

  void StartServer(base::OnceClosure complete_callback) {
    EXPECT_FALSE(server_handler_);

    server_handler_ = new CookieAccessServerHandler();
    AddResponses(server_handler_);
    server_handler_->CreateServer(std::move(complete_callback));
  }

  void StartSchemeHandler(base::OnceClosure complete_callback) {
    // Add the factory registration.
    scheme_factory_ = new CookieAccessSchemeHandlerFactory();
    AddResponses(scheme_factory_.get());
    if (test_backend_ == SCHEME_HANDLER) {
      context_->RegisterSchemeHandlerFactory(scheme_, kCookieAccessDomain,
                                             scheme_factory_.get());
    }

    std::move(complete_callback).Run();
  }

  void RunTestContinue() {
    if (!CefCurrentlyOn(TID_UI)) {
      CefPostTask(TID_UI, base::BindOnce(
                              &CookieAccessTestHandler::RunTestContinue, this));
      return;
    }

    CreateBrowser(GetCookieAccessUrl1(scheme_, test_backend_ == SERVER),
                  context_);
  }

  void FinishTest() {
    // Verify that cookies were set correctly.
    class TestVisitor : public CefCookieVisitor {
     public:
      explicit TestVisitor(CookieAccessTestHandler* handler)
          : handler_(handler) {}
      ~TestVisitor() override {
        // Destroy the test.
        CefPostTask(
            TID_UI,
            base::BindOnce(&CookieAccessTestHandler::ShutdownBackend, handler_,
                           base::BindOnce(&CookieAccessTestHandler::DestroyTest,
                                          handler_)));
      }

      bool Visit(const CefCookie& cookie,
                 int count,
                 int total,
                 bool& deleteCookie) override {
        const std::string& name = CefString(&cookie.name);
        const std::string& value = CefString(&cookie.value);
        if (name == "name_js" && value == "value_js")
          handler_->cookie_js3_ct_++;
        else if (name == "name_net" && value == "value_net")
          handler_->cookie_net3_ct_++;

        // Clean up the cookies.
        deleteCookie = true;

        return true;
      }

     private:
      CookieAccessTestHandler* handler_;
      IMPLEMENT_REFCOUNTING(TestVisitor);
    };

    cookie_manager_->VisitAllCookies(new TestVisitor(this));
  }

  void ShutdownBackend(base::OnceClosure complete_callback) {
    if (test_backend_ == SERVER) {
      ShutdownServer(std::move(complete_callback));
    } else {
      ShutdownSchemeHandler(std::move(complete_callback));
    }
  }

  void ShutdownServer(base::OnceClosure complete_callback) {
    EXPECT_TRUE(server_handler_);

    // |server_handler_| will delete itself after shutdown.
    server_handler_->ShutdownServer(std::move(complete_callback));
    server_handler_ = nullptr;
  }

  void ShutdownSchemeHandler(base::OnceClosure complete_callback) {
    EXPECT_TRUE(scheme_factory_);

    if (test_backend_ == SCHEME_HANDLER) {
      context_->RegisterSchemeHandlerFactory(scheme_, kCookieAccessDomain,
                                             nullptr);
    }
    scheme_factory_->Shutdown(std::move(complete_callback));
    scheme_factory_ = nullptr;
  }

  const TestMode test_mode_;
  const TestBackend test_backend_;
  const std::string scheme_;
  const bool use_global_;
  CefRefPtr<CefRequestContext> context_;
  CefRefPtr<CefCookieManager> cookie_manager_;

  CookieAccessServerHandler* server_handler_ = nullptr;
  CefRefPtr<CookieAccessSchemeHandlerFactory> scheme_factory_;

  CookieAccessData data1_;
  CookieAccessData data2_;

  // 1st request.
  int can_save_cookie1_ct_ = 0;
  int cookie_js1_ct_ = 0;
  int cookie_net1_ct_ = 0;

  // 2nd request.
  int can_send_cookie2_ct_ = 0;
  int cookie_js2_ct_ = 0;
  int cookie_net2_ct_ = 0;

  // From cookie manager.
  int cookie_js3_ct_ = 0;
  int cookie_net3_ct_ = 0;

  DISALLOW_COPY_AND_ASSIGN(CookieAccessTestHandler);
  IMPLEMENT_REFCOUNTING(CookieAccessTestHandler);
};

}  // namespace

#define ACCESS_TEST(name, test_mode, backend_mode, custom_scheme, use_global) \
  TEST(CookieTest, Access##name) {                                            \
    CefRefPtr<CookieAccessTestHandler> handler = new CookieAccessTestHandler( \
        CookieAccessTestHandler::test_mode,                                   \
        CookieAccessTestHandler::backend_mode, custom_scheme, use_global);    \
    handler->ExecuteTest();                                                   \
    ReleaseAndWaitForDestructor(handler);                                     \
  }

#define ACCESS_TEST_ALL_MODES(name, backend_mode, custom_scheme, use_global) \
  ACCESS_TEST(name##Allow, ALLOW, backend_mode, custom_scheme, use_global)   \
  ACCESS_TEST(name##AllowNoFilter, ALLOW_NO_FILTER, backend_mode,            \
              custom_scheme, use_global)                                     \
  ACCESS_TEST(name##BlockRead, BLOCK_READ, backend_mode, custom_scheme,      \
              use_global)                                                    \
  ACCESS_TEST(name##BlockWrite, BLOCK_WRITE, backend_mode, custom_scheme,    \
              use_global)                                                    \
  ACCESS_TEST(name##BlockReadWrite, BLOCK_READ_WRITE, backend_mode,          \
              custom_scheme, use_global)

// These tests only work with a non-global context.
#define ACCESS_TEST_BLOCKALLCOOKIES_MODES(name, backend_mode, custom_scheme) \
  ACCESS_TEST(name##BlockAllCookies, BLOCK_ALL_COOKIES, backend_mode,        \
              custom_scheme, false)

// These tests only work with SERVER and SCHEME_HANDLER backends.
#define ACCESS_TEST_ALLOWNOHANDLER_MODES(name, backend_mode, custom_scheme) \
  ACCESS_TEST(name##GlobalAllowNoHandler, ALLOW_NO_HANDLER, backend_mode,   \
              custom_scheme, false)                                         \
  ACCESS_TEST(name##InMemoryAllowNoHandler, ALLOW_NO_HANDLER, backend_mode, \
              custom_scheme, true)

#define ACCESS_TEST_CUSTOM(name, backend_mode)                           \
  ACCESS_TEST_ALL_MODES(name##CustomGlobal, backend_mode, true, true)    \
  ACCESS_TEST_ALL_MODES(name##CustomInMemory, backend_mode, true, false) \
  ACCESS_TEST_BLOCKALLCOOKIES_MODES(name##CustomInMemory, backend_mode, true)

#define ACCESS_TEST_STANDARD(name, backend_mode)                            \
  ACCESS_TEST_ALL_MODES(name##StandardGlobal, backend_mode, false, true)    \
  ACCESS_TEST_ALL_MODES(name##StandardInMemory, backend_mode, false, false) \
  ACCESS_TEST_BLOCKALLCOOKIES_MODES(name##StandardInMemory, backend_mode, false)

// Server backend only works with standard schemes.
ACCESS_TEST_STANDARD(Server, SERVER)
ACCESS_TEST_ALLOWNOHANDLER_MODES(ServerStandard, SERVER, false)

// Other backends work with all schemes.
ACCESS_TEST_CUSTOM(Scheme, SCHEME_HANDLER)
ACCESS_TEST_ALLOWNOHANDLER_MODES(SchemeCustom, SCHEME_HANDLER, true)
ACCESS_TEST_STANDARD(Scheme, SCHEME_HANDLER)
ACCESS_TEST_ALLOWNOHANDLER_MODES(SchemeStandard, SCHEME_HANDLER, false)

ACCESS_TEST_CUSTOM(Resource, RESOURCE_HANDLER)
ACCESS_TEST_STANDARD(Resource, RESOURCE_HANDLER)

namespace {

// Tests the behavior of restarting of a network request that sets cookies and
// a network request that includes cookies.
// 1. Begin loading URL1, then restart the request in OnResourceResponse.
//    No cookies are saved.
// 2. Load URL1 successfully. Network and JS cookies are saved.
// 3. Begin loading URL2, then restart the request in OnResourceResponse.
//    Cookies are sent with the request/response.
// 4. Load URL2 successfully. Cookies are sent with the request/response.
class CookieRestartTestHandler : public RoutingTestHandler,
                                 public CefCookieAccessFilter {
 public:
  explicit CookieRestartTestHandler(bool use_global)
      : scheme_(kCookieAccessScheme), use_global_(use_global) {}

  void RunTest() override {
    if (use_global_) {
      context_ = CefRequestContext::GetGlobalContext();
    } else {
      // Create the request context that will use an in-memory cache.
      CefRequestContextSettings settings;
      context_ = CefRequestContext::CreateContext(settings, nullptr);
    }

    cookie_manager_ = context_->GetCookieManager(nullptr);

    SetTestTimeout();
    RunTestSetupContinue();
  }

  void DestroyTest() override {
    if (!CefCurrentlyOn(TID_UI)) {
      CefPostTask(TID_UI,
                  base::BindOnce(&CookieRestartTestHandler::DestroyTest, this));
      return;
    }

    cookie_manager_ = nullptr;
    context_ = nullptr;

    // Get 2 network requests for each URL.
    EXPECT_EQ(2, data1_.request_ct_);
    EXPECT_EQ(2, data2_.request_ct_);

    // Get resource request callbacks for all requests (2 for each URL).
    EXPECT_EQ(4, before_resource_load_ct_);
    EXPECT_EQ(4, resource_response_ct_);

    // Get JS query callbacks for the successful requests (1 for each URL).
    EXPECT_EQ(2, query_ct_);

    // No cookies sent for the URL1 network requests because (a) we don't have
    // any cookies set initially and (b) we don't save cookies from the 1st URL1
    // request which is restarted.
    EXPECT_EQ(0, data1_.cookie_js_ct_);
    EXPECT_EQ(0, data1_.cookie_net_ct_);

    // Net and JS cookies sent for both URL2 network requests.
    EXPECT_EQ(2, data2_.cookie_js_ct_);
    EXPECT_EQ(2, data2_.cookie_net_ct_);

    // 1 call to CanSaveCookie for the net cookie returned by the successful
    // URL1 request.
    EXPECT_EQ(1, can_save_cookie_ct_);
    // 4 calls to CanSendCookie because both net and JS cookies are sent for
    // each URL2 request.
    EXPECT_EQ(4, can_send_cookie_ct_);

    // Get the net and JS cookies from the JS query for the successful requests
    // (1 for each URL).
    EXPECT_EQ(1, cookie_js1_ct_);
    EXPECT_EQ(1, cookie_net1_ct_);
    EXPECT_EQ(1, cookie_js2_ct_);
    EXPECT_EQ(1, cookie_net2_ct_);

    // Get the net and JS cookies from the cookie manager at the end.
    EXPECT_EQ(1, cookie_manager_js_ct_);
    EXPECT_EQ(1, cookie_manager_net_ct_);

    TestHandler::DestroyTest();
  }

  CefRefPtr<CefCookieAccessFilter> GetCookieAccessFilter(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request) override {
    EXPECT_IO_THREAD();
    return this;
  }

  CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request,
      bool is_navigation,
      bool is_download,
      const CefString& request_initiator,
      bool& disable_default_handling) override {
    return this;
  }

  CefRefPtr<CefResourceHandler> GetResourceHandler(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request) override {
    return nullptr;
  }

  bool CanSendCookie(CefRefPtr<CefBrowser> browser,
                     CefRefPtr<CefFrame> frame,
                     CefRefPtr<CefRequest> request,
                     const CefCookie& cookie) override {
    EXPECT_IO_THREAD();
    can_send_cookie_ct_++;

    // Called before the URL2 network requests.
    EXPECT_LE(2, before_resource_load_ct_);

    return true;
  }

  bool CanSaveCookie(CefRefPtr<CefBrowser> browser,
                     CefRefPtr<CefFrame> frame,
                     CefRefPtr<CefRequest> request,
                     CefRefPtr<CefResponse> response,
                     const CefCookie& cookie) override {
    EXPECT_IO_THREAD();
    can_save_cookie_ct_++;

    // Called after the successful URL1 network request.
    EXPECT_EQ(2, before_resource_load_ct_);

    // Expecting the network cookie only.
    EXPECT_STREQ("name_net", CefString(&cookie.name).ToString().c_str());
    EXPECT_STREQ("value_net", CefString(&cookie.value).ToString().c_str());

    return true;
  }

  cef_return_value_t OnBeforeResourceLoad(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request,
      CefRefPtr<CefCallback> callback) override {
    EXPECT_IO_THREAD();
    before_resource_load_ct_++;

    const std::string& url = request->GetURL();

    if (before_resource_load_ct_ <= 2) {
      EXPECT_STREQ(GetCookieAccessUrl1(scheme_, true).c_str(), url.c_str());
    } else {
      EXPECT_STREQ(GetCookieAccessUrl2(scheme_, true).c_str(), url.c_str());
    }

    const std::string& cookie_str = request->GetHeaderByName("Cookie");
    int cookie_js_ct = 0;
    int cookie_net_ct = 0;
    TestCookieString(cookie_str, cookie_js_ct, cookie_net_ct);

    // Expect both cookies with the URL2 requests only.
    if (before_resource_load_ct_ >= 3) {
      EXPECT_EQ(1, cookie_js_ct);
      EXPECT_EQ(1, cookie_net_ct);
    } else {
      EXPECT_EQ(0, cookie_js_ct);
      EXPECT_EQ(0, cookie_net_ct);
    }

    return RV_CONTINUE;
  }

  bool OnResourceResponse(CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefFrame> frame,
                          CefRefPtr<CefRequest> request,
                          CefRefPtr<CefResponse> response) override {
    EXPECT_IO_THREAD();
    resource_response_ct_++;

    const std::string& url = request->GetURL();
    const std::string& set_cookie_str = response->GetHeaderByName("Set-Cookie");

    // Expect the network cookie with URL1 requests only.
    if (resource_response_ct_ <= 2) {
      EXPECT_STREQ(GetCookieAccessUrl1(scheme_, true).c_str(), url.c_str());
      EXPECT_STREQ("name_net=value_net", set_cookie_str.c_str());
    } else {
      EXPECT_STREQ(GetCookieAccessUrl2(scheme_, true).c_str(), url.c_str());
      EXPECT_TRUE(set_cookie_str.empty());
    }

    if (resource_response_ct_ == 1 || resource_response_ct_ == 3) {
      // Restart the request loading this data.
      request->SetHeaderByName("X-Custom-Header", "value", false);
      return true;
    }
    return false;
  }

  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64 query_id,
               const CefString& request,
               bool persistent,
               CefRefPtr<Callback> callback) override {
    query_ct_++;

    const std::string& url = frame->GetURL();
    const std::string& cookie_str = request.ToString();
    if (url == GetCookieAccessUrl1(scheme_, true)) {
      TestCookieString(cookie_str, cookie_js1_ct_, cookie_net1_ct_);
      browser->GetMainFrame()->LoadURL(GetCookieAccessUrl2(scheme_, true));
    } else if (url == GetCookieAccessUrl2(scheme_, true)) {
      TestCookieString(cookie_str, cookie_js2_ct_, cookie_net2_ct_);
      FinishTest();
    } else {
      ADD_FAILURE() << "Unexpected url: " << url;
    }
    return true;
  }

 private:
  void AddResponses(CookieAccessResponseHandler* handler) {
    // Sets a cookie via net response headers and JS, then retrieves the cookies
    // via JS.
    {
      data1_.response = CefResponse::Create();
      data1_.response->SetMimeType("text/html");
      data1_.response->SetStatus(200);
      data1_.response->SetStatusText("OK");

      CefResponse::HeaderMap headerMap;
      data1_.response->GetHeaderMap(headerMap);
      headerMap.insert(std::make_pair("Set-Cookie", "name_net=value_net"));
      data1_.response->SetHeaderMap(headerMap);

      data1_.response_data =
          "<html><head>"
          "<script>"
          "document.cookie='name_js=value_js';"
          "window.testQuery({request:document.cookie});"
          "</script>"
          "</head><body>COOKIE RESTART TEST1</body></html>";

      handler->AddResponse(GetCookieAccessUrl1(scheme_, true), &data1_);
    }

    // Retrieves the cookies via JS.
    {
      data2_.response = CefResponse::Create();
      data2_.response->SetMimeType("text/html");
      data2_.response->SetStatus(200);
      data2_.response->SetStatusText("OK");

      data2_.response_data =
          "<html><head>"
          "<script>"
          "window.testQuery({request:document.cookie});"
          "</script>"
          "</head><body>COOKIE RESTART TEST2</body></html>";

      handler->AddResponse(GetCookieAccessUrl2(scheme_, true), &data2_);
    }
  }

  void RunTestSetupContinue() {
    CefPostTask(
        TID_UI,
        base::BindOnce(
            &CookieRestartTestHandler::StartServer, this,
            base::BindOnce(&CookieRestartTestHandler::RunTestContinue, this)));
  }

  void StartServer(base::OnceClosure complete_callback) {
    EXPECT_FALSE(server_handler_);

    server_handler_ = new CookieAccessServerHandler();
    AddResponses(server_handler_);
    // 2 requests for each URL.
    server_handler_->SetExpectedRequestCount(4);
    server_handler_->CreateServer(std::move(complete_callback));
  }

  void RunTestContinue() {
    if (!CefCurrentlyOn(TID_UI)) {
      CefPostTask(
          TID_UI,
          base::BindOnce(&CookieRestartTestHandler::RunTestContinue, this));
      return;
    }

    CreateBrowser(GetCookieAccessUrl1(scheme_, true), context_);
  }

  void FinishTest() {
    // Verify that cookies were set correctly.
    class TestVisitor : public CefCookieVisitor {
     public:
      explicit TestVisitor(CookieRestartTestHandler* handler)
          : handler_(handler) {}
      ~TestVisitor() override {
        // Destroy the test.
        CefPostTask(TID_UI,
                    base::BindOnce(
                        &CookieRestartTestHandler::ShutdownServer, handler_,
                        base::BindOnce(&CookieRestartTestHandler::DestroyTest,
                                       handler_)));
      }

      bool Visit(const CefCookie& cookie,
                 int count,
                 int total,
                 bool& deleteCookie) override {
        const std::string& name = CefString(&cookie.name);
        const std::string& value = CefString(&cookie.value);
        if (name == "name_js" && value == "value_js")
          handler_->cookie_manager_js_ct_++;
        else if (name == "name_net" && value == "value_net")
          handler_->cookie_manager_net_ct_++;

        // Clean up the cookies.
        deleteCookie = true;

        return true;
      }

     private:
      CookieRestartTestHandler* handler_;
      IMPLEMENT_REFCOUNTING(TestVisitor);
    };

    cookie_manager_->VisitAllCookies(new TestVisitor(this));
  }

  void ShutdownServer(base::OnceClosure complete_callback) {
    EXPECT_TRUE(server_handler_);

    // |server_handler_| will delete itself after shutdown.
    server_handler_->ShutdownServer(std::move(complete_callback));
    server_handler_ = nullptr;
  }

  const std::string scheme_;
  const bool use_global_;
  CefRefPtr<CefRequestContext> context_;
  CefRefPtr<CefCookieManager> cookie_manager_;

  CookieAccessServerHandler* server_handler_ = nullptr;

  CookieAccessData data1_;
  CookieAccessData data2_;

  int before_resource_load_ct_ = 0;
  int resource_response_ct_ = 0;
  int query_ct_ = 0;

  // From network requests.
  int can_save_cookie_ct_ = 0;
  int can_send_cookie_ct_ = 0;
  int cookie_js1_ct_ = 0;
  int cookie_net1_ct_ = 0;
  int cookie_js2_ct_ = 0;
  int cookie_net2_ct_ = 0;

  // From cookie manager.
  int cookie_manager_js_ct_ = 0;
  int cookie_manager_net_ct_ = 0;

  DISALLOW_COPY_AND_ASSIGN(CookieRestartTestHandler);
  IMPLEMENT_REFCOUNTING(CookieRestartTestHandler);
};

}  // namespace

TEST(CookieTest, RestartGlobal) {
  CefRefPtr<CookieRestartTestHandler> handler =
      new CookieRestartTestHandler(true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(CookieTest, RestartInMemory) {
  CefRefPtr<CookieRestartTestHandler> handler =
      new CookieRestartTestHandler(false);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Entry point for registering custom schemes.
// Called from client_app_delegates.cc.
void RegisterCookieCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar) {
  // Used by GetCookieManagerCustom* tests.
  registrar->AddCustomScheme(
      kCustomCookieScheme,
      CEF_SCHEME_OPTION_STANDARD | CEF_SCHEME_OPTION_CORS_ENABLED);
}

// Entry point for registering cookieable schemes.
// Called from client_app_delegates.cc.
void RegisterCookieCookieableSchemes(
    std::vector<std::string>& cookieable_schemes) {
  // Used by GetCookieManagerCustom* tests.
  cookieable_schemes.push_back(kCustomCookieScheme);
}
