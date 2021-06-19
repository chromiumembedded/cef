// Copyright (c) 2014 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <memory>

#include "include/base/cef_callback.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_stream_resource_handler.h"
#include "tests/ceftests/test_handler.h"
#include "tests/ceftests/test_util.h"
#include "tests/gtest/include/gtest/gtest.h"
#include "tests/shared/browser/client_app_browser.h"
#include "tests/shared/renderer/client_app_renderer.h"

using client::ClientAppBrowser;
using client::ClientAppRenderer;

namespace {

// The frame navigation test harness work as follows:
//
// In the browser process:
// 1. TEST() function creates a new FrameNavTestHandler instance with a unique
//    FrameNavFactoryId.
// 2. FrameNavTestHandler calls FrameNavExpectationsFactoryBrowser::FromID to
//    create a new factory instance.
// 3. FrameNavTestHandler calls FrameNavExpectationsFactoryBrowser::Create to
//    create a new FrameNavExpectationsBrowser instance for the current
//    navigation.
// 4. FrameNavTestHandler retrieves the URL to load via
//    FrameNavExpectationsBrowser::GetMainURL and calls either CreateBrowser
//    (for the first navigation) or LoadURL (for the following navigations).
// 5. If the renderer process does not already exist CEF creates it with
//    command-line arguments that specify the FrameNavFactoryId via
//    FrameNavBrowserTest::OnBeforeChildProcessLaunch.
//
// In the renderer process:
// 6. If the renderer process is newly created FrameNavRendererTest calls
//    FrameNavExpectationsFactoryRenderer::FromID to create a new factory
//    instance.
// 7. FrameNavRendererTest calls FrameNavExpectationsFactoryRenderer::Create to
//    create a new FrameNavExpectationsRenderer instance for the current
//    navigation.
//
// In both processes:
// 8. Callback notifications are sent to the FrameNavExpectations* instances.
//
// In the renderer process:
// 9. When the FrameNavExpectationsRenderer instance determines that the
//    renderer side of the test is complete it calls SignalComplete which
//    finalizes and deletes the FrameNavExpectationsRenderer instance and
//    sends an IPC message to the browser process.
//
// In the browser process:
// 11.FrameNavExpectationsBrowser::OnRendererComplete is called in response to
//    renderer-side test completion message.
// 12.When the FrameNavExpectationsBrowser instance determines that the browser
//    side of the test is complete it calls SignalComplete which finalizes and
//    deletes the FrameNavExpectationsBrowser instance.
// 13.If FrameNavExpectationsFactoryBrowser::HasMoreNavigations returns false
//    then DestroyTest is called and the test ends. Otherwise, the navigation
//    count is incremented and the process repeats starting with step #3.
//
//
// To add a new test case:
// 1. Add a new value to the FrameNavFactoryId enumeration.
// 2. Provide implementations of FrameNavExpectations*.
// 3. Add a case for the new factory ID to FrameNavExpectationsFactory*::FromID.
// 4. Implement a TEST() function that creates a FrameNavTestHandler instance
//    and passes the new factory ID.
//
//
// Run with the `--single-process` command-line flag to see expectation failures
// from the renderer process.
//

// All known factory IDs.
enum FrameNavFactoryId {
  FNF_ID_INVALID = 0,
  FNF_ID_SINGLE_NAV_HARNESS,
  FNF_ID_SINGLE_NAV,
  FNF_ID_MULTI_NAV_HARNESS,
  FNF_ID_MULTI_NAV,
  FNF_ID_NESTED_IFRAMES_SAME_ORIGIN,
  FNF_ID_NESTED_IFRAMES_DIFF_ORIGIN,
};

// IPC message name.
const char kFrameNavMsg[] = "FrameTest.Navigation";

// Extra info parameter keys.
const char kFrameNavTestCmdKey[] = "frame-nav-test";

// Origins used in tests.
const char kFrameNavOrigin0[] = "http://tests-framenav0.com/";
const char kFrameNavOrigin1[] = "http://tests-framenav1.com/";
const char kFrameNavOrigin2[] = "http://tests-framenav2.com/";
const char kFrameNavOrigin3[] = "http://tests-framenav3.com/";

// Maximum number of navigations. Should be kept synchronized with the number
// of kFrameNavOrigin* values. Don't modify this value without checking the
// below use cases.
const int kMaxMultiNavNavigations = 4;

// Abstract base class representing expectations that result from a navigation.
class FrameNavExpectations {
 public:
  typedef base::OnceCallback<void(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>)>
      CompletionCallback;

  FrameNavExpectations(int nav, bool renderer)
      : nav_(nav), renderer_(renderer) {}
  virtual ~FrameNavExpectations() {}

  // Browser and renderer notifications.
  virtual bool OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                                    bool isLoading) {
    return true;
  }
  virtual bool OnLoadStart(CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefFrame> frame) {
    return true;
  }
  virtual bool OnLoadEnd(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame) {
    return true;
  }

  // Final expectations check before this object is deleted.
  virtual bool Finalize() = 0;

  // Signal that all expectations are completed. Should be called as a result of
  // notifications.
  void SignalComplete(CefRefPtr<CefBrowser> browser,
                      CefRefPtr<CefFrame> frame) {
    if (!completion_callback_.is_null()) {
      // Execute the callback asynchronously to avoid any issues with what's
      // currently on the stack.
      CefPostTask(
          (renderer_ ? TID_RENDERER : TID_UI),
          base::BindOnce(std::move(completion_callback_), browser, frame));
    }
  }

  // Returns the current navigation count. In the browser process this value
  // increments over the life span of the FrameNavTestHandler instance. In the
  // renderer process this value increments over the life span of a single
  // renderer instance (i.e. cross-origin navigations will cause this value to
  // reset).
  int nav() const { return nav_; }

  // Returns true if this is a renderer-side expectation object.
  bool renderer() const { return renderer_; }

  void set_completion_callback(CompletionCallback completion_callback) {
    completion_callback_ = std::move(completion_callback);
  }

 private:
  int nav_;
  bool renderer_;
  CompletionCallback completion_callback_;
};

// Browser process expectations abstract base class.
class FrameNavExpectationsBrowser : public FrameNavExpectations {
 public:
  explicit FrameNavExpectationsBrowser(int nav)
      : FrameNavExpectations(nav, false) {}

  // Loading information.
  virtual std::string GetMainURL() = 0;
  virtual std::string GetContentForURL(const std::string& url) = 0;

  // Browser-only notifications.
  virtual bool OnAfterCreated(CefRefPtr<CefBrowser> browser) {
    EXPECT_TRUE(browser.get());
    return true;
  }
  virtual bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                              CefRefPtr<CefFrame> frame,
                              const std::string& url) {
    EXPECT_TRUE(browser.get());
    EXPECT_TRUE(frame.get());
    EXPECT_FALSE(url.empty());
    return true;
  }
  virtual bool GetResourceHandler(CefRefPtr<CefBrowser> browser,
                                  CefRefPtr<CefFrame> frame) {
    EXPECT_TRUE(browser.get());
    EXPECT_TRUE(frame.get());
    return true;
  }

  // Called when the renderer signals completion.
  virtual bool OnRendererComplete(CefRefPtr<CefBrowser> browser,
                                  CefRefPtr<CefFrame> frame,
                                  int renderer_nav,
                                  bool renderer_result) = 0;
};

// Renderer process expectations abstract base class.
class FrameNavExpectationsRenderer : public FrameNavExpectations {
 public:
  explicit FrameNavExpectationsRenderer(int nav)
      : FrameNavExpectations(nav, true) {}
};

// Abstract base class for the factory that creates expectations objects.
class FrameNavExpectationsFactory {
 public:
  FrameNavExpectationsFactory() {}
  virtual ~FrameNavExpectationsFactory() {}

  // Returns the unique ID for this factory type.
  virtual FrameNavFactoryId GetID() const = 0;
};

// Browser process expectations factory abstact base class.
class FrameNavExpectationsFactoryBrowser : public FrameNavExpectationsFactory {
 public:
  FrameNavExpectationsFactoryBrowser() {}

  // Create a new factory instance of the specified type.
  static std::unique_ptr<FrameNavExpectationsFactoryBrowser> FromID(
      FrameNavFactoryId id);

  // Returns true if there will be more navigations in the browser process
  // handler.
  virtual bool HasMoreNavigations() const = 0;

  // Verify final expectations results.
  virtual bool Finalize() = 0;

  std::unique_ptr<FrameNavExpectationsBrowser> Create(
      int nav,
      FrameNavExpectations::CompletionCallback completion_callback) {
    auto expectations = Create(nav);
    expectations->set_completion_callback(std::move(completion_callback));
    return expectations;
  }

 protected:
  // Implement in the test-specific factory instance.
  virtual std::unique_ptr<FrameNavExpectationsBrowser> Create(int nav) = 0;
};

// Renderer process expectations factory abstact base class.
class FrameNavExpectationsFactoryRenderer : public FrameNavExpectationsFactory {
 public:
  FrameNavExpectationsFactoryRenderer() {}

  // Create a new factory instance of the specified type.
  static std::unique_ptr<FrameNavExpectationsFactoryRenderer> FromID(
      FrameNavFactoryId id);

  std::unique_ptr<FrameNavExpectationsRenderer> Create(
      int nav,
      FrameNavExpectations::CompletionCallback completion_callback) {
    auto expectations = Create(nav);
    expectations->set_completion_callback(std::move(completion_callback));
    return expectations;
  }

 protected:
  // Implement in the test-specific factory instance.
  virtual std::unique_ptr<FrameNavExpectationsRenderer> Create(int nav) = 0;
};

