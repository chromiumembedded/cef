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

namespace {

const char kTestChineseDomain[] = "test-chinese-download.com";
const char kTestChineseStartUrl[] =
    "https://test-chinese-download.com/test.html";
const char kTestChineseDownloadUrl[] =
    "https://test-chinese-download.com/order.xls";

// UTF-8 encoded Chinese filename (订单报表 = order report)
const char kTestChineseFileName[] =
    "\xe8\xae\xa2\xe5\x8d\x95\xe6\x8a\xa5\xe8\xa1\xa8_2021-05-25.xls";

// Scenario A: filename* with explicit UTF-8 charset (RFC 5987).
// This test covers upstream Chromium's RFC 5987 decoding, which works
// regardless of the referrer_charset parameter.
// filename*=UTF-8''%E8%AE%A2%E5%8D%95%E6%8A%A5%E8%A1%A8_2021-05-25.xls
const char kTestContentDispositionRFC5987[] =
    "attachment; "
    "filename*=UTF-8''%E8%AE%A2%E5%8D%95%E6%8A%A5%E8%A1%A8_2021-05-25.xls";

// Scenario B: filename with raw UTF-8 bytes and a non-standard charset=
// parameter on the Content-Disposition header. This is not defined by any
// RFC (only filename and filename* are standard), but Chromium's
// HttpContentDisposition checks for charset= as a workaround for sites like
// JD.com (see issue #3135). This test covers that upstream pathway.
const char kTestContentDispositionCharsetParam[] =
    "attachment;charset=utf-8;filename="
    "\xe8\xae\xa2\xe5\x8d\x95\xe6\x8a\xa5\xe8\xa1\xa8_2021-05-25.xls";

// Scenario C: filename with raw UTF-8 bytes, no charset hint anywhere.
// Relies on Chromium's platform-independent IsStringUTF8() check in
// DecodeWord(). Since the bytes are valid UTF-8, the system locale is
// never consulted.
const char kTestContentDispositionRawUTF8[] =
    "attachment; filename="
    "\xe8\xae\xa2\xe5\x8d\x95\xe6\x8a\xa5\xe8\xa1\xa8_2021-05-25.xls";

// Scenario D (not tested through scheme handler): raw GBK bytes with
// charset=gb18030 on the HTTP Content-Type response header. This is the
// real-world encoding scenario from issue #3135. The CEF fix extracts the
// Content-Type charset and passes it as referrer_charset to
// net::GenerateFileName(), allowing CharsetConverter to decode the raw bytes.
//
// This scenario cannot be tested through CefResourceHandler because
// CefResponse::HeaderMap uses CefString values, which assume UTF-8 encoding
// for all header strings. Raw GBK bytes (which are not valid UTF-8) are
// corrupted to U+FFFD replacement characters during the CefString conversion,
// before the download parser ever sees them. In real HTTP traffic, the raw
// bytes are preserved in net::HttpResponseHeaders::raw_headers_.

class DownloadChineseFilenameTest : public TestHandler {
 public:
  DownloadChineseFilenameTest(const std::string& content_disposition,
                              const std::string& charset = {})
      : content_disposition_(content_disposition), charset_(charset) {}

  void RunTest() override {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
    test_path_ =
        client::file_util::JoinPath(temp_dir_.GetPath(), kTestChineseFileName);

    CreateTestRequestContext(
        TEST_RC_MODE_CUSTOM_WITH_HANDLER, /*cache_path=*/"",
        base::BindOnce(&DownloadChineseFilenameTest::RunTestContinue, this));

    SetTestTimeout();
  }

  void RunTestContinue(CefRefPtr<CefRequestContext> request_context) {
    EXPECT_UI_THREAD();

    request_context_ = request_context;

    CefRefPtr<CefSchemeHandlerFactory> scheme_factory =
        new DownloadChineseFilenameSchemeHandlerFactory(
            content_disposition_, charset_, &got_download_request_);

    request_context_->RegisterSchemeHandlerFactory(
        "https", kTestChineseDomain, scheme_factory);

    AddResource(kTestChineseStartUrl,
                "<html><body>Download Test</body></html>", "text/html");
    CreateBrowser(kTestChineseStartUrl, request_context_);
  }

