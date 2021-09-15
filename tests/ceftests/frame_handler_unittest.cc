// Copyright 2021 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include <limits>
#include <map>
#include <memory>
#include <queue>
#include <sstream>
#include <string>

#include "include/base/cef_callback.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/routing_test_handler.h"
#include "tests/ceftests/test_handler.h"
#include "tests/gtest/include/gtest/gtest.h"

// Set to 1 to enable verbose debugging info logging.
#define VERBOSE_DEBUGGING 0

namespace {

// Must match CefFrameHostImpl::kInvalidFrameId.
const int kInvalidFrameId = -4;

// Tracks callback status for a single frame object.
struct FrameStatus {
  // Callbacks in expected order. Not all callbacks are executed in all cases
  // (see IsExpectedCallback).
  enum CallbackType {
    FRAME_CREATED,
    MAIN_FRAME_INITIAL_ASSIGNED,
    AFTER_CREATED,
    FRAME_ATTACHED,
    MAIN_FRAME_CHANGED_ASSIGNED,
    LOAD_START,
    LOAD_END,
    BEFORE_CLOSE,
    FRAME_DETACHED,
    MAIN_FRAME_CHANGED_REMOVED,
    MAIN_FRAME_FINAL_REMOVED,

    CALLBACK_LAST = MAIN_FRAME_FINAL_REMOVED,
  };

  static const char* GetCallbackName(int type) {
    switch (type) {
      case FRAME_CREATED:
        return "OnFrameCreated";
      case MAIN_FRAME_INITIAL_ASSIGNED:
        return "OnMainFrameChanged(initial_assigned)";
      case AFTER_CREATED:
        return "OnAfterCreated";
      case FRAME_ATTACHED:
        return "OnFrameAttached";
      case MAIN_FRAME_CHANGED_ASSIGNED:
        return "OnMainFrameChanged(changed_assigned)";
      case LOAD_START:
        return "OnLoadStart";
      case LOAD_END:
        return "OnLoadEnd";
      case BEFORE_CLOSE:
        return "OnBeforeClose";
      case FRAME_DETACHED:
        return "OnFrameDetached";
      case MAIN_FRAME_CHANGED_REMOVED:
        return "OnMainFrameChanged(changed_removed)";
      case MAIN_FRAME_FINAL_REMOVED:
        return "OnMainFrameChanged(final_removed)";
    }
    NOTREACHED();
    return "Unknown";
  }

  // Returns true for callbacks that should only execute for main frames.
  static bool IsMainFrameOnlyCallback(int type) {
    return (type == MAIN_FRAME_INITIAL_ASSIGNED || type == AFTER_CREATED ||
            type == MAIN_FRAME_CHANGED_ASSIGNED || type == BEFORE_CLOSE ||
            type == MAIN_FRAME_CHANGED_REMOVED ||
            type == MAIN_FRAME_FINAL_REMOVED);
  }

  static std::string GetFrameDebugString(CefRefPtr<CefFrame> frame) {
    // Match the logic in frame_util::GetFrameDebugString.
    // Specific formulation of the frame ID is an implementation detail that
    // should generally not be relied upon, but this decomposed format makes the
    // debug logging easier to follow.
    uint64 frame_id = frame->GetIdentifier();
    uint32_t process_id = frame_id >> 32;
    uint32_t routing_id = std::numeric_limits<uint32_t>::max() & frame_id;
    std::stringstream ss;
    ss << (frame->IsMain() ? "main" : " sub") << "[" << process_id << ","
       << routing_id << "]";
    return ss.str();
  }

  FrameStatus(CefRefPtr<CefFrame> frame)
      : frame_id_(frame->GetIdentifier()),
        is_main_(frame->IsMain()),
        ident_str_(GetFrameDebugString(frame)) {}

  int64 frame_id() const { return frame_id_; }
  bool is_main() const { return is_main_; }

  bool AllQueriesDelivered(std::string* msg = nullptr) const {
    EXPECT_UI_THREAD();
    const int expected_ct = is_temporary_ ? 0 : expected_query_ct_;
#if VERBOSE_DEBUGGING
    if (msg) {
      std::stringstream ss;
      ss << ident_str_ << "(expected=" << expected_ct
         << " delivered=" << delivered_query_ct_ << ")";
      *msg += ss.str();
    }
#endif
    return delivered_query_ct_ == expected_ct;
  }
  int QueriesDeliveredCount() const {
    EXPECT_UI_THREAD();
    return delivered_query_ct_;
  }

  bool IsSame(CefRefPtr<CefFrame> frame) const {
    return frame->GetIdentifier() == frame_id();
  }

  bool IsLoaded(std::string* msg = nullptr) const {
#if VERBOSE_DEBUGGING
    if (msg) {
      std::stringstream ss;
      ss << ident_str_ << "(";
      for (int i = 0; i <= LOAD_END; ++i) {
        ss << GetCallbackName(i) << "=" << got_callback_[i];
        if (i < LOAD_END)
          ss << " ";
      }
      ss << ")";
      *msg += ss.str();
    }
#endif
    return got_callback_[LOAD_END];
  }
  bool IsDetached() const { return got_callback_[FRAME_DETACHED]; }

  void SetIsFirstMain(bool val) {
    EXPECT_TRUE(is_main_);
    is_first_main_ = val;
    if (is_first_main_) {
      // Also expect OnAfterCreated
      expected_query_ct_++;
    }
  }
  void SetIsLastMain(bool val) {
    EXPECT_TRUE(is_main_);
    is_last_main_ = val;
  }

  void SetIsTemporary(bool val) {
    EXPECT_FALSE(is_main_);
    is_temporary_ = val;
  }
  bool IsTemporary() const { return is_temporary_; }

  void SetAdditionalDebugInfo(const std::string& debug_info) {
    debug_info_ = debug_info;
  }

  std::string GetDebugString() const { return debug_info_ + ident_str_; }

  // The main frame will be reused for same-origin navigations.
  void ResetMainLoadStatus() {
    EXPECT_TRUE(is_main_);

    ResetCallbackStatus(LOAD_START, /*expect_query=*/true);
    ResetCallbackStatus(LOAD_END, /*expect_query=*/true);
  }

  void OnFrameCreated(CefRefPtr<CefBrowser> browser,
                      CefRefPtr<CefFrame> frame) {
    EXPECT_UI_THREAD();
    VerifyBrowser(__FUNCTION__, browser);
    VerifyFrame(__FUNCTION__, frame);

    GotCallback(__FUNCTION__, FRAME_CREATED);

    // Test delivery of messages using a frame that isn't connected yet.
    // This tests queuing of messages in the browser process and possibly the
    // renderer process.
    ExecuteQuery(frame, FRAME_CREATED);
  }

  void OnFrameAttached(CefRefPtr<CefBrowser> browser,
                       CefRefPtr<CefFrame> frame) {
    EXPECT_UI_THREAD();
    VerifyBrowser(__FUNCTION__, browser);
    VerifyFrame(__FUNCTION__, frame);

    GotCallback(__FUNCTION__, FRAME_ATTACHED);

    // Test delivery of messages using a frame that just connected.
    // This tests queuing of messages in the browser process and possibly the
    // renderer process.
    ExecuteQuery(frame, FRAME_ATTACHED);
  }