// Renderer side handler.
class FrameNavRendererTest : public ClientAppRenderer::Delegate,
                             public CefLoadHandler {
 public:
  FrameNavRendererTest() : run_test_(false), nav_(0) {}

  void OnBrowserCreated(CefRefPtr<ClientAppRenderer> app,
                        CefRefPtr<CefBrowser> browser,
                        CefRefPtr<CefDictionaryValue> extra_info) override {
    if (!extra_info || !extra_info->HasKey(kFrameNavTestCmdKey))
      return;

    FrameNavFactoryId factory_id =
        static_cast<FrameNavFactoryId>(extra_info->GetInt(kFrameNavTestCmdKey));
    run_test_ = factory_id != FNF_ID_INVALID;
    if (!run_test_)
      return;

    factory_ = FrameNavExpectationsFactoryRenderer::FromID(factory_id);
  }

  CefRefPtr<CefLoadHandler> GetLoadHandler(
      CefRefPtr<ClientAppRenderer> app) override {
    if (!run_test_)
      return nullptr;

    return this;
  }

  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading,
                            bool canGoBack,
                            bool canGoForward) override {
    CreateExpectationsIfNecessary();
    EXPECT_TRUE(expectations_->OnLoadingStateChange(browser, isLoading))
        << "isLoading = " << isLoading << ", nav = " << nav_;
  }

  void OnLoadStart(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   TransitionType transition_type) override {
    CreateExpectationsIfNecessary();
    EXPECT_TRUE(expectations_->OnLoadStart(browser, frame)) << "nav = " << nav_;
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    CreateExpectationsIfNecessary();
    EXPECT_TRUE(expectations_->OnLoadEnd(browser, frame)) << "nav = " << nav_;
  }

 protected:
  // Create a new expectations object if one does not already exist for the
  // current navigation.
  void CreateExpectationsIfNecessary() {
    if (expectations_)
      return;
    expectations_ = factory_->Create(
        nav_, base::BindOnce(&FrameNavRendererTest::SendTestResults, this));
  }

  // Send the test results.
  // Will be called via FrameNavExpectations::SignalComplete.
  void SendTestResults(CefRefPtr<CefBrowser> browser,
                       CefRefPtr<CefFrame> frame) {
    // End of the current expectations object.
    EXPECT_TRUE(expectations_->Finalize()) << "nav = " << nav_;
    expectations_.reset(nullptr);

    // Check if the test has failed.
    bool result = !TestFailed();

    // Return the result to the browser process.
    CefRefPtr<CefProcessMessage> return_msg =
        CefProcessMessage::Create(kFrameNavMsg);
    CefRefPtr<CefListValue> args = return_msg->GetArgumentList();
    EXPECT_TRUE(args.get());
    EXPECT_TRUE(args->SetInt(0, nav_));
    EXPECT_TRUE(args->SetBool(1, result));

    const int64 frame_id = frame->GetIdentifier();
    EXPECT_TRUE(args->SetInt(2, CefInt64GetLow(frame_id)));
    EXPECT_TRUE(args->SetInt(3, CefInt64GetHigh(frame_id)));

    frame->SendProcessMessage(PID_BROWSER, return_msg);

    nav_++;
  }

  bool run_test_;
  int nav_;
  std::unique_ptr<FrameNavExpectationsFactoryRenderer> factory_;
  std::unique_ptr<FrameNavExpectationsRenderer> expectations_;

  IMPLEMENT_REFCOUNTING(FrameNavRendererTest);
};

// Browser side handler.
class FrameNavTestHandler : public TestHandler {
 public:
  explicit FrameNavTestHandler(FrameNavFactoryId factory_id)
      : nav_(0),
        factory_(FrameNavExpectationsFactoryBrowser::FromID(factory_id)) {}

  ~FrameNavTestHandler() override { EXPECT_TRUE(got_destroyed_); }

  void RunTest() override {
    // Create the first expectations object.
    expectations_ = factory_->Create(
        nav_, base::BindOnce(&FrameNavTestHandler::RunNextNav, this));

    CefRefPtr<CefDictionaryValue> extra_info = CefDictionaryValue::Create();
    extra_info->SetInt(kFrameNavTestCmdKey, factory_->GetID());

    // Create the browser with the initial URL.
    CreateBrowser(expectations_->GetMainURL(), nullptr, extra_info);

    // Time out the test after a reasonable period of time.
    SetTestTimeout(15000);
  }

  // Transition to the next navigation.
  // Will be called via FrameNavExpectations::SignalComplete.
  void RunNextNav(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame) {
    // End of the current expectations object.
    EXPECT_TRUE(expectations_->Finalize());
    expectations_.reset(nullptr);

    if (!factory_->HasMoreNavigations()) {
      // End of the test.
      DestroyTest();
      return;
    }

    nav_++;

    // Create the next expectations object.
    expectations_ = factory_->Create(
        nav_, base::BindOnce(&FrameNavTestHandler::RunNextNav, this));

    // Load the main URL.
    browser->GetMainFrame()->LoadURL(expectations_->GetMainURL());
  }

  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    TestHandler::OnAfterCreated(browser);

    EXPECT_TRUE(expectations_->OnAfterCreated(browser)) << "nav = " << nav_;
  }

  CefRefPtr<CefResourceHandler> GetResourceHandler(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request) override {
    const std::string& url = request->GetURL();
    if (IgnoreURL(url))
      return nullptr;

    EXPECT_TRUE(expectations_->GetResourceHandler(browser, frame))
        << "nav = " << nav_;

    const std::string& content = expectations_->GetContentForURL(url);
    EXPECT_TRUE(!content.empty()) << "nav = " << nav_;

    CefRefPtr<CefStreamReader> stream = CefStreamReader::CreateForData(
        static_cast<void*>(const_cast<char*>(content.c_str())),
        content.length());
    return new CefStreamResourceHandler("text/html", stream);
  }

  bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                      CefRefPtr<CefFrame> frame,
                      CefRefPtr<CefRequest> request,
                      bool user_gesture,
                      bool is_redirect) override {
    EXPECT_TRUE(
        expectations_->OnBeforeBrowse(browser, frame, request->GetURL()))
        << "nav = " << nav_;

    return false;
  }

  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading,
                            bool canGoBack,
                            bool canGoForward) override {
    EXPECT_TRUE(expectations_->OnLoadingStateChange(browser, isLoading))
        << "isLoading = " << isLoading << ", nav = " << nav_;
    ;
  }

  void OnLoadStart(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   TransitionType transition_type) override {
    EXPECT_TRUE(expectations_->OnLoadStart(browser, frame)) << "nav = " << nav_;
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    EXPECT_TRUE(expectations_->OnLoadEnd(browser, frame)) << "nav = " << nav_;
  }

  bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefProcessId source_process,
                                CefRefPtr<CefProcessMessage> message) override {
    if (message->GetName().ToString() == kFrameNavMsg) {
      // Test that the renderer side succeeded.
      CefRefPtr<CefListValue> args = message->GetArgumentList();
      EXPECT_TRUE(args.get());

      EXPECT_TRUE(expectations_->OnRendererComplete(
          browser, frame, args->GetInt(0), args->GetBool(1)))
          << "nav = " << nav_;

      // Test that browser and render process frame IDs match.
      const int64 frame_id = CefInt64Set(args->GetInt(2), args->GetInt(3));
      EXPECT_EQ(frame->GetIdentifier(), frame_id);

      return true;
    }

    // Message not handled.
    return false;
  }

  void DestroyTest() override {
    if (got_destroyed_)
      return;

    got_destroyed_.yes();

    // The expectations should have been tested already.
    EXPECT_FALSE(expectations_.get());

    // Test that factory conditions we met.
    EXPECT_TRUE(factory_->Finalize()) << "nav = " << nav_;

    TestHandler::DestroyTest();
  }

  int nav_;
  TrackCallback got_destroyed_;
  std::unique_ptr<FrameNavExpectationsFactoryBrowser> factory_;
  std::unique_ptr<FrameNavExpectationsBrowser> expectations_;

  IMPLEMENT_REFCOUNTING(FrameNavTestHandler);
};

// Helper for defining frame tests.
#define FRAME_TEST(name, factory_id)         \
  TEST(FrameTest, name) {                    \
    CefRefPtr<FrameNavTestHandler> handler = \
        new FrameNavTestHandler(factory_id); \
    handler->ExecuteTest();                  \
    ReleaseAndWaitForDestructor(handler);    \
  }

// Browser process expectations for a single navigation.
class FrameNavExpectationsBrowserSingleNav
    : public FrameNavExpectationsBrowser {
 public:
  explicit FrameNavExpectationsBrowserSingleNav(int nav)
      : FrameNavExpectationsBrowser(nav) {}

  ~FrameNavExpectationsBrowserSingleNav() override {
    EXPECT_TRUE(got_finalize_);
  }

  bool OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading) override {
    if (isLoading) {
      EXPECT_FALSE(got_loading_state_change_start_);
      got_loading_state_change_start_.yes();
    } else {
      EXPECT_FALSE(got_loading_state_change_end_);
      got_loading_state_change_end_.yes();
      SignalCompleteIfDone(browser, browser->GetMainFrame());
    }
    return true;
  }

  bool OnLoadStart(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame) override {
    EXPECT_FALSE(got_load_start_);
    got_load_start_.yes();
    return true;
  }

  bool OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame) override {
    EXPECT_FALSE(got_load_end_);
    got_load_end_.yes();
    SignalCompleteIfDone(browser, frame);
    return true;
  }

  bool OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    EXPECT_FALSE(got_after_created_);
    got_after_created_.yes();
    return true;
  }

  bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                      CefRefPtr<CefFrame> frame,
                      const std::string& url) override {
    EXPECT_FALSE(got_before_browse_);
    got_before_browse_.yes();
    return true;
  }

  bool GetResourceHandler(CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefFrame> frame) override {
    EXPECT_FALSE(got_get_resource_handler_);
    got_get_resource_handler_.yes();
    return true;
  }

  bool OnRendererComplete(CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefFrame> frame,
                          int renderer_nav,
                          bool renderer_result) override {
    EXPECT_EQ(nav(), renderer_nav);
    EXPECT_TRUE(renderer_result);
    EXPECT_FALSE(got_renderer_done_);
    got_renderer_done_.yes();
    SignalCompleteIfDone(browser, frame);
    return true;
  }

  bool Finalize() override {
    V_DECLARE();
    V_EXPECT_TRUE(got_load_start_);
    V_EXPECT_TRUE(got_load_end_);
    V_EXPECT_TRUE(got_loading_state_change_start_);
    V_EXPECT_TRUE(got_loading_state_change_end_);
    V_EXPECT_TRUE(got_renderer_done_);
    V_EXPECT_TRUE(got_after_created_);
    V_EXPECT_TRUE(got_before_browse_);
    V_EXPECT_TRUE(got_get_resource_handler_);
    V_EXPECT_FALSE(got_finalize_);

    got_finalize_.yes();

    V_RETURN();
  }

 private:
  void SignalCompleteIfDone(CefRefPtr<CefBrowser> browser,
                            CefRefPtr<CefFrame> frame) {
    if (got_renderer_done_ && got_load_end_ && got_loading_state_change_end_)
      SignalComplete(browser, frame);
  }

  TrackCallback got_load_start_;
  TrackCallback got_load_end_;
  TrackCallback got_loading_state_change_start_;
  TrackCallback got_loading_state_change_end_;
  TrackCallback got_renderer_done_;
  TrackCallback got_after_created_;
  TrackCallback got_before_browse_;
  TrackCallback got_get_resource_handler_;
  TrackCallback got_finalize_;
};

