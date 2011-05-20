// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "test_handler.h"

namespace {

class TestResults
{
public:
  TestResults()
  {
  }

  void reset()
  {
    url.clear();
    html.clear();
    status_code = 0;
    got_request.reset();
    got_read.reset();
    got_output.reset();
    got_redirect.reset();
  }

  std::string url;
  std::string html;
  int status_code;
  std::string redirect_url;

  TrackCallback 
    got_request,
    got_read,
    got_output,
    got_redirect;
};

class TestSchemeHandler : public TestHandler
{
public:
  TestSchemeHandler(TestResults* tr)
    : test_results_(tr)
  {
  }

  virtual void RunTest() OVERRIDE
  {
    CreateBrowser(test_results_->url);
  }

  virtual bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                              CefRefPtr<CefFrame> frame,
                              CefRefPtr<CefRequest> request,
                              NavType navType,
                              bool isRedirect) OVERRIDE
  {
    if (isRedirect) {
      test_results_->got_redirect.yes();
      std::string newUrl = request->GetURL();
      EXPECT_EQ(newUrl, test_results_->redirect_url);

      // No read should have occurred for the redirect.
      EXPECT_TRUE(test_results_->got_request);
      EXPECT_FALSE(test_results_->got_read);

      // Now loading the redirect URL.
      test_results_->url = test_results_->redirect_url;
      test_results_->redirect_url.clear();
    }
    
    return false;
  }

  virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         int httpStatusCode) OVERRIDE
  {
    // Test that the output is correct.
    std::string output = frame->GetSource();
    if (output == test_results_->html)
      test_results_->got_output.yes();

    // Test that the status code is correct.
    EXPECT_EQ(httpStatusCode, test_results_->status_code);

    DestroyTest();
  }

protected:
  TestResults* test_results_;
};

class ClientSchemeHandler : public CefSchemeHandler
{
public:
  ClientSchemeHandler(TestResults* tr)
    : test_results_(tr), offset_(0) {}

  virtual bool ProcessRequest(CefRefPtr<CefRequest> request, 
                              CefString& redirectUrl,
                              CefRefPtr<CefResponse> response, 
                              int* response_length)
  {
    EXPECT_TRUE(CefCurrentlyOn(TID_IO));

    test_results_->got_request.yes();

    std::string url = request->GetURL();
    EXPECT_EQ(url, test_results_->url);

    response->SetStatus(test_results_->status_code);

    if (!test_results_->redirect_url.empty()) {
      redirectUrl = test_results_->redirect_url;
      return true;
    } else if (!test_results_->html.empty()) {
      response->SetMimeType("text/html");
      *response_length = test_results_->html.size();
      return true;
    }

    return false;
  }

  virtual void Cancel()
  {
    EXPECT_TRUE(CefCurrentlyOn(TID_IO));
  }

  virtual bool ReadResponse(void* data_out, int bytes_to_read, int* bytes_read)
  {
    EXPECT_TRUE(CefCurrentlyOn(TID_IO));

    test_results_->got_read.yes();

    bool has_data = false;
    *bytes_read = 0;

    AutoLock lock_scope(this);

    size_t size = test_results_->html.size();
    if(offset_ < size) {
      int transfer_size =
          std::min(bytes_to_read, static_cast<int>(size - offset_));
      memcpy(data_out, test_results_->html.c_str() + offset_, transfer_size);
      offset_ += transfer_size;

      *bytes_read = transfer_size;
      has_data = true;
    }

    return has_data;
  }

private:
  TestResults* test_results_;
  size_t offset_;

  IMPLEMENT_REFCOUNTING(ClientSchemeHandler);
  IMPLEMENT_LOCKING(ClientSchemeHandler);
};

class ClientSchemeHandlerFactory : public CefSchemeHandlerFactory
{
public:
  ClientSchemeHandlerFactory(TestResults* tr)
    : test_results_(tr){}

  virtual CefRefPtr<CefSchemeHandler> Create()
  {
    EXPECT_TRUE(CefCurrentlyOn(TID_IO));
    return new ClientSchemeHandler(test_results_);
  }

  TestResults* test_results_;

  IMPLEMENT_REFCOUNTING(ClientSchemeHandlerFactory);
};

// Global test results object.
TestResults g_TestResults;

void CreateStandardTestScheme()
{
  g_TestResults.reset();
  static bool registered = false;
  if (!registered) {
    CefRegisterScheme("stdscheme", "tests", true,
                      new ClientSchemeHandlerFactory(&g_TestResults));
    registered = true;
  }
}

void CreateNonStandardTestScheme()
{
  g_TestResults.reset();
  static bool registered = false;
  if (!registered) {
    CefRegisterScheme("nonstdscheme", CefString(), false,
                      new ClientSchemeHandlerFactory(&g_TestResults));
    registered = true;
  }
}

} // anonymous