  void OnFrameDetached(CefRefPtr<CefBrowser> browser,
                       CefRefPtr<CefFrame> frame) {
    EXPECT_UI_THREAD();
    VerifyBrowser(__FUNCTION__, browser);
    // A frame is never valid after it's detached.
    VerifyFrame(__FUNCTION__, frame, /*expect_valid=*/false);

    GotCallback(__FUNCTION__, FRAME_DETACHED);
  }

  void OnMainFrameChanged(CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefFrame> old_frame,
                          CefRefPtr<CefFrame> new_frame) {
    EXPECT_UI_THREAD();
    EXPECT_TRUE(is_main_);
    VerifyBrowser(__FUNCTION__, browser);

    bool got_match = false;

    if (old_frame && new_frame) {
      EXPECT_NE(old_frame->GetIdentifier(), new_frame->GetIdentifier());
    }

    if (old_frame && IsSame(old_frame)) {
      got_match = true;
      // A frame is never valid after it's detached.
      VerifyFrame(__FUNCTION__, old_frame, /*expect_valid=*/false);
      GotCallback(__FUNCTION__, new_frame ? MAIN_FRAME_CHANGED_REMOVED
                                          : MAIN_FRAME_FINAL_REMOVED);
      if (is_last_main_) {
        EXPECT_FALSE(new_frame);
      }
    }

    if (new_frame && IsSame(new_frame)) {
      got_match = true;
      VerifyFrame(__FUNCTION__, new_frame);
      GotCallback(__FUNCTION__, old_frame ? MAIN_FRAME_CHANGED_ASSIGNED
                                          : MAIN_FRAME_INITIAL_ASSIGNED);
      if (is_first_main_) {
        EXPECT_FALSE(old_frame);
      }
    }

    EXPECT_TRUE(got_match);
  }

  void OnAfterCreated(CefRefPtr<CefBrowser> browser) {
    EXPECT_UI_THREAD();
    VerifyBrowser(__FUNCTION__, browser);

    auto frame = browser->GetMainFrame();
    VerifyFrame(__FUNCTION__, frame);

    GotCallback(__FUNCTION__, AFTER_CREATED);
    ExecuteQuery(frame, AFTER_CREATED);
  }

  // Called for all existing frames, not just the target frame.
  // We need to track this status to know if the browser should be valid in
  // following calls to OnFrameDetached.
  void OnBeforeClose(CefRefPtr<CefBrowser> browser) {
    EXPECT_UI_THREAD();
    VerifyBrowser(__FUNCTION__, browser);

    auto frame = browser->GetMainFrame();
    EXPECT_TRUE(frame->IsValid());

    got_before_close_ = true;
    if (IsSame(frame)) {
      VerifyFrame(__FUNCTION__, frame);
      GotCallback(__FUNCTION__, BEFORE_CLOSE);
    }
  }

  void OnLoadStart(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame) {
    EXPECT_UI_THREAD();
    VerifyBrowser(__FUNCTION__, browser);
    VerifyFrame(__FUNCTION__, frame);

    GotCallback(__FUNCTION__, LOAD_START);
    ExecuteQuery(frame, LOAD_START);
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame) {
    EXPECT_UI_THREAD();
    VerifyBrowser(__FUNCTION__, browser);
    VerifyFrame(__FUNCTION__, frame);

    GotCallback(__FUNCTION__, LOAD_END);
    ExecuteQuery(frame, LOAD_END);
  }

  void OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               const CefString& request) {
    EXPECT_UI_THREAD();

    const std::string& received_query = request;

#if VERBOSE_DEBUGGING
    LOG(INFO) << GetDebugString() << " recv query " << received_query << " ("
              << (delivered_query_ct_ + 1) << " of " << expected_query_ct_
              << ")";
#endif

    VerifyBrowser(__FUNCTION__, browser);
    VerifyFrame(__FUNCTION__, frame);

    EXPECT_GE(pending_queries_.size(), 1U);
    const std::string& expected_query = pending_queries_.front();
    EXPECT_STREQ(expected_query.c_str(), received_query.c_str());
    if (expected_query == received_query)
      pending_queries_.pop();

    EXPECT_LT(delivered_query_ct_, expected_query_ct_);
    delivered_query_ct_++;
  }

  void VerifyTestResults() {
    EXPECT_UI_THREAD();

    // Verify that all expected callbacks have executed.
    VerifyCallbackStatus(__FUNCTION__, CALLBACK_LAST + 1);

    if (is_temporary_) {
      // Should not receive any queries.
      EXPECT_FALSE(is_main_);
      EXPECT_EQ(0, delivered_query_ct_);
    } else {
      // Verify that all expected messages have been sent and received.
      EXPECT_EQ(expected_query_ct_, delivered_query_ct_);
      EXPECT_EQ(0U, pending_queries_.size());
      while (!pending_queries_.empty()) {
        ADD_FAILURE() << "Query sent but not received: "
                      << pending_queries_.front();
        pending_queries_.pop();
      }
    }
  }

  bool DidGetCallback(int callback) const { return got_callback_[callback]; }

 private:
  void GotCallback(const std::string& func, int callback) {
#if VERBOSE_DEBUGGING
    LOG(INFO) << GetDebugString() << " callback " << GetCallbackName(callback);
#endif

    EXPECT_TRUE(IsExpectedCallback(callback)) << func;
    VerifyCallbackStatus(func, callback);
    got_callback_[callback].yes();
  }

  bool IsExpectedCallback(int callback) const {
    if (!is_main_ && IsMainFrameOnlyCallback(callback))
      return false;

    if (is_main_) {
      if ((callback == MAIN_FRAME_INITIAL_ASSIGNED ||
           callback == AFTER_CREATED) &&
          !is_first_main_) {
        return false;
      }
      if ((callback == BEFORE_CLOSE || callback == MAIN_FRAME_FINAL_REMOVED) &&
          !is_last_main_) {
        return false;
      }
      if (callback == MAIN_FRAME_CHANGED_ASSIGNED && is_first_main_) {
        return false;
      }
      if (callback == MAIN_FRAME_CHANGED_REMOVED && is_last_main_) {
        return false;
      }
    } else if (is_temporary_) {
      // For cross-process sub-frame navigation a sub-frame is first created in
      // the parent's renderer process. That sub-frame is then discarded after
      // the real cross-origin sub-frame is created in a different renderer
      // process. These discarded sub-frames will get OnFrameCreated/
      // OnFrameAttached immediately followed by OnFrameDetached.
      return callback == FRAME_CREATED || callback == FRAME_ATTACHED ||
             callback == FRAME_DETACHED;
    }

    return true;
  }

  void VerifyCallbackStatus(const std::string& func,
                            int current_callback) const {
    EXPECT_UI_THREAD();

    for (int i = 0; i <= CALLBACK_LAST; ++i) {
      if (i < current_callback && IsExpectedCallback(i)) {
        EXPECT_TRUE(got_callback_[i])
            << "inside " << func << " should already have gotten "
            << GetCallbackName(i);
      } else {
        EXPECT_FALSE(got_callback_[i])
            << "inside " << func << " should not already have gotten "
            << GetCallbackName(i);
      }
    }
  }