// Renderer process expectations for a single navigation.
class FrameNavExpectationsRendererSingleNav
    : public FrameNavExpectationsRenderer {
 public:
  explicit FrameNavExpectationsRendererSingleNav(int nav)
      : FrameNavExpectationsRenderer(nav) {}

  ~FrameNavExpectationsRendererSingleNav() override {
    EXPECT_TRUE(got_finalize_);
  }

  bool OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading) override {
    if (isLoading) {
      EXPECT_FALSE(got_loading_state_change_start_);
      got_loading_state_change_start_.yes();
    } else {
      EXPECT_FALSE(got_loading_state_change_end_);
      got_loading_state_change_end_.yes();
      SignalCompleteIfDone(browser, browser->GetMainFrame());
    }
    return true;
  }

  bool OnLoadStart(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame) override {
    EXPECT_FALSE(got_load_start_);
    got_load_start_.yes();
    return true;
  }

  bool OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame) override {
    EXPECT_FALSE(got_load_end_);
    got_load_end_.yes();
    SignalCompleteIfDone(browser, frame);
    return true;
  }

  bool Finalize() override {
    V_DECLARE();
    V_EXPECT_TRUE(got_load_start_);
    V_EXPECT_TRUE(got_load_end_);
    V_EXPECT_TRUE(got_loading_state_change_start_);
    V_EXPECT_TRUE(got_loading_state_change_end_);
    V_EXPECT_FALSE(got_finalize_);

    got_finalize_.yes();

    V_RETURN();
  }

 private:
  void SignalCompleteIfDone(CefRefPtr<CefBrowser> browser,
                            CefRefPtr<CefFrame> frame) {
    if (got_load_end_ && got_loading_state_change_end_)
      SignalComplete(browser, frame);
  }

  TrackCallback got_load_start_;
  TrackCallback got_load_end_;
  TrackCallback got_loading_state_change_start_;
  TrackCallback got_loading_state_change_end_;
  TrackCallback got_finalize_;
};

// Test that the single nav harness works.
class FrameNavExpectationsBrowserTestSingleNavHarness
    : public FrameNavExpectationsBrowserSingleNav {
 public:
  typedef FrameNavExpectationsBrowserSingleNav parent;

  explicit FrameNavExpectationsBrowserTestSingleNavHarness(int nav)
      : parent(nav) {}

  ~FrameNavExpectationsBrowserTestSingleNavHarness() override {
    EXPECT_TRUE(got_finalize_);
  }

  std::string GetMainURL() override {
    EXPECT_FALSE(got_get_main_url_);
    got_get_main_url_.yes();
    return kFrameNavOrigin0;
  }

  std::string GetContentForURL(const std::string& url) override {
    EXPECT_FALSE(got_get_content_for_url_);
    got_get_content_for_url_.yes();
    EXPECT_STREQ(kFrameNavOrigin0, url.c_str());
    return "<html><body>Nav</body></html>";
  }

  bool Finalize() override {
    EXPECT_FALSE(got_finalize_);
    got_finalize_.yes();

    V_DECLARE();
    V_EXPECT_TRUE(got_get_main_url_);
    V_EXPECT_TRUE(got_get_content_for_url_);
    V_EXPECT_TRUE(parent::Finalize());
    V_RETURN();
  }

 private:
  TrackCallback got_get_main_url_;
  TrackCallback got_get_content_for_url_;
  TrackCallback got_finalize_;
};

class FrameNavExpectationsRendererTestSingleNavHarness
    : public FrameNavExpectationsRendererSingleNav {
 public:
  typedef FrameNavExpectationsRendererSingleNav parent;

  explicit FrameNavExpectationsRendererTestSingleNavHarness(int nav)
      : parent(nav) {}

  ~FrameNavExpectationsRendererTestSingleNavHarness() override {
    EXPECT_TRUE(got_finalize_);
  }

  bool Finalize() override {
    EXPECT_FALSE(got_finalize_);
    got_finalize_.yes();
    return parent::Finalize();
  }

 private:
  TrackCallback got_finalize_;
};

class FrameNavExpectationsFactoryBrowserTestSingleNavHarness
    : public FrameNavExpectationsFactoryBrowser {
 public:
  FrameNavExpectationsFactoryBrowserTestSingleNavHarness() {}

  ~FrameNavExpectationsFactoryBrowserTestSingleNavHarness() override {
    EXPECT_TRUE(got_finalize_);
  }

  FrameNavFactoryId GetID() const override { return FNF_ID_SINGLE_NAV_HARNESS; }

  bool HasMoreNavigations() const override {
    EXPECT_FALSE(got_get_browser_navigation_count_);
    got_get_browser_navigation_count_.yes();
    return false;
  }

  bool Finalize() override {
    EXPECT_FALSE(got_finalize_);
    got_finalize_.yes();

    V_DECLARE();
    V_EXPECT_TRUE(got_get_browser_navigation_count_);
    V_EXPECT_TRUE(got_create_);
    V_RETURN();
  }

 protected:
  std::unique_ptr<FrameNavExpectationsBrowser> Create(int nav) override {
    EXPECT_FALSE(got_create_);
    got_create_.yes();
    return std::make_unique<FrameNavExpectationsBrowserTestSingleNavHarness>(
        nav);
  }

 private:
  mutable TrackCallback got_get_browser_navigation_count_;
  TrackCallback got_create_;
  TrackCallback got_finalize_;
};

class FrameNavExpectationsFactoryRendererTestSingleNavHarness
    : public FrameNavExpectationsFactoryRenderer {
 public:
  FrameNavExpectationsFactoryRendererTestSingleNavHarness() {}

  FrameNavFactoryId GetID() const override { return FNF_ID_SINGLE_NAV_HARNESS; }

 protected:
  std::unique_ptr<FrameNavExpectationsRenderer> Create(int nav) override {
    return std::make_unique<FrameNavExpectationsRendererTestSingleNavHarness>(
        nav);
  }
};

}  // namespace

// Test that the single nav harness works.
FRAME_TEST(SingleNavHarness, FNF_ID_SINGLE_NAV_HARNESS)

namespace {

bool VerifySingleBrowserFrame(CefRefPtr<CefBrowser> browser,
                              CefRefPtr<CefFrame> frame,
                              const std::string& expected_url) {
  V_DECLARE();
  V_EXPECT_TRUE(frame.get());
  V_EXPECT_TRUE(frame->IsValid());
  const int64 frame_id = frame->GetIdentifier();
  V_EXPECT_TRUE(frame_id > 0) << frame_id;
  V_EXPECT_TRUE(frame->IsValid());
  V_EXPECT_TRUE(frame->IsMain());
  V_EXPECT_TRUE(frame->IsFocused());
  V_EXPECT_FALSE(frame->GetParent().get());
  V_EXPECT_TRUE(frame->GetName().empty());
  V_EXPECT_TRUE(browser->GetIdentifier() ==
                frame->GetBrowser()->GetIdentifier());

  const std::string& frame_url = frame->GetURL();
  V_EXPECT_TRUE(frame_url == expected_url)
      << "frame_url = " << frame_url << ", expected_url = " << expected_url;

  V_RETURN();
}

bool VerifySingleBrowserFrames(CefRefPtr<CefBrowser> browser,
                               CefRefPtr<CefFrame> frame,
                               const std::string& expected_url) {
  V_DECLARE();
  V_EXPECT_TRUE(browser.get());

  // |frame| may be nullptr for callbacks that don't specify one.
  if (frame.get()) {
    V_EXPECT_TRUE(VerifySingleBrowserFrame(browser, frame, expected_url));
  }

  CefRefPtr<CefFrame> main_frame = browser->GetMainFrame();
  V_EXPECT_TRUE(VerifySingleBrowserFrame(browser, main_frame, expected_url));

  CefRefPtr<CefFrame> focused_frame = browser->GetFocusedFrame();
  V_EXPECT_TRUE(VerifySingleBrowserFrame(browser, focused_frame, expected_url));

  size_t frame_count = browser->GetFrameCount();
  V_EXPECT_TRUE(frame_count == 1U);

  std::vector<int64> identifiers;
  browser->GetFrameIdentifiers(identifiers);
  V_EXPECT_TRUE(identifiers.size() == 1U);
  if (identifiers.size() == 1U) {
    V_EXPECT_TRUE(identifiers[0] == main_frame->GetIdentifier());
    V_EXPECT_TRUE(identifiers[0] == focused_frame->GetIdentifier());
  }

  // Names may be empty for callbacks that execute while the frame is loading.
  std::vector<CefString> names;
  browser->GetFrameNames(names);
  V_EXPECT_TRUE(names.size() <= 1U);
  if (names.size() == 1U) {
    V_EXPECT_TRUE(names[0].ToString() == main_frame->GetName().ToString());
    V_EXPECT_TRUE(names[0].ToString() == focused_frame->GetName().ToString());
  }

  V_RETURN();
}

// Test that single navigation works.
class FrameNavExpectationsBrowserTestSingleNav
    : public FrameNavExpectationsBrowserSingleNav {
 public:
  typedef FrameNavExpectationsBrowserSingleNav parent;

  explicit FrameNavExpectationsBrowserTestSingleNav(int nav) : parent(nav) {}

  std::string GetMainURL() override { return kFrameNavOrigin0; }

  std::string GetContentForURL(const std::string& url) override {
    return "<html><body>Nav</body></html>";
  }

  bool OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading) override {
    V_DECLARE();
    V_EXPECT_TRUE(VerifySingleBrowserFrames(
        browser, nullptr, isLoading ? std::string() : kFrameNavOrigin0));
    V_EXPECT_TRUE(parent::OnLoadingStateChange(browser, isLoading));
    V_RETURN();
  }

