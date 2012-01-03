// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "base/scoped_temp_dir.h"
#include "base/synchronization/waitable_event.h"
#include "include/cef.h"
#include "include/cef_runnable.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "test_suite.h"
#include <vector>

namespace {

const char* kTestUrl = "http://www.test.com/path/to/cookietest/foo.html";
const char* kTestDomain = "www.test.com";
const char* kTestPath = "/path/to/cookietest";

typedef std::vector<CefCookie> CookieVector;

void IOT_Set(const CefString& url, CookieVector* cookies,
             base::WaitableEvent* event)
{
  CookieVector::const_iterator it = cookies->begin();
  for (; it != cookies->end(); ++it)
    EXPECT_TRUE(CefSetCookie(url, *it));
  event->Signal();
}

void IOT_Delete(const CefString& url, const CefString& cookie_name,
                base::WaitableEvent* event)
{
  EXPECT_TRUE(CefDeleteCookies(url, cookie_name));
  event->Signal();
}

class TestVisitor : public CefCookieVisitor
{
public:
  TestVisitor(CookieVector* cookies, bool deleteCookies,
              base::WaitableEvent* event)
    : cookies_(cookies), delete_cookies_(deleteCookies), event_(event)
  {
  }
  virtual ~TestVisitor()
  {
    event_->Signal();
  }

  virtual bool Visit(const CefCookie& cookie, int count, int total,
                     bool& deleteCookie)
  {
    cookies_->push_back(cookie);
    if (delete_cookies_)
      deleteCookie = true;
    return true;
  }

  CookieVector* cookies_;
  bool delete_cookies_;
  base::WaitableEvent* event_;

  IMPLEMENT_REFCOUNTING(TestVisitor);
};

// Create a test cookie. If |withDomain| is true a domain cookie will be
// created, otherwise a host cookie will be created.
void CreateCookie(CefCookie& cookie, bool withDomain,
                 base::WaitableEvent& event)
{
  CefString(&cookie.name).FromASCII("my_cookie");
  CefString(&cookie.value).FromASCII("My Value");
  if (withDomain)
    CefString(&cookie.domain).FromASCII(kTestDomain);
  CefString(&cookie.path).FromASCII(kTestPath);
  cookie.has_expires = true;
  cookie.expires.year = 2200;
  cookie.expires.month = 4;
#if !defined(OS_MACOSX)
  cookie.expires.day_of_week = 5;
#endif
  cookie.expires.day_of_month = 11;

  CookieVector cookies;
  cookies.push_back(cookie);
  
  // Set the cookie.
  CefPostTask(TID_IO, NewCefRunnableFunction(IOT_Set, kTestUrl, &cookies,
                                             &event));
  event.Wait();
}

// Retrieve the test cookie. If |withDomain| is true check that the cookie
// is a domain cookie, otherwise a host cookie. if |deleteCookies| is true
// the cookie will be deleted when it's retrieved.
void GetCookie(const CefCookie& cookie, bool withDomain,
               base::WaitableEvent& event, bool deleteCookies)
{
  CookieVector cookies;
  
  // Get the cookie and delete it.
  EXPECT_TRUE(CefVisitUrlCookies(kTestUrl, false,
      new TestVisitor(&cookies, deleteCookies, &event)));
  event.Wait();

  EXPECT_EQ((CookieVector::size_type)1, cookies.size());
  
  const CefCookie& cookie_read = cookies[0];
  EXPECT_EQ(CefString(&cookie_read.name), "my_cookie");
  EXPECT_EQ(CefString(&cookie_read.value), "My Value");
  if (withDomain)
    EXPECT_EQ(CefString(&cookie_read.domain), ".www.test.com");
  else
    EXPECT_EQ(CefString(&cookie_read.domain), kTestDomain);
  EXPECT_EQ(CefString(&cookie_read.path), kTestPath);
  EXPECT_TRUE(cookie_read.has_expires);
  EXPECT_EQ(cookie.expires.year, cookie_read.expires.year);
  EXPECT_EQ(cookie.expires.month, cookie_read.expires.month);
#if !defined(OS_MACOSX)
  EXPECT_EQ(cookie.expires.day_of_week, cookie_read.expires.day_of_week);
#endif
  EXPECT_EQ(cookie.expires.day_of_month, cookie_read.expires.day_of_month);
  EXPECT_EQ(cookie.expires.hour, cookie_read.expires.hour);
  EXPECT_EQ(cookie.expires.minute, cookie_read.expires.minute);
  EXPECT_EQ(cookie.expires.second, cookie_read.expires.second);
  EXPECT_EQ(cookie.expires.millisecond, cookie_read.expires.millisecond);
}

// Verify that no cookies exist. If |withUrl| is true it will only check for
// cookies matching the URL.
void VerifyNoCookies(base::WaitableEvent& event, bool withUrl)
{
  CookieVector cookies;

  // Verify that the cookie has been deleted.
  if (withUrl) {
    EXPECT_TRUE(CefVisitUrlCookies(kTestUrl, false,
        new TestVisitor(&cookies, false, &event)));
  } else {
    EXPECT_TRUE(CefVisitAllCookies(new TestVisitor(&cookies, false, &event)));
  }
  event.Wait();

  EXPECT_EQ((CookieVector::size_type)0, cookies.size());
}

// Delete all system cookies.
void DeleteAllCookies(base::WaitableEvent& event)
{
  CefPostTask(TID_IO, NewCefRunnableFunction(IOT_Delete, CefString(),
                                             CefString(), &event));
  event.Wait();
}

} // anonymous

