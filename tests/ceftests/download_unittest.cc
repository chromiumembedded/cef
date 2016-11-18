// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef_scheme.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_scoped_temp_dir.h"
#include "tests/ceftests/file_util.h"
#include "tests/ceftests/test_handler.h"
#include "tests/gtest/include/gtest/gtest.h"

namespace {

const char kTestDomain[] = "test-download.com";
const char kTestEntryUrl[] = "http://test-download.com/test.html";
const char kTestDownloadUrl[] = "http://test-download.com/download.txt";
const char kTestNavUrl[] = "http://test-download-nav.com/nav.html";
const char kTestFileName[] = "download_test.txt";
const char kTestContentDisposition[] =
    "attachment; filename=\"download_test.txt\"";
const char kTestMimeType[] = "text/plain";
const char kTestContent[] = "Download test text";

typedef base::Callback<void(CefRefPtr<CefCallback>/*callback*/)> DelayCallback;

class DownloadSchemeHandler : public CefResourceHandler {
 public:
  DownloadSchemeHandler(const DelayCallback& delay_callback,
                        TrackCallback* got_download_request)
      : delay_callback_(delay_callback),
        got_download_request_(got_download_request),
        should_delay_(false),
        offset_(0) {}

  bool ProcessRequest(CefRefPtr<CefRequest> request,
                      CefRefPtr<CefCallback> callback) override {
    std::string url = request->GetURL();
    if (url == kTestEntryUrl) {
      content_ = "<html><body>Download Test</body></html>";
      mime_type_ = "text/html";
    } else if (url == kTestDownloadUrl) {
      got_download_request_->yes();
      content_ = kTestContent;
      mime_type_ = kTestMimeType;
      content_disposition_ = kTestContentDisposition;
      should_delay_ = true;
    } else {
      EXPECT_TRUE(false); // Not reached.
      return false;
    }
    
    callback->Continue();
    return true;
  }

  void GetResponseHeaders(CefRefPtr<CefResponse> response,
                          int64& response_length,
                          CefString& redirectUrl) override {
    response_length = content_.size();

    response->SetStatus(200);
    response->SetMimeType(mime_type_);

    if (!content_disposition_.empty()) {
      CefResponse::HeaderMap headerMap;
      response->GetHeaderMap(headerMap);
      headerMap.insert(
          std::make_pair("Content-Disposition", content_disposition_));
      response->SetHeaderMap(headerMap);
    }
  }

  bool ReadResponse(void* data_out,
                    int bytes_to_read,
                    int& bytes_read,
                    CefRefPtr<CefCallback> callback) override {
    bytes_read = 0;

    if (should_delay_ && !delay_callback_.is_null()) {
      // Delay the download response a single time.
      delay_callback_.Run(callback);
      delay_callback_.Reset();
      return true;
    }

    bool has_data = false;
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

  void Cancel() override {
  }

 private:
  DelayCallback delay_callback_;
  TrackCallback* got_download_request_;
  bool should_delay_;
  std::string content_;
  std::string mime_type_;
  std::string content_disposition_;
  size_t offset_;

  IMPLEMENT_REFCOUNTING(DownloadSchemeHandler);
};

class DownloadSchemeHandlerFactory : public CefSchemeHandlerFactory {
 public:
  DownloadSchemeHandlerFactory(const DelayCallback& delay_callback,
                               TrackCallback* got_download_request)
    : delay_callback_(delay_callback),
      got_download_request_(got_download_request) {}

  CefRefPtr<CefResourceHandler> Create(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      const CefString& scheme_name,
      CefRefPtr<CefRequest> request) override {
    return new DownloadSchemeHandler(delay_callback_, got_download_request_);
  }

 private:
  DelayCallback delay_callback_;
  TrackCallback* got_download_request_;

  IMPLEMENT_REFCOUNTING(DownloadSchemeHandlerFactory);
};

class DownloadTestHandler : public TestHandler {
 public:
  enum TestMode {
    NORMAL,
    NAVIGATED,
    PENDING,
  };

  DownloadTestHandler(TestMode test_mode)
      : test_mode_(test_mode) {}