  CefRefPtr<CefDownloadHandler> GetDownloadHandler() override { return this; }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    browser->GetHost()->StartDownload(kTestChineseDownloadUrl);
  }

  bool OnBeforeDownload(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefDownloadItem> download_item,
      const CefString& suggested_name,
      CefRefPtr<CefBeforeDownloadCallback> callback) override {
    EXPECT_TRUE(CefCurrentlyOn(TID_UI));
    EXPECT_FALSE(got_on_before_download_);
    got_on_before_download_.yes();

    // Verify the Chinese filename is correctly decoded, not garbled.
    EXPECT_STREQ(kTestChineseFileName, suggested_name.ToString().c_str())
        << "Chinese filename mismatch. Got: " << suggested_name.ToString();

    callback->Continue(test_path_, false);
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

    if (download_item->IsComplete()) {
      got_download_complete_.yes();
      DestroyTest();
    }
  }

  void VerifyResultsOnFileThread() {
    EXPECT_TRUE(CefCurrentlyOn(TID_FILE_USER_VISIBLE));
    std::string contents;
    EXPECT_TRUE(client::file_util::ReadFileToString(test_path_, &contents));
    EXPECT_STREQ("test", contents.c_str());
    EXPECT_TRUE(temp_dir_.Delete());
    EXPECT_TRUE(temp_dir_.IsEmpty());

    CefPostTask(TID_UI,
                base::BindOnce(&DownloadChineseFilenameTest::DestroyTest, this));
  }

  void DestroyTest() override {
    if (destroyed_) {
      return;
    }

    if (!verified_results_ && !temp_dir_.IsEmpty()) {
      verified_results_ = true;
      CefPostTask(
          TID_FILE_USER_VISIBLE,
          base::BindOnce(&DownloadChineseFilenameTest::VerifyResultsOnFileThread,
                         this));
      return;
    }

    destroyed_ = true;

    if (request_context_) {
      request_context_->RegisterSchemeHandlerFactory("https", kTestChineseDomain,
                                                     nullptr);
      request_context_ = nullptr;
    }

    EXPECT_TRUE(got_on_before_download_);
    EXPECT_TRUE(got_on_download_updated_);
    EXPECT_TRUE(got_download_complete_);

    TestHandler::DestroyTest();
  }

 private:
  class DownloadChineseFilenameSchemeHandler : public CefResourceHandler {
   public:
    DownloadChineseFilenameSchemeHandler(
        const std::string& content_disposition,
        const std::string& charset,
        TrackCallback* got_download_request)
        : content_disposition_(content_disposition),
          charset_(charset),
          got_download_request_(got_download_request) {}

    bool Open(CefRefPtr<CefRequest> request,
              bool& handle_request,
              CefRefPtr<CefCallback> callback) override {
      EXPECT_FALSE(CefCurrentlyOn(TID_UI) || CefCurrentlyOn(TID_IO));

      if (request->GetURL() == kTestChineseDownloadUrl) {
        got_download_request_->yes();
        content_ = "test";
        mime_type_ = "application/vnd.ms-excel";
      } else {
        handle_request = true;
        return false;
      }
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

      if (!charset_.empty()) {
        response->SetCharset(charset_);
      }
    }

    bool Read(void* data_out,
              int bytes_to_read,
              int& bytes_read,
              CefRefPtr<CefResourceReadCallback> callback) override {
      EXPECT_FALSE(CefCurrentlyOn(TID_UI) || CefCurrentlyOn(TID_IO));

      size_t size = content_.size();
      if (offset_ < size) {
        int transfer_size =
            std::min(bytes_to_read, static_cast<int>(size - offset_));
        memcpy(data_out, content_.c_str() + offset_, transfer_size);
        offset_ += transfer_size;
        bytes_read = transfer_size;
        return true;
      }
      bytes_read = 0;
      return false;
    }

    void Cancel() override {}