// Test that a standard scheme can return normal results.
TEST(SchemeHandlerTest, StandardSchemeNormalResponse)
{
  CreateStandardTestScheme();
  g_TestResults.url = "stdscheme://tests/run.html";
  g_TestResults.html =
      "<html><head></head><body><h1>Success!</h1></body></html>";
  g_TestResults.status_code = 200;

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
}

// Test that a standard scheme can return an error code.
TEST(SchemeHandlerTest, StandardSchemeErrorResponse)
{
  CreateStandardTestScheme();
  g_TestResults.url = "stdscheme://tests/run.html";
  g_TestResults.html =
      "<html><head></head><body><h1>404</h1></body></html>";
  g_TestResults.status_code = 404;
  
  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
}

// Test that standard scheme handling fails when the scheme name is incorrect.
TEST(SchemeHandlerTest, StandardSchemeNameNotHandled)
{
  CreateStandardTestScheme();
  g_TestResults.url = "stdscheme2://tests/run.html";
  
  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();

  EXPECT_FALSE(g_TestResults.got_request);
  EXPECT_FALSE(g_TestResults.got_read);
  EXPECT_FALSE(g_TestResults.got_output);
}

// Test that standard scheme handling fails when the domain name is incorrect.
TEST(SchemeHandlerTest, StandardSchemeDomainNotHandled)
{
  CreateStandardTestScheme();
  g_TestResults.url = "stdscheme://tests2/run.html";
  
  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();

  EXPECT_FALSE(g_TestResults.got_request);
  EXPECT_FALSE(g_TestResults.got_read);
  EXPECT_FALSE(g_TestResults.got_output);
}

// Test that a standard scheme can return no response.
TEST(SchemeHandlerTest, StandardSchemeNoResponse)
{
  CreateStandardTestScheme();
  g_TestResults.url = "stdscheme://tests/run.html";
  
  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_FALSE(g_TestResults.got_read);
  EXPECT_FALSE(g_TestResults.got_output);
}

// Test that a standard scheme can generate redirects.
TEST(SchemeHandlerTest, StandardSchemeRedirect)
{
  CreateStandardTestScheme();
  g_TestResults.url = "stdscheme://tests/run.html";
  g_TestResults.redirect_url = "stdscheme://tests/redirect.html";
  g_TestResults.html =
      "<html><head></head><body><h1>Redirected</h1></body></html>";
  
  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_redirect);
}

// Test that a non-standard scheme can return normal results.
TEST(SchemeHandlerTest, NonStandardSchemeNormalResponse)
{
  CreateNonStandardTestScheme();
  g_TestResults.url = "nonstdscheme:some%20value";
  g_TestResults.html =
      "<html><head></head><body><h1>Success!</h1></body></html>";
  g_TestResults.status_code = 200;

  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
}

// Test that a non-standard scheme can return an error code.
TEST(SchemeHandlerTest, NonStandardSchemeErrorResponse)
{
  CreateNonStandardTestScheme();
  g_TestResults.url = "nonstdscheme:some%20value";
  g_TestResults.html =
      "<html><head></head><body><h1>404</h1></body></html>";
  g_TestResults.status_code = 404;
  
  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
}

// Test that non-standard scheme handling fails when the scheme name is
// incorrect.
TEST(SchemeHandlerTest, NonStandardSchemeNameNotHandled)
{
  CreateNonStandardTestScheme();
  g_TestResults.url = "nonstdscheme2:some%20value";
  
  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();

  EXPECT_FALSE(g_TestResults.got_request);
  EXPECT_FALSE(g_TestResults.got_read);
  EXPECT_FALSE(g_TestResults.got_output);
}

// Test that a non-standard scheme can return no response.
TEST(SchemeHandlerTest, NonStandardSchemeNoResponse)
{
  CreateNonStandardTestScheme();
  g_TestResults.url = "nonstdscheme:some%20value";
  
  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_FALSE(g_TestResults.got_read);
  EXPECT_FALSE(g_TestResults.got_output);
}

// Test that a non-standard scheme can generate redirects.
TEST(SchemeHandlerTest, NonStandardSchemeRedirect)
{
  CreateNonStandardTestScheme();
  g_TestResults.url = "nonstdscheme:some%20value";
  g_TestResults.redirect_url = "nonstdscheme:some%20other%20value";
  g_TestResults.html =
      "<html><head></head><body><h1>Redirected</h1></body></html>";
  
  CefRefPtr<TestSchemeHandler> handler = new TestSchemeHandler(&g_TestResults);
  handler->ExecuteTest();

  EXPECT_TRUE(g_TestResults.got_request);
  EXPECT_TRUE(g_TestResults.got_read);
  EXPECT_TRUE(g_TestResults.got_output);
  EXPECT_TRUE(g_TestResults.got_redirect);
}
