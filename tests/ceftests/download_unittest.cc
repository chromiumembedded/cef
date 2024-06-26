// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <algorithm>
#include <memory>

#include "include/base/cef_callback.h"
#include "include/base/cef_callback_helpers.h"
#include "include/cef_scheme.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_scoped_temp_dir.h"
#include "tests/ceftests/test_handler.h"
#include "tests/ceftests/test_util.h"
#include "tests/gtest/include/gtest/gtest.h"
#include "tests/shared/browser/file_util.h"

namespace {

const char kTestDomain[] = "test-download.com";
const char kTestStartUrl[] = "https://test-download.com/test.html";
const char kTestDownloadUrl[] = "https://test-download.com/download.txt";
const char kTestNavUrl[] = "https://test-download-nav.com/nav.html";
const char kTestFileName[] = "download_test.txt";
const char kTestContentDisposition[] =
    "attachment; filename=\"download_test.txt\"";
const char kTestMimeType[] = "text/plain";
const char kTestContent[] = "Download test text";

using DelayCallback = base::OnceCallback<void(base::OnceClosure /*callback*/)>;

class DownloadSchemeHandler : public CefResourceHandler {
 public:
  DownloadSchemeHandler(DelayCallback delay_callback,
                        TrackCallback* got_download_request)
      : delay_callback_(std::move(delay_callback)),
        got_download_request_(got_download_request) {}

  bool Open(CefRefPtr<CefRequest> request,
            bool& handle_request,
            CefRefPtr<CefCallback> callback) override {
    EXPECT_FALSE(CefCurrentlyOn(TID_UI) || CefCurrentlyOn(TID_IO));

    std::string url = request->GetURL();
    if (url == kTestDownloadUrl) {
      got_download_request_->yes();
      content_ = kTestContent;
      mime_type_ = kTestMimeType;
      content_disposition_ = kTestContentDisposition;
      should_delay_ = true;
    } else {
      EXPECT_TRUE(IgnoreURL(url)) << url;

      // Cancel immediately.
      handle_request = true;
      return false;
    }

    // Continue immediately.
    handle_request = true;
    return true;
  }