// Test creation of a domain cookie.
TEST(CookieTest, DomainCookie)
{
  base::WaitableEvent event(false, false);
  CefCookie cookie;

  // Create a domain cookie.
  CreateCookie(cookie, true, event);

  // Retrieve, verify and delete the domain cookie.
  GetCookie(cookie, true, event, true);

  // Verify that the cookie was deleted.
  VerifyNoCookies(event, true);
}

// Test creation of a host cookie.
TEST(CookieTest, HostCookie)
{
  base::WaitableEvent event(false, false);
  CefCookie cookie;

  // Create a host cookie.
  CreateCookie(cookie, false, event);

  // Retrieve, verify and delete the host cookie.
  GetCookie(cookie, false, event, true);

  // Verify that the cookie was deleted.
  VerifyNoCookies(event, true);
}

// Test creation of multiple cookies.
TEST(CookieTest, MultipleCookies)
{
  base::WaitableEvent event(false, false);
  std::stringstream ss;
  int i;

  CookieVector cookies;

  const int kNumCookies = 4;
  
  // Create the cookies.
  for(i = 0; i < kNumCookies; i++) {
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
  CefPostTask(TID_IO, NewCefRunnableFunction(IOT_Set, kTestUrl, &cookies,
                                             &event));
  event.Wait();
  cookies.clear();

  // Get the cookies without deleting them.
  EXPECT_TRUE(CefVisitUrlCookies(kTestUrl, false,
      new TestVisitor(&cookies, false, &event)));
  event.Wait();

  EXPECT_EQ((CookieVector::size_type)kNumCookies, cookies.size());

  CookieVector::const_iterator it = cookies.begin();
  for(i = 0; it != cookies.end(); ++it, ++i) {
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
  CefPostTask(TID_IO, NewCefRunnableFunction(IOT_Delete, kTestUrl,
                                             CefString("my_cookie1"), &event));
  event.Wait();
  
  // Verify that the cookie has been deleted.
  EXPECT_TRUE(CefVisitUrlCookies(kTestUrl, false,
      new TestVisitor(&cookies, false, &event)));
  event.Wait();

  EXPECT_EQ((CookieVector::size_type)3, cookies.size());
  EXPECT_EQ(CefString(&cookies[0].name), "my_cookie0");
  EXPECT_EQ(CefString(&cookies[1].name), "my_cookie2");
  EXPECT_EQ(CefString(&cookies[2].name), "my_cookie3");

  cookies.clear();

  // Delete the rest of the cookies.
  CefPostTask(TID_IO, NewCefRunnableFunction(IOT_Delete, kTestUrl,
                                             CefString(), &event));
  event.Wait();
  
  // Verify that the cookies have been deleted.
  EXPECT_TRUE(CefVisitUrlCookies(kTestUrl, false,
      new TestVisitor(&cookies, false, &event)));
  event.Wait();

  EXPECT_EQ((CookieVector::size_type)0, cookies.size());

  // Create the cookies.
  for(i = 0; i < kNumCookies; i++) {
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
  EXPECT_TRUE(CefVisitUrlCookies(kTestUrl, false,
      new TestVisitor(&cookies, true, &event)));
  event.Wait();

  cookies.clear();

  // Verify that the cookies have been deleted.
  EXPECT_TRUE(CefVisitUrlCookies(kTestUrl, false,
      new TestVisitor(&cookies, false, &event)));
  event.Wait();

  EXPECT_EQ((CookieVector::size_type)0, cookies.size());
}

TEST(CookieTest, AllCookies)
{
  base::WaitableEvent event(false, false);
  CookieVector cookies;
  
  // Delete all system cookies just in case something is left over from a
  // different test.
  CefPostTask(TID_IO, NewCefRunnableFunction(IOT_Delete, CefString(),
                                             CefString(), &event));
  event.Wait();

  // Verify that all system cookies have been deleted.
  EXPECT_TRUE(CefVisitAllCookies(new TestVisitor(&cookies, false, &event)));
  event.Wait();

  EXPECT_EQ((CookieVector::size_type)0, cookies.size());

  // Create cookies with 2 separate hosts.
  CefCookie cookie1;
  const char* kUrl1 = "http://www.foo.com";
  CefString(&cookie1.name).FromASCII("my_cookie1");
  CefString(&cookie1.value).FromASCII("My Value 1");

  cookies.push_back(cookie1);
  CefPostTask(TID_IO, NewCefRunnableFunction(IOT_Set, kUrl1, &cookies, &event));
  event.Wait();
  cookies.clear();

  CefCookie cookie2;
  const char* kUrl2 = "http://www.bar.com";
  CefString(&cookie2.name).FromASCII("my_cookie2");
  CefString(&cookie2.value).FromASCII("My Value 2");

  cookies.push_back(cookie2);
  CefPostTask(TID_IO, NewCefRunnableFunction(IOT_Set, kUrl2, &cookies, &event));
  event.Wait();
  cookies.clear();

  // Verify that all system cookies can be retrieved.
  EXPECT_TRUE(CefVisitAllCookies(new TestVisitor(&cookies, false, &event)));
  event.Wait();

  EXPECT_EQ((CookieVector::size_type)2, cookies.size());
  EXPECT_EQ(CefString(&cookies[0].name), "my_cookie1");
  EXPECT_EQ(CefString(&cookies[0].value), "My Value 1");
  EXPECT_EQ(CefString(&cookies[0].domain), "www.foo.com");
  EXPECT_EQ(CefString(&cookies[1].name), "my_cookie2");
  EXPECT_EQ(CefString(&cookies[1].value), "My Value 2");
  EXPECT_EQ(CefString(&cookies[1].domain), "www.bar.com");
  cookies.clear();

  // Verify that the cookies can be retrieved separately.
  EXPECT_TRUE(CefVisitUrlCookies(kUrl1, false,
      new TestVisitor(&cookies, false, &event)));
  event.Wait();

  EXPECT_EQ((CookieVector::size_type)1, cookies.size());
  EXPECT_EQ(CefString(&cookies[0].name), "my_cookie1");
  EXPECT_EQ(CefString(&cookies[0].value), "My Value 1");
  EXPECT_EQ(CefString(&cookies[0].domain), "www.foo.com");
  cookies.clear();

  EXPECT_TRUE(CefVisitUrlCookies(kUrl2, false,
      new TestVisitor(&cookies, false, &event)));
  event.Wait();

  EXPECT_EQ((CookieVector::size_type)1, cookies.size());
  EXPECT_EQ(CefString(&cookies[0].name), "my_cookie2");
  EXPECT_EQ(CefString(&cookies[0].value), "My Value 2");
  EXPECT_EQ(CefString(&cookies[0].domain), "www.bar.com");
  cookies.clear();

  // Delete all of the system cookies.
  DeleteAllCookies(event);

  // Verify that all system cookies have been deleted.
  VerifyNoCookies(event, false);
}

TEST(CookieTest, ChangeDirectory)
{
  base::WaitableEvent event(false, false);
  CefCookie cookie;

  std::string cache_path;
  CefTestSuite::GetCachePath(cache_path);

  ScopedTempDir temp_dir;

  // Create a new temporary directory.
  EXPECT_TRUE(temp_dir.CreateUniqueTempDir());

  // Delete all of the system cookies.
  DeleteAllCookies(event);

  // Set the new temporary directory as the storage location.
  EXPECT_TRUE(CefSetCookiePath(temp_dir.path().value()));

  // Verify that no cookies exist.
  VerifyNoCookies(event, true);

  // Create a domain cookie.
  CreateCookie(cookie, true, event);

  // Retrieve and verify the domain cookie.
  GetCookie(cookie, true, event, false);

  // Restore the original storage location.
  EXPECT_TRUE(CefSetCookiePath(cache_path));

  // Verify that no cookies exist.
  VerifyNoCookies(event, true);

  // Set the new temporary directory as the storage location.
  EXPECT_TRUE(CefSetCookiePath(temp_dir.path().value()));

  // Retrieve and verify the domain cookie that was set previously.
  GetCookie(cookie, true, event, false);

  // Restore the original storage location.
  EXPECT_TRUE(CefSetCookiePath(cache_path));
}