  void RunTest() override {
    DelayCallback delay_callback;
    if (test_mode_ == NAVIGATED || test_mode_ == PENDING)
      delay_callback = base::Bind(&DownloadTestHandler::OnDelayCallback, this);

    CefRegisterSchemeHandlerFactory("http", kTestDomain,
        new DownloadSchemeHandlerFactory(delay_callback,
                                         &got_download_request_));

    // Create a new temporary directory.
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    test_path_ = file_util::JoinPath(temp_dir_.GetPath(), kTestFileName);

    if (test_mode_ == NAVIGATED) {
      // Add the resource that we'll navigate to.
      AddResource(kTestNavUrl, "<html><body>Navigated</body></html>",
                  "text/html");
    }

    // Create the browser
    CreateBrowser(kTestEntryUrl);

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    const std::string& url = frame->GetURL().ToString();
    if (url == kTestEntryUrl) {
      // Begin the download.
      browser->GetHost()->StartDownload(kTestDownloadUrl);
    } else if (url == kTestNavUrl) {
      got_nav_load_.yes();
      ContinueNavigatedIfReady();
    }
  }

  // Callback from the scheme handler when the download request is delayed.
  void OnDelayCallback(CefRefPtr<CefCallback> callback) {
    if (!CefCurrentlyOn(TID_UI)) {
      CefPostTask(TID_UI,
          base::Bind(&DownloadTestHandler::OnDelayCallback, this, callback));
      return;
    }

    got_delay_callback_.yes();

    if (test_mode_ == NAVIGATED) {
      delay_callback_ = callback;
      ContinueNavigatedIfReady();
    } else if (test_mode_ == PENDING) {
      ContinuePendingIfReady();
    } else {
      EXPECT_TRUE(false);  // Not reached.
    }
  }

  void ContinueNavigatedIfReady() {
    DCHECK_EQ(test_mode_, NAVIGATED);
    if (got_delay_callback_ && got_nav_load_) {
      delay_callback_->Continue();
      delay_callback_ = nullptr;
    }
  }

  void ContinuePendingIfReady() {
    DCHECK_EQ(test_mode_, PENDING);
    if (got_delay_callback_ && got_on_before_download_ &&
        got_on_download_updated_) {
      // Destroy the test without waiting for the download to complete.
      DestroyTest();
    }
  }

  void OnBeforeDownload(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefDownloadItem> download_item,
      const CefString& suggested_name,
      CefRefPtr<CefBeforeDownloadCallback> callback) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_UI));
    EXPECT_FALSE(got_on_before_download_);

    got_on_before_download_.yes();

    EXPECT_TRUE(browser->IsSame(GetBrowser()));
    EXPECT_STREQ(kTestFileName, suggested_name.ToString().c_str());
    EXPECT_TRUE(download_item.get());
    EXPECT_TRUE(callback.get());

    download_id_ = download_item->GetId();
    EXPECT_LT(0U, download_id_);
    
    EXPECT_TRUE(download_item->IsValid());
    EXPECT_TRUE(download_item->IsInProgress());
    EXPECT_FALSE(download_item->IsComplete());
    EXPECT_FALSE(download_item->IsCanceled());
    EXPECT_EQ(static_cast<int64>(sizeof(kTestContent)-1),
        download_item->GetTotalBytes());
    EXPECT_EQ(0UL, download_item->GetFullPath().length());
    EXPECT_STREQ(kTestDownloadUrl, download_item->GetURL().ToString().c_str());
    EXPECT_EQ(0UL, download_item->GetSuggestedFileName().length());
    EXPECT_STREQ(kTestContentDisposition,
        download_item->GetContentDisposition().ToString().c_str());
    EXPECT_STREQ(kTestMimeType, download_item->GetMimeType().ToString().c_str());

    callback->Continue(test_path_, false);

