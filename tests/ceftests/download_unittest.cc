// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <algorithm>
#include <memory>

#include "include/base/cef_callback.h"
#include "include/base/cef_callback_helpers.h"
#include "include/cef_request_context.h"
#include "include/cef_scheme.h"
#include "include/cef_values.h"
#include "include/test/cef_test_server.h"
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

  DownloadSchemeHandler(const DownloadSchemeHandler&) = delete;
  DownloadSchemeHandler& operator=(const DownloadSchemeHandler&) = delete;

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
};

using DelayCallbackVendor = base::RepeatingCallback<DelayCallback(void)>;

class DownloadSchemeHandlerFactory : public CefSchemeHandlerFactory {
 public:
  DownloadSchemeHandlerFactory(const DelayCallbackVendor& delay_callback_vendor,
                               TrackCallback* got_download_request)
      : delay_callback_vendor_(delay_callback_vendor),
        got_download_request_(got_download_request) {}

  DownloadSchemeHandlerFactory(const DownloadSchemeHandlerFactory&) = delete;
  DownloadSchemeHandlerFactory& operator=(const DownloadSchemeHandlerFactory&) =
      delete;

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
    SendJavaScriptClickEvent(browser, mouse_event);
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

namespace {

// Test handler for Pause/Resume functionality
class DownloadPauseResumeTestHandler : public TestHandler {
 public:
  DownloadPauseResumeTestHandler() = default;

  DownloadPauseResumeTestHandler(const DownloadPauseResumeTestHandler&) =
      delete;
  DownloadPauseResumeTestHandler& operator=(
      const DownloadPauseResumeTestHandler&) = delete;

  void RunTest() override {
    // Create a new temporary directory.
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    test_path_ =
        client::file_util::JoinPath(temp_dir_.GetPath(), kTestFileName);

    // Use an in-memory request context to avoid caching the download between
    // test runs.
    CreateTestRequestContext(
        TEST_RC_MODE_CUSTOM_WITH_HANDLER, /*cache_path=*/"",
        base::BindOnce(&DownloadPauseResumeTestHandler::RunTestContinue, this));

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  void RunTestContinue(CefRefPtr<CefRequestContext> request_context) {
    EXPECT_UI_THREAD();
    EXPECT_TRUE(request_context);

    request_context_ = request_context;

    // Use a delay callback to slow down the download, giving us time to pause
    // and resume it.
    DelayCallbackVendor delay_callback_vendor = base::BindRepeating(
        [](CefRefPtr<DownloadPauseResumeTestHandler> self) {
          return base::BindOnce(
              &DownloadPauseResumeTestHandler::OnDelayCallback, self);
        },
        CefRefPtr<DownloadPauseResumeTestHandler>(this));

    CefRefPtr<CefSchemeHandlerFactory> scheme_factory =
        new DownloadSchemeHandlerFactory(delay_callback_vendor,
                                         &got_download_request_);

    request_context->RegisterSchemeHandlerFactory("https", kTestDomain,
                                                  scheme_factory);

    AddResource(kTestStartUrl, "<html><body>Download Test</body></html>",
                "text/html");
    CreateBrowser(kTestStartUrl, request_context);
  }

  CefRefPtr<CefDownloadHandler> GetDownloadHandler() override { return this; }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    // Start the download programmatically
    browser->GetHost()->StartDownload(kTestDownloadUrl);
  }

  bool OnBeforeDownload(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefDownloadItem> download_item,
      const CefString& suggested_name,
      CefRefPtr<CefBeforeDownloadCallback> callback) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_UI));
    EXPECT_FALSE(got_on_before_download_);
    got_on_before_download_.yes();

    EXPECT_TRUE(browser->IsSame(GetBrowser()));
    EXPECT_STREQ(kTestDownloadUrl, download_item->GetURL().ToString().c_str());
    EXPECT_STREQ(kTestFileName, suggested_name.ToString().c_str());

    download_id_ = download_item->GetId();
    EXPECT_LT(0U, download_id_);

    // Continue the download
    callback->Continue(test_path_, false);