  void GetResponseHeaders(CefRefPtr<CefResponse> response,
                          int64_t& response_length,
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

  bool Read(void* data_out,
            int bytes_to_read,
            int& bytes_read,
            CefRefPtr<CefResourceReadCallback> callback) override {
    EXPECT_FALSE(CefCurrentlyOn(TID_UI) || CefCurrentlyOn(TID_IO));

    bytes_read = 0;

    if (should_delay_ && !delay_callback_.is_null()) {
      // Delay the download response a single time.
      std::move(delay_callback_)
          .Run(base::BindOnce(&DownloadSchemeHandler::ContinueRead, this,
                              data_out, bytes_to_read, callback));
      return true;
    }

    return DoRead(data_out, bytes_to_read, bytes_read);
  }

  void Cancel() override {}

 private:
  void ContinueRead(void* data_out,
                    int bytes_to_read,
                    CefRefPtr<CefResourceReadCallback> callback) {
    int bytes_read = 0;
    DoRead(data_out, bytes_to_read, bytes_read);
    callback->Continue(bytes_read);
  }

  bool DoRead(void* data_out, int bytes_to_read, int& bytes_read) {
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

  DelayCallback delay_callback_;
  TrackCallback* got_download_request_;
  bool should_delay_ = false;
  std::string content_;
  std::string mime_type_;
  std::string content_disposition_;
  size_t offset_ = 0;
  CefRefPtr<CefResourceReadCallback> read_callback_;

  IMPLEMENT_REFCOUNTING(DownloadSchemeHandler);
  DISALLOW_COPY_AND_ASSIGN(DownloadSchemeHandler);
};

using DelayCallbackVendor = base::RepeatingCallback<DelayCallback(void)>;

class DownloadSchemeHandlerFactory : public CefSchemeHandlerFactory {
 public:
  DownloadSchemeHandlerFactory(const DelayCallbackVendor& delay_callback_vendor,
                               TrackCallback* got_download_request)
      : delay_callback_vendor_(delay_callback_vendor),
        got_download_request_(got_download_request) {}

  CefRefPtr<CefResourceHandler> Create(CefRefPtr<CefBrowser> browser,
                                       CefRefPtr<CefFrame> frame,
                                       const CefString& scheme_name,
                                       CefRefPtr<CefRequest> request) override {
    return new DownloadSchemeHandler(delay_callback_vendor_.is_null()
                                         ? base::NullCallback()
                                         : delay_callback_vendor_.Run(),
                                     got_download_request_);
  }

 private:
  DelayCallbackVendor delay_callback_vendor_;
  TrackCallback* got_download_request_;

  IMPLEMENT_REFCOUNTING(DownloadSchemeHandlerFactory);
  DISALLOW_COPY_AND_ASSIGN(DownloadSchemeHandlerFactory);
};

class DownloadTestHandler : public TestHandler {
 public:
  enum TestMode {
    PROGRAMMATIC,
    NAVIGATED,
    PENDING,
    CLICKED,
    CLICKED_INVALID,
    CLICKED_BLOCKED,
  };

  DownloadTestHandler(TestMode test_mode,
                      TestRequestContextMode rc_mode,
                      const std::string& rc_cache_path)
      : test_mode_(test_mode),
        rc_mode_(rc_mode),
        rc_cache_path_(rc_cache_path) {}

  bool is_clicked() const {
    return test_mode_ == CLICKED || test_mode_ == CLICKED_INVALID ||
           test_mode_ == CLICKED_BLOCKED;
  }

  bool is_clicked_invalid() const { return test_mode_ == CLICKED_INVALID; }

  bool is_clicked_and_downloaded() const { return test_mode_ == CLICKED; }

  bool is_downloaded() const {
    return test_mode_ == PROGRAMMATIC || test_mode_ == NAVIGATED ||
           is_clicked_and_downloaded();
  }

  void RunTest() override {
    if (!is_clicked() || is_clicked_and_downloaded()) {
      // Create a new temporary directory.
      EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
      test_path_ =
          client::file_util::JoinPath(temp_dir_.GetPath(), kTestFileName);
    }

    CreateTestRequestContext(
        rc_mode_, rc_cache_path_,
        base::BindOnce(&DownloadTestHandler::RunTestContinue, this));

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  void RunTestContinue(CefRefPtr<CefRequestContext> request_context) {
    EXPECT_UI_THREAD();

    DelayCallbackVendor delay_callback_vendor;
    if (test_mode_ == NAVIGATED || test_mode_ == PENDING) {
      delay_callback_vendor = base::BindRepeating(
          [](CefRefPtr<DownloadTestHandler> self) {
            return base::BindOnce(&DownloadTestHandler::OnDelayCallback, self);
          },
          CefRefPtr<DownloadTestHandler>(this));
    }

    CefRefPtr<CefSchemeHandlerFactory> scheme_factory =
        new DownloadSchemeHandlerFactory(delay_callback_vendor,
                                         &got_download_request_);

    if (request_context) {
      request_context->RegisterSchemeHandlerFactory("https", kTestDomain,
                                                    scheme_factory);
    } else {
      CefRegisterSchemeHandlerFactory("https", kTestDomain, scheme_factory);
    }

    if (test_mode_ == NAVIGATED) {
      // Add the resource that we'll navigate to.
      AddResource(kTestNavUrl, "<html><body>Navigated</body></html>",
                  "text/html");
    }

    if (is_clicked()) {
      if (test_mode_ == CLICKED || test_mode_ == CLICKED_BLOCKED) {
        download_url_ = kTestDownloadUrl;
      } else if (test_mode_ == CLICKED_INVALID) {
        download_url_ = "invalid:foo@example.com";
      } else {
        EXPECT_TRUE(false);  // Not reached.
      }
      AddResource(kTestStartUrl,
                  "<html><body><a href=\"" + download_url_ +
                      "\">CLICK ME</a></body></html>",
                  "text/html");
    } else {
      download_url_ = kTestStartUrl;
      AddResource(kTestStartUrl, "<html><body>Download Test</body></html>",
                  "text/html");
    }

    // Create the browser
    CreateBrowser(kTestStartUrl, request_context);
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    const std::string& url = frame->GetURL().ToString();
    if (url == kTestNavUrl) {
      got_nav_load_.yes();
      ContinueNavigatedIfReady();
      return;
    }

    if (is_clicked()) {
      // Begin the download by clicking a link.
      // ALT key will trigger download of custom protocol links.
      SendClick(browser,
                test_mode_ == CLICKED_INVALID ? EVENTFLAG_ALT_DOWN : 0);

      if (is_clicked_invalid()) {
        // Destroy the test after a bit because there will be no further
        // callbacks.
        CefPostDelayedTask(
            TID_UI, base::BindOnce(&DownloadTestHandler::DestroyTest, this),
            200);
      }
    } else {
      // Begin the download progammatically.
      browser->GetHost()->StartDownload(kTestDownloadUrl);
    }
  }

  // Callback from the scheme handler when the download request is delayed.
  void OnDelayCallback(base::OnceClosure callback) {
    if (!CefCurrentlyOn(TID_UI)) {
      CefPostTask(TID_UI, base::BindOnce(&DownloadTestHandler::OnDelayCallback,
                                         this, std::move(callback)));
      return;
    }

    got_delay_callback_.yes();

    if (test_mode_ == NAVIGATED) {
      delay_callback_ = std::move(callback);
      ContinueNavigatedIfReady();
    } else if (test_mode_ == PENDING) {
      ContinuePendingIfReady();
    } else {
      EXPECT_TRUE(false);  // Not reached.
    }
  }

  void ContinueNavigatedIfReady() {
    EXPECT_EQ(test_mode_, NAVIGATED);
    if (got_delay_callback_ && got_nav_load_) {
      EXPECT_FALSE(delay_callback_.is_null());
      std::move(delay_callback_).Run();
    }
  }

  void ContinuePendingIfReady() {
    EXPECT_EQ(test_mode_, PENDING);
    if (got_delay_callback_ && got_on_before_download_ &&
        got_on_download_updated_) {
      // Destroy the test without waiting for the download to complete.
      DestroyTest();
    }
  }

  bool CanDownload(CefRefPtr<CefBrowser> browser,
                   const CefString& url,
                   const CefString& request_method) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_UI));
    EXPECT_FALSE(got_can_download_);
    EXPECT_FALSE(got_on_before_download_);
    EXPECT_TRUE(is_clicked());

    got_can_download_.yes();

    EXPECT_TRUE(browser->IsSame(GetBrowser()));
    EXPECT_STREQ(download_url_.c_str(), url.ToString().c_str());
    EXPECT_STREQ("GET", request_method.ToString().c_str());

    if (is_clicked() && !is_clicked_and_downloaded()) {
      // Destroy the test after a bit because there will be no further
      // callbacks.
      CefPostDelayedTask(
          TID_UI, base::BindOnce(&DownloadTestHandler::DestroyTest, this), 200);
    }

    return test_mode_ != CLICKED_BLOCKED;
  }