  bool OnLoadStart(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame) override {
    V_DECLARE();
    V_EXPECT_TRUE(VerifySingleBrowserFrames(browser, frame, kFrameNavOrigin0));
    V_EXPECT_TRUE(parent::OnLoadStart(browser, frame));
    V_RETURN();
  }

  bool OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame) override {
    V_DECLARE();
    V_EXPECT_TRUE(VerifySingleBrowserFrames(browser, frame, kFrameNavOrigin0));
    V_EXPECT_TRUE(parent::OnLoadEnd(browser, frame));
    V_RETURN();
  }

  bool OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    V_DECLARE();
    V_EXPECT_TRUE(VerifySingleBrowserFrames(browser, nullptr, std::string()));
    V_EXPECT_TRUE(parent::OnAfterCreated(browser));
    V_RETURN();
  }

  bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                      CefRefPtr<CefFrame> frame,
                      const std::string& url) override {
    V_DECLARE();
    V_EXPECT_TRUE(VerifySingleBrowserFrames(browser, frame, std::string()));
    V_EXPECT_TRUE(parent::OnBeforeBrowse(browser, frame, url));
    V_RETURN();
  }

  bool GetResourceHandler(CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefFrame> frame) override {
    V_DECLARE();
    V_EXPECT_TRUE(VerifySingleBrowserFrames(browser, frame, std::string()));
    V_EXPECT_TRUE(parent::GetResourceHandler(browser, frame));
    V_RETURN();
  }

  bool OnRendererComplete(CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefFrame> frame,
                          int renderer_nav,
                          bool renderer_result) override {
    return parent::OnRendererComplete(browser, frame, renderer_nav,
                                      renderer_result);
  }

  bool Finalize() override { return parent::Finalize(); }
};

class FrameNavExpectationsRendererTestSingleNav
    : public FrameNavExpectationsRendererSingleNav {
 public:
  typedef FrameNavExpectationsRendererSingleNav parent;

  explicit FrameNavExpectationsRendererTestSingleNav(int nav) : parent(nav) {}

  bool OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading) override {
    V_DECLARE();
    // A frame should always exist in the renderer process.
    V_EXPECT_TRUE(
        VerifySingleBrowserFrames(browser, nullptr, kFrameNavOrigin0));
    V_EXPECT_TRUE(parent::OnLoadingStateChange(browser, isLoading));
    V_RETURN();
  }

  bool OnLoadStart(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame) override {
    V_DECLARE();
    V_EXPECT_TRUE(VerifySingleBrowserFrames(browser, frame, kFrameNavOrigin0));
    V_EXPECT_TRUE(parent::OnLoadStart(browser, frame));
    V_RETURN();
  }

  bool OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame) override {
    V_DECLARE();
    V_EXPECT_TRUE(VerifySingleBrowserFrames(browser, frame, kFrameNavOrigin0));
    V_EXPECT_TRUE(parent::OnLoadEnd(browser, frame));
    V_RETURN();
  }

  bool Finalize() override { return parent::Finalize(); }
};

class FrameNavExpectationsFactoryBrowserTestSingleNav
    : public FrameNavExpectationsFactoryBrowser {
 public:
  FrameNavExpectationsFactoryBrowserTestSingleNav() {}

  FrameNavFactoryId GetID() const override { return FNF_ID_SINGLE_NAV; }

  bool HasMoreNavigations() const override { return false; }

  bool Finalize() override { return true; }

 protected:
  std::unique_ptr<FrameNavExpectationsBrowser> Create(int nav) override {
    return std::make_unique<FrameNavExpectationsBrowserTestSingleNav>(nav);
  }
};

class FrameNavExpectationsFactoryRendererTestSingleNav
    : public FrameNavExpectationsFactoryRenderer {
 public:
  FrameNavExpectationsFactoryRendererTestSingleNav() {}

  FrameNavFactoryId GetID() const override { return FNF_ID_SINGLE_NAV; }

 protected:
  std::unique_ptr<FrameNavExpectationsRenderer> Create(int nav) override {
    return std::make_unique<FrameNavExpectationsRendererTestSingleNav>(nav);
  }
};

}  // namespace

// Test that single navigation works.
FRAME_TEST(SingleNav, FNF_ID_SINGLE_NAV)

namespace {

// Browser process expectations for a multiple navigations.
class FrameNavExpectationsBrowserMultiNav : public FrameNavExpectationsBrowser {
 public:
  explicit FrameNavExpectationsBrowserMultiNav(int nav)
      : FrameNavExpectationsBrowser(nav) {}

  ~FrameNavExpectationsBrowserMultiNav() override {
    EXPECT_TRUE(got_finalize_);
  }

  // Returns true if all navigation is done.
  virtual bool IsNavigationDone() const = 0;

  bool OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading) override {
    if (!isLoading)
      SignalCompleteIfDone(browser, browser->GetMainFrame());
    return true;
  }

  bool OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame) override {
    SignalCompleteIfDone(browser, frame);
    return true;
  }

  bool OnRendererComplete(CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefFrame> frame,
                          int renderer_nav,
                          bool renderer_result) override {
    EXPECT_TRUE(renderer_result);
    SignalCompleteIfDone(browser, frame);
    return true;
  }

  bool Finalize() override {
    V_DECLARE();
    V_EXPECT_FALSE(got_finalize_);

    got_finalize_.yes();

    V_RETURN();
  }

 private:
  void SignalCompleteIfDone(CefRefPtr<CefBrowser> browser,
                            CefRefPtr<CefFrame> frame) {
    if (IsNavigationDone())
      SignalComplete(browser, frame);
  }

  TrackCallback got_finalize_;
};

// Renderer process expectations for a multiple navigations.
class FrameNavExpectationsRendererMultiNav
    : public FrameNavExpectationsRenderer {
 public:
  explicit FrameNavExpectationsRendererMultiNav(int nav)
      : FrameNavExpectationsRenderer(nav) {}

  ~FrameNavExpectationsRendererMultiNav() override {
    EXPECT_TRUE(got_finalize_);
  }

  // Returns true if all navigation is done.
  virtual bool IsNavigationDone() const = 0;

  bool OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading) override {
    if (!isLoading)
      SignalCompleteIfDone(browser, browser->GetMainFrame());
    return true;
  }

  bool OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame) override {
    SignalCompleteIfDone(browser, frame);
    return true;
  }

  bool Finalize() override {
    V_DECLARE();
    V_EXPECT_FALSE(got_finalize_);

    got_finalize_.yes();

    V_RETURN();
  }

 private:
  void SignalCompleteIfDone(CefRefPtr<CefBrowser> browser,
                            CefRefPtr<CefFrame> frame) {
    if (IsNavigationDone())
      SignalComplete(browser, frame);
  }

  TrackCallback got_finalize_;
};

// Create a URL containing the nav number.
std::string GetMultiNavURL(const std::string& origin, int nav) {
  std::stringstream ss;
  ss << origin << "nav" << nav << ".html";
  return ss.str();
}

// Extract the nav number from the URL.
int GetNavFromMultiNavURL(const std::string& url) {
  const size_t start = url.find("/nav");
  const size_t end = url.find(".html", start);
  EXPECT_TRUE(start < end && start > 0U);
  const std::string& nav = url.substr(start + 4, end - start - 4);
  return atoi(nav.c_str());
}

// Extract the origin from the URL.
std::string GetOriginFromMultiNavURL(const std::string& url) {
  const size_t pos = url.rfind("/");
  EXPECT_TRUE(pos > 0U);
  return url.substr(0, pos + 1);
}

// Test that the multi nav harness works.
class FrameNavExpectationsBrowserTestMultiNavHarness
    : public FrameNavExpectationsBrowserMultiNav {
 public:
  typedef FrameNavExpectationsBrowserMultiNav parent;

  explicit FrameNavExpectationsBrowserTestMultiNavHarness(int nav)
      : parent(nav), navigation_done_count_(0) {}

  ~FrameNavExpectationsBrowserTestMultiNavHarness() override {
    EXPECT_TRUE(got_finalize_);
  }

  std::string GetMainURL() override {
    EXPECT_FALSE(got_get_main_url_);
    got_get_main_url_.yes();
    return GetMultiNavURL(kFrameNavOrigin0, nav());
  }

  std::string GetContentForURL(const std::string& url) override {
    EXPECT_FALSE(got_get_content_for_url_);
    got_get_content_for_url_.yes();
    EXPECT_STREQ(GetMultiNavURL(kFrameNavOrigin0, nav()).c_str(), url.c_str());
    return "<html><body>Nav</body></html>";
  }

  bool IsNavigationDone() const override {
    navigation_done_count_++;
    return got_load_state_change_done_ && got_load_end_ &&
           got_renderer_complete_;
  }

  bool OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading) override {
    if (!isLoading) {
      EXPECT_FALSE(got_load_state_change_done_);
      got_load_state_change_done_.yes();
    }
    return parent::OnLoadingStateChange(browser, isLoading);
  }

  bool OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame) override {
    EXPECT_FALSE(got_load_end_);
    got_load_end_.yes();
    return parent::OnLoadEnd(browser, frame);
  }

  bool OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    EXPECT_FALSE(got_on_after_created_);
    got_on_after_created_.yes();
    return parent::OnAfterCreated(browser);
  }

  bool OnRendererComplete(CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefFrame> frame,
                          int renderer_nav,
                          bool renderer_result) override {
    EXPECT_FALSE(got_renderer_complete_);
    got_renderer_complete_.yes();
    EXPECT_EQ(nav(), renderer_nav);
    return parent::OnRendererComplete(browser, frame, renderer_nav,
                                      renderer_result);
  }

  bool Finalize() override {
    EXPECT_FALSE(got_finalize_);
    got_finalize_.yes();

    V_DECLARE();
    V_EXPECT_TRUE(got_get_main_url_);
    V_EXPECT_TRUE(got_get_content_for_url_);
    V_EXPECT_TRUE(got_load_state_change_done_);
    V_EXPECT_TRUE(got_load_end_);
    if (nav() == 0) {
      V_EXPECT_TRUE(got_on_after_created_);
    } else {
      V_EXPECT_FALSE(got_on_after_created_);
    }
    V_EXPECT_TRUE(got_renderer_complete_);
    V_EXPECT_TRUE(navigation_done_count_ == 3);
    V_EXPECT_TRUE(parent::Finalize());
    V_RETURN();
  }

 private:
  TrackCallback got_get_main_url_;
  TrackCallback got_get_content_for_url_;
  TrackCallback got_load_state_change_done_;
  TrackCallback got_load_end_;
  TrackCallback got_on_after_created_;
  TrackCallback got_renderer_complete_;
  mutable int navigation_done_count_;
  TrackCallback got_finalize_;
};