    // Immediately pause the download (it hasn't started receiving data yet)
    // This will be saved as the callback from the first OnDownloadUpdated
    return true;
  }

  void OnDownloadUpdated(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefDownloadItem> download_item,
                         CefRefPtr<CefDownloadItemCallback> callback) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_UI));

    got_on_download_updated_.yes();

    EXPECT_TRUE(browser->IsSame(GetBrowser()));
    EXPECT_TRUE(download_item.get());
    EXPECT_TRUE(callback.get());

    if (got_on_before_download_) {
      EXPECT_EQ(download_id_, download_item->GetId());
    }

    // Pause on the very first OnDownloadUpdated call, before any bytes received
    if (!got_paused_ && download_item->IsInProgress()) {
      got_paused_.yes();
      callback->Pause();

      // Resume after a short delay
      CefPostDelayedTask(
          TID_UI,
          base::BindOnce(&DownloadPauseResumeTestHandler::ResumeDownload, this,
                         callback),
          50);
      return;
    }

    // Wait for the download to actually be paused before continuing
    if (got_paused_ && !got_paused_confirmed_ && download_item->IsPaused()) {
      got_paused_confirmed_.yes();
      // Continue the delayed download now that pause has taken effect.
      ContinueDelayedDownloadIfReady();
      return;
    }

    // Check if download completed after resume
    if (download_item->IsComplete()) {
      got_download_complete_.yes();

      EXPECT_FALSE(download_item->IsInProgress());
      EXPECT_FALSE(download_item->IsCanceled());
      EXPECT_EQ(100, download_item->GetPercentComplete());
      EXPECT_EQ(static_cast<int64_t>(sizeof(kTestContent) - 1),
                download_item->GetReceivedBytes());
      EXPECT_EQ(static_cast<int64_t>(sizeof(kTestContent) - 1),
                download_item->GetTotalBytes());
      EXPECT_STREQ(test_path_.c_str(),
                   download_item->GetFullPath().ToString().c_str());

      DestroyTest();
    }
  }

  void VerifyResultsOnFileThread() {
    EXPECT_TRUE(CefCurrentlyOn(TID_FILE_USER_VISIBLE));

    // Verify the file contents.
    std::string contents;
    EXPECT_TRUE(client::file_util::ReadFileToString(test_path_, &contents));
    EXPECT_STREQ(kTestContent, contents.c_str());

    EXPECT_TRUE(temp_dir_.Delete());
    EXPECT_TRUE(temp_dir_.IsEmpty());

    CefPostTask(
        TID_UI,
        base::BindOnce(&DownloadPauseResumeTestHandler::DestroyTest, this));
  }

  void DestroyTest() override {
    if (!verified_results_ && !temp_dir_.IsEmpty()) {
      // Avoid an endless failure loop.
      verified_results_ = true;
      // Clean up temp_dir_ on the FILE thread before destroying the test.
      CefPostTask(
          TID_FILE_USER_VISIBLE,
          base::BindOnce(
              &DownloadPauseResumeTestHandler::VerifyResultsOnFileThread,
              this));
      return;
    }

    EXPECT_TRUE(request_context_);
    request_context_->RegisterSchemeHandlerFactory("https", kTestDomain,
                                                   nullptr);
    request_context_ = nullptr;

    EXPECT_TRUE(got_on_before_download_);
    EXPECT_TRUE(got_on_download_updated_);
    EXPECT_TRUE(got_paused_);
    EXPECT_TRUE(got_paused_confirmed_);
    EXPECT_TRUE(got_resumed_);
    EXPECT_TRUE(got_download_complete_);

    TestHandler::DestroyTest();
  }

 private:
  // Callback from the scheme handler to delay the download response.
  void OnDelayCallback(base::OnceClosure callback) {
    if (!CefCurrentlyOn(TID_UI)) {
      CefPostTask(TID_UI, base::BindOnce(
                              &DownloadPauseResumeTestHandler::OnDelayCallback,
                              this, std::move(callback)));
      return;
    }

    // Store the callback and wait for the pause to be initiated before
    // continuing the download.
    delay_callback_ = std::move(callback);
    ContinueDelayedDownloadIfReady();
  }

  void ContinueDelayedDownloadIfReady() {
    // Continue the download once we've confirmed the pause has taken effect.
    if (got_paused_confirmed_ && !delay_callback_.is_null()) {
      std::move(delay_callback_).Run();
    }
  }

  void ResumeDownload(CefRefPtr<CefDownloadItemCallback> callback) {
    got_resumed_.yes();
    callback->Resume();
  }

  CefRefPtr<CefRequestContext> request_context_;
  CefScopedTempDir temp_dir_;
  std::string test_path_;
  uint32_t download_id_ = 0;
  bool verified_results_ = false;

  // Used to delay the download response until pause is initiated.
  base::OnceClosure delay_callback_;

  TrackCallback got_download_request_;
  TrackCallback got_on_before_download_;
  TrackCallback got_on_download_updated_;
  TrackCallback got_paused_;
  TrackCallback got_paused_confirmed_;
  TrackCallback got_resumed_;
  TrackCallback got_download_complete_;

  IMPLEMENT_REFCOUNTING(DownloadPauseResumeTestHandler);
};

}  // namespace