  void VerifyBrowser(const std::string& func,
                     CefRefPtr<CefBrowser> browser) const {
    const bool expect_valid = !got_before_close_;
    if (expect_valid) {
      EXPECT_TRUE(browser->IsValid()) << func;
    } else {
      EXPECT_FALSE(browser->IsValid()) << func;
    }

    // Note that this might not be the same main frame as us when navigating
    // cross-origin, because the new main frame object is assigned to the
    // browser before the CefFrameHandler callbacks related to main frame change
    // have executed. This started out as an implementation detail but it fits
    // nicely with the concept that "GetMainFrame() always returns a frame that
    // can be used", which wouldn't be the case if we returned the old frame
    // when calling GetMainFrame() from inside OnFrameCreated (for the new
    // frame), OnFrameDetached (for the old frame) or OnMainFrameChanged.
    auto main_frame = browser->GetMainFrame();
    if (expect_valid) {
      EXPECT_TRUE(main_frame) << func;
      EXPECT_TRUE(main_frame->IsValid()) << func;
      EXPECT_TRUE(main_frame->IsMain()) << func;
    } else {
      // GetMainFrame() returns nullptr after OnBeforeClose.
      EXPECT_FALSE(main_frame) << func;
    }
  }

  void VerifyFrame(const std::string& func,
                   CefRefPtr<CefFrame> frame,
                   bool expect_valid = true) const {
    if (expect_valid) {
      EXPECT_TRUE(frame->IsValid()) << func;
    } else {
      EXPECT_FALSE(frame->IsValid()) << func;
    }

    // |frame| should be us. This checks the frame type and ID.
    EXPECT_STREQ(ident_str_.c_str(), GetFrameDebugString(frame).c_str())
        << func;
  }

  void ExecuteQuery(CefRefPtr<CefFrame> frame, int callback) {
    EXPECT_UI_THREAD();
    const std::string& value = GetCallbackName(callback);

    std::string js_string;

#if VERBOSE_DEBUGGING
    LOG(INFO) << GetDebugString() << " sent query " << value;
    js_string +=
        "console.log('" + GetDebugString() + " exec query " + value + "');";
#endif

    js_string += "window.testQuery({request:'" + value + "'});";

    pending_queries_.push(value);

    // GetURL() will return an empty string for early callbacks, but that
    // doesn't appear to cause any issues.
    frame->ExecuteJavaScript(js_string, frame->GetURL(), 0);
  }

  // Reset state so we can get the same callback again.
  void ResetCallbackStatus(int callback, bool expect_query) {
    EXPECT_UI_THREAD();

    EXPECT_TRUE(got_callback_[callback]) << GetCallbackName(callback);
    got_callback_[callback].reset();

    if (expect_query)
      delivered_query_ct_--;
  }

  const int64 frame_id_;
  const bool is_main_;
  const std::string ident_str_;

  bool is_first_main_ = false;
  bool is_last_main_ = false;
  bool is_temporary_ = false;
  std::string debug_info_;

  bool got_before_close_ = false;

  TrackCallback got_callback_[CALLBACK_LAST + 1];

  std::queue<std::string> pending_queries_;

  // Expect OnCreated, OnAttached, OnLoadStart, OnLoadEnd.
  int expected_query_ct_ = 4;
  int delivered_query_ct_ = 0;
};

const char kOrderMainUrl[] = "http://tests-frame-handler/main-order.html";

class OrderMainTestHandler : public RoutingTestHandler, public CefFrameHandler {
 public:
  OrderMainTestHandler(CompletionState* completion_state = nullptr)
      : RoutingTestHandler(completion_state) {}

  CefRefPtr<CefFrameHandler> GetFrameHandler() override {
    get_frame_handler_ct_++;
    return this;
  }

  void RunTest() override {
    // Add the main resource that we will navigate to/from.
    AddResource(GetMainURL(), GetMainHtml(), "text/html");

    // Create the browser.
    CreateBrowser(GetMainURL());

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    EXPECT_UI_THREAD();
    RoutingTestHandler::OnAfterCreated(browser);

    EXPECT_FALSE(got_after_created_);
    got_after_created_ = true;

    EXPECT_TRUE(current_main_frame_);
    current_main_frame_->OnAfterCreated(browser);
  }

  void OnLoadStart(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   TransitionType transition_type) override {
    EXPECT_UI_THREAD();
    RoutingTestHandler::OnLoadStart(browser, frame, transition_type);

    EXPECT_TRUE(current_main_frame_);
    current_main_frame_->OnLoadStart(browser, frame);
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    EXPECT_UI_THREAD();
    RoutingTestHandler::OnLoadEnd(browser, frame, httpStatusCode);

    EXPECT_TRUE(current_main_frame_);
    current_main_frame_->OnLoadEnd(browser, frame);

    MaybeDestroyTest();
  }

  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
    EXPECT_UI_THREAD();

    EXPECT_FALSE(got_before_close_);
    got_before_close_ = true;

    EXPECT_TRUE(current_main_frame_);
    current_main_frame_->OnBeforeClose(browser);

