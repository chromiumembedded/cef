// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFTESTS_TEST_HANDLER_H_
#define CEF_TESTS_CEFTESTS_TEST_HANDLER_H_
#pragma once

#include <atomic>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "include/base/cef_callback.h"
#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_frame.h"
#include "include/cef_task.h"
#include "include/cef_waitable_event.h"
#include "tests/ceftests/thread_helper.h"
#include "tests/ceftests/track_callback.h"
#include "tests/gtest/include/gtest/gtest.h"

class ResourceContent {
 public:
  typedef std::multimap<std::string, std::string> HeaderMap;

  ResourceContent(const std::string& content,
                  const std::string& mime_type,
                  const HeaderMap& header_map)
      : content_(content), mime_type_(mime_type), header_map_(header_map) {}

  const std::string& content() const { return content_; }
  const std::string& mimeType() const { return mime_type_; }
  const HeaderMap& headerMap() const { return header_map_; }

 private:
  std::string content_;
  std::string mime_type_;
  HeaderMap header_map_;
};

// Base implementation of CefClient for unit tests. Add new interfaces as needed
// by test cases.
class TestHandler : public CefClient,
                    public CefDialogHandler,
                    public CefDisplayHandler,
                    public CefDownloadHandler,
                    public CefJSDialogHandler,
                    public CefLifeSpanHandler,
                    public CefLoadHandler,
                    public CefRequestHandler,
                    public CefResourceRequestHandler {
 public:
  // Close pooled browsers on the test thread before shutting down CEF.
  static void ShutdownReusableBrowsers();
  // Snapshot for harness diagnostics and focused reuse tests. May be called on
  // the UI or test thread. Closing entries remain included in total.
  struct ReusableBrowserStats {
    bool enabled = false;
    size_t limit = 0;
    size_t total = 0;
    size_t idle = 0;
    size_t leased = 0;
    size_t closing = 0;
    size_t hidden_windows = 0;
    size_t window_hides = 0;
    size_t window_shows = 0;
  };
  static ReusableBrowserStats GetReusableBrowserStats();
  // A default handle selects the global context. Obtain custom handles from
  // CreateReusableRequestContext; their identities belong to individual
  // creations and remain distinct after context-cache eviction.
  struct ReusableRequestContext {
    ReusableRequestContext() : identity(0) {}
    ReusableRequestContext(CefRefPtr<CefRequestContext> context,
                           size_t identity)
        : context(context), identity(identity) {}
    CefRefPtr<CefRequestContext> context;
    size_t identity;
  };
  // Create an initialized context, with or without a browser, reusing an entry
  // with matching settings when browser reuse is enabled. The UI-thread
  // callback must not retain a fixture after it returns. Contexts have a
  // separate LRU cache bounded by --reuse-test-browsers-max and are released
  // before CEF shutdown. Callers must clear relevant cache, authentication,
  // cookies, and storage between uses. Do not use for context initialization,
  // destruction, or isolation tests.
  // |is_global| uses the shared-global creation overload with a handler,
  // rather than creating an independent context from |settings|. This choice
  // is part of the creation key; pass default settings for that mode.
  static void CreateReusableRequestContext(
      const CefRequestContextSettings& settings,
      bool is_global,
      base::OnceCallback<void(ReusableRequestContext)> callback);
  struct ReusableBrowserOptions {
    bool enabled = false;
    size_t max_browsers = 4;
  };
  // Override command-line configuration for targeted harness tests. The cache
  // must be empty. Pass nullopt to restore command-line behavior. UI/test
  // thread.
  static void SetReusableBrowserOptionsForTesting(
      std::optional<ReusableBrowserOptions> options);
  // Tracks the completion state of related test runs.
  class CompletionState {
   public:
    // |total| is the number of times that TestComplete() must be called before
    // WaitForTests() will return.
    explicit CompletionState(int total);

    // Call this method to indicate that a test has completed.
    void TestComplete();

    // This method blocks until TestComplete() has been called the required
    // number of times.
    void WaitForTests();

    int total() const { return total_; }
    int count() const { return count_; }

   private:
    int total_;
    int count_ = 0;

    // Handle used to notify when the test is complete
    CefRefPtr<CefWaitableEvent> event_;
  };

  // Represents a collection of related tests that need to be run
  // simultaniously.
  class Collection {
   public:
    // The |completion_state| object must outlive this class.
    explicit Collection(CompletionState* completion_state);

    // The |test_handler| object must outlive this class and it must share the
    // same CompletionState object passed to the constructor.
    void AddTestHandler(TestHandler* test_handler);

    // Manages the test run.
    // 1. Calls TestHandler::SetupTest() for all of the test objects.
    // 2. Waits for all TestHandler objects to report that initial setup is
    //    complete by calling TestHandler::SetupComplete().
    // 3. Calls TestHandler::RunTest() for all of the test objects.
    // 4. Waits for all TestHandler objects to report that the test is
    //    complete by calling TestHandler::DestroyTest().
    void ExecuteTests();

   private:
    CompletionState* completion_state_;

    typedef std::list<TestHandler*> TestHandlerList;
    TestHandlerList handler_list_;
  };

  typedef std::map<int, CefRefPtr<CefBrowser>> BrowserMap;

  // Helper for executing methods using WeakPtr references to TestHandler.
  class UIThreadHelper {
   public:
    UIThreadHelper();

    // Pass in a |task| with an unretained reference to TestHandler. |task| will
    // be executed only if TestHandler::DestroyTest has not yet been called.
    // For example:
    //    GetUIThreadHelper()->PostTask(
    //        base::BindOnce(&TestHandler::DoSomething,
    //        base::Unretained(this)));
    void PostTask(base::OnceClosure task);
    void PostDelayedTask(base::OnceClosure task, int delay_ms);

   private:
    void TaskHelper(base::OnceClosure task);

    // Must be the last member.
    base::WeakPtrFactory<UIThreadHelper> weak_ptr_factory_;
  };

  // The |completion_state| object if specified must outlive this class.
  explicit TestHandler(CompletionState* completion_state = nullptr);
  ~TestHandler() override;

  TestHandler(const TestHandler&) = delete;
  TestHandler& operator=(const TestHandler&) = delete;

  // Implement this method to set up the test. Only used in combination with a
  // Collection. Call SetupComplete() once the setup is complete.
  virtual void SetupTest() {}

  // Implement this method to run the test. Call DestroyTest() once the test is
  // complete.
  virtual void RunTest() = 0;

  // CefClient methods. Add new methods as needed by test cases.
  CefRefPtr<CefDialogHandler> GetDialogHandler() override { return this; }
  CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
  CefRefPtr<CefDownloadHandler> GetDownloadHandler() override { return this; }
  CefRefPtr<CefJSDialogHandler> GetJSDialogHandler() override { return this; }
  CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
  CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
  CefRefPtr<CefRequestHandler> GetRequestHandler() override { return this; }

  // CefDisplayHandler methods
  bool OnConsoleMessage(CefRefPtr<CefBrowser> browser,
                        cef_log_severity_t level,
                        const CefString& message,
                        const CefString& source,
                        int line) override;

  // CefLifeSpanHandler methods
  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

  // CefRequestHandler methods
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

  // CefResourceRequestHandler methods
  CefRefPtr<CefResourceHandler> GetResourceHandler(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request) override;

  void OnRenderProcessTerminated(CefRefPtr<CefBrowser> browser,
                                 TerminationStatus status,
                                 int error_code,
                                 const CefString& error_string) override;

  // These methods should only be used if at most one non-popup browser exists.
  CefRefPtr<CefBrowser> GetBrowser() const;
  int GetBrowserId() const;

  // Copies the map of all the currently existing browsers into |map|. Must be
  // called on the UI thread.
  void GetAllBrowsers(BrowserMap* map);

  // Called by the test function to execute the test. This method blocks until
  // the test is complete. Do not reference the object after this method
  // returns. Do not use this method if the CompletionState object is shared by
  // multiple handlers or when using a Collection object.
  void ExecuteTest();

  // Event that will be signaled from the TestHandler destructor.
  // Used by ReleaseAndWaitForDestructor.
  void SetDestroyEvent(CefRefPtr<CefWaitableEvent> event) {
    destroy_event_ = event;
  }

  // If a test will not call DestroyTest() indicate so using this method.
  void SetDestroyTestExpected(bool expected) {
    destroy_test_expected_ = expected;
  }

  // Called from TestWindowDelegate when Views is enabled.
  void OnWindowCreated(int browser_id);
  virtual void OnWindowDestroyed(int browser_id);
  virtual void OnWindowActivationChanged(int browser_id, bool active) {}
#if CEF_API_ADDED(CEF_NEXT)
  virtual bool OnWindowKeyboardEvent(int browser_id,
                                     const CefKeyboardInputEvent& event) {
    return false;
  }
#endif  // CEF_API_ADDED(CEF_NEXT)
  // DEPRECATED: Use OnWindowKeyboardEvent(); retained for legacy route tests.
  virtual bool OnWindowKeyEvent(int browser_id, const CefKeyEvent& event) {
    return false;
  }

  // Returns the count of TestHandlers the currently exist.
  static size_t GetTestHandlerCount() {
    return test_handler_count_.load(std::memory_order_relaxed);
  }

  std::string debug_string_prefix() const { return debug_string_prefix_; }
  bool use_alloy_style_browser() const { return use_alloy_style_browser_; }
  bool use_alloy_style_window() const { return use_alloy_style_window_; }

 protected:
  // Opt-in browser reuse for sequential, single-browser tests. Uses the global
  // context by default, or a handle from CreateReusableRequestContext.
  // --reuse-test-browsers enables reuse; without it this creates an ordinary
  // browser. Test selection is unchanged.
  // --reuse-test-browsers-timing adds lease/navigation timing logs.
  // --reuse-test-browsers-max limits all pool-owned browsers (default 4; zero
  // forces isolated execution). Leased and closing browsers count toward the
  // limit. A full pool evicts its least recently released idle browser and
  // waits for closure before replacing it. If none is idle, this test runs
  // isolated; its browser closes normally and never enters the pool.
  // Windowed browsers stay visible across sequential leases. After one second
  // idle, they are hidden and shown again on acquisition. If native visibility
  // control is unavailable, they close after that grace period instead.
  //
  // The harness derives a capability key from browser settings and windowed
  // hosting/runtime styles and size, or OSR parent, texture/begin-frame modes,
  // GetViewRect, and GetScreenInfo (including scale). Compatible browsers may
  // be shared across fixtures. For OSR, the render handler must supply its
  // initial geometry/screen information with a null browser. This geometry
  // must stay fixed for the lease, unless the test restores it.
  //
  // Each lease loads its URL, including when unchanged, preserving the normal
  // navigation-focus request. DestroyTest releases the handler; the document
  // is replaced when the next lease begins. Document callbacks are gated until
  // the new main-frame commit and restricted to its current frame tree.
  // Resource requests arriving while idle or before the lease's navigation
  // starts are canceled instead of falling through to the real network.
  // Subsequent navigations within the lease, initial redirects, and failed
  // navigations are supported. A failure without a new document commit does
  // not admit messages from the outgoing document. Reuse invokes
  // OnReusableBrowserAttached and OnReusableBrowserReleased instead of fixture
  // lifespan callbacks.
  //
  // Do NOT use this for tests that:
  // - Require browser creation/closure, lifespan callbacks, initial browser
  //   state, or browser destruction to cancel pending work.
  // - Create multiple browsers/popups or run concurrently through Collection.
  //   These are not supported by this trial.
  // - Require a fresh request context or its initialization/destruction.
  // - Leave outstanding requests, delayed tasks, or input callbacks that can
  //   reach the next test. Quiesce/cancel all such work before DestroyTest.
  // - Change browser-scoped state (zoom, DevTools overrides, preferences, etc.)
  //   without restoring it, or require an empty per-browser navigation history.
  //
  // The context creation identity is part of the key. A default handle selects
  // the global context. Recreated contexts never reuse browsers tied to older
  // contexts.
  // A cached browser retains its context until eviction or shutdown.
  // Use CreateReusableRequestContext to select contexts by creation settings,
  // then pass its handle here. No context-object comparisons are performed.
  // Cookies/storage follow normal request-context behavior; this helper does
  // not introduce a profile-state reset. Use ordinary creation for ineligible
  // tests, rather than assigning them a separate capability key.
  // Use browser-assigned IDs (for example, DevTools message IDs); their
  // counters do not restart with each handler.
  //
  // When enabling reuse for a new suite, validate with --gtest_shuffle and
  // include other reuse-enabled suites to expose state leaking between leases.
  // Record the seed so failures can be reproduced. Compare representative
  // cases with reuse disabled; configuration-equivalence tests can cover
  // equivalent modes without repeating the full suite in each mode.
  //
  // Non-null handlers from |client| override the fixture's handlers in both
  // shared and isolated modes. The harness retains its lifecycle/navigation
  // routing. The client is automatically cleared when its lease ends; idle
  // browsers and subsequent leases do not retain its handlers. Quiesce pending
  // work before DestroyTest; a previously returned handler can still receive
  // late callbacks and must safely detach any fixture references.
  // Custom lifespan callbacks describe actual creation/closure while the
  // client is attached; releasing a lease does not manufacture these callbacks.
  void CreateReusableBrowser(
      const CefWindowInfo& window_info,
      const CefString& url,
      const CefBrowserSettings& settings,
      ReusableRequestContext request_context = ReusableRequestContext(),
      CefRefPtr<CefClient> client = nullptr);
  // Windowed equivalent using the fixture's Views/native hosting and Alloy/
  // Chrome style settings. Window dimensions are part of the capability key.
  // Do not use for window lifecycle, activation, initial focus, or resize
  // tests. Keyboard callbacks are forwarded to the current lease. Leave window
  // geometry/state unchanged, or restore it before release. Defaults match
  // ordinary Views test windows.
  void CreateReusableBrowser(
      const CefString& url,
      const CefSize& window_size = CefSize(800, 600),
      const CefBrowserSettings& settings = CefBrowserSettings(),
      ReusableRequestContext request_context = ReusableRequestContext(),
      CefRefPtr<CefClient> client = nullptr);
  virtual void OnReusableBrowserAttached(CefRefPtr<CefBrowser> browser);
  virtual void OnReusableBrowserReleased(CefRefPtr<CefBrowser> browser);
  // Indicate that test setup is complete. Only used in combination with a
  // Collection.
  virtual void SetupComplete();

  // Close any remaining browsers. This may result in a call to TestComplete(),
  // depending on the configuration of SetSignalCompletionCount().
  virtual void DestroyTest();

  // Called on the UI thread if the test times out as a result of calling
  // SetTestTimeout(). Calls DestroyTest() by default.
  virtual void OnTestTimeout(int timeout_ms, bool treat_as_error);

  void CreateBrowser(const CefString& url,
                     CefRefPtr<CefRequestContext> request_context = nullptr,
                     CefRefPtr<CefDictionaryValue> extra_info = nullptr);
  void CreateBrowserWithSettings(
      const CefString& url,
      const CefBrowserSettings& settings,
      CefRefPtr<CefRequestContext> request_context = nullptr,
      CefRefPtr<CefDictionaryValue> extra_info = nullptr,
      const CefSize& window_size = CefSize(800, 600),
      CefRefPtr<CefClient> client = nullptr);

  static void CloseBrowser(CefRefPtr<CefBrowser> browser, bool force_close);

  void AddResource(const std::string& url,
                   const std::string& content,
                   const std::string& mime_type,
                   const ResourceContent::HeaderMap& header_map = {});

  void AddResourceEx(const std::string& url, const ResourceContent& content);

  void ClearResources();

  // Specify the number of times that SignalTestCompletion() needs to be
  // explicitly called for test completion. Must be configured during test
  // initialization before any browsers are created.
  // - If the test creates browsers and does not explicitly call
  //   SignalTestCompletion() then the default value (0) can be used.
  // - If the test creates browsers and explicitly calls SignalTestCompletion()
  //   then set a value >= 1.
  // - If the test does not create browsers then it must explicitly call
  //   SignalTestCompletion() and set a value >= 1.
  void SetSignalTestCompletionCount(size_t count);

  // Explicitly signal test completion a single time. Used in combination with
  // SetSignalTestCompletionCount(). Results in a call to TestComplete() if
  // all browsers have closed and this method has been called the expected
  // number of times.
  void SignalTestCompletion();

  // Returns true if SignalTestCompletion() has been called the necessary
  // number of times (may be 0), in which case TestComplete() will be called
  // automatically when all browsers have closed. Must be called on the
  // UI thread.
  bool AllowTestCompletionWhenAllBrowsersClose() const;

  // Returns true if all browsers have closed. Must be called on the UI
  // thread.
  bool AllBrowsersClosed() const;

  // Call OnTestTimeout() after the specified amount of time.
  void SetTestTimeout(int timeout_ms = 5000, bool treat_as_error = true);

  // Call prior to CreateBrowser() to configure whether browsers and windows
  // will be created as Views-hosted. Defaults to false unless the `--use-views`
  // command-line flag is specified.
  void SetUseViews(bool use_views);

  // Call prior to CreateBrowser() to configure whether browsers (and windows
  // with Views) will be created as Alloy style or Chrome style. Alloy style
  // optional. Defaults to false unless the `--use-alloy-style` command-line
  // flag is specified.
  void SetUseAlloyStyle(bool use_alloy_style_browser,
                        bool use_alloy_style_window);

  // Returns the single UIThreadHelper instance, creating it if necessary. Must
  // be called on the UI thread.
  UIThreadHelper* GetUIThreadHelper();

 private:
  void CreateReusableBrowserImpl(const CefWindowInfo& window_info,
                                 const CefString& url,
                                 const CefBrowserSettings& settings,
                                 bool use_views,
                                 cef_runtime_style_t window_style,
                                 ReusableRequestContext request_context,
                                 CefRefPtr<CefClient> client);
  friend class ReusableBrowserClient;
  CefRefPtr<class ReusableBrowserClient> reusable_client_;
  bool reusable_creation_pending_ = false;
  void MaybeTestComplete();

  // Complete the test. It is an error to call this method before all browsers
  // have closed.
  void TestComplete();

  enum NotifyType {
    NT_BROWSER,
    NT_WINDOW,
  };
  void OnCreated(int browser_id, NotifyType type, bool views_hosted);
  void OnClosed(int browser_id, NotifyType type);

  std::string MakeDebugStringPrefix() const;
  const std::string debug_string_prefix_;

  // Used to notify when the test is complete. Can be accessed on any thread.
  CompletionState* completion_state_;
  bool completion_state_owned_;

  bool use_views_;
  bool use_alloy_style_browser_;
  bool use_alloy_style_window_;

  // Map of browser ID to browser object. Only accessed on the UI thread.
  BrowserMap browser_map_;

  struct NotifyStatus {
    // True if this particular browser window is Views-hosted (see SetUseViews).
    // Some tests create popup or DevTools windows with default handling (e.g.
    // not Views-hosted).
    bool views_hosted;

    // Keyed by NotifyType.
    TrackCallback got_created[2];
    TrackCallback got_closed[2];
  };

  // Map of browser ID to current status. Only accessed on the UI thread.
  std::map<int, NotifyStatus> browser_status_map_;

  // Values for the first created browser. Modified on the UI thread but can be
  // accessed on any thread.
  int first_browser_id_ = 0;
  CefRefPtr<CefBrowser> first_browser_;

  // Map of resources that can be automatically loaded. Only accessed on the
  // IO thread.
  typedef std::map<std::string, ResourceContent> ResourceMap;
  ResourceMap resource_map_;

  // Number of times that SignalTestCompletion() must be called.
  size_t signal_completion_count_ = 0U;

  CefRefPtr<CefWaitableEvent> destroy_event_;

  // Tracks whether OnTestTimeout() has been called.
  bool test_timeout_called_ = false;

  // Tracks whether DestroyTest() is expected or has been called.
  bool destroy_test_expected_ = true;
  bool destroy_test_called_ = false;

  std::unique_ptr<UIThreadHelper> ui_thread_helper_;

  // Used to track the number of currently existing TestHandlers.
  static std::atomic<size_t> test_handler_count_;
};