   private:
    std::string content_;
    std::string mime_type_;
    std::string content_disposition_;
    std::string charset_;
    TrackCallback* got_download_request_;
    size_t offset_ = 0;

    IMPLEMENT_REFCOUNTING(DownloadChineseFilenameSchemeHandler);
  };

  class DownloadChineseFilenameSchemeHandlerFactory
      : public CefSchemeHandlerFactory {
   public:
    DownloadChineseFilenameSchemeHandlerFactory(
        const std::string& content_disposition,
        const std::string& charset,
        TrackCallback* got_download_request)
        : content_disposition_(content_disposition),
          charset_(charset),
          got_download_request_(got_download_request) {}

    CefRefPtr<CefResourceHandler> Create(CefRefPtr<CefBrowser> browser,
                                         CefRefPtr<CefFrame> frame,
                                         const CefString& scheme_name,
                                         CefRefPtr<CefRequest> request) override {
      return new DownloadChineseFilenameSchemeHandler(content_disposition_,
                                                      charset_,
                                                      got_download_request_);
    }

   private:
    std::string content_disposition_;
    std::string charset_;
    TrackCallback* got_download_request_;

    IMPLEMENT_REFCOUNTING(DownloadChineseFilenameSchemeHandlerFactory);
  };

  std::string content_disposition_;
  std::string charset_;
  CefScopedTempDir temp_dir_;
  std::string test_path_;
  CefRefPtr<CefRequestContext> request_context_;
  bool verified_results_ = false;
  bool destroyed_ = false;

  TrackCallback got_download_request_;
  TrackCallback got_on_before_download_;
  TrackCallback got_on_download_updated_;
  TrackCallback got_download_complete_;

  IMPLEMENT_REFCOUNTING(DownloadChineseFilenameTest);
};

}  // namespace

// Test: RFC 5987 filename*=UTF-8''... (self-encoding, always works regardless
// of referrer_charset). Covers upstream Chromium behavior; not dependent on
// the issue #3135 fix.
TEST(DownloadTest, ChineseFilenameRFC5987) {
  CefRefPtr<DownloadChineseFilenameTest> handler =
      new DownloadChineseFilenameTest(kTestContentDispositionRFC5987);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Test: charset=utf-8 parameter on Content-Disposition alongside raw UTF-8
// filename bytes. This is a non-standard header (not defined by any RFC),
// but Chromium's HttpContentDisposition checks for charset= as a workaround
// for servers like JD.com. Covers upstream behavior; not dependent on the
// issue #3135 fix.
TEST(DownloadTest, ChineseFilenameCharsetParam) {
  CefRefPtr<DownloadChineseFilenameTest> handler =
      new DownloadChineseFilenameTest(kTestContentDispositionCharsetParam);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Test: raw UTF-8 filename bytes with no charset hint. Relies on Chromium's
// platform-independent IsStringUTF8() check in DecodeWord() — since the bytes
// are valid UTF-8, the system locale is never consulted.
// Covers upstream behavior; not dependent on the issue #3135 fix.
TEST(DownloadTest, ChineseFilenameRawUTF8) {
  CefRefPtr<DownloadChineseFilenameTest> handler =
      new DownloadChineseFilenameTest(kTestContentDispositionRawUTF8);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Test: raw UTF-8 filename bytes with charset=utf-8 on the HTTP Content-Type
// response header. Exercises the CEF fix for issue #3135: extracting charset
// from GetResponseHeaders() and passing it as referrer_charset to
// net::GenerateFileName(). Note that this particular test would pass even
// without the fix, because IsStringUTF8() detects the raw UTF-8 bytes
// regardless of charset. A true regression guard for the GBK case requires
// raw non-UTF-8 bytes, which CefResponse's UTF-8-assuming CefString API
// cannot relay (see Scenario D comment).
TEST(DownloadTest, ChineseFilenameContentTypeCharset) {
  CefRefPtr<DownloadChineseFilenameTest> handler =
      new DownloadChineseFilenameTest(kTestContentDispositionRawUTF8, "utf-8");
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}