  bool OnBeforeDownload(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefDownloadItem> download_item,
      const CefString& suggested_name,
      CefRefPtr<CefBeforeDownloadCallback> callback) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_UI));
    EXPECT_FALSE(got_on_before_download_);

    if (is_clicked()) {
      EXPECT_TRUE(got_can_download_);
    } else {
      EXPECT_FALSE(got_can_download_);
    }

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
    EXPECT_FALSE(download_item->IsInterrupted());
    EXPECT_EQ(CEF_DOWNLOAD_INTERRUPT_REASON_NONE,
              download_item->GetInterruptReason());
    EXPECT_EQ(static_cast<int64_t>(sizeof(kTestContent) - 1),
              download_item->GetTotalBytes());
    EXPECT_EQ(0UL, download_item->GetFullPath().length());
    EXPECT_STREQ(kTestDownloadUrl, download_item->GetURL().ToString().c_str());
    EXPECT_EQ(0UL, download_item->GetSuggestedFileName().length());
    EXPECT_STREQ(kTestContentDisposition,
                 download_item->GetContentDisposition().ToString().c_str());
    EXPECT_STREQ(kTestMimeType,
                 download_item->GetMimeType().ToString().c_str());

    callback->Continue(test_path_, false);

    if (test_mode_ == NAVIGATED) {
      CefRefPtr<CefFrame> main_frame = browser->GetMainFrame();
      EXPECT_TRUE(main_frame->IsMain());
      main_frame->LoadURL(kTestNavUrl);
    } else if (test_mode_ == PENDING) {
      ContinuePendingIfReady();
    }

    return true;
  }

  void OnDownloadUpdated(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefDownloadItem> download_item,
                         CefRefPtr<CefDownloadItemCallback> callback) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_UI));

    if (destroyed_) {
      return;
    }

    got_on_download_updated_.yes();

    EXPECT_TRUE(browser->IsSame(GetBrowser()));
    EXPECT_TRUE(download_item.get());
    EXPECT_TRUE(callback.get());

    if (got_on_before_download_) {
      EXPECT_EQ(download_id_, download_item->GetId());
    }

    EXPECT_LE(0LL, download_item->GetCurrentSpeed());
    EXPECT_LE(0, download_item->GetPercentComplete());

    EXPECT_TRUE(download_item->IsValid());
    EXPECT_FALSE(download_item->IsCanceled());
    EXPECT_FALSE(download_item->IsInterrupted());
    EXPECT_EQ(CEF_DOWNLOAD_INTERRUPT_REASON_NONE,
              download_item->GetInterruptReason());
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
      EXPECT_EQ(static_cast<int64_t>(sizeof(kTestContent) - 1),
                download_item->GetReceivedBytes());
      EXPECT_EQ(static_cast<int64_t>(sizeof(kTestContent) - 1),
                download_item->GetTotalBytes());

      DestroyTest();
    } else {
      EXPECT_TRUE(download_item->IsInProgress());
      EXPECT_LE(0LL, download_item->GetReceivedBytes());
    }

    if (test_mode_ == PENDING) {
      download_item_callback_ = callback;
      ContinuePendingIfReady();
    }
  }

  void VerifyResultsOnFileThread() {
    EXPECT_TRUE(CefCurrentlyOn(TID_FILE_USER_VISIBLE));

    if (test_mode_ != PENDING) {
      // Verify the file contents.
      std::string contents;
      EXPECT_TRUE(client::file_util::ReadFileToString(test_path_, &contents));
      EXPECT_STREQ(kTestContent, contents.c_str());
    }

    EXPECT_TRUE(temp_dir_.Delete());
    EXPECT_TRUE(temp_dir_.IsEmpty());

    CefPostTask(TID_UI,
                base::BindOnce(&DownloadTestHandler::DestroyTest, this));
  }

  void DestroyTest() override {
    if (!verified_results_ && !temp_dir_.IsEmpty()) {
      // Avoid an endless failure loop.
      verified_results_ = true;
      // Clean up temp_dir_ on the FILE thread before destroying the test.
      CefPostTask(TID_FILE_USER_VISIBLE,
                  base::BindOnce(
                      &DownloadTestHandler::VerifyResultsOnFileThread, this));
      return;
    }

    destroyed_ = true;

    if (download_item_callback_) {
      // Cancel the pending download to avoid leaking request objects.
      download_item_callback_->Cancel();
      download_item_callback_ = nullptr;
    }

    if (request_context_) {
      request_context_->RegisterSchemeHandlerFactory("https", kTestDomain,
                                                     nullptr);
      request_context_ = nullptr;
    } else {
      CefRegisterSchemeHandlerFactory("https", kTestDomain, nullptr);
    }

    if (is_clicked_invalid()) {
      // No CanDownload for invalid protocol links.
      EXPECT_FALSE(got_can_download_);
    } else if (is_clicked()) {
      EXPECT_TRUE(got_can_download_);
    } else {
      EXPECT_FALSE(got_can_download_);
    }

    if (test_mode_ == CLICKED_INVALID) {
      // The invalid protocol request is not handled.
      EXPECT_FALSE(got_download_request_);
    } else {
      EXPECT_TRUE(got_download_request_);
    }

    if (is_clicked() && !is_clicked_and_downloaded()) {
      // The download never proceeds.
      EXPECT_FALSE(got_on_before_download_);
      EXPECT_FALSE(got_on_download_updated_);
    } else {
      EXPECT_TRUE(got_on_before_download_);
      EXPECT_TRUE(got_on_download_updated_);
    }

    if (test_mode_ == NAVIGATED) {
      EXPECT_TRUE(got_nav_load_);
    } else {
      EXPECT_FALSE(got_nav_load_);
    }

    if (!is_downloaded()) {
      // The download never completes.
      EXPECT_FALSE(got_download_complete_);
      EXPECT_FALSE(got_full_path_);
    } else {
      EXPECT_TRUE(got_download_complete_);
      EXPECT_TRUE(got_full_path_);
    }

    TestHandler::DestroyTest();
  }

 private:
  void SendClick(CefRefPtr<CefBrowser> browser, uint32_t modifiers) {
    EXPECT_TRUE(is_clicked());
    CefMouseEvent mouse_event;
    mouse_event.x = 20;
    mouse_event.y = 20;
    mouse_event.modifiers = modifiers;
    SendMouseClickEvent(browser, mouse_event);
  }

  const TestMode test_mode_;
  const TestRequestContextMode rc_mode_;
  const std::string rc_cache_path_;

  CefRefPtr<CefRequestContext> request_context_;

  // Used with NAVIGATED and PENDING test modes.
  base::OnceClosure delay_callback_;

  // Used with PENDING test mode.
  CefRefPtr<CefDownloadItemCallback> download_item_callback_;

  std::string download_url_;
  CefScopedTempDir temp_dir_;
  std::string test_path_;
  uint32_t download_id_ = 0;
  bool verified_results_ = false;
  bool destroyed_ = false;

  TrackCallback got_download_request_;
  TrackCallback got_can_download_;
  TrackCallback got_on_before_download_;
  TrackCallback got_on_download_updated_;
  TrackCallback got_full_path_;
  TrackCallback got_download_complete_;
  TrackCallback got_delay_callback_;
  TrackCallback got_nav_load_;

  IMPLEMENT_REFCOUNTING(DownloadTestHandler);
};

}  // namespace

#define DOWNLOAD_TEST_GROUP(test_name, test_mode) \
  RC_TEST_GROUP_ALL(DownloadTest, test_name, DownloadTestHandler, test_mode)

// Test a programmatic download.
DOWNLOAD_TEST_GROUP(Programmatic, PROGRAMMATIC)

// Test a clicked download.
DOWNLOAD_TEST_GROUP(Clicked, CLICKED)

// Test a clicked download where the protocol is invalid and therefore rejected.
// There will be no resulting CefDownloadHandler callbacks.
DOWNLOAD_TEST_GROUP(ClickedInvalid, CLICKED_INVALID)

// Test a clicked download where CanDownload returns false.
// There will be no resulting CefDownloadHandler callbacks.
DOWNLOAD_TEST_GROUP(ClickedBlocked, CLICKED_BLOCKED)

// Test where the download completes after cross-origin navigation.
DOWNLOAD_TEST_GROUP(Navigated, NAVIGATED)

// Test where the download is still pending when the browser is destroyed.
DOWNLOAD_TEST_GROUP(Pending, PENDING)