class FrameNavExpectationsRendererTestMultiNavHarness
    : public FrameNavExpectationsRendererMultiNav {
 public:
  typedef FrameNavExpectationsRendererMultiNav parent;

  explicit FrameNavExpectationsRendererTestMultiNavHarness(int nav)
      : parent(nav), navigation_done_count_(0) {}

  ~FrameNavExpectationsRendererTestMultiNavHarness() override {
    EXPECT_TRUE(got_finalize_);
  }

  bool IsNavigationDone() const override {
    navigation_done_count_++;
    return got_load_state_change_done_ && got_load_end_;
  }

  bool OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading) override {
    if (!isLoading) {
      EXPECT_FALSE(got_load_state_change_done_);
      got_load_state_change_done_.yes();
    }
    return parent::OnLoadingStateChange(browser, isLoading);
  }

  bool OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame) override {
    EXPECT_FALSE(got_load_end_);
    got_load_end_.yes();
    return parent::OnLoadEnd(browser, frame);
  }

  bool Finalize() override {
    EXPECT_FALSE(got_finalize_);
    got_finalize_.yes();

    V_DECLARE();
    V_EXPECT_TRUE(got_load_state_change_done_);
    V_EXPECT_TRUE(got_load_end_);
    V_EXPECT_TRUE(navigation_done_count_ == 2);
    V_EXPECT_TRUE(parent::Finalize());
    V_RETURN();
  }

 private:
  TrackCallback got_load_state_change_done_;
  TrackCallback got_load_end_;
  mutable int navigation_done_count_;
  TrackCallback got_finalize_;
};

class FrameNavExpectationsFactoryBrowserTestMultiNavHarness
    : public FrameNavExpectationsFactoryBrowser {
 public:
  FrameNavExpectationsFactoryBrowserTestMultiNavHarness()
      : get_browser_navigation_count_(0), create_count_(0) {}

  ~FrameNavExpectationsFactoryBrowserTestMultiNavHarness() override {
    EXPECT_TRUE(got_finalize_);
  }

  FrameNavFactoryId GetID() const override { return FNF_ID_MULTI_NAV_HARNESS; }

  bool HasMoreNavigations() const override {
    get_browser_navigation_count_++;
    return (get_browser_navigation_count_ < kMaxMultiNavNavigations);
  }

  bool Finalize() override {
    EXPECT_FALSE(got_finalize_);
    got_finalize_.yes();

    V_DECLARE();
    V_EXPECT_TRUE(get_browser_navigation_count_ == kMaxMultiNavNavigations);
    V_EXPECT_TRUE(create_count_ == kMaxMultiNavNavigations);
    V_RETURN();
  }

 protected:
  std::unique_ptr<FrameNavExpectationsBrowser> Create(int nav) override {
    create_count_++;
    return std::make_unique<FrameNavExpectationsBrowserTestMultiNavHarness>(
        nav);
  }

 private:
  mutable int get_browser_navigation_count_;
  int create_count_;
  TrackCallback got_finalize_;
};

class FrameNavExpectationsFactoryRendererTestMultiNavHarness
    : public FrameNavExpectationsFactoryRenderer {
 public:
  FrameNavExpectationsFactoryRendererTestMultiNavHarness() {}

  FrameNavFactoryId GetID() const override { return FNF_ID_MULTI_NAV_HARNESS; }

 protected:
  std::unique_ptr<FrameNavExpectationsRenderer> Create(int nav) override {
    return std::make_unique<FrameNavExpectationsRendererTestMultiNavHarness>(
        nav);
  }
};

}  // namespace

// Test that the multiple nav harness works.
FRAME_TEST(MultiNavHarness, FNF_ID_MULTI_NAV_HARNESS)

namespace {

// Test that multiple navigation works.
class FrameNavExpectationsBrowserTestMultiNav
    : public FrameNavExpectationsBrowserMultiNav {
 public:
  typedef FrameNavExpectationsBrowserMultiNav parent;

  explicit FrameNavExpectationsBrowserTestMultiNav(int nav) : parent(nav) {}

  std::string GetMainURL() override {
    return GetMultiNavURL(kFrameNavOrigin0, nav());
  }

  std::string GetContentForURL(const std::string& url) override {
    return "<html><body>Nav</body></html>";
  }

  bool IsNavigationDone() const override {
    return got_load_state_change_done_ && got_load_end_ &&
           got_renderer_complete_;
  }

  bool OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading) override {
    if (!isLoading)
      got_load_state_change_done_.yes();
    V_DECLARE();
    if (isLoading && nav() == 0) {
      V_EXPECT_TRUE(VerifySingleBrowserFrames(browser, nullptr, std::string()));
    } else if (isLoading) {
      // Expect the URL from the previous load.
      V_EXPECT_TRUE(
          VerifySingleBrowserFrames(browser, nullptr, GetPreviousMainURL()));
    } else {
      V_EXPECT_TRUE(VerifySingleBrowserFrames(browser, nullptr, GetMainURL()));
    }
    V_EXPECT_TRUE(parent::OnLoadingStateChange(browser, isLoading));
    V_RETURN();
  }

  bool OnLoadStart(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame) override {
    V_DECLARE();
    V_EXPECT_TRUE(VerifySingleBrowserFrames(browser, frame, GetMainURL()));
    V_EXPECT_TRUE(parent::OnLoadStart(browser, frame));
    V_RETURN();
  }

  bool OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame) override {
    got_load_end_.yes();
    V_DECLARE();
    V_EXPECT_TRUE(VerifySingleBrowserFrames(browser, frame, GetMainURL()));
    V_EXPECT_TRUE(parent::OnLoadEnd(browser, frame));
    V_RETURN();
  }

  bool OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    V_DECLARE();
    V_EXPECT_TRUE(VerifySingleBrowserFrames(browser, nullptr, std::string()));
    V_EXPECT_TRUE(parent::OnAfterCreated(browser));
    V_RETURN();
  }

  bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                      CefRefPtr<CefFrame> frame,
                      const std::string& url) override {
    V_DECLARE();
    std::string expected_url;
    if (nav() > 0)
      expected_url = GetPreviousMainURL();
    V_EXPECT_TRUE(VerifySingleBrowserFrames(browser, frame, expected_url));
    V_EXPECT_TRUE(parent::OnBeforeBrowse(browser, frame, url));
    V_RETURN();
  }

  bool GetResourceHandler(CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefFrame> frame) override {
    V_DECLARE();
    std::string expected_url;
    if (nav() > 0)
      expected_url = GetPreviousMainURL();
    V_EXPECT_TRUE(VerifySingleBrowserFrames(browser, frame, expected_url));
    V_EXPECT_TRUE(parent::GetResourceHandler(browser, frame));
    V_RETURN();
  }

  bool OnRendererComplete(CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefFrame> frame,
                          int renderer_nav,
                          bool renderer_result) override {
    got_renderer_complete_.yes();
    V_DECLARE();
    V_EXPECT_TRUE(nav() == renderer_nav);
    V_EXPECT_TRUE(parent::OnRendererComplete(browser, frame, renderer_nav,
                                             renderer_result));
    V_RETURN();
  }

  bool Finalize() override {
    V_DECLARE();
    V_EXPECT_TRUE(got_load_state_change_done_);
    V_EXPECT_TRUE(got_load_end_);
    V_EXPECT_TRUE(got_renderer_complete_);
    V_EXPECT_TRUE(parent::Finalize());
    V_RETURN();
  }

 private:
  // Helper for VerifySingleBrowserFrames.
  std::string GetPreviousMainURL() {
    EXPECT_GT(nav(), 0);
    return GetMultiNavURL(kFrameNavOrigin0, nav() - 1);
  }

  TrackCallback got_load_state_change_done_;
  TrackCallback got_load_end_;
  TrackCallback got_renderer_complete_;
};