// Test pause and resume functionality.
TEST(DownloadTest, PauseResume) {
  CefRefPtr<DownloadPauseResumeTestHandler> handler =
      new DownloadPauseResumeTestHandler();
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

#if CEF_API_ADDED(15100)

namespace {

// Regression test for issue #3135: a server sends a download whose filename is
// encoded with a non-UTF-8 charset (GBK/GB18030) in the Content-Disposition
// header and does not provide an RFC 5987 "filename*" parameter. The charset
// used to decode the raw filename bytes comes from one of two sources, each
// covered by a test below:
// - CONTENT_TYPE_CHARSET: the server declares the charset via the Content-Type
//   response header (e.g. "...; charset=gb18030").
// - DEFAULT_CHARSET_PREF: the server declares no charset and the value of the
//   "intl.charset_default" preference is used as a fallback.
// In both cases CEF must pass the charset to net::GenerateFileName();
// otherwise the raw filename bytes cannot be decoded and the suggested
// filename is garbled.
//
// A real CefTestServer is used (rather than a CefResourceHandler) because the
// raw GBK header bytes must reach the network stack intact. CefResponse's
// HeaderMap uses CefString, which assumes UTF-8 and would corrupt the bytes
// before the download parser ever sees them.

// "订单报表_2021-05-25.xls" as UTF-8 (the expected, correctly-decoded name).
const char kGbkExpectedUtf8FileName[] =
    "\xe8\xae\xa2\xe5\x8d\x95\xe6\x8a\xa5\xe8\xa1\xa8_2021-05-25.xls";

// "订单报表" encoded as raw GB18030/GBK bytes, as it appears on the wire.
const char kGbkRawFileNameBytes[] =
    "\xb6\xa9\xb5\xa5\xb1\xa8\xb1\xed_2021-05-25.xls";

const char kGbkDownloadPath[] = "/order.xls";
const char kGbkDownloadContent[] = "test";
const char kDefaultCharsetPrefName[] = "intl.charset_default";

// Serves the start page and the GBK download response on the dedicated server
// thread. The download response is sent with raw (non-UTF-8) header bytes.
class GbkFilenameServerHandler : public CefTestServerHandler {
 public:
  // If |declare_charset| is true the Content-Type response header declares the
  // gb18030 charset; otherwise no charset is declared.
  explicit GbkFilenameServerHandler(bool declare_charset)
      : declare_charset_(declare_charset) {}

  GbkFilenameServerHandler(const GbkFilenameServerHandler&) = delete;
  GbkFilenameServerHandler& operator=(const GbkFilenameServerHandler&) = delete;

  bool OnTestServerRequest(
      CefRefPtr<CefTestServer> server,
      CefRefPtr<CefRequest> request,
      CefRefPtr<CefTestServerConnection> connection) override {
    const std::string url = request->GetURL();
    const std::string origin = server->GetOrigin();
    const std::string path =
        url.size() >= origin.size() ? url.substr(origin.size()) : url;

    if (path == kGbkDownloadPath) {
      // Build the raw HTTP response. The Content-Disposition carries the raw
      // GBK filename bytes with no RFC 5987 "filename*" parameter.
      const std::string body = kGbkDownloadContent;
      std::string headers = "HTTP/1.1 200 OK\r\n";
      headers += declare_charset_
                     ? "Content-Type: application/octet-stream; "
                       "charset=gb18030\r\n"
                     : "Content-Type: application/octet-stream\r\n";
      headers += "Content-Disposition: attachment; filename=\"";
      headers.append(kGbkRawFileNameBytes);
      headers +=
          "\"\r\nContent-Length: " + std::to_string(body.size()) + "\r\n";
      connection->SendHttpResponseWithRawHeaders(headers.data(), headers.size(),
                                                 body.data(), body.size());
    } else {
      const std::string body = "<html><body>Download Test</body></html>";
      const std::string headers =
          "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: " +
          std::to_string(body.size()) + "\r\n";
      connection->SendHttpResponseWithRawHeaders(headers.data(), headers.size(),
                                                 body.data(), body.size());
    }
    return true;
  }

 private:
  const bool declare_charset_;

  IMPLEMENT_REFCOUNTING(GbkFilenameServerHandler);
};

class DownloadGbkFilenameTestHandler : public TestHandler {
 public:
  enum class Mode {
    // The charset comes from the Content-Type response header.
    CONTENT_TYPE_CHARSET,
    // The server declares no charset; the charset comes from the
    // "intl.charset_default" preference.
    DEFAULT_CHARSET_PREF,
  };

  explicit DownloadGbkFilenameTestHandler(Mode mode) : mode_(mode) {}

  void RunTest() override {
    // Create the temp directory on this (non-UI) thread, where blocking file
    // I/O is permitted.
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    test_path_ = client::file_util::JoinPath(temp_dir_.GetPath(),
                                             kGbkExpectedUtf8FileName);

    // CefTestServer must be created and stopped on the same thread. DestroyTest
    // (which stops it) runs on the UI thread, so create it there too.
    CefPostTask(
        TID_UI,
        base::BindOnce(&DownloadGbkFilenameTestHandler::StartServer, this));
    SetTestTimeout();
  }

  void StartServer() {
    EXPECT_UI_THREAD();

    if (mode_ == Mode::DEFAULT_CHARSET_PREF) {
      // Set the default charset preference on the global request context
      // (which the browser created below will use), saving the current value
      // for restoration in DestroyTest.
      auto context = CefRequestContext::GetGlobalContext();
      previous_charset_pref_ = context->GetPreference(kDefaultCharsetPrefName);
      auto value = CefValue::Create();
      value->SetString("gb18030");
      CefString error;
      EXPECT_TRUE(context->SetPreference(kDefaultCharsetPrefName, value, error))
          << error.ToString();
    }

    // Start a real HTTP test server. CreateAndStart blocks until the dedicated
    // server thread is running.
    server_handler_ = new GbkFilenameServerHandler(
        /*declare_charset=*/mode_ == Mode::CONTENT_TYPE_CHARSET);
    server_ =
        CefTestServer::CreateAndStart(/*port=*/0, /*https_server=*/false,
                                      CEF_TEST_CERT_OK_IP, server_handler_);
    EXPECT_TRUE(server_);
    if (!server_) {
      // Avoid dereferencing a null server below if startup failed.
      DestroyTest();
      return;
    }

    const std::string origin = server_->GetOrigin();
    download_url_ = origin + kGbkDownloadPath;

    CreateBrowser(origin + "/");
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    browser->GetHost()->StartDownload(download_url_);
  }

  bool OnBeforeDownload(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefDownloadItem> download_item,
      const CefString& suggested_name,
      CefRefPtr<CefBeforeDownloadCallback> callback) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_UI));
    EXPECT_FALSE(got_on_before_download_);
    got_on_before_download_.yes();

    // The GBK filename must be decoded to the expected UTF-8 name, not garbled.
    EXPECT_STREQ(kGbkExpectedUtf8FileName, suggested_name.ToString().c_str())
        << "GBK filename not decoded; got: " << suggested_name.ToString();

    callback->Continue(test_path_, /*show_dialog=*/false);
    return true;
  }

  void OnDownloadUpdated(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefDownloadItem> download_item,
                         CefRefPtr<CefDownloadItemCallback> callback) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_UI));
    if (got_download_complete_) {
      return;
    }
    if (download_item->IsComplete()) {
      got_download_complete_.yes();
      DestroyTest();
    }
  }

  void DestroyTest() override {
    EXPECT_UI_THREAD();

    // Run teardown only once. This guards against re-entry (e.g. the test
    // timeout firing) racing the FILE-thread temp-directory cleanup below.
    if (destroy_pending_) {
      return;
    }
    destroy_pending_ = true;

    if (server_) {
      // Stop must be called on the same thread as CreateAndStart (the UI
      // thread).
      server_->Stop();
      server_ = nullptr;
      server_handler_ = nullptr;
    }

    if (previous_charset_pref_) {
      // Restore the default charset preference modified in StartServer.
      CefString error;
      EXPECT_TRUE(CefRequestContext::GetGlobalContext()->SetPreference(
          kDefaultCharsetPrefName, previous_charset_pref_, error))
          << error.ToString();
      previous_charset_pref_ = nullptr;
    }

    // Delete the temp directory on the FILE thread, where blocking file I/O is
    // allowed (it is not allowed on the UI thread, including during the
    // handler's destruction). |temp_dir_| is created in RunTest() before any
    // cross-thread access and only deleted here, so task-post ordering avoids
    // a data race. DeleteTempDir reposts to the UI thread to finish teardown.
    CefPostTask(
        TID_FILE_USER_VISIBLE,
        base::BindOnce(&DownloadGbkFilenameTestHandler::DeleteTempDir, this));
  }

  void DeleteTempDir() {
    EXPECT_TRUE(CefCurrentlyOn(TID_FILE_USER_VISIBLE));
    if (!temp_dir_.IsEmpty()) {
      EXPECT_TRUE(temp_dir_.Delete());
    }
    CefPostTask(TID_UI,
                base::BindOnce(
                    &DownloadGbkFilenameTestHandler::FinishDestroyTest, this));
  }

  void FinishDestroyTest() {
    EXPECT_UI_THREAD();
    EXPECT_TRUE(got_on_before_download_);
    EXPECT_TRUE(got_download_complete_);
    TestHandler::DestroyTest();
  }

 private:
  const Mode mode_;

  CefRefPtr<CefTestServer> server_;
  CefRefPtr<GbkFilenameServerHandler> server_handler_;
  CefRefPtr<CefValue> previous_charset_pref_;
  CefScopedTempDir temp_dir_;
  std::string download_url_;
  std::string test_path_;
  bool destroy_pending_ = false;

  TrackCallback got_on_before_download_;
  TrackCallback got_download_complete_;

  IMPLEMENT_REFCOUNTING(DownloadGbkFilenameTestHandler);
};

}  // namespace

// Test that a GBK-encoded download filename is decoded using the Content-Type
// charset. Regression test for issue #3135.
TEST(DownloadTest, GbkFilenameContentTypeCharset) {
  CefRefPtr<DownloadGbkFilenameTestHandler> handler =
      new DownloadGbkFilenameTestHandler(
          DownloadGbkFilenameTestHandler::Mode::CONTENT_TYPE_CHARSET);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Test that a GBK-encoded download filename is decoded using the
// "intl.charset_default" preference when the server declares no charset.
TEST(DownloadTest, GbkFilenameDefaultCharsetPref) {
  CefRefPtr<DownloadGbkFilenameTestHandler> handler =
      new DownloadGbkFilenameTestHandler(
          DownloadGbkFilenameTestHandler::Mode::DEFAULT_CHARSET_PREF);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

#endif  // CEF_API_ADDED(15100)