// Release |handler| and wait for the destructor to be called.
// This function is used to avoid test state leakage and to verify that
// all Handler references have been released on test completion.
template <typename T>
void ReleaseAndWaitForDestructor(CefRefPtr<T>& handler, int delay_ms = 2000) {
  CefRefPtr<CefWaitableEvent> event =
      CefWaitableEvent::CreateWaitableEvent(true, false);
  handler->SetDestroyEvent(event);
  handler = nullptr;
  bool handler_destructed = event->TimedWait(delay_ms);
  EXPECT_TRUE(handler_destructed);
  // The handler retains the ref-counted event if destruction is delayed.
  // Do not access it after dropping our reference: it may be destroyed just
  // as the wait times out.
}

// Returns true if the currently running test has failed.
bool TestFailed();

// Helper macros for executing checks in a method with a boolean return value.
// For example:
//
// bool VerifyVals(bool a, bool b) {
//   V_DECLARE();
//   V_EXPECT_TRUE(a);
//   V_EXPECT_FALSE(b);
//   V_RETURN();
// }
//
// EXPECT_TRUE(VerifyVals(true, false));

#define V_DECLARE()     \
  bool __verify = true; \
  bool __result

#define V_RETURN() return __verify

#define V_EXPECT_TRUE(condition)                         \
  __result = !!(condition);                              \
  __verify &= __result;                                  \
  GTEST_TEST_BOOLEAN_(__result, #condition, false, true, \
                      GTEST_NONFATAL_FAILURE_)

#define V_EXPECT_FALSE(condition)                           \
  __result = !!(condition);                                 \
  __verify &= !__result;                                    \
  GTEST_TEST_BOOLEAN_(!(__result), #condition, true, false, \
                      GTEST_NONFATAL_FAILURE_)

#endif  // CEF_TESTS_CEFTESTS_TEST_HANDLER_H_