class FrameNavExpectationsRendererTestMultiNav
    : public FrameNavExpectationsRendererMultiNav {
 public:
  typedef FrameNavExpectationsRendererMultiNav parent;

  explicit FrameNavExpectationsRendererTestMultiNav(int nav) : parent(nav) {}

  bool IsNavigationDone() const override {
    return got_load_state_change_done_ && got_load_end_;
  }

  bool OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading) override {
    if (!isLoading)
      got_load_state_change_done_.yes();
    V_DECLARE();
    V_EXPECT_TRUE(VerifySingleBrowserFrames(browser, nullptr, GetMainURL()));
    V_EXPECT_TRUE(parent::OnLoadingStateChange(browser, isLoading));
    V_RETURN();
  }

  bool OnLoadStart(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame) override {
    V_DECLARE();
    V_EXPECT_TRUE(VerifySingleBrowserFrames(browser, frame, GetMainURL()));
    V_EXPECT_TRUE(parent::OnLoadStart(browser, frame));
    V_RETURN();
  }

  bool OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame) override {
    got_load_end_.yes();
    V_DECLARE();
    V_EXPECT_TRUE(VerifySingleBrowserFrames(browser, frame, GetMainURL()));
    V_EXPECT_TRUE(parent::OnLoadEnd(browser, frame));
    V_RETURN();
  }

  bool Finalize() override {
    V_DECLARE();
    V_EXPECT_TRUE(got_load_state_change_done_);
    V_EXPECT_TRUE(got_load_end_);
    V_EXPECT_TRUE(parent::Finalize());
    V_RETURN();
  }

 private:
  // Helpers for calling VerifySingleBrowserFrames.
  std::string GetMainURL() const {
    return GetMultiNavURL(kFrameNavOrigin0, nav());
  }
  std::string GetPreviousMainURL() {
    EXPECT_GT(nav(), 0);
    return GetMultiNavURL(kFrameNavOrigin0, nav() - 1);
  }

  TrackCallback got_load_state_change_done_;
  TrackCallback got_load_end_;
};

class FrameNavExpectationsFactoryBrowserTestMultiNav
    : public FrameNavExpectationsFactoryBrowser {
 public:
  FrameNavExpectationsFactoryBrowserTestMultiNav() : nav_count_(0) {}

  FrameNavFactoryId GetID() const override { return FNF_ID_MULTI_NAV; }

  bool HasMoreNavigations() const override {
    return (nav_count_ < kMaxMultiNavNavigations);
  }

  bool Finalize() override {
    V_DECLARE();
    V_EXPECT_TRUE(nav_count_ == kMaxMultiNavNavigations);
    V_RETURN();
  }

 protected:
  std::unique_ptr<FrameNavExpectationsBrowser> Create(int nav) override {
    nav_count_++;
    return std::make_unique<FrameNavExpectationsBrowserTestMultiNav>(nav);
  }

 private:
  int nav_count_;
};

class FrameNavExpectationsFactoryRendererTestMultiNav
    : public FrameNavExpectationsFactoryRenderer {
 public:
  FrameNavExpectationsFactoryRendererTestMultiNav() {}

  FrameNavFactoryId GetID() const override { return FNF_ID_MULTI_NAV; }

 protected:
  std::unique_ptr<FrameNavExpectationsRenderer> Create(int nav) override {
    return std::make_unique<FrameNavExpectationsRendererTestMultiNav>(nav);
  }
};

}  // namespace

// Test that multiple navigation works.
FRAME_TEST(MultiNav, FNF_ID_MULTI_NAV)

namespace {

const char kFrame0Name[] = "";
const char kFrame1Name[] = "nav2";
const char kFrame2Name[] = "<!--framePath //nav2/<!--frame0-->-->";
const char kFrame3Name[] = "nav3";

bool VerifyBrowserIframe(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         const std::string& origin,
                         int frame_number) {
  V_DECLARE();

  // frame0 contains frame1 contains frame2, contains frame3.
  CefRefPtr<CefFrame> frame0, frame1, frame2, frame3;
  CefRefPtr<CefFrame> frame0b, frame1b, frame2b, frame3b;
  int64 frame0id, frame1id, frame2id, frame3id;
  std::string frame0url, frame1url, frame2url, frame3url;

  // Verify the GetFrameNames result.
  std::set<std::string> expected_names = {kFrame0Name, kFrame1Name, kFrame2Name,
                                          kFrame3Name};

  std::vector<CefString> names;
  browser->GetFrameNames(names);
  V_EXPECT_TRUE(names.size() == expected_names.size())
      << "expected: " << expected_names.size() << " actual: " << names.size();

  for (const auto& name : names) {
    const std::string& nameStr = name;
    auto it = expected_names.find(nameStr);
    V_EXPECT_FALSE(it == expected_names.end())
        << "Unexpected name: \"" << nameStr << "\"";
    if (it != expected_names.end())
      expected_names.erase(it);
  }

  for (const auto& name : expected_names) {
    V_EXPECT_FALSE(true) << "Missing name: \"" << name << "\"";
  }

  // Find frames by name.
  frame0 = browser->GetFrame(kFrame0Name);
  V_EXPECT_TRUE(frame0.get());
  frame1 = browser->GetFrame(kFrame1Name);
  V_EXPECT_TRUE(frame1.get());
  frame2 = browser->GetFrame(kFrame2Name);
  V_EXPECT_TRUE(frame2.get());
  frame3 = browser->GetFrame(kFrame3Name);
  V_EXPECT_TRUE(frame3.get());

  if (!frame0 || !frame1 || !frame2 || !frame3) {
    V_RETURN();
  }

  // Verify that the name matches.
  V_EXPECT_TRUE(frame0->GetName().ToString() == kFrame0Name)
      << "expected: " << kFrame0Name
      << " actual: " << frame0->GetName().ToString();
  V_EXPECT_TRUE(frame1->GetName().ToString() == kFrame1Name)
      << "expected: " << kFrame1Name
      << " actual: " << frame1->GetName().ToString();
  V_EXPECT_TRUE(frame2->GetName().ToString() == kFrame2Name)
      << "expected: " << kFrame2Name
      << " actual: " << frame2->GetName().ToString();
  V_EXPECT_TRUE(frame3->GetName().ToString() == kFrame3Name)
      << "expected: " << kFrame3Name
      << " actual: " << frame3->GetName().ToString();

  // Verify that the URL matches.
  frame0url = GetMultiNavURL(origin, 0);
  V_EXPECT_TRUE(frame0->GetURL() == frame0url)
      << "expected: " << frame0url
      << " actual: " << frame0->GetURL().ToString();
  frame1url = GetMultiNavURL(origin, 1);
  V_EXPECT_TRUE(frame1->GetURL() == frame1url)
      << "expected: " << frame1url
      << " actual: " << frame1->GetURL().ToString();
  frame2url = GetMultiNavURL(origin, 2);
  V_EXPECT_TRUE(frame2->GetURL() == frame2url)
      << "expected: " << frame2url
      << " actual: " << frame2->GetURL().ToString();
  frame3url = GetMultiNavURL(origin, 3);
  V_EXPECT_TRUE(frame3->GetURL() == frame3url)
      << "expected: " << frame3url
      << " actual: " << frame3->GetURL().ToString();

  // Verify that the frame id is valid.
  frame0id = frame0->GetIdentifier();
  V_EXPECT_TRUE(frame0id > 0) << "actual: " << frame0id;
  frame1id = frame1->GetIdentifier();
  V_EXPECT_TRUE(frame1id > 0) << "actual: " << frame1id;
  frame2id = frame2->GetIdentifier();
  V_EXPECT_TRUE(frame2id > 0) << "actual: " << frame2id;
  frame3id = frame3->GetIdentifier();
  V_EXPECT_TRUE(frame3id > 0) << "actual: " << frame3id;

  // Verify that the current frame has the correct id.
  if (frame_number == 0) {
    V_EXPECT_TRUE(frame->GetIdentifier() == frame0id)
        << "expected: " << frame0id << " actual: " << frame->GetIdentifier();
  } else if (frame_number == 1) {
    V_EXPECT_TRUE(frame->GetIdentifier() == frame1id)
        << "expected: " << frame1id << " actual: " << frame->GetIdentifier();
  } else if (frame_number == 2) {
    V_EXPECT_TRUE(frame->GetIdentifier() == frame2id)
        << "expected: " << frame2id << " actual: " << frame->GetIdentifier();
  } else if (frame_number == 3) {
    V_EXPECT_TRUE(frame->GetIdentifier() == frame3id)
        << "expected: " << frame3id << " actual: " << frame->GetIdentifier();
  }

  // Find frames by id.
  frame0b = browser->GetFrame(frame0->GetIdentifier());
  V_EXPECT_TRUE(frame0b.get());
  frame1b = browser->GetFrame(frame1->GetIdentifier());
  V_EXPECT_TRUE(frame1b.get());
  frame2b = browser->GetFrame(frame2->GetIdentifier());
  V_EXPECT_TRUE(frame2b.get());
  frame3b = browser->GetFrame(frame3->GetIdentifier());
  V_EXPECT_TRUE(frame3b.get());

  if (!frame0b || !frame1b || !frame2b || !frame3b) {
    V_RETURN();
  }

  // Verify that the id matches.
  V_EXPECT_TRUE(frame0b->GetIdentifier() == frame0id)
      << "expected: " << frame0id << " actual: " << frame0b->GetIdentifier();
  V_EXPECT_TRUE(frame1b->GetIdentifier() == frame1id)
      << "expected: " << frame1id << " actual: " << frame1b->GetIdentifier();
  V_EXPECT_TRUE(frame2b->GetIdentifier() == frame2id)
      << "expected: " << frame2id << " actual: " << frame2b->GetIdentifier();
  V_EXPECT_TRUE(frame3b->GetIdentifier() == frame3id)
      << "expected: " << frame3id << " actual: " << frame3b->GetIdentifier();

  size_t frame_count = browser->GetFrameCount();
  V_EXPECT_TRUE(frame_count == 4U) << " actual: " << frame_count;

  // Verify the GetFrameIdentifiers result.
  std::set<int64> expected_idents = {frame0id, frame1id, frame2id, frame3id};

  std::vector<int64> idents;
  browser->GetFrameIdentifiers(idents);
  V_EXPECT_TRUE(idents.size() == expected_idents.size())
      << "expected: " << expected_idents.size() << " actual: " << idents.size();

  for (const auto& ident : idents) {
    auto it = expected_idents.find(ident);
    V_EXPECT_FALSE(it == expected_idents.end()) << "Unexpected id: " << ident;
    if (it != expected_idents.end())
      expected_idents.erase(it);
  }

  for (const auto& ident : expected_idents) {
    V_EXPECT_FALSE(true) << "Missing id: " << ident;
  }

  // Verify parent hierarchy.
  V_EXPECT_FALSE(frame0->GetParent().get());
  V_EXPECT_TRUE(frame1->GetParent()->GetIdentifier() == frame0id)
      << "expected: " << frame0id
      << " actual: " << frame1->GetParent()->GetIdentifier();
  V_EXPECT_TRUE(frame2->GetParent()->GetIdentifier() == frame1id)
      << "expected: " << frame1id
      << " actual: " << frame2->GetParent()->GetIdentifier();
  V_EXPECT_TRUE(frame3->GetParent()->GetIdentifier() == frame2id)
      << "expected: " << frame2id
      << " actual: " << frame3->GetParent()->GetIdentifier();

  V_RETURN();
}

// Test that nested iframes work.
class FrameNavExpectationsBrowserTestNestedIframes
    : public FrameNavExpectationsBrowserMultiNav {
 public:
  typedef FrameNavExpectationsBrowserMultiNav parent;

  FrameNavExpectationsBrowserTestNestedIframes(int nav, bool same_origin)
      : parent(nav), same_origin_(same_origin) {
    // In the browser process we can rely on the |nav| value to determine the
    // origin.
    if (same_origin) {
      origin_ = kFrameNavOrigin0;
    } else {
      switch (nav) {
        case 0:
          origin_ = kFrameNavOrigin0;
          break;
        case 1:
          origin_ = kFrameNavOrigin1;
          break;
        case 2:
          origin_ = kFrameNavOrigin2;
          break;
        case 3:
          origin_ = kFrameNavOrigin3;
          break;
        default:
          EXPECT_TRUE(false);  // Not reached.
          break;
      }
    }
  }

  std::string GetMainURL() override {
    // Load the first (main) frame.
    return GetMultiNavURL(origin_, 0);
  }

  std::string GetContentForURL(const std::string& url) override {
    const int frame_number = GetNavFromMultiNavURL(url);
    switch (frame_number) {
      case 0:
        // Frame 0. Contains a named iframe.
        return "<html><body>Nav1<iframe src=\"" + GetMultiNavURL(origin_, 1) +
               "\" name=\"nav2\"></body></html>";
      case 1:
        // Frame 1. Contains an unnamed iframe.
        return "<html><body>Nav2<iframe src=\"" + GetMultiNavURL(origin_, 2) +
               "\"></body></html>";
      case 2: {
        // Frame 2. Contains an named iframe created via javascript.
        std::stringstream ss;
        ss << "<html><script>"
           << "  function createFrame() {"
           << "    var f = document.createElement('iframe');"
           << "    f.name = 'nav3';"
           << "    f.src = '" << GetMultiNavURL(origin_, 3) << "';"
           << "    document.body.appendChild(f);"
           << "  }</script><body onload=\"createFrame()\">Nav3</body></html>";
        return ss.str();
      }
      case 3:
        // Frame 3.
        return "<html><body>Nav4</body></html>";
      default:
        EXPECT_TRUE(false);  // Not reached.
        return "";
    }
  }

  bool IsNavigationDone() const override {
    return got_load_state_change_done_ && got_renderer_complete_ &&
           got_load_end_[0] && got_load_end_[1] && got_load_end_[2] &&
           got_load_end_[3];
  }

  bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                      CefRefPtr<CefFrame> frame,
                      const std::string& url) override {
    V_DECLARE();
    V_EXPECT_TRUE(frame.get());
    const int frame_number = GetNavFromMultiNavURL(url);
    if (frame_number == 0) {
      // Main frame.
      V_EXPECT_TRUE(frame->IsMain());
    } else {
      // Sub frame.
      V_EXPECT_FALSE(frame->IsMain());
    }
    V_EXPECT_TRUE(parent::OnBeforeBrowse(browser, frame, url));
    V_RETURN();
  }