    RoutingTestHandler::OnBeforeClose(browser);
  }

  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64 query_id,
               const CefString& request,
               bool persistent,
               CefRefPtr<Callback> callback) override {
    EXPECT_UI_THREAD();

    // Messages for the old and new frames are interleaved during cross-origin
    // navigation.
    if (pending_main_frame_) {
      EXPECT_TRUE(IsCrossOrigin());
      pending_main_frame_->OnQuery(browser, frame, request);
    } else {
      EXPECT_TRUE(current_main_frame_);
      current_main_frame_->OnQuery(browser, frame, request);
    }

    MaybeDestroyTest();
    return true;
  }

  void OnFrameCreated(CefRefPtr<CefBrowser> browser,
                      CefRefPtr<CefFrame> frame) override {
    EXPECT_UI_THREAD();

    EXPECT_TRUE(frame->IsMain());
    EXPECT_FALSE(pending_main_frame_);

    // First callback referencing the new frame.
    pending_main_frame_ = new FrameStatus(frame);
    pending_main_frame_->SetAdditionalDebugInfo(GetAdditionalDebugInfo());
    pending_main_frame_->SetIsFirstMain(!got_after_created_);
    pending_main_frame_->SetIsLastMain(!IsCrossOrigin() || IsLastNavigation());
    pending_main_frame_->OnFrameCreated(browser, frame);
  }

  void OnFrameAttached(CefRefPtr<CefBrowser> browser,
                       CefRefPtr<CefFrame> frame,
                       bool reattached) override {
    EXPECT_UI_THREAD();

    // May arrive before or after OnMainFrameChanged switches the frame (after
    // on initial browser creation, before on cross-origin navigation).
    if (pending_main_frame_) {
      EXPECT_TRUE(IsCrossOrigin());
      pending_main_frame_->OnFrameAttached(browser, frame);
    } else {
      EXPECT_TRUE(current_main_frame_);
      current_main_frame_->OnFrameAttached(browser, frame);
    }
  }

  void OnFrameDetached(CefRefPtr<CefBrowser> browser,
                       CefRefPtr<CefFrame> frame) override {
    EXPECT_UI_THREAD();
    EXPECT_TRUE(current_main_frame_);
    current_main_frame_->OnFrameDetached(browser, frame);
  }

  void OnMainFrameChanged(CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefFrame> old_frame,
                          CefRefPtr<CefFrame> new_frame) override {
    EXPECT_UI_THREAD();
    EXPECT_TRUE(old_frame || new_frame);
    if (old_frame) {
      EXPECT_FALSE(old_frame->IsValid());
      EXPECT_TRUE(old_frame->IsMain());

      // May be nullptr with PopupOrderMainTestHandler.
      if (current_main_frame_) {
        // Last callback referencing the old frame.
        EXPECT_TRUE(current_main_frame_);
        current_main_frame_->OnMainFrameChanged(browser, old_frame, new_frame);
        current_main_frame_->VerifyTestResults();
        delete current_main_frame_;
        current_main_frame_ = nullptr;
      }
    }
    if (new_frame) {
      EXPECT_TRUE(new_frame->IsValid());
      EXPECT_TRUE(new_frame->IsMain());

      // Always called after OnFrameCreated. See also comments in
      // OnFrameAttached.
      EXPECT_TRUE(pending_main_frame_);
      pending_main_frame_->OnMainFrameChanged(browser, old_frame, new_frame);

      // The pending frame becomes the current frame.
      EXPECT_FALSE(current_main_frame_);
      current_main_frame_ = pending_main_frame_;
      pending_main_frame_ = nullptr;
    }

    if (old_frame && new_frame) {
      // Main frame changed due to cross-origin navigation.
      EXPECT_TRUE(IsCrossOrigin());
      main_frame_changed_ct_++;
    }

    if (old_frame && !new_frame) {
      // Very last callback.
      VerifyTestResults();
    }
  }

 protected:
  virtual std::string GetMainURL() const { return kOrderMainUrl; }

  virtual std::string GetMainHtml() const {
    return "<html><body>TEST</body></html>";
  }

  virtual std::string GetNextMainURL() { return std::string(); }
  virtual bool IsFirstNavigation() const { return true; }
  virtual bool IsLastNavigation() const { return true; }
  virtual bool IsCrossOrigin() const { return false; }

  virtual std::string GetAdditionalDebugInfo() const { return std::string(); }

  virtual bool AllQueriesDelivered(std::string* msg = nullptr) const {
    EXPECT_UI_THREAD();
    if (pending_main_frame_)
      return false;
    return current_main_frame_->AllQueriesDelivered(msg);
  }

  virtual bool AllFramesLoaded(std::string* msg = nullptr) const {
    EXPECT_UI_THREAD();
    if (pending_main_frame_)
      return false;
    return current_main_frame_->IsLoaded(msg);
  }

  void MaybeDestroyTest() {
#if VERBOSE_DEBUGGING
    std::string delivered_msg, loaded_msg;
    const bool all_queries_delivered = AllQueriesDelivered(&delivered_msg);
    const bool all_frames_loaded = AllFramesLoaded(&loaded_msg);
    LOG(INFO) << (current_main_frame_ ? current_main_frame_->GetDebugString()
                                      : "")
              << " AllQueriesDelivered=" << all_queries_delivered << " {"
              << delivered_msg << "}"
              << " AllFramesLoaded=" << all_frames_loaded << " {" << loaded_msg
              << "}";
    if (all_queries_delivered && all_frames_loaded) {
#else
    if (AllQueriesDelivered() && AllFramesLoaded()) {
#endif
      const std::string& next_url = GetNextMainURL();
      if (!next_url.empty()) {
        if (!IsCrossOrigin()) {
          // Reusing the same main frame for same origin nav.
          current_main_frame_->ResetMainLoadStatus();
        }

#if VERBOSE_DEBUGGING
        LOG(INFO) << current_main_frame_->GetDebugString()
                  << "--> Navigating to " << next_url;
#endif
        GetBrowser()->GetMainFrame()->LoadURL(next_url);
      } else {
#if VERBOSE_DEBUGGING
        LOG(INFO) << "--> Destroy test";
#endif
        DestroyTest();
      }
    }
  }

  virtual void VerifyTestResults() {
    EXPECT_UI_THREAD();

    // OnMainFrameChanged should have cleaned up.
    EXPECT_FALSE(pending_main_frame_);
    EXPECT_FALSE(current_main_frame_);

    EXPECT_TRUE(got_after_created_);
    EXPECT_TRUE(got_before_close_);

    // We document GetFrameHandler() as only being called a single time.
    EXPECT_EQ(1, get_frame_handler_ct_);

    // Make sure we get the expected number OnMainFrameChanged callbacks for the
    // main frame.
    EXPECT_EQ(expected_main_frame_changed_ct_, main_frame_changed_ct_);
  }

  // Number of times we expect the main frame to change (e.g. once per
  // cross-origin navigation).
  int expected_main_frame_changed_ct_ = 0;

  bool got_after_created_ = false;
  bool got_before_close_ = false;

 private:
  int get_frame_handler_ct_ = 0;
  int main_frame_changed_ct_ = 0;

  FrameStatus* current_main_frame_ = nullptr;
  FrameStatus* pending_main_frame_ = nullptr;

  IMPLEMENT_REFCOUNTING(OrderMainTestHandler);
};

}  // namespace