    if (test_mode_ == NAVIGATED)
      browser->GetMainFrame()->LoadURL(kTestNavUrl);
    else if (test_mode_ == PENDING)
      ContinuePendingIfReady();
  }

  void OnDownloadUpdated(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefDownloadItem> download_item,
      CefRefPtr<CefDownloadItemCallback> callback) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_UI));

    got_on_download_updated_.yes();

    EXPECT_TRUE(browser->IsSame(GetBrowser()));
    EXPECT_TRUE(download_item.get());
    EXPECT_TRUE(callback.get());

    if (got_on_before_download_)
      EXPECT_EQ(download_id_, download_item->GetId());

    EXPECT_LE(0LL, download_item->GetCurrentSpeed());
    EXPECT_LE(0, download_item->GetPercentComplete());

    EXPECT_TRUE(download_item->IsValid());
    EXPECT_FALSE(download_item->IsCanceled());
    EXPECT_STREQ(kTestDownloadUrl, download_item->GetURL().ToString().c_str());
    EXPECT_STREQ(kTestContentDisposition,
        download_item->GetContentDisposition().ToString().c_str());
    EXPECT_STREQ(kTestMimeType,
        download_item->GetMimeType().ToString().c_str());

    std::string full_path = download_item->GetFullPath();
    if (!full_path.empty()) {
      got_full_path_.yes();
      EXPECT_STREQ(test_path_.c_str(), full_path.c_str());
    }

    if (download_item->IsComplete()) {
      got_download_complete_.yes();

      EXPECT_FALSE(download_item->IsInProgress());
      EXPECT_EQ(100, download_item->GetPercentComplete());
      EXPECT_EQ(static_cast<int64>(sizeof(kTestContent)-1),
          download_item->GetReceivedBytes());
      EXPECT_EQ(static_cast<int64>(sizeof(kTestContent)-1),
          download_item->GetTotalBytes());

      DestroyTest();
    } else {
      EXPECT_TRUE(download_item->IsInProgress());
      EXPECT_LE(0LL, download_item->GetReceivedBytes());
    }

    if (test_mode_ == PENDING)
      ContinuePendingIfReady();
  }

  void VerifyResultsOnFileThread() {
    EXPECT_TRUE(CefCurrentlyOn(TID_FILE));

    if (test_mode_ != PENDING) {
      // Verify the file contents.
      std::string contents;
      EXPECT_TRUE(file_util::ReadFileToString(test_path_, &contents));
      EXPECT_STREQ(kTestContent, contents.c_str());
    }

    EXPECT_TRUE(temp_dir_.Delete());
    EXPECT_TRUE(temp_dir_.IsEmpty());

    CefPostTask(TID_UI, base::Bind(&DownloadTestHandler::DestroyTest, this));
  }

  void DestroyTest() override {
    if (!temp_dir_.IsEmpty()) {
      // Clean up temp_dir_ on the FILE thread before destroying the test.
      CefPostTask(TID_FILE,
          base::Bind(&DownloadTestHandler::VerifyResultsOnFileThread, this));
      return;
    }

    CefRegisterSchemeHandlerFactory("http", kTestDomain, NULL);

    EXPECT_TRUE(got_download_request_);
    EXPECT_TRUE(got_on_before_download_);
    EXPECT_TRUE(got_on_download_updated_);

    if (test_mode_ == NAVIGATED)
      EXPECT_TRUE(got_nav_load_);
    else
      EXPECT_FALSE(got_nav_load_);

    if (test_mode_ == PENDING) {
      EXPECT_FALSE(got_download_complete_);
      EXPECT_FALSE(got_full_path_);
    } else {
      EXPECT_TRUE(got_download_complete_);
      EXPECT_TRUE(got_full_path_);
    }

    TestHandler::DestroyTest();
  }

 private:
  const TestMode test_mode_;

  // Used with NAVIGATED test mode.
  CefRefPtr<CefCallback> delay_callback_;

  CefScopedTempDir temp_dir_;
  std::string test_path_;
  uint32 download_id_;

  TrackCallback got_download_request_;
  TrackCallback got_on_before_download_;
  TrackCallback got_on_download_updated_;
  TrackCallback got_full_path_;
  TrackCallback got_download_complete_;
  TrackCallback got_delay_callback_;
  TrackCallback got_nav_load_;

  IMPLEMENT_REFCOUNTING(DownloadTestHandler);
};

}  // namespace

// Test a basic download.
TEST(DownloadTest, Download) {
  CefRefPtr<DownloadTestHandler> handler =
      new DownloadTestHandler(DownloadTestHandler::NORMAL);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Test where the download completes after cross-origin navigation.
TEST(DownloadTest, DownloadNavigated) {
  CefRefPtr<DownloadTestHandler> handler =
      new DownloadTestHandler(DownloadTestHandler::NAVIGATED);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Test where the download is still pending when the browser is destroyed.
TEST(DownloadTest, DownloadPending) {
  CefRefPtr<DownloadTestHandler> handler =
      new DownloadTestHandler(DownloadTestHandler::PENDING);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}