  bool OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading) override {
    V_DECLARE();
    V_EXPECT_FALSE(got_load_state_change_done_);

    if (!isLoading) {
      got_load_state_change_done_.yes();
    }

    V_EXPECT_TRUE(parent::OnLoadingStateChange(browser, isLoading));
    V_RETURN();
  }

  bool OnLoadStart(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame) override {
    const int frame_number = GetNavFromMultiNavURL(frame->GetURL());

    V_DECLARE();
    V_EXPECT_FALSE(got_load_start_[frame_number]);
    V_EXPECT_FALSE(got_load_end_[frame_number]);

    // Notification should be received for parent frame before child frame.
    if (frame_number == 0) {
      V_EXPECT_FALSE(got_load_start_[1]);
      V_EXPECT_FALSE(got_load_start_[2]);
      V_EXPECT_FALSE(got_load_start_[3]);
    } else if (frame_number == 1) {
      V_EXPECT_TRUE(got_load_start_[0]);
      V_EXPECT_FALSE(got_load_start_[2]);
      V_EXPECT_FALSE(got_load_start_[3]);
    } else if (frame_number == 2) {
      V_EXPECT_TRUE(got_load_start_[0]);
      V_EXPECT_TRUE(got_load_start_[1]);
      V_EXPECT_FALSE(got_load_start_[3]);
    } else if (frame_number == 3) {
      V_EXPECT_TRUE(got_load_start_[0]);
      V_EXPECT_TRUE(got_load_start_[1]);
      V_EXPECT_TRUE(got_load_start_[2]);
    } else {
      V_EXPECT_TRUE(false);  // Not reached.
    }

    got_load_start_[frame_number].yes();

    V_EXPECT_TRUE(parent::OnLoadStart(browser, frame));
    V_RETURN();
  }

  bool OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame) override {
    const int frame_number = GetNavFromMultiNavURL(frame->GetURL());

    V_DECLARE();
    V_EXPECT_TRUE(got_load_start_[frame_number]);
    V_EXPECT_FALSE(got_load_end_[frame_number]);

    // Notification should be received for child frame before parent frame.
    if (frame_number == 0) {
      V_EXPECT_TRUE(got_load_end_[1]);
      V_EXPECT_TRUE(got_load_end_[2]);
      V_EXPECT_TRUE(got_load_end_[3]);
    } else if (frame_number == 1) {
      V_EXPECT_FALSE(got_load_end_[0]);
      V_EXPECT_TRUE(got_load_end_[2]);
      V_EXPECT_TRUE(got_load_end_[3]);
    } else if (frame_number == 2) {
      V_EXPECT_FALSE(got_load_end_[0]);
      V_EXPECT_FALSE(got_load_end_[1]);
      V_EXPECT_TRUE(got_load_end_[3]);
    } else if (frame_number == 3) {
      V_EXPECT_FALSE(got_load_end_[0]);
      V_EXPECT_FALSE(got_load_end_[1]);
      V_EXPECT_FALSE(got_load_end_[2]);
    } else {
      V_EXPECT_TRUE(false);  // Not reached.
    }

    V_EXPECT_TRUE(VerifyBrowserIframe(browser, frame, origin_, frame_number))
        << "frame_number = " << frame_number;

    got_load_end_[frame_number].yes();

    V_EXPECT_TRUE(parent::OnLoadEnd(browser, frame));
    V_RETURN();
  }

  bool OnRendererComplete(CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefFrame> frame,
                          int renderer_nav,
                          bool renderer_result) override {
    V_DECLARE();
    V_EXPECT_FALSE(got_renderer_complete_);
    if (same_origin_) {
      V_EXPECT_TRUE(renderer_nav == nav());
    } else {
      // Because each navigation is in a new renderer process.
      V_EXPECT_TRUE(renderer_nav == 0);
    }

    got_renderer_complete_.yes();

    V_EXPECT_TRUE(parent::OnRendererComplete(browser, frame, renderer_nav,
                                             renderer_result));
    V_RETURN();
  }

  bool Finalize() override {
    V_DECLARE();
    V_EXPECT_TRUE(got_load_state_change_done_);
    V_EXPECT_TRUE(got_load_start_[0]);
    V_EXPECT_TRUE(got_load_start_[1]);
    V_EXPECT_TRUE(got_load_start_[2]);
    V_EXPECT_TRUE(got_load_start_[3]);
    V_EXPECT_TRUE(got_load_end_[0]);
    V_EXPECT_TRUE(got_load_end_[1]);
    V_EXPECT_TRUE(got_load_end_[2]);
    V_EXPECT_TRUE(got_load_end_[3]);
    V_EXPECT_TRUE(got_renderer_complete_);
    V_EXPECT_TRUE(parent::Finalize());
    V_RETURN();
  }

 private:
  bool same_origin_;
  std::string origin_;

  TrackCallback got_load_state_change_done_;
  TrackCallback got_load_start_[4];
  TrackCallback got_load_end_[4];
  TrackCallback got_renderer_complete_;
};