// Test the ordering and behavior of main frame callbacks.
TEST(FrameHandlerTest, OrderMain) {
  CefRefPtr<OrderMainTestHandler> handler = new OrderMainTestHandler();
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

namespace {

const char kOrderMainUrlPrefix[] = "http://tests-frame-handler";

class NavigateOrderMainTestHandler : public OrderMainTestHandler {
 public:
  NavigateOrderMainTestHandler(bool cross_origin, int additional_nav_ct = 2)
      : cross_origin_(cross_origin), additional_nav_ct_(additional_nav_ct) {
    // Once for each cross-origin LoadURL call.
    expected_main_frame_changed_ct_ = cross_origin ? additional_nav_ct_ : 0;
  }

  void RunTest() override {
    // Resources for the 2nd+ navigation.
    for (int i = 1; i <= additional_nav_ct_; i++) {
      AddResource(GetURLForNav(i), GetMainHtmlForNav(i), "text/html");
    }

    OrderMainTestHandler::RunTest();
  }

 protected:
  // Loaded when the browser is created.
  std::string GetMainURL() const override { return GetURLForNav(0); }
  std::string GetMainHtml() const override { return GetMainHtmlForNav(0); }

  std::string GetNextMainURL() override {
    if (current_nav_ct_ == additional_nav_ct_) {
      // No more navigations.
      return std::string();
    }
    return GetURLForNav(++current_nav_ct_);
  }

  bool IsFirstNavigation() const override { return current_nav_ct_ == 0U; }
  bool IsLastNavigation() const override {
    return current_nav_ct_ == additional_nav_ct_;
  }
  bool IsCrossOrigin() const override { return cross_origin_; }
  int additional_nav_ct() const { return additional_nav_ct_; }

  void VerifyTestResults() override {
    OrderMainTestHandler::VerifyTestResults();
    EXPECT_TRUE(IsLastNavigation());
  }

  virtual std::string GetMainHtmlForNav(int nav) const {
    return "<html><body>TEST " + std::to_string(nav) + "</body></html>";
  }

  std::string GetURLForNav(int nav, const std::string& suffix = "") const {
    std::stringstream ss;
    if (cross_origin_) {
      ss << kOrderMainUrlPrefix << nav << "/cross-origin" << suffix << ".html";
    } else {
      ss << kOrderMainUrlPrefix << "/" << nav << "same-origin" << suffix
         << ".html";
    }
    return ss.str();
  }

 private:
  const bool cross_origin_;
  const int additional_nav_ct_;

  int current_nav_ct_ = 0;
};

}  // namespace

// Main frame navigating to different URLs with the same origin.
TEST(FrameHandlerTest, OrderMainNavSameOrigin) {
  CefRefPtr<NavigateOrderMainTestHandler> handler =
      new NavigateOrderMainTestHandler(/*cross_origin=*/false);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Main frame navigating cross-origin.
TEST(FrameHandlerTest, OrderMainNavCrossOrigin) {
  CefRefPtr<NavigateOrderMainTestHandler> handler =
      new NavigateOrderMainTestHandler(/*cross_origin=*/true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

namespace {

// Tracks sub-frames for a single main frame load.
class FrameStatusMap {
 public:
  explicit FrameStatusMap(size_t expected_frame_ct)
      : expected_frame_ct_(expected_frame_ct) {}
  ~FrameStatusMap() { EXPECT_TRUE(frame_map_.empty()); }

  bool Contains(CefRefPtr<CefFrame> frame) const {
    return frame_map_.find(frame->GetIdentifier()) != frame_map_.end();
  }

  FrameStatus* CreateFrameStatus(CefRefPtr<CefFrame> frame) {
    EXPECT_UI_THREAD();

    EXPECT_LT(size(), expected_frame_ct_);

    const int64 id = frame->GetIdentifier();
    EXPECT_NE(kInvalidFrameId, id);
    EXPECT_EQ(frame_map_.find(id), frame_map_.end());

    FrameStatus* status = new FrameStatus(frame);
    frame_map_.insert(std::make_pair(id, status));
    return status;
  }

  FrameStatus* GetFrameStatus(CefRefPtr<CefFrame> frame) const {
    EXPECT_UI_THREAD();

    const int64 id = frame->GetIdentifier();
    EXPECT_NE(kInvalidFrameId, id);
    Map::const_iterator it = frame_map_.find(id);
    EXPECT_NE(it, frame_map_.end());
    return it->second;
  }

  void RemoveFrameStatus(CefRefPtr<CefFrame> frame) {
    const int64 id = frame->GetIdentifier();
    Map::iterator it = frame_map_.find(id);
    EXPECT_NE(it, frame_map_.end());
    frame_map_.erase(it);
  }

  void OnBeforeClose(CefRefPtr<CefBrowser> browser) {
    Map::const_iterator it = frame_map_.begin();
    for (; it != frame_map_.end(); ++it) {
      it->second->OnBeforeClose(browser);
    }
  }

  bool AllQueriesDelivered(std::string* msg = nullptr) const {
    if (size() != expected_frame_ct_) {
#if VERBOSE_DEBUGGING
      if (msg) {
        std::stringstream ss;
        ss << " SUB COUNT MISMATCH! size=" << size()
           << " expected=" << expected_frame_ct_;
        *msg += ss.str();
      }
#endif
      return false;
    }

    Map::const_iterator it = frame_map_.begin();
    for (; it != frame_map_.end(); ++it) {
      if (!it->second->AllQueriesDelivered(msg)) {
#if VERBOSE_DEBUGGING
        if (msg) {
          *msg += " " + it->second->GetDebugString() + " PENDING";
        }
#endif
        return false;
      }
    }

    return true;
  }

  bool AllFramesLoaded(std::string* msg = nullptr) const {
    if (size() != expected_frame_ct_) {
#if VERBOSE_DEBUGGING
      if (msg) {
        std::stringstream ss;
        ss << " SUB COUNT MISMATCH! size=" << size()
           << " expected=" << expected_frame_ct_;
        *msg += ss.str();
      }
#endif
      return false;
    }

    Map::const_iterator it = frame_map_.begin();
    for (; it != frame_map_.end(); ++it) {
      if (!it->second->IsTemporary() && !it->second->IsLoaded(msg)) {
#if VERBOSE_DEBUGGING
        if (msg) {
          *msg += " " + it->second->GetDebugString() + " PENDING";
        }
#endif
        return false;
      }
    }

    return true;
  }

  bool AllFramesDetached() const {
    if (size() != expected_frame_ct_)
      return false;

    Map::const_iterator it = frame_map_.begin();
    for (; it != frame_map_.end(); ++it) {
      if (!it->second->IsDetached())
        return false;
    }

    return true;
  }

  void VerifyAndClearTestResults() {
    EXPECT_EQ(expected_frame_ct_, size());
    Map::const_iterator it = frame_map_.begin();
    for (; it != frame_map_.end(); ++it) {
      it->second->VerifyTestResults();
      delete it->second;
    }
    frame_map_.clear();
  }

  size_t size() const { return frame_map_.size(); }

 private:
  using Map = std::map<int64, FrameStatus*>;
  Map frame_map_;

  // The expected number of sub-frames.
  const size_t expected_frame_ct_;
};

class OrderSubTestHandler : public NavigateOrderMainTestHandler {
 public:
  enum TestMode {
    // Two sub-frames at the same level.
    SUBFRAME_PEERS,

    // One sub-frame inside the other.
    SUBFRAME_CHILDREN,
  };

  OrderSubTestHandler(bool cross_origin,
                      int additional_nav_ct,
                      TestMode mode,
                      size_t expected_frame_ct = 2U)
      : NavigateOrderMainTestHandler(cross_origin, additional_nav_ct),
        test_mode_(mode),
        expected_frame_ct_(expected_frame_ct) {}
  ~OrderSubTestHandler() override { EXPECT_TRUE(frame_maps_.empty()); }

  void RunTest() override {
    for (int i = 0; i <= additional_nav_ct(); i++) {
      AddResource(GetSubURL1ForNav(i), GetSubFrameHtml1ForNav(i), "text/html");
      AddResource(GetSubURL2ForNav(i), GetSubFrameHtml2ForNav(i), "text/html");
    }

    NavigateOrderMainTestHandler::RunTest();
  }

  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
    NavigateOrderMainTestHandler::OnBeforeClose(browser);

    for (auto& map : frame_maps_) {
      // Also need to notify any sub-frames.
      map->OnBeforeClose(browser);
    }
  }

  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64 query_id,
               const CefString& request,
               bool persistent,
               CefRefPtr<Callback> callback) override {
    if (!frame->IsMain()) {
      auto map = GetFrameMap(frame);
      auto status = map->GetFrameStatus(frame);
      status->OnQuery(browser, frame, request);
      if (status->AllQueriesDelivered()) {
        MaybeDestroyTest();
      }
      return true;
    }

    return NavigateOrderMainTestHandler::OnQuery(browser, frame, query_id,
                                                 request, persistent, callback);
  }

  void OnFrameCreated(CefRefPtr<CefBrowser> browser,
                      CefRefPtr<CefFrame> frame) override {
    if (!frame->IsMain()) {
      // Potentially the first notification of a new sub-frame after navigation.
      auto map = GetOrCreateFrameMap(frame);
      auto status = map->CreateFrameStatus(frame);
      status->SetAdditionalDebugInfo(GetAdditionalDebugInfo());
      status->OnFrameCreated(browser, frame);
      return;
    }

    NavigateOrderMainTestHandler::OnFrameCreated(browser, frame);
  }

  void OnFrameAttached(CefRefPtr<CefBrowser> browser,
                       CefRefPtr<CefFrame> frame,
                       bool reattached) override {
    if (!frame->IsMain()) {
      auto map = GetFrameMap(frame);
      auto status = map->GetFrameStatus(frame);
      status->OnFrameAttached(browser, frame);
      return;
    }

    NavigateOrderMainTestHandler::OnFrameAttached(browser, frame, reattached);
  }

  void OnFrameDetached(CefRefPtr<CefBrowser> browser,
                       CefRefPtr<CefFrame> frame) override {
    if (!frame->IsMain()) {
      // Potentially the last notification for an old sub-frame after
      // navigation.
      auto map = GetFrameMap(frame);
      auto status = map->GetFrameStatus(frame);
      status->OnFrameDetached(browser, frame);

      if (map->AllFramesDetached()) {
        // Verify results from the previous navigation.
        VerifyAndClearSubFrameTestResults(map);
      }
      return;
    }

    NavigateOrderMainTestHandler::OnFrameDetached(browser, frame);
  }

  void OnLoadStart(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   TransitionType transition_type) override {
    if (!frame->IsMain()) {
      auto map = GetFrameMap(frame);
      auto status = map->GetFrameStatus(frame);
      status->OnLoadStart(browser, frame);
      return;
    }

    NavigateOrderMainTestHandler::OnLoadStart(browser, frame, transition_type);
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    if (!frame->IsMain()) {
      auto map = GetFrameMap(frame);
      auto status = map->GetFrameStatus(frame);
      status->OnLoadEnd(browser, frame);
      return;
    }

    NavigateOrderMainTestHandler::OnLoadEnd(browser, frame, httpStatusCode);
  }

 protected:
  virtual std::string GetSubURL1ForNav(int nav) const {
    return GetURLForNav(nav, "sub1");
  }

  std::string GetSubFrameHtml1ForNav(int nav) const {
    if (test_mode_ == SUBFRAME_CHILDREN) {
      return "<iframe src=\"" + GetSubURL2ForNav(nav) + "\">";
    }
    return "<html><body>Sub1</body></html>";
  }

  virtual std::string GetSubURL2ForNav(int nav) const {
    return GetURLForNav(nav, "sub2");
  }

  std::string GetSubFrameHtml2ForNav(int nav) const {
    return "<html><body>Sub2</body></html>";
  }

  std::string GetMainHtmlForNav(int nav) const override {
    if (test_mode_ == SUBFRAME_PEERS) {
      return "<html><body><iframe src=\"" + GetSubURL1ForNav(nav) +
             "\"></iframe><iframe src=\"" + GetSubURL2ForNav(nav) +
             "\"></iframe></body></html>";
    } else if (test_mode_ == SUBFRAME_CHILDREN) {
      return "<html><body><iframe src=\"" + GetSubURL1ForNav(nav) +
             "\"></iframe></iframe></body></html>";
    }
    NOTREACHED();
    return std::string();
  }

  bool AllQueriesDelivered(std::string* msg = nullptr) const override {
    if (!NavigateOrderMainTestHandler::AllQueriesDelivered(msg)) {
#if VERBOSE_DEBUGGING
      if (msg)
        *msg += " MAIN PENDING";
#endif
      return false;
    }

    if (frame_maps_.empty()) {
#if VERBOSE_DEBUGGING
      if (msg)
        *msg += " NO SUBS";
#endif
      return false;
    }

    if (!frame_maps_.back()->AllQueriesDelivered(msg)) {
#if VERBOSE_DEBUGGING
      if (msg)
        *msg += " SUBS PENDING";
#endif
      return false;
    }
    return true;
  }

  bool AllFramesLoaded(std::string* msg = nullptr) const override {
    if (!NavigateOrderMainTestHandler::AllFramesLoaded(msg)) {
#if VERBOSE_DEBUGGING
      if (msg)
        *msg += " MAIN PENDING";
#endif
      return false;
    }

    if (frame_maps_.empty()) {
#if VERBOSE_DEBUGGING
      if (msg)
        *msg += " NO SUBS";
#endif
      return false;
    }

    if (!frame_maps_.back()->AllFramesLoaded(msg)) {
#if VERBOSE_DEBUGGING
      if (msg)
        *msg += " SUBS PENDING";
#endif
      return false;
    }
    return true;
  }

  void VerifyTestResults() override {
    NavigateOrderMainTestHandler::VerifyTestResults();
    EXPECT_TRUE(frame_maps_.empty());
  }

  size_t expected_frame_ct() const { return expected_frame_ct_; }

  FrameStatusMap* GetFrameMap(CefRefPtr<CefFrame> frame) const {
    for (auto& map : frame_maps_) {
      if (map->Contains(frame))
        return map.get();
    }
    return nullptr;
  }

 private:
  // All sub-frame objects should already have received all callbacks.
  void VerifyAndClearSubFrameTestResults(FrameStatusMap* map) {
    map->VerifyAndClearTestResults();

    bool found = false;
    FrameStatusMapVector::iterator it = frame_maps_.begin();
    for (; it != frame_maps_.end(); ++it) {
      if ((*it).get() == map) {
        frame_maps_.erase(it);
        found = true;
        break;
      }
    }

    EXPECT_TRUE(found);
  }

  FrameStatusMap* GetOrCreateFrameMap(CefRefPtr<CefFrame> frame) {
    if (auto map = GetFrameMap(frame))
      return map;

    if (frame_maps_.empty() ||
        frame_maps_.back()->size() >= expected_frame_ct_) {
      // Start a new frame map.
      auto map = std::make_unique<FrameStatusMap>(expected_frame_ct_);
      frame_maps_.push_back(std::move(map));
    }

    return frame_maps_.back().get();
  }

  const TestMode test_mode_;

  // The expected number of sub-frames.
  const size_t expected_frame_ct_;

  using FrameStatusMapVector = std::vector<std::unique_ptr<FrameStatusMap>>;
  FrameStatusMapVector frame_maps_;
};

}  // namespace

// Main frame loads two sub-frames that are peers in the same origin.
TEST(FrameHandlerTest, OrderSubSameOriginPeers) {
  CefRefPtr<OrderSubTestHandler> handler =
      new OrderSubTestHandler(/*cross_origin=*/false, /*additional_nav_ct=*/0,
                              OrderSubTestHandler::SUBFRAME_PEERS);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Main frame loads two sub-frames that are peers in the same origin, then
// navigates in the same origin and does it again twice.
TEST(FrameHandlerTest, OrderSubSameOriginPeersNavSameOrigin) {
  CefRefPtr<OrderSubTestHandler> handler =
      new OrderSubTestHandler(/*cross_origin=*/false, /*additional_nav_ct=*/2,
                              OrderSubTestHandler::SUBFRAME_PEERS);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Main frame loads two sub-frames that are peers in the same origin, then
// navigates cross-origin and does it again twice.
TEST(FrameHandlerTest, OrderSubSameOriginPeersNavCrossOrigin) {
  CefRefPtr<OrderSubTestHandler> handler =
      new OrderSubTestHandler(/*cross_origin=*/true, /*additional_nav_ct=*/2,
                              OrderSubTestHandler::SUBFRAME_PEERS);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Main frame loads a sub-frame that then has it's own sub-frame.
TEST(FrameHandlerTest, OrderSubSameOriginChildren) {
  CefRefPtr<OrderSubTestHandler> handler =
      new OrderSubTestHandler(/*cross_origin=*/false, /*additional_nav_ct=*/0,
                              OrderSubTestHandler::SUBFRAME_CHILDREN);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Main frame loads a sub-frame that then has it's own sub-frame, then navigates
// in the same origin and does it again twice.
TEST(FrameHandlerTest, OrderSubSameOriginChildrenNavSameOrigin) {
  CefRefPtr<OrderSubTestHandler> handler =
      new OrderSubTestHandler(/*cross_origin=*/false, /*additional_nav_ct=*/2,
                              OrderSubTestHandler::SUBFRAME_CHILDREN);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Main frame loads a sub-frame that then has it's own sub-frame, then navigates
// cross-origin and does it again twice.
TEST(FrameHandlerTest, OrderSubSameOriginChildrenNavCrossOrigin) {
  CefRefPtr<OrderSubTestHandler> handler =
      new OrderSubTestHandler(/*cross_origin=*/true, /*additional_nav_ct=*/2,
                              OrderSubTestHandler::SUBFRAME_CHILDREN);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

namespace {

// Like above, but also navigating the sub-frames cross-origin.
class CrossOriginOrderSubTestHandler : public OrderSubTestHandler {
 public:
  CrossOriginOrderSubTestHandler(int additional_nav_ct, TestMode mode)
      : OrderSubTestHandler(/*cross_origin=*/true,
                            additional_nav_ct,
                            mode,
                            /*expected_frame_ct=*/4U) {}

  void OnFrameDetached(CefRefPtr<CefBrowser> browser,
                       CefRefPtr<CefFrame> frame) override {
    // A sub-frame is first created in the parent's renderer process. That
    // sub-frame is then discarded after the real cross-origin sub-frame is
    // created in a different renderer process. These discarded sub-frames will
    // get OnFrameCreated/OnFrameAttached immediately followed by
    // OnFrameDetached.
    if (!frame->IsMain()) {
      auto map = GetFrameMap(frame);
      auto status = map->GetFrameStatus(frame);
      if (status && !status->DidGetCallback(FrameStatus::LOAD_START)) {
        status->SetIsTemporary(true);
        temp_frame_detached_ct_++;
      }
    }

    OrderSubTestHandler::OnFrameDetached(browser, frame);
  }

 protected:
  std::string GetSubURL1ForNav(int nav) const override {
    std::stringstream ss;
    ss << kOrderMainUrlPrefix << nav << "-sub1/sub-cross-origin.html";
    return ss.str();
  }

  std::string GetSubURL2ForNav(int nav) const override {
    std::stringstream ss;
    ss << kOrderMainUrlPrefix << nav << "-sub2/sub-cross-origin.html";
    return ss.str();
  }

  void VerifyTestResults() override {
    OrderSubTestHandler::VerifyTestResults();

    const size_t expected_temp_ct =
        (expected_frame_ct() / 2U) * (1U + additional_nav_ct());
    EXPECT_EQ(expected_temp_ct, temp_frame_detached_ct_);
  }

 private:
  size_t temp_frame_detached_ct_ = 0U;
};

}  // namespace

// Main frame loads two sub-frames that are peers in a different origin.
TEST(FrameHandlerTest, OrderSubCrossOriginPeers) {
  CefRefPtr<CrossOriginOrderSubTestHandler> handler =
      new CrossOriginOrderSubTestHandler(
          /*additional_nav_ct=*/0,
          CrossOriginOrderSubTestHandler::SUBFRAME_PEERS);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Main frame loads two sub-frames that are peers in a different origin, then
// navigates cross-origin and does it again twice.
TEST(FrameHandlerTest, OrderSubCrossOriginPeersNavCrossOrigin) {
  CefRefPtr<CrossOriginOrderSubTestHandler> handler =
      new CrossOriginOrderSubTestHandler(
          /*additional_nav_ct=*/2,
          CrossOriginOrderSubTestHandler::SUBFRAME_PEERS);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Main frame loads a sub-frame in a different origin that then has it's own
// sub-frame in a different origin.
TEST(FrameHandlerTest, OrderSubCrossOriginChildren) {
  CefRefPtr<CrossOriginOrderSubTestHandler> handler =
      new CrossOriginOrderSubTestHandler(
          /*additional_nav_ct=*/0,
          CrossOriginOrderSubTestHandler::SUBFRAME_CHILDREN);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Main frame loads a sub-frame in a different origin that then has it's own
// sub-frame in a different origin, then navigates cross-origin and does it
// again twice.
TEST(FrameHandlerTest, OrderSubCrossOriginChildrenNavCrossOrigin) {
  CefRefPtr<CrossOriginOrderSubTestHandler> handler =
      new CrossOriginOrderSubTestHandler(
          /*additional_nav_ct=*/2,
          CrossOriginOrderSubTestHandler::SUBFRAME_CHILDREN);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

namespace {

const char kOrderMainCrossUrl[] =
    "http://tests-frame-handler-cross/main-order.html";

// Will be assigned as popup handler via
// ParentOrderMainTestHandler::OnBeforePopup.
class PopupOrderMainTestHandler : public OrderMainTestHandler {
 public:
  PopupOrderMainTestHandler(CompletionState* completion_state,
                            bool cross_origin)
      : OrderMainTestHandler(completion_state), cross_origin_(cross_origin) {
    expected_main_frame_changed_ct_ = cross_origin_ ? 1 : 0;
  }

  void SetupTest() override {
    // Proceed to RunTest().
    SetupComplete();
  }

  void RunTest() override {
    // Add the main resource that we will navigate to/from.
    AddResource(GetMainURL(), GetMainHtml(), "text/html");

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  void OnFrameCreated(CefRefPtr<CefBrowser> browser,
                      CefRefPtr<CefFrame> frame) override {
    EXPECT_UI_THREAD();

    EXPECT_TRUE(frame->IsMain());
    if (cross_origin_ && !temp_main_frame_) {
      // The first main frame in the popup will be created in the parent
      // process.
      EXPECT_FALSE(got_temp_created_);
      got_temp_created_.yes();

      temp_main_frame_ = new FrameStatus(frame);
      temp_main_frame_->SetAdditionalDebugInfo(GetAdditionalDebugInfo() +
                                               "temp ");
      temp_main_frame_->SetIsFirstMain(true);
      temp_main_frame_->OnFrameCreated(browser, frame);
      return;
    }

    OrderMainTestHandler::OnFrameCreated(browser, frame);
  }

  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    if (temp_main_frame_ && temp_main_frame_->IsSame(browser->GetMainFrame())) {
      EXPECT_FALSE(got_after_created_);
      got_after_created_ = true;

      EXPECT_TRUE(cross_origin_);
      temp_main_frame_->OnAfterCreated(browser);

      // Intentionally skipping the immediate parent class method.
      RoutingTestHandler::OnAfterCreated(browser);
      return;
    }

    OrderMainTestHandler::OnAfterCreated(browser);
  }

  void OnFrameAttached(CefRefPtr<CefBrowser> browser,
                       CefRefPtr<CefFrame> frame,
                       bool reattached) override {
    if (temp_main_frame_ && temp_main_frame_->IsSame(frame)) {
      EXPECT_TRUE(cross_origin_);
      temp_main_frame_->OnFrameAttached(browser, frame);
      return;
    }

    OrderMainTestHandler::OnFrameAttached(browser, frame, reattached);
  }

  void OnMainFrameChanged(CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefFrame> old_frame,
                          CefRefPtr<CefFrame> new_frame) override {
    if (temp_main_frame_ && temp_main_frame_->IsSame(new_frame)) {
      EXPECT_TRUE(cross_origin_);
      temp_main_frame_->OnMainFrameChanged(browser, old_frame, new_frame);
      return;
    }

    OrderMainTestHandler::OnMainFrameChanged(browser, old_frame, new_frame);
  }

  void OnFrameDetached(CefRefPtr<CefBrowser> browser,
                       CefRefPtr<CefFrame> frame) override {
    if (temp_main_frame_ && temp_main_frame_->IsSame(frame)) {
      EXPECT_TRUE(cross_origin_);
      EXPECT_FALSE(got_temp_destroyed_);
      got_temp_destroyed_.yes();

#if VERBOSE_DEBUGGING
      LOG(INFO) << temp_main_frame_->GetDebugString()
                << " callback OnFrameDetached(discarded)";
#endif

      // All of the initial main frame callbacks go to the proxy.
      EXPECT_TRUE(temp_main_frame_->DidGetCallback(FrameStatus::AFTER_CREATED));
      EXPECT_TRUE(temp_main_frame_->DidGetCallback(
          FrameStatus::MAIN_FRAME_INITIAL_ASSIGNED));
      EXPECT_TRUE(!temp_main_frame_->DidGetCallback(FrameStatus::LOAD_START));
      EXPECT_TRUE(temp_main_frame_->DidGetCallback(FrameStatus::FRAME_CREATED));
      EXPECT_TRUE(
          temp_main_frame_->DidGetCallback(FrameStatus::FRAME_ATTACHED));

      // Should receive queries for OnFrameCreated, OnAfterCreated,
      // OnFrameAttached.
      EXPECT_EQ(temp_main_frame_->QueriesDeliveredCount(), 3);

      delete temp_main_frame_;
      temp_main_frame_ = nullptr;
      return;
    }

    OrderMainTestHandler::OnFrameDetached(browser, frame);
  }

  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64 query_id,
               const CefString& request,
               bool persistent,
               CefRefPtr<Callback> callback) override {
    if (temp_main_frame_ && temp_main_frame_->IsSame(frame)) {
      EXPECT_TRUE(cross_origin_);
      temp_main_frame_->OnQuery(browser, frame, request);
      return true;
    }

    return OrderMainTestHandler::OnQuery(browser, frame, query_id, request,
                                         persistent, callback);
  }

  std::string GetMainURL() const override {
    return cross_origin_ ? kOrderMainCrossUrl : kOrderMainUrl;
  }

 protected:
  bool IsCrossOrigin() const override { return cross_origin_; }

  void VerifyTestResults() override {
    OrderMainTestHandler::VerifyTestResults();

    if (cross_origin_) {
      EXPECT_TRUE(got_temp_created_);
      EXPECT_TRUE(got_temp_destroyed_);
    } else {
      EXPECT_FALSE(got_temp_created_);
      EXPECT_FALSE(got_temp_destroyed_);
    }
    EXPECT_FALSE(temp_main_frame_);
  }

 private:
  std::string GetAdditionalDebugInfo() const override { return " popup: "; }

  const bool cross_origin_;

  TrackCallback got_temp_created_;
  TrackCallback got_temp_destroyed_;
  FrameStatus* temp_main_frame_ = nullptr;
};

class ParentOrderMainTestHandler : public OrderMainTestHandler {
 public:
  ParentOrderMainTestHandler(CompletionState* completion_state,
                             CefRefPtr<PopupOrderMainTestHandler> popup_handler)
      : OrderMainTestHandler(completion_state), popup_handler_(popup_handler) {}

  bool OnBeforePopup(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      const CefString& target_url,
      const CefString& target_frame_name,
      CefLifeSpanHandler::WindowOpenDisposition target_disposition,
      bool user_gesture,
      const CefPopupFeatures& popupFeatures,
      CefWindowInfo& windowInfo,
      CefRefPtr<CefClient>& client,
      CefBrowserSettings& settings,
      CefRefPtr<CefDictionaryValue>& extra_info,
      bool* no_javascript_access) override {
    // Intentionally not calling the parent class method.
    EXPECT_FALSE(got_on_before_popup_);
    got_on_before_popup_.yes();

    client = popup_handler_;
    popup_handler_ = nullptr;

    // Proceed with popup creation.
    return false;
  }

  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    OrderMainTestHandler::OnAfterCreated(browser);

    // Create the popup ASAP.
    browser->GetMainFrame()->ExecuteJavaScript(
        "window.open('" + popup_handler_->GetMainURL() + "');", CefString(), 0);
  }

  void SetupTest() override {
    // Proceed to RunTest().
    SetupComplete();
  }

  void DestroyTest() override {
    EXPECT_TRUE(got_on_before_popup_);
    OrderMainTestHandler::DestroyTest();
  }

 private:
  std::string GetAdditionalDebugInfo() const override { return "parent: "; }

  CefRefPtr<PopupOrderMainTestHandler> popup_handler_;

  TrackCallback got_on_before_popup_;
};

void RunOrderMainPopupTest(bool cross_origin) {
  TestHandler::CompletionState completion_state(/*count=*/2);
  TestHandler::Collection collection(&completion_state);

  CefRefPtr<PopupOrderMainTestHandler> popup_handler =
      new PopupOrderMainTestHandler(&completion_state, cross_origin);
  CefRefPtr<ParentOrderMainTestHandler> parent_handler =
      new ParentOrderMainTestHandler(&completion_state, popup_handler);

  collection.AddTestHandler(popup_handler.get());
  collection.AddTestHandler(parent_handler.get());
  collection.ExecuteTests();

  ReleaseAndWaitForDestructor(parent_handler);
  ReleaseAndWaitForDestructor(popup_handler);
}

}  // namespace

// Test the ordering and behavior of main frame callbacks in a popup with the
// same origin.
TEST(FrameHandlerTest, OrderMainPopupSameOrigin) {
  RunOrderMainPopupTest(/*cross_origin=*/false);
}

// Test the ordering and behavior of main frame callbacks in a popup with a
// different origin.
TEST(FrameHandlerTest, OrderMainPopupCrossOrigin) {
  RunOrderMainPopupTest(/*cross_origin=*/true);
}
