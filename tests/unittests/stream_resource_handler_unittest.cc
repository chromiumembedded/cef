// Copyright (c) 2014 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <algorithm>
#include <cstdlib>
#include <string>

// Include this first to avoid type conflicts with CEF headers.
#include "tests/unittests/chromium_includes.h"

#include "include/base/cef_bind.h"
#include "include/cef_stream.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_stream_resource_handler.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "tests/unittests/routing_test_handler.h"

// Comment out this define to disable the unit test timeout.
#define TIMEOUT_ENABLED 1

namespace {

const char kTestUrl[] = "http://tests-srh/test.html";
const size_t kReadBlockSize = 1024U;  // 1k.

// The usual network buffer size is about 32k. Choose a value that's larger.
const size_t kReadDesiredSize = 100U * 1024U;  // 100k

class ReadHandler : public CefReadHandler {
 public:
  explicit ReadHandler(bool may_block)
    : may_block_(may_block),
      offset_(0),
      expected_result_(0) {
  }

  void CreateContent() {
    // To verify that the data transers successfully we're going to make a big
    // math problem.
    content_.reserve(kReadDesiredSize + 50U);
    content_ = "<html><body><script>var myratherlongvariablename=0;";

    while (content_.size() < kReadDesiredSize) {
      content_ += "myratherlongvariablename=myratherlongvariablename+1;";
      expected_result_++;
    }

    content_ += "window.testQuery({request:myratherlongvariablename+''});"
                "</script></body></html>";
  }

  int GetExpectedResult() const {
    return expected_result_;
  }

  virtual size_t Read(void* ptr, size_t size, size_t n) OVERRIDE {
    EXPECT_EQ(1U, size);

    // Read the minimum of requested size, remaining size or kReadBlockSize.
    const size_t read_bytes =
        std::min(std::min(size * n, content_.size() - offset_), kReadBlockSize);
    if (read_bytes > 0) {
      memcpy(ptr, content_.c_str() + offset_, read_bytes);
      offset_ += read_bytes;
    }

    return read_bytes;
  }

  virtual int Seek(int64 offset, int whence) OVERRIDE {
    EXPECT_TRUE(false);  // Not reached.
    return 0;
  }

  virtual int64 Tell() OVERRIDE {
    EXPECT_TRUE(false);  // Not reached.
    return 0;
  }

  virtual int Eof() OVERRIDE {
    EXPECT_TRUE(false);  // Not reached.
    return 0;
  }

  virtual bool MayBlock() OVERRIDE {
    return may_block_;
  }

 private:
  const bool may_block_;
  std::string content_;
  size_t offset_;
  int expected_result_;

  IMPLEMENT_REFCOUNTING(ReadHandler);
};

class ReadTestHandler : public RoutingTestHandler {
 public:
  explicit ReadTestHandler(bool may_block)
      : may_block_(may_block),
        expected_result_(0) {}

  virtual void RunTest() OVERRIDE {
    // Create the browser.
    CreateBrowser(kTestUrl);

#if defined(TIMEOUT_ENABLED)
    // Time out the test after a reasonable period of time.
    CefPostDelayedTask(TID_UI,
        base::Bind(&ReadTestHandler::DestroyTest, this), 3000);
#endif
  }

  virtual CefRefPtr<CefResourceHandler> GetResourceHandler(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request) OVERRIDE {
    got_resource_handler_.yes();

    const std::string& url = request->GetURL();
    EXPECT_STREQ(kTestUrl, url.c_str());

    CefRefPtr<ReadHandler> handler = new ReadHandler(may_block_);
    handler->CreateContent();
    expected_result_ = handler->GetExpectedResult();

    CefRefPtr<CefStreamReader> stream =
        CefStreamReader::CreateForHandler(handler.get());
    return new CefStreamResourceHandler("text/html", stream);
  }

  virtual bool OnQuery(CefRefPtr<CefBrowser> browser,
                       CefRefPtr<CefFrame> frame,
                       int64 query_id,
                       const CefString& request,
                       bool persistent,
                       CefRefPtr<Callback> callback) OVERRIDE {
    got_on_query_.yes();

    const int actual_result = atoi(request.ToString().c_str());
    EXPECT_EQ(expected_result_, actual_result);

    DestroyTestIfDone();

    return true;
  }

  virtual void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                                    bool isLoading,
                                    bool canGoBack,
                                    bool canGoForward) OVERRIDE {
    if (!isLoading) {
      got_on_loading_state_change_done_.yes();
      DestroyTestIfDone();
    }
  }

 private:
  void DestroyTestIfDone() {
    if (got_on_query_ && got_on_loading_state_change_done_)
      DestroyTest();
  }

  virtual void DestroyTest() OVERRIDE {
    EXPECT_TRUE(got_resource_handler_);
    EXPECT_TRUE(got_on_query_);
    EXPECT_TRUE(got_on_loading_state_change_done_);
    RoutingTestHandler::DestroyTest();
  }

  const bool may_block_;

  int expected_result_;
  TrackCallback got_resource_handler_;
  TrackCallback got_on_query_;
  TrackCallback got_on_loading_state_change_done_;
};

}  // namespace

TEST(StreamResourceHandlerTest, ReadWillBlock) {
  CefRefPtr<ReadTestHandler> handler = new ReadTestHandler(true);
  handler->ExecuteTest();
}

TEST(StreamResourceHandlerTest, ReadWontBlock) {
  CefRefPtr<ReadTestHandler> handler = new ReadTestHandler(false);
  handler->ExecuteTest();
}