class FrameNavExpectationsRendererTestNestedIframes
    : public FrameNavExpectationsRendererMultiNav {
 public:
  typedef FrameNavExpectationsRendererMultiNav parent;

  FrameNavExpectationsRendererTestNestedIframes(int nav, bool same_origin)
      : parent(nav) {
    if (same_origin)
      origin_ = kFrameNavOrigin0;
  }

  bool IsNavigationDone() const override {
    return got_load_state_change_done_ && got_load_end_[0] &&
           got_load_end_[1] && got_load_end_[2] && got_load_end_[3];
  }

  bool OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading) override {
    V_DECLARE();
    V_EXPECT_FALSE(got_load_state_change_done_);

    if (!isLoading) {
      got_load_state_change_done_.yes();
    }

    V_EXPECT_TRUE(parent::OnLoadingStateChange(browser, isLoading));
    V_RETURN();
  }

  bool OnLoadStart(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame) override {
    if (origin_.empty()) {
      // When navigating different origins we can't rely on the nav() value
      // because each navigation creates a new renderer process. Get the origin
      // by parsing the URL instead.
      origin_ = GetOriginFromMultiNavURL(browser->GetMainFrame()->GetURL());
    }

    const int frame_number = GetNavFromMultiNavURL(frame->GetURL());

    V_DECLARE();
    V_EXPECT_FALSE(got_load_start_[frame_number]);
    V_EXPECT_FALSE(got_load_end_[frame_number]);

    // Notification should be received for parent frame before child frame.
    if (frame_number == 0) {
      V_EXPECT_FALSE(got_load_start_[1]);
      V_EXPECT_FALSE(got_load_start_[2]);
      V_EXPECT_FALSE(got_load_start_[3]);
    } else if (frame_number == 1) {
      V_EXPECT_TRUE(got_load_start_[0]);
      V_EXPECT_FALSE(got_load_start_[2]);
      V_EXPECT_FALSE(got_load_start_[3]);
    } else if (frame_number == 2) {
      V_EXPECT_TRUE(got_load_start_[0]);
      V_EXPECT_TRUE(got_load_start_[1]);
      V_EXPECT_FALSE(got_load_start_[3]);
    } else if (frame_number == 3) {
      V_EXPECT_TRUE(got_load_start_[0]);
      V_EXPECT_TRUE(got_load_start_[1]);
      V_EXPECT_TRUE(got_load_start_[2]);
    }

    got_load_start_[frame_number].yes();

    V_EXPECT_TRUE(parent::OnLoadStart(browser, frame));
    V_RETURN();
  }

  bool OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame) override {
    const int frame_number = GetNavFromMultiNavURL(frame->GetURL());

    V_DECLARE();
    V_EXPECT_TRUE(got_load_start_[frame_number]);
    V_EXPECT_FALSE(got_load_end_[frame_number]);

    // Notification should be received for child frame before parent frame.
    if (frame_number == 0) {
      V_EXPECT_TRUE(got_load_end_[1]);
      V_EXPECT_TRUE(got_load_end_[2]);
      V_EXPECT_TRUE(got_load_end_[3]);
    } else if (frame_number == 1) {
      V_EXPECT_FALSE(got_load_end_[0]);
      V_EXPECT_TRUE(got_load_end_[2]);
      V_EXPECT_TRUE(got_load_end_[3]);
    } else if (frame_number == 2) {
      V_EXPECT_FALSE(got_load_end_[0]);
      V_EXPECT_FALSE(got_load_end_[1]);
      V_EXPECT_TRUE(got_load_end_[3]);
    } else if (frame_number == 3) {
      V_EXPECT_FALSE(got_load_end_[0]);
      V_EXPECT_FALSE(got_load_end_[1]);
      V_EXPECT_FALSE(got_load_end_[2]);
    }

    V_EXPECT_TRUE(VerifyBrowserIframe(browser, frame, origin_, frame_number))
        << "frame_number = " << frame_number;

    got_load_end_[frame_number].yes();

    V_EXPECT_TRUE(parent::OnLoadEnd(browser, frame));
    V_RETURN();
  }

  bool Finalize() override {
    V_DECLARE();
    V_EXPECT_TRUE(got_load_state_change_done_);
    V_EXPECT_TRUE(got_load_start_[0]);
    V_EXPECT_TRUE(got_load_start_[1]);
    V_EXPECT_TRUE(got_load_start_[2]);
    V_EXPECT_TRUE(got_load_start_[3]);
    V_EXPECT_TRUE(got_load_end_[0]);
    V_EXPECT_TRUE(got_load_end_[1]);
    V_EXPECT_TRUE(got_load_end_[2]);
    V_EXPECT_TRUE(got_load_end_[3]);
    V_EXPECT_TRUE(parent::Finalize());
    V_RETURN();
  }

 private:
  std::string origin_;

  TrackCallback got_load_state_change_done_;
  TrackCallback got_load_start_[4];
  TrackCallback got_load_end_[4];
};

class FrameNavExpectationsFactoryBrowserTestNestedIframesSameOrigin
    : public FrameNavExpectationsFactoryBrowser {
 public:
  FrameNavExpectationsFactoryBrowserTestNestedIframesSameOrigin()
      : create_count_(0) {}

  FrameNavFactoryId GetID() const override {
    return FNF_ID_NESTED_IFRAMES_SAME_ORIGIN;
  }

  bool HasMoreNavigations() const override {
    return (create_count_ < kMaxMultiNavNavigations);
  }

  bool Finalize() override {
    V_DECLARE();
    V_EXPECT_TRUE(create_count_ == kMaxMultiNavNavigations);
    V_RETURN();
  }

 protected:
  std::unique_ptr<FrameNavExpectationsBrowser> Create(int nav) override {
    create_count_++;
    return std::make_unique<FrameNavExpectationsBrowserTestNestedIframes>(nav,
                                                                          true);
  }

 private:
  int create_count_;
};

class FrameNavExpectationsFactoryRendererTestNestedIframesSameOrigin
    : public FrameNavExpectationsFactoryRenderer {
 public:
  FrameNavExpectationsFactoryRendererTestNestedIframesSameOrigin() {}

  FrameNavFactoryId GetID() const override {
    return FNF_ID_NESTED_IFRAMES_SAME_ORIGIN;
  }

 protected:
  std::unique_ptr<FrameNavExpectationsRenderer> Create(int nav) override {
    return std::make_unique<FrameNavExpectationsRendererTestNestedIframes>(
        nav, true);
  }
};

}  // namespace

// Test that nested iframes work.
FRAME_TEST(NestedIframesSameOrigin, FNF_ID_NESTED_IFRAMES_SAME_ORIGIN)

namespace {

class FrameNavExpectationsFactoryBrowserTestNestedIframesDiffOrigin
    : public FrameNavExpectationsFactoryBrowser {
 public:
  FrameNavExpectationsFactoryBrowserTestNestedIframesDiffOrigin()
      : create_count_(0) {}

  FrameNavFactoryId GetID() const override {
    return FNF_ID_NESTED_IFRAMES_DIFF_ORIGIN;
  }

  bool HasMoreNavigations() const override {
    return (create_count_ < kMaxMultiNavNavigations);
  }

  bool Finalize() override {
    V_DECLARE();
    V_EXPECT_TRUE(create_count_ == kMaxMultiNavNavigations);
    V_RETURN();
  }

 protected:
  std::unique_ptr<FrameNavExpectationsBrowser> Create(int nav) override {
    create_count_++;
    return std::make_unique<FrameNavExpectationsBrowserTestNestedIframes>(
        nav, false);
  }

 private:
  int create_count_;
};

class FrameNavExpectationsFactoryRendererTestNestedIframesDiffOrigin
    : public FrameNavExpectationsFactoryRenderer {
 public:
  FrameNavExpectationsFactoryRendererTestNestedIframesDiffOrigin() {}

  FrameNavFactoryId GetID() const override {
    return FNF_ID_NESTED_IFRAMES_DIFF_ORIGIN;
  }

 protected:
  std::unique_ptr<FrameNavExpectationsRenderer> Create(int nav) override {
    return std::make_unique<FrameNavExpectationsRendererTestNestedIframes>(
        nav, false);
  }
};

}  // namespace

// Test that nested iframes work.
FRAME_TEST(NestedIframesDiffOrigin, FNF_ID_NESTED_IFRAMES_DIFF_ORIGIN)

namespace {

// Returns a new factory in the browser or renderer process. All factory types
// must be listed here.

// static
std::unique_ptr<FrameNavExpectationsFactoryBrowser>
FrameNavExpectationsFactoryBrowser::FromID(FrameNavFactoryId id) {
  std::unique_ptr<FrameNavExpectationsFactoryBrowser> factory;
  switch (id) {
    case FNF_ID_SINGLE_NAV_HARNESS:
      factory.reset(new FrameNavExpectationsFactoryBrowserTestSingleNavHarness);
      break;
    case FNF_ID_SINGLE_NAV:
      factory.reset(new FrameNavExpectationsFactoryBrowserTestSingleNav);
      break;
    case FNF_ID_MULTI_NAV_HARNESS:
      factory.reset(new FrameNavExpectationsFactoryBrowserTestMultiNavHarness);
      break;
    case FNF_ID_MULTI_NAV:
      factory.reset(new FrameNavExpectationsFactoryBrowserTestMultiNav);
      break;
    case FNF_ID_NESTED_IFRAMES_SAME_ORIGIN:
      factory.reset(
          new FrameNavExpectationsFactoryBrowserTestNestedIframesSameOrigin);
      break;
    case FNF_ID_NESTED_IFRAMES_DIFF_ORIGIN:
      factory.reset(
          new FrameNavExpectationsFactoryBrowserTestNestedIframesDiffOrigin);
      break;
    default:
      break;
  }
  EXPECT_TRUE(factory);
  EXPECT_EQ(id, factory->GetID());
  return factory;
}

// static
std::unique_ptr<FrameNavExpectationsFactoryRenderer>
FrameNavExpectationsFactoryRenderer::FromID(FrameNavFactoryId id) {
  std::unique_ptr<FrameNavExpectationsFactoryRenderer> factory;
  switch (id) {
    case FNF_ID_SINGLE_NAV_HARNESS:
      factory.reset(
          new FrameNavExpectationsFactoryRendererTestSingleNavHarness);
      break;
    case FNF_ID_SINGLE_NAV:
      factory.reset(new FrameNavExpectationsFactoryRendererTestSingleNav);
      break;
    case FNF_ID_MULTI_NAV_HARNESS:
      factory.reset(new FrameNavExpectationsFactoryRendererTestMultiNavHarness);
      break;
    case FNF_ID_MULTI_NAV:
      factory.reset(new FrameNavExpectationsFactoryRendererTestMultiNav);
      break;
    case FNF_ID_NESTED_IFRAMES_SAME_ORIGIN:
      factory.reset(
          new FrameNavExpectationsFactoryRendererTestNestedIframesSameOrigin);
      break;
    case FNF_ID_NESTED_IFRAMES_DIFF_ORIGIN:
      factory.reset(
          new FrameNavExpectationsFactoryRendererTestNestedIframesDiffOrigin);
      break;
    default:
      break;
  }
  EXPECT_TRUE(factory);
  EXPECT_EQ(id, factory->GetID());
  return factory;
}

}  // namespace

// Entry point for creating frame renderer test objects.
// Called from client_app_delegates.cc.
void CreateFrameRendererTests(ClientAppRenderer::DelegateSet& delegates) {
  delegates.insert(new FrameNavRendererTest);
}
