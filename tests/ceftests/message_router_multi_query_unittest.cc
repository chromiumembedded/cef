// Copyright (c) 2022 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/ceftests/message_router_unittest_utils.h"

namespace {

const char kTestDomain1[] = "http://tests-mr1.com/";
const char kTestDomain2[] = "http://tests-mr2.com/";
const char kTestDomain3[] = "http://tests-mr3.com/";

const char kMultiQueryRequestId[] = "request_id";
const char kMultiQueryRepeatCt[] = "repeat_ct";
const char kMultiQueryRequest[] = "request";
const char kMultiQueryResponse[] = "response";
const char kMultiQuerySuccess[] = "success";
const char kMultiQueryError[] = "error";
const char kMultiQueryErrorMessage[] = "errormsg";
const int kMultiQueryPersistentResponseCount = 5;

// Generates HTML and verifies results for multiple simultanious queries.
class MultiQueryManager : public CefMessageRouterBrowserSide::Handler {
 public:
  enum TestType {
    // Initiates a non-persistent query with a successful response.
    // OnQuery and OnNotify will be called.
    SUCCESS,

    // Initiates a non-persistent query with a failure response.
    // OnQuery and OnNotify will be called.
    FAILURE,

    // Initiates a persistent query with multiple successful responses.
    // OnQuery, OnNotify and OnQueryCanceled will be called.
    PERSISTENT_SUCCESS,

    // Initiates a persistent query with multiple successful responses and one
    // failure response.
    // OnQuery and OnNotify will be called.
    PERSISTENT_FAILURE,

    // Initiates a non-persistent query that will be canceled via JavaScript.
    // No JavaScript callbacks will be executed.
    // OnQuery and OnQueryCanceled will be called.
    CANCEL,

    // Initiates a non-persistent query that will not be manually canceled.
    // No JavaScript callbacks will be executed.
    // OnQuery and OnQueryCanceled will be called.
    AUTOCANCEL,

    // Initiates a persistent query with multiple successful responses that will
    // not be manually canceled.
    // OnQuery, OnNotify and OnQueryCanceled will be called.
    PERSISTENT_AUTOCANCEL,
  };

  class Observer {
   public:
    // Called when all manual queries are complete.
    virtual void OnManualQueriesCompleted(MultiQueryManager* manager) {}

    // Called when all queries are complete.
    virtual void OnAllQueriesCompleted(MultiQueryManager* manager) {}

   protected:
    virtual ~Observer() {}
  };

  MultiQueryManager(const std::string& label,
                    bool synchronous,
                    int id_offset = 0)
      : label_(label),
        synchronous_(synchronous),
        id_offset_(id_offset),
        finalized_(false),
        running_(false),
        manual_total_(0),
        received_count_(0),
        manual_complete_count_(0),
        auto_complete_count_(0),
        will_cancel_by_removing_handler_(false),
        weak_ptr_factory_(this) {}

  virtual ~MultiQueryManager() {}

  std::string label() const { return label_; }

  void AddObserver(Observer* observer) {
    EXPECT_FALSE(running_);
    observer_set_.insert(observer);
  }

  void RemoveObserver(Observer* observer) {
    EXPECT_FALSE(running_);
    EXPECT_TRUE(observer_set_.erase(observer));
  }

  // Can be called from any thread, but should always be called from the same
  // thread.
  void AddTestQuery(TestType type) {
    EXPECT_FALSE(finalized_);
    test_query_vector_.push_back(TestQuery(type));
    if (!IsAuto(type))
      manual_total_++;
  }

  // Must be called after AddTestQuery and before the manager is used.
  void Finalize() {
    EXPECT_FALSE(finalized_);
    finalized_ = true;
  }

  // Call after all manual queries have completed if you intend to cancel auto
  // queries by removing the handler.
  void WillCancelByRemovingHandler() {
    EXPECT_TRUE(IsManualComplete());
    will_cancel_by_removing_handler_ = true;
  }

  std::string GetHTML(bool assert_total, bool assert_browser) const {
    EXPECT_TRUE(finalized_);
    EXPECT_FALSE(running_);

    std::string html;

    html = "<html><body>" + label_ + "<script>\n";

    // No requests should exist.
    if (assert_total)
      html += "window.mrtAssertTotalCount(" LINESTR ",0);\n";
    if (assert_browser)
      html += "window.mrtAssertBrowserCount(" LINESTR ",0);\n";
    html += "window.mrtAssertContextCount(" LINESTR ",0);\n";

    if (synchronous_) {
      // Run all of the queries synchronously. None will complete before the
      // last one begins.
      for (size_t i = 0; i < test_query_vector_.size(); ++i) {
        const TestQuery& query = test_query_vector_[i];
        html += GetQueryHTML(static_cast<int>(i), query);
      }

      const int total_ct = static_cast<int>(test_query_vector_.size());

      // Pending requests should match the total created.
      const std::string& total_val = GetIntString(total_ct);
      if (assert_total)
        html += "window.mrtAssertTotalCount(" LINESTR "," + total_val + ");\n";
      if (assert_browser)
        html +=
            "window.mrtAssertBrowserCount(" LINESTR "," + total_val + ");\n";
      html += "window.mrtAssertContextCount(" LINESTR "," + total_val + ");\n";

      int cancel_ct = 0;

      // Cancel all of the queries with type CANCEL.
      for (size_t i = 0; i < test_query_vector_.size(); ++i) {
        const TestQuery& query = test_query_vector_[i];
        if (query.type == CANCEL) {
          html += GetCancelHTML(static_cast<int>(i), query);
          cancel_ct++;
        }
      }

      if (cancel_ct > 0) {
        // Pending requests should match the total not canceled.
        const std::string& cancel_val = GetIntString(total_ct - cancel_ct);
        if (assert_total)
          html +=
              "window.mrtAssertTotalCount(" LINESTR "," + cancel_val + ");\n";
        if (assert_browser)
          html +=
              "window.mrtAssertBrowserCount(" LINESTR "," + cancel_val + ");\n";
        html +=
            "window.mrtAssertContextCount(" LINESTR "," + cancel_val + ");\n";
      }
    } else {
      // Run all of the queries asynchronously. Some may complete before
      // others begin.
      for (size_t i = 0; i < test_query_vector_.size(); ++i) {
        const TestQuery& query = test_query_vector_[i];

        const int index = static_cast<int>(i);

        // Each request is delayed by 10ms from the previous request.
        const std::string& delay_val = GetIntString(index);
        const std::string& query_html = GetQueryHTML(index, query);

        html += "window.setTimeout(function() {\n" + query_html;

        if (query.type == CANCEL) {
          // Cancel the query asynchronously with a 10ms delay.
          const std::string& request_id_var =
              GetIDString(kMultiQueryRequestId, index);
          html +=
              "  window.setTimeout(function() {\n"
              "    window.mrtQueryCancel(" +
              request_id_var +
              ");\n"
              "  }, 1);\n";
        }

        html += "\n}, " + delay_val + ");\n";
      }
    }

    html += "</script></body></html>";

    return html;
  }

  void OnNotify(CefRefPtr<CefBrowser> browser,
                CefRefPtr<CefFrame> frame,
                const std::string& message) {
    EXPECT_TRUE(finalized_);
    EXPECT_UI_THREAD();

    if (!running_)
      running_ = true;

    EXPECT_TRUE(browser.get());
    EXPECT_TRUE(frame.get());

    std::string value;
    int index = 0;
    EXPECT_TRUE(SplitIDString(message, &value, &index));

    TestQuery& query = test_query_vector_[index];

    // Verify that browser and frame are the same.
    EXPECT_EQ(query.browser_id, browser->GetIdentifier()) << index;
    EXPECT_EQ(query.frame_id, frame->GetIdentifier()) << index;

    // Verify a successful/expected result.
    if (will_cancel_by_removing_handler_) {
      // Auto queries receive an onFailure callback which will notify with error
      // when the handler is removed.
      EXPECT_STREQ(kMultiQueryError, value.c_str()) << index;
      EXPECT_TRUE(IsAuto(query.type)) << index;
      EXPECT_TRUE(query.got_query) << index;
      if (query.type == PERSISTENT_AUTOCANCEL)
        EXPECT_TRUE(query.got_success) << index;
      else
        EXPECT_FALSE(query.got_success) << index;

      query.got_error.yes();

      // There's a race between OnQueryCanceled and OnNotification. Only call
      // OnQueryCompleted a single time.
      if (query.got_query_canceled)
        OnQueryCompleted(query.type);
    } else {
      EXPECT_STREQ(kMultiQuerySuccess, value.c_str()) << index;
      EXPECT_TRUE(WillNotify(query.type)) << index;
      EXPECT_TRUE(query.got_query) << index;
      EXPECT_FALSE(query.got_query_canceled) << index;
      EXPECT_FALSE(query.got_success) << index;

      query.got_success.yes();

      // PERSISTENT_AUTOCANCEL doesn't call OnReceiveCompleted from OnQuery.
      if (query.type == PERSISTENT_AUTOCANCEL)
        OnReceiveCompleted(query.type);

      // Call OnQueryCompleted for types that don't get OnQueryCanceled.
      if (!WillCancel(query.type))
        OnQueryCompleted(query.type);
    }
  }

  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64 query_id,
               const CefString& request,
               bool persistent,
               CefRefPtr<Callback> callback) override {
    EXPECT_TRUE(finalized_);
    EXPECT_UI_THREAD();

    if (!running_)
      running_ = true;

    EXPECT_TRUE(browser.get());
    EXPECT_TRUE(frame.get());
    EXPECT_NE(0, query_id);

    std::string value;
    int index = 0;
    EXPECT_TRUE(SplitIDString(request, &value, &index));

    TestQuery& query = test_query_vector_[index];

    if (IsPersistent(query.type))
      EXPECT_TRUE(persistent);
    else
      EXPECT_FALSE(persistent);

    // Verify expected request.
    EXPECT_STREQ(kMultiQueryRequest, value.c_str()) << index;

    // Verify that call order is correct.
    EXPECT_FALSE(query.got_query) << index;
    EXPECT_FALSE(query.got_query_canceled) << index;
    EXPECT_FALSE(query.got_success) << index;
    EXPECT_FALSE(query.got_error) << index;

    query.got_query.yes();

    query.browser_id = browser->GetIdentifier();
    query.frame_id = frame->GetIdentifier();
    query.is_main_frame = frame->IsMain();

    if (query.type == SUCCESS) {
      // Send the single success response.
      callback->Success(GetIDString(kMultiQueryResponse, index));
    } else if (IsPersistent(query.type)) {
      // Send the required number of successful responses.
      const std::string& response = GetIDString(kMultiQueryResponse, index);
      for (int i = 0; i < kMultiQueryPersistentResponseCount; ++i)
        callback->Success(response);
    }

    if (WillFail(query.type)) {
      // Send the single failure response.
      callback->Failure(index, GetIDString(kMultiQueryErrorMessage, index));
    }

    if (WillCancel(query.type)) {
      // Hold onto the callback until the query is canceled.
      query.query_id = query_id;
      query.callback = callback;
    }

    // PERSISTENT_AUTOCANCEL will call OnReceiveCompleted once the success
    // notification is received.
    if (query.type != PERSISTENT_AUTOCANCEL)
      OnReceiveCompleted(query.type);

    return true;
  }

  void OnQueryCanceled(CefRefPtr<CefBrowser> browser,
                       CefRefPtr<CefFrame> frame,
                       int64 query_id) override {
    EXPECT_TRUE(finalized_);
    EXPECT_UI_THREAD();

    if (!running_)
      running_ = true;

    EXPECT_TRUE(browser.get());
    EXPECT_TRUE(frame.get());
    EXPECT_NE(0, query_id);

    bool found = false;
    for (size_t i = 0; i < test_query_vector_.size(); ++i) {
      TestQuery& query = test_query_vector_[i];
      if (query.query_id == query_id) {
        // Verify that browser and frame are the same.
        EXPECT_EQ(query.browser_id, browser->GetIdentifier()) << i;
        if (query.is_main_frame) {
          EXPECT_TRUE(frame->IsMain()) << i;
        } else {
          EXPECT_FALSE(frame->IsMain()) << i;
          EXPECT_EQ(query.frame_id, frame->GetIdentifier()) << i;
        }

        // Verify a successful/expected result.
        EXPECT_TRUE(WillCancel(query.type)) << i;
        EXPECT_TRUE(query.callback.get()) << i;

        // Release the callback.
        query.callback = nullptr;

        // Verify that call order is correct.
        EXPECT_TRUE(query.got_query) << i;

        if (query.type == CANCEL || query.type == AUTOCANCEL) {
          // No JavaScript onSuccess callback executes.
          EXPECT_FALSE(query.got_success) << i;
        } else {
          // JavaScript onSuccess does execute before cancellation.
          EXPECT_TRUE(query.got_success) << i;
        }

        query.got_query_canceled.yes();

        if (will_cancel_by_removing_handler_) {
          // There's a race between OnQueryCanceled and OnNotification. Only
          // call OnQueryCompleted a single time.
          if (query.got_error)
            OnQueryCompleted(query.type);
        } else {
          EXPECT_FALSE(query.got_error) << i;

          // Cancellation is always completion.
          OnQueryCompleted(query.type);
        }

        found = true;
        break;
      }
    }
    EXPECT_TRUE(found);
  }

  // Asserts that all queries have completed.
  void AssertAllComplete() const {
    EXPECT_TRUE(finalized_);
    EXPECT_FALSE(running_);
    EXPECT_UI_THREAD();

    for (size_t i = 0; i < test_query_vector_.size(); ++i) {
      const TestQuery& query = test_query_vector_[i];
      EXPECT_TRUE(query.got_query) << i;

      if (WillCancel(query.type))
        EXPECT_TRUE(query.got_query_canceled) << i;
      else
        EXPECT_FALSE(query.got_query_canceled) << i;

      if (WillNotify(query.type))
        EXPECT_TRUE(query.got_success) << i;
      else
        EXPECT_FALSE(query.got_success) << i;

      if (IsAuto(query.type) && will_cancel_by_removing_handler_)
        EXPECT_TRUE(query.got_error);
      else
        EXPECT_FALSE(query.got_error);

      EXPECT_FALSE(query.callback.get()) << i;
    }
  }

  // Returns true if all manual queries have completed.
  bool IsManualComplete() const {
    EXPECT_TRUE(finalized_);
    EXPECT_UI_THREAD();

    return (manual_complete_count_ == manual_total_);
  }

  // Returns true if all queries have completed.
  bool IsAllComplete() const {
    EXPECT_TRUE(finalized_);
    EXPECT_UI_THREAD();

    return (manual_complete_count_ + auto_complete_count_ ==
            static_cast<int>(test_query_vector_.size()));
  }

  bool HasAutoQueries() const {
    return (manual_total_ != static_cast<int>(test_query_vector_.size()));
  }

 private:
  struct TestQuery {
    explicit TestQuery(TestType test_type)
        : type(test_type),
          browser_id(0),
          frame_id(0),
          is_main_frame(false),
          query_id(0) {}

    TestType type;

    // Set in OnQuery and verified in OnNotify or OnQueryCanceled.
    int browser_id;
    int64 frame_id;
    bool is_main_frame;

    // Used when a query is canceled.
    int64 query_id;
    CefRefPtr<Callback> callback;

    TrackCallback got_query;
    TrackCallback got_query_canceled;
    TrackCallback got_success;
    TrackCallback got_error;
  };

  class NotifyTask : public CefTask {
   public:
    NotifyTask(base::WeakPtr<MultiQueryManager> weak_ptr, bool notify_all)
        : weak_ptr_(weak_ptr), notify_all_(notify_all) {}

    void Execute() override {
      if (weak_ptr_) {
        if (notify_all_)
          weak_ptr_->NotifyAllQueriesCompleted();
        else
          weak_ptr_->NotifyManualQueriesCompleted();
      }
    }

   private:
    base::WeakPtr<MultiQueryManager> weak_ptr_;
    const bool notify_all_;

    IMPLEMENT_REFCOUNTING(NotifyTask);
  };

  static bool IsAuto(TestType type) {
    return (type == AUTOCANCEL || type == PERSISTENT_AUTOCANCEL);
  }

  static bool IsPersistent(TestType type) {
    return (type == PERSISTENT_SUCCESS || type == PERSISTENT_FAILURE ||
            type == PERSISTENT_AUTOCANCEL);
  }

  static bool WillFail(TestType type) {
    return (type == FAILURE || type == PERSISTENT_FAILURE);
  }

  static bool WillCancel(TestType type) {
    return (type == PERSISTENT_SUCCESS || type == CANCEL ||
            type == AUTOCANCEL || type == PERSISTENT_AUTOCANCEL);
  }

  static bool WillNotify(TestType type) {
    return (type == SUCCESS || type == PERSISTENT_SUCCESS || type == FAILURE ||
            type == PERSISTENT_FAILURE || type == PERSISTENT_AUTOCANCEL);
  }

  void OnReceiveCompleted(TestType type) {
    const int total_count = static_cast<int>(test_query_vector_.size());
    if (++received_count_ == total_count && manual_total_ == 0) {
      // There aren't any manual queries so notify here.
      CefPostTask(TID_UI,
                  new NotifyTask(weak_ptr_factory_.GetWeakPtr(), false));
    }
  }

  void OnQueryCompleted(TestType type) {
    const int total_count = static_cast<int>(test_query_vector_.size());
    EXPECT_LT(manual_complete_count_ + auto_complete_count_, total_count);
    EXPECT_LE(manual_complete_count_, manual_total_);

    const bool is_auto = IsAuto(type);
    if (is_auto)
      auto_complete_count_++;
    else if (++manual_complete_count_ == manual_total_) {
      CefPostTask(TID_UI,
                  new NotifyTask(weak_ptr_factory_.GetWeakPtr(), false));
    }

    if (auto_complete_count_ + manual_complete_count_ == total_count) {
      running_ = false;
      CefPostTask(TID_UI, new NotifyTask(weak_ptr_factory_.GetWeakPtr(), true));
    }
  }

  void NotifyManualQueriesCompleted() {
    if (observer_set_.empty())
      return;

    // Use a copy of the set in case an Observer is removed while we're
    // iterating.
    ObserverSet observer_set = observer_set_;

    ObserverSet::const_iterator it = observer_set.begin();
    for (; it != observer_set.end(); ++it) {
      (*it)->OnManualQueriesCompleted(this);
    }
  }

  void NotifyAllQueriesCompleted() {
    if (observer_set_.empty())
      return;

    // Use a copy of the set in case an Observer is removed while we're
    // iterating.
    ObserverSet observer_set = observer_set_;

    ObserverSet::const_iterator it = observer_set.begin();
    for (; it != observer_set.end(); ++it) {
      (*it)->OnAllQueriesCompleted(this);
    }
  }

  std::string GetQueryHTML(const int index, const TestQuery& query) const {
    const std::string& request_id_var =
        GetIDString(kMultiQueryRequestId, index);
    const std::string& repeat_ct_var = GetIDString(kMultiQueryRepeatCt, index);
    const std::string& request_val =
        GetIDString(std::string(kMultiQueryRequest) + ":", index);
    const std::string& success_val =
        GetIDString(std::string(kMultiQuerySuccess) + ":", index);
    const std::string& error_val =
        GetIDString(std::string(kMultiQueryError) + ":", index);

    std::string html;

    const bool persistent = IsPersistent(query.type);

    if (persistent)
      html += "var " + repeat_ct_var + " = 0;\n";

    html += "var " + request_id_var +
            " = window.mrtQuery({\n"
            "  request: '" +
            request_val +
            "',\n"
            "  persistent: " +
            (persistent ? "true" : "false") + ",\n";

    if (query.type == SUCCESS) {
      const std::string& response_val = GetIDString(kMultiQueryResponse, index);

      html +=
          "  onSuccess: function(response) {\n"
          "    if (response == '" +
          response_val +
          "')\n"
          "      window.mrtNotify('" +
          success_val +
          "');\n"
          "    else\n"
          "      window.mrtNotify('" +
          error_val +
          "');\n"
          "  },\n"
          "  onFailure: function(error_code, error_message) {\n"
          "    window.mrtNotify('" +
          error_val +
          "');\n"
          "  }\n";
    } else if (query.type == FAILURE) {
      const std::string& error_code_val = GetIntString(index);
      const std::string& error_message_val =
          GetIDString(kMultiQueryErrorMessage, index);

      html +=
          "  onSuccess: function(response) {\n"
          "    window.mrtNotify('" +
          error_val +
          "');\n"
          "  },\n"
          "  onFailure: function(error_code, error_message) {\n"
          "    if (error_code == " +
          error_code_val + " && error_message == '" + error_message_val +
          "')\n"
          "      window.mrtNotify('" +
          success_val +
          "');\n"
          "    else\n"
          "      window.mrtNotify('" +
          error_val +
          "');\n"
          "  }\n";
    } else if (query.type == PERSISTENT_SUCCESS ||
               query.type == PERSISTENT_AUTOCANCEL) {
      const std::string& response_val = GetIDString(kMultiQueryResponse, index);
      const std::string& repeat_ct =
          GetIntString(kMultiQueryPersistentResponseCount);

      html +=
          "  onSuccess: function(response) {\n"
          "    if (response == '" +
          response_val +
          "') {\n"
          // Should get repeat_ct number of successful responses.
          "      if (++" +
          repeat_ct_var + " == " + repeat_ct +
          ") {\n"
          "        window.mrtNotify('" +
          success_val + "');\n";

      if (query.type == PERSISTENT_SUCCESS) {
        // Manually cancel the request.
        html += "        window.mrtQueryCancel(" + request_id_var + ");\n";
      }

      html +=
          "      }\n"
          "    } else {\n"
          "      window.mrtNotify('" +
          error_val +
          "');\n"
          "    }\n"
          "  },\n"
          "  onFailure: function(error_code, error_message) {\n"
          "    window.mrtNotify('" +
          error_val +
          "');\n"
          "  }\n";
    } else if (query.type == PERSISTENT_FAILURE) {
      const std::string& error_code_val = GetIntString(index);
      const std::string& error_message_val =
          GetIDString(kMultiQueryErrorMessage, index);
      const std::string& repeat_ct =
          GetIntString(kMultiQueryPersistentResponseCount);

      html +=
          "  onSuccess: function(response) {\n"
          // Should get some successful responses before failure.
          "    if (++" +
          repeat_ct_var + " > " + repeat_ct +
          ") {\n"
          "      window.mrtNotify('" +
          error_val +
          "');\n"
          "    }\n"
          "  },\n"
          "  onFailure: function(error_code, error_message) {\n"
          "    if (error_code == " +
          error_code_val + " && error_message == '" + error_message_val +
          "'"
          " && " +
          repeat_ct_var + " == " + repeat_ct +
          ")\n"
          "      window.mrtNotify('" +
          success_val +
          "');\n"
          "    else\n"
          "      window.mrtNotify('" +
          error_val +
          "');\n"
          "  }\n";
    } else if (query.type == CANCEL || query.type == AUTOCANCEL) {
      html +=
          "  onSuccess: function(response) {\n"
          "    window.mrtNotify('" +
          error_val +
          "');\n"
          "  },\n"
          "  onFailure: function(error_code, error_message) {\n"
          "    window.mrtNotify('" +
          error_val +
          "');\n"
          "  }\n";
    }

    html += "});\n";

    return html;
  }

  std::string GetCancelHTML(const int index, const TestQuery& query) const {
    const std::string& request_id_var =
        GetIDString(kMultiQueryRequestId, index);
    return "window.mrtQueryCancel(" + request_id_var + ");\n";
  }

  std::string GetIDString(const std::string& prefix, int index) const {
    EXPECT_TRUE(!prefix.empty());
    std::stringstream ss;
    ss << prefix << GetIDFromIndex(index);
    return ss.str();
  }

  bool SplitIDString(const std::string& str,
                     std::string* value,
                     int* index) const {
    size_t pos = str.find(':');
    if (pos != std::string::npos) {
      *value = str.substr(0, pos);
      *index = GetIndexFromID(atoi(str.substr(pos + 1).c_str()));
      return (*index >= 0 &&
              *index < static_cast<int>(test_query_vector_.size()));
    }

    return false;
  }

  std::string GetIntString(int val) const {
    std::stringstream ss;
    ss << val;
    return ss.str();
  }

  int GetIDFromIndex(int index) const { return id_offset_ + index; }
  int GetIndexFromID(int id) const { return id - id_offset_; }

  const std::string label_;
  const bool synchronous_;
  const int id_offset_;

  typedef std::vector<TestQuery> TestQueryVector;
  TestQueryVector test_query_vector_;

  typedef std::set<Observer*> ObserverSet;
  ObserverSet observer_set_;

  // Set to true after all queries have been added.
  bool finalized_;
  // Set to true while queries are pending.
  bool running_;

  // Total number of queries that will manually complete.
  int manual_total_;

  // Number of queries that have been received.
  int received_count_;

  // Number of queries that have completed successfully.
  int manual_complete_count_;
  int auto_complete_count_;

  // If true any pending queries will receive an onFailure callback in addition
  // to be canceled.
  bool will_cancel_by_removing_handler_;

  // Should always be the last member.
  base::WeakPtrFactory<MultiQueryManager> weak_ptr_factory_;
};

void MakeTestQueries(MultiQueryManager* manager,
                     bool some,
                     int many_count = 200) {
  if (some) {
    // Test some queries of arbitrary types.
    // Use a hard-coded list so the behavior is deterministic across test runs.
    MultiQueryManager::TestType types[] = {
        MultiQueryManager::PERSISTENT_AUTOCANCEL,
        MultiQueryManager::SUCCESS,
        MultiQueryManager::AUTOCANCEL,
        MultiQueryManager::PERSISTENT_FAILURE,
        MultiQueryManager::CANCEL,
        MultiQueryManager::FAILURE,
        MultiQueryManager::AUTOCANCEL,
        MultiQueryManager::SUCCESS,
        MultiQueryManager::PERSISTENT_SUCCESS,
        MultiQueryManager::SUCCESS,
        MultiQueryManager::PERSISTENT_AUTOCANCEL,
        MultiQueryManager::CANCEL,
        MultiQueryManager::PERSISTENT_SUCCESS,
        MultiQueryManager::FAILURE,
    };
    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); ++i) {
      manager->AddTestQuery(types[i]);
    }
  } else {
    // Test every type of query.
    for (int i = 0; i < many_count; ++i) {
      MultiQueryManager::TestType type = MultiQueryManager::SUCCESS;
      switch (i % 7) {
        case 0:
          type = MultiQueryManager::SUCCESS;
          break;
        case 1:
          type = MultiQueryManager::FAILURE;
          break;
        case 2:
          type = MultiQueryManager::PERSISTENT_SUCCESS;
          break;
        case 3:
          type = MultiQueryManager::PERSISTENT_FAILURE;
          break;
        case 4:
          type = MultiQueryManager::CANCEL;
          break;
        case 5:
          type = MultiQueryManager::AUTOCANCEL;
          break;
        case 6:
          type = MultiQueryManager::PERSISTENT_AUTOCANCEL;
          break;
      }
      manager->AddTestQuery(type);
    }
  }
  manager->Finalize();
}

// Test multiple queries in a single page load with a single frame.
class MultiQuerySingleFrameTestHandler : public SingleLoadTestHandler,
                                         public MultiQueryManager::Observer {
 public:
  enum CancelType {
    CANCEL_BY_NAVIGATION,
    CANCEL_BY_REMOVING_HANDLER,
    CANCEL_BY_CLOSING_BROWSER,
  };

  MultiQuerySingleFrameTestHandler(
      bool synchronous,
      CancelType cancel_type = CANCEL_BY_NAVIGATION)
      : manager_(std::string(), synchronous), cancel_type_(cancel_type) {
    manager_.AddObserver(this);
  }

  std::string GetMainHTML() override { return manager_.GetHTML(true, true); }

  void OnNotify(CefRefPtr<CefBrowser> browser,
                CefRefPtr<CefFrame> frame,
                const std::string& message) override {
    AssertMainBrowser(browser);
    AssertMainFrame(frame);

    manager_.OnNotify(browser, frame, message);
  }

  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64 query_id,
               const CefString& request,
               bool persistent,
               CefRefPtr<Callback> callback) override {
    AssertMainBrowser(browser);
    AssertMainFrame(frame);

    return manager_.OnQuery(browser, frame, query_id, request, persistent,
                            callback);
  }

  void OnQueryCanceled(CefRefPtr<CefBrowser> browser,
                       CefRefPtr<CefFrame> frame,
                       int64 query_id) override {
    AssertMainBrowser(browser);
    AssertMainFrame(frame);

    manager_.OnQueryCanceled(browser, frame, query_id);
  }

  void OnManualQueriesCompleted(MultiQueryManager* manager) override {
    EXPECT_EQ(manager, &manager_);
    if (manager_.HasAutoQueries()) {
      if (cancel_type_ == CANCEL_BY_NAVIGATION) {
        // Navigate somewhere else to terminate the auto queries.
        GetBrowser()->GetMainFrame()->LoadURL(std::string(kTestDomain1) +
                                              "cancel.html");
      } else if (cancel_type_ == CANCEL_BY_REMOVING_HANDLER) {
        // Change the expected behavior in the manager.
        manager_.WillCancelByRemovingHandler();
        GetRouter()->RemoveHandler(this);
        // All queries should be immediately canceled.
        AssertQueryCount(nullptr, nullptr, 0);
      } else if (cancel_type_ == CANCEL_BY_CLOSING_BROWSER) {
        // Change the expected behavior in the handler.
        SetSignalCompletionWhenAllBrowsersClose(false);
        CloseBrowser(GetBrowser(), false);
      }
    }
  }

  void OnAllQueriesCompleted(MultiQueryManager* manager) override {
    EXPECT_EQ(manager, &manager_);

    // All queries should be canceled.
    AssertQueryCount(nullptr, nullptr, 0);

    DestroyTest();

    if (!SignalCompletionWhenAllBrowsersClose()) {
      // Complete asynchronously so the call stack has a chance to unwind.
      CefPostTask(TID_UI,
                  base::BindOnce(
                      &MultiQuerySingleFrameTestHandler::TestComplete, this));
    }
  }

  void DestroyTest() override {
    manager_.AssertAllComplete();
    TestHandler::DestroyTest();
  }

  MultiQueryManager* GetManager() { return &manager_; }

 private:
  MultiQueryManager manager_;
  const CancelType cancel_type_;
};

}  // namespace

#define MULTI_QUERY_SINGLE_FRAME_TYPE_TEST(name, type, synchronous) \
  TEST(MessageRouterTest, name) {                                   \
    CefRefPtr<MultiQuerySingleFrameTestHandler> handler =           \
        new MultiQuerySingleFrameTestHandler(synchronous);          \
    MultiQueryManager* manager = handler->GetManager();             \
    manager->AddTestQuery(MultiQueryManager::type);                 \
    manager->Finalize();                                            \
    handler->ExecuteTest();                                         \
    ReleaseAndWaitForDestructor(handler);                           \
  }

// Test the query types individually.
MULTI_QUERY_SINGLE_FRAME_TYPE_TEST(MultiQuerySingleFrameSyncSuccess,
                                   SUCCESS,
                                   true)
MULTI_QUERY_SINGLE_FRAME_TYPE_TEST(MultiQuerySingleFrameAsyncSuccess,
                                   SUCCESS,
                                   false)
MULTI_QUERY_SINGLE_FRAME_TYPE_TEST(MultiQuerySingleFrameSyncFailure,
                                   FAILURE,
                                   true)
MULTI_QUERY_SINGLE_FRAME_TYPE_TEST(MultiQuerySingleFrameAsyncFailure,
                                   FAILURE,
                                   false)
MULTI_QUERY_SINGLE_FRAME_TYPE_TEST(MultiQuerySingleFrameSyncPersistentSuccess,
                                   PERSISTENT_SUCCESS,
                                   true)
MULTI_QUERY_SINGLE_FRAME_TYPE_TEST(MultiQuerySingleFrameAsyncPersistentSuccess,
                                   PERSISTENT_SUCCESS,
                                   false)
MULTI_QUERY_SINGLE_FRAME_TYPE_TEST(MultiQuerySingleFrameSyncPersistentFailure,
                                   PERSISTENT_FAILURE,
                                   true)
MULTI_QUERY_SINGLE_FRAME_TYPE_TEST(MultiQuerySingleFrameAsyncPersistentFailure,
                                   PERSISTENT_FAILURE,
                                   false)
MULTI_QUERY_SINGLE_FRAME_TYPE_TEST(MultiQuerySingleFrameCancel, CANCEL, true)
MULTI_QUERY_SINGLE_FRAME_TYPE_TEST(MultiQuerySingleFrameAutoCancel,
                                   AUTOCANCEL,
                                   true)
MULTI_QUERY_SINGLE_FRAME_TYPE_TEST(MultiQuerySingleFramePersistentAutoCancel,
                                   PERSISTENT_AUTOCANCEL,
                                   true)

// Test that one frame can run some queries successfully in a synchronous
// manner.
TEST(MessageRouterTest, MultiQuerySingleFrameSyncSome) {
  CefRefPtr<MultiQuerySingleFrameTestHandler> handler =
      new MultiQuerySingleFrameTestHandler(true);
  MakeTestQueries(handler->GetManager(), true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Test that one frame can run some queries successfully in an asynchronous
// manner.
TEST(MessageRouterTest, MultiQuerySingleFrameAsyncSome) {
  CefRefPtr<MultiQuerySingleFrameTestHandler> handler =
      new MultiQuerySingleFrameTestHandler(false);
  MakeTestQueries(handler->GetManager(), true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Test that one frame can run many queries successfully in a synchronous
// manner.
TEST(MessageRouterTest, MultiQuerySingleFrameSyncMany) {
  CefRefPtr<MultiQuerySingleFrameTestHandler> handler =
      new MultiQuerySingleFrameTestHandler(true);
  MakeTestQueries(handler->GetManager(), false);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Test that one frame can run many queries successfully in an asynchronous
// manner.
TEST(MessageRouterTest, MultiQuerySingleFrameAsyncMany) {
  CefRefPtr<MultiQuerySingleFrameTestHandler> handler =
      new MultiQuerySingleFrameTestHandler(false);
  MakeTestQueries(handler->GetManager(), false);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Test that pending queries can be canceled by removing the handler.
TEST(MessageRouterTest, MultiQuerySingleFrameCancelByRemovingHandler) {
  CefRefPtr<MultiQuerySingleFrameTestHandler> handler =
      new MultiQuerySingleFrameTestHandler(
          false, MultiQuerySingleFrameTestHandler::CANCEL_BY_REMOVING_HANDLER);
  MakeTestQueries(handler->GetManager(), false);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Test that pending queries can be canceled by closing the browser.
TEST(MessageRouterTest, MultiQuerySingleFrameCancelByClosingBrowser) {
  CefRefPtr<MultiQuerySingleFrameTestHandler> handler =
      new MultiQuerySingleFrameTestHandler(
          false, MultiQuerySingleFrameTestHandler::CANCEL_BY_CLOSING_BROWSER);
  MakeTestQueries(handler->GetManager(), false);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

namespace {

// Test multiple handlers.
class MultiQueryMultiHandlerTestHandler : public SingleLoadTestHandler,
                                          public MultiQueryManager::Observer {
 public:
  class Handler : public CefMessageRouterBrowserSide::Handler {
   public:
    Handler(MultiQueryMultiHandlerTestHandler* test_handler, int index)
        : test_handler_(test_handler), index_(index), query_id_(0) {}

    bool OnQuery(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int64 query_id,
                 const CefString& request,
                 bool persistent,
                 CefRefPtr<Callback> callback) override {
      // Each handler only handles a single request.
      std::stringstream ss;
      ss << kMultiQueryRequest << ":" << index_;
      const std::string& handled_request = ss.str();
      if (request != handled_request)
        return false;

      // Verify that handlers are called in the correct order.
      if (index_ == 0) {
        EXPECT_FALSE(test_handler_->got_query0_);
        EXPECT_FALSE(test_handler_->got_query1_);
        EXPECT_FALSE(test_handler_->got_query2_);

        test_handler_->got_query0_.yes();
      } else if (index_ == 1) {
        EXPECT_TRUE(test_handler_->got_query0_);
        EXPECT_FALSE(test_handler_->got_query1_);
        EXPECT_FALSE(test_handler_->got_query2_);

        test_handler_->got_query1_.yes();
      } else if (index_ == 2) {
        EXPECT_TRUE(test_handler_->got_query0_);
        EXPECT_TRUE(test_handler_->got_query1_);
        EXPECT_FALSE(test_handler_->got_query2_);

        test_handler_->got_query2_.yes();
      }

      query_id_ = query_id;
      return test_handler_->OnQuery(browser, frame, query_id, request,
                                    persistent, callback);
    }

    void OnQueryCanceled(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         int64 query_id) override {
      // Verify that the correct handler is called for cancellation.
      EXPECT_EQ(query_id_, query_id);

      if (index_ == 0) {
        EXPECT_FALSE(test_handler_->got_query_canceled0_);
        test_handler_->got_query_canceled0_.yes();
      } else if (index_ == 1) {
        EXPECT_FALSE(test_handler_->got_query_canceled1_);
        test_handler_->got_query_canceled1_.yes();
      } else if (index_ == 2) {
        EXPECT_FALSE(test_handler_->got_query_canceled2_);
        test_handler_->got_query_canceled2_.yes();
      }

      test_handler_->OnQueryCanceled(browser, frame, query_id);
    }

   private:
    MultiQueryMultiHandlerTestHandler* test_handler_;
    const int index_;
    int query_id_;
  };

  MultiQueryMultiHandlerTestHandler(bool synchronous,
                                    bool cancel_by_removing_handler)
      : manager_(std::string(), synchronous, 0),
        handler0_(this, 0),
        handler1_(this, 1),
        handler2_(this, 2),
        cancel_by_removing_handler_(cancel_by_removing_handler) {
    manager_.AddObserver(this);

    // Each handler will handle one of the queries.
    manager_.AddTestQuery(MultiQueryManager::PERSISTENT_AUTOCANCEL);
    manager_.AddTestQuery(MultiQueryManager::PERSISTENT_AUTOCANCEL);
    manager_.AddTestQuery(MultiQueryManager::PERSISTENT_AUTOCANCEL);
    manager_.Finalize();
  }

  std::string GetMainHTML() override { return manager_.GetHTML(true, true); }

  void OnNotify(CefRefPtr<CefBrowser> browser,
                CefRefPtr<CefFrame> frame,
                const std::string& message) override {
    AssertMainBrowser(browser);
    AssertMainFrame(frame);

    manager_.OnNotify(browser, frame, message);
  }

  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64 query_id,
               const CefString& request,
               bool persistent,
               CefRefPtr<Callback> callback) override {
    AssertMainBrowser(browser);
    AssertMainFrame(frame);

    return manager_.OnQuery(browser, frame, query_id, request, persistent,
                            callback);
  }

  void OnQueryCanceled(CefRefPtr<CefBrowser> browser,
                       CefRefPtr<CefFrame> frame,
                       int64 query_id) override {
    AssertMainBrowser(browser);
    AssertMainFrame(frame);

    manager_.OnQueryCanceled(browser, frame, query_id);
  }

  void OnManualQueriesCompleted(MultiQueryManager* manager) override {
    EXPECT_EQ(manager, &manager_);

    EXPECT_TRUE(got_query0_);
    EXPECT_TRUE(got_query1_);
    EXPECT_TRUE(got_query2_);
    EXPECT_FALSE(got_query_canceled0_);
    EXPECT_FALSE(got_query_canceled1_);
    EXPECT_FALSE(got_query_canceled2_);

    EXPECT_TRUE(manager_.HasAutoQueries());

    CefRefPtr<CefMessageRouterBrowserSide> router = GetRouter();

    // Remove one handler to cancel a query.

    if (cancel_by_removing_handler_) {
      manager_.WillCancelByRemovingHandler();

      // Each query should be canceled as the handler is removed.
      EXPECT_TRUE(router->RemoveHandler(&handler1_));
      EXPECT_FALSE(got_query_canceled0_);
      EXPECT_TRUE(got_query_canceled1_);
      EXPECT_FALSE(got_query_canceled2_);

      EXPECT_TRUE(router->RemoveHandler(&handler2_));
      EXPECT_FALSE(got_query_canceled0_);
      EXPECT_TRUE(got_query_canceled2_);

      EXPECT_TRUE(router->RemoveHandler(&handler0_));
      EXPECT_TRUE(got_query_canceled0_);
    } else {
      GetBrowser()->GetMainFrame()->LoadURL(std::string(kTestDomain1) +
                                            "cancel.html");
    }
  }

  void OnAllQueriesCompleted(MultiQueryManager* manager) override {
    EXPECT_EQ(manager, &manager_);

    // All queries should be canceled.
    AssertQueryCount(nullptr, nullptr, 0);

    DestroyTest();
  }

  void DestroyTest() override {
    EXPECT_TRUE(got_query0_);
    EXPECT_TRUE(got_query1_);
    EXPECT_TRUE(got_query2_);
    EXPECT_TRUE(got_query_canceled0_);
    EXPECT_TRUE(got_query_canceled1_);
    EXPECT_TRUE(got_query_canceled2_);

    manager_.AssertAllComplete();
    TestHandler::DestroyTest();
  }

 protected:
  void AddHandlers(
      CefRefPtr<CefMessageRouterBrowserSide> message_router) override {
    // OnQuery call order will verify that the first/last ordering works as
    // expected.
    EXPECT_TRUE(message_router->AddHandler(&handler1_, true));
    EXPECT_TRUE(message_router->AddHandler(&handler0_, true));
    EXPECT_TRUE(message_router->AddHandler(&handler2_, false));

    // Can't add the same handler multiple times.
    EXPECT_FALSE(message_router->AddHandler(&handler1_, true));
  }

 private:
  MultiQueryManager manager_;
  Handler handler0_;
  Handler handler1_;
  Handler handler2_;

  const bool cancel_by_removing_handler_;

  TrackCallback got_query0_;
  TrackCallback got_query1_;
  TrackCallback got_query2_;

  TrackCallback got_query_canceled0_;
  TrackCallback got_query_canceled1_;
  TrackCallback got_query_canceled2_;
};

}  // namespace

// Test that multiple handlers behave correctly.
TEST(MessageRouterTest, MultiQueryMultiHandler) {
  CefRefPtr<MultiQueryMultiHandlerTestHandler> handler =
      new MultiQueryMultiHandlerTestHandler(false, false);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Test that multiple handlers behave correctly. Cancel by removing the
// handlers.
TEST(MessageRouterTest, MultiQueryMultiHandlerCancelByRemovingHandler) {
  CefRefPtr<MultiQueryMultiHandlerTestHandler> handler =
      new MultiQueryMultiHandlerTestHandler(false, true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

namespace {

// Map of managers on a per-URL basis.
class MultiQueryManagerMap : public CefMessageRouterBrowserSide::Handler,
                             public MultiQueryManager::Observer {
 public:
  class Observer {
   public:
    // Called when all manual queries are complete.
    virtual void OnMapManualQueriesCompleted(MultiQueryManagerMap* map) {}

    // Called when all queries are complete.
    virtual void OnMapAllQueriesCompleted(MultiQueryManagerMap* map) {}

   protected:
    virtual ~Observer() {}
  };

  MultiQueryManagerMap()
      : finalized_(false),
        running_(false),
        manual_complete_count_(0),
        total_complete_count_(0) {}

  virtual ~MultiQueryManagerMap() { RemoveAllManagers(); }

  void AddObserver(Observer* observer) {
    EXPECT_FALSE(running_);
    observer_set_.insert(observer);
  }

  void RemoveObserver(Observer* observer) {
    EXPECT_FALSE(running_);
    EXPECT_TRUE(observer_set_.erase(observer));
  }

  MultiQueryManager* CreateManager(const std::string& url, bool synchronous) {
    EXPECT_FALSE(finalized_);

    MultiQueryManager* manager = new MultiQueryManager(
        url, synchronous, static_cast<int>(manager_map_.size()) * 1000);
    manager->AddObserver(this);
    all_managers_.push_back(manager);
    pending_managers_.push_back(manager);

    return manager;
  }

  void Finalize() {
    EXPECT_FALSE(finalized_);
    finalized_ = true;
  }

  std::string GetMainHTML() const {
    EXPECT_TRUE(finalized_);
    EXPECT_FALSE(running_);

    std::string html = "<html><body>\n";

    for (size_t i = 0; i < all_managers_.size(); ++i) {
      const std::string& url = all_managers_[i]->label();
      const std::string& name = GetNameForURL(url);
      html += "<iframe id=\"" + name + "\" src=\"" + url + "\"></iframe>\n";
    }

    html += "</body></html>";
    return html;
  }

  void OnNotify(CefRefPtr<CefBrowser> browser,
                CefRefPtr<CefFrame> frame,
                const std::string& message) {
    EXPECT_TRUE(finalized_);
    if (!running_)
      running_ = true;

    MultiQueryManager* manager = GetManager(browser, frame);
    manager->OnNotify(browser, frame, message);
  }

  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64 query_id,
               const CefString& request,
               bool persistent,
               CefRefPtr<Callback> callback) override {
    EXPECT_TRUE(finalized_);
    if (!running_)
      running_ = true;

    MultiQueryManager* manager = GetManager(browser, frame);
    return manager->OnQuery(browser, frame, query_id, request, persistent,
                            callback);
  }

  void OnQueryCanceled(CefRefPtr<CefBrowser> browser,
                       CefRefPtr<CefFrame> frame,
                       int64 query_id) override {
    EXPECT_TRUE(finalized_);
    if (!running_)
      running_ = true;

    MultiQueryManager* manager = GetManager(browser, frame);
    manager->OnQueryCanceled(browser, frame, query_id);
  }

  void OnManualQueriesCompleted(MultiQueryManager* manager) override {
    const int size = static_cast<int>(all_managers_.size());
    EXPECT_LT(manual_complete_count_, size);
    if (++manual_complete_count_ == size) {
      running_ = false;

      // Notify observers.
      if (!observer_set_.empty()) {
        // Use a copy of the set in case an Observer is removed while we're
        // iterating.
        ObserverSet observer_set = observer_set_;

        ObserverSet::const_iterator it = observer_set.begin();
        for (; it != observer_set.end(); ++it) {
          (*it)->OnMapManualQueriesCompleted(this);
        }
      }
    }
  }

  void OnAllQueriesCompleted(MultiQueryManager* manager) override {
    const int size = static_cast<int>(all_managers_.size());
    EXPECT_LT(total_complete_count_, size);
    if (++total_complete_count_ == size) {
      running_ = false;

      // Notify observers.
      if (!observer_set_.empty()) {
        // Use a copy of the set in case an Observer is removed while we're
        // iterating.
        ObserverSet observer_set = observer_set_;

        ObserverSet::const_iterator it = observer_set.begin();
        for (; it != observer_set.end(); ++it) {
          (*it)->OnMapAllQueriesCompleted(this);
        }
      }
    }
  }

  bool AllComplete() const {
    EXPECT_TRUE(finalized_);

    for (size_t i = 0; i < all_managers_.size(); ++i) {
      if (!all_managers_[i]->IsAllComplete())
        return false;
    }
    return true;
  }

  void AssertAllComplete() const {
    EXPECT_TRUE(finalized_);
    EXPECT_TRUE(pending_managers_.empty());
    EXPECT_FALSE(running_);

    for (size_t i = 0; i < all_managers_.size(); ++i) {
      all_managers_[i]->AssertAllComplete();
    }
  }

  bool HasAutoQueries() const {
    for (size_t i = 0; i < all_managers_.size(); ++i) {
      if (all_managers_[i]->HasAutoQueries())
        return true;
    }

    return false;
  }

  void OnLoadStart(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame) {
    if (pending_managers_.empty())
      return;

    const std::string& expected_url = frame->GetURL();
    MultiQueryManager* next_manager = nullptr;

    // Find the pending manager that matches the expected URL.
    ManagerList::iterator it = pending_managers_.begin();
    for (; it != pending_managers_.end(); ++it) {
      if ((*it)->label() == expected_url) {
        next_manager = *it;
        pending_managers_.erase(it);
        break;
      }
    }

    EXPECT_TRUE(next_manager);

    const int browser_id = browser->GetIdentifier();
    // Always use the same ID for the main frame.
    const int64 frame_id = frame->IsMain() ? -1 : frame->GetIdentifier();

    const std::pair<int, int64>& id = std::make_pair(browser_id, frame_id);

    // Remove the currently active manager, if any.
    ManagerMap::iterator it2 = manager_map_.find(id);
    if (it2 != manager_map_.end())
      manager_map_.erase(it2);

    // Add the next manager to the active map.
    manager_map_.insert(std::make_pair(id, next_manager));
  }

  MultiQueryManager* GetManager(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame) const {
    const int browser_id = browser->GetIdentifier();
    // Always use the same ID for the main frame.
    const int64 frame_id = frame->IsMain() ? -1 : frame->GetIdentifier();

    // Find the manager in the active map.
    ManagerMap::const_iterator it =
        manager_map_.find(std::make_pair(browser_id, frame_id));
    EXPECT_NE(it, manager_map_.end())
        << "browser_id = " << browser_id << ", frame_id = " << frame_id;
    return it->second;
  }

  void RemoveAllManagers() {
    EXPECT_TRUE(pending_managers_.empty());
    if (all_managers_.empty())
      return;

    for (size_t i = 0; i < all_managers_.size(); ++i) {
      delete all_managers_[i];
    }
    all_managers_.clear();
    manager_map_.clear();
  }

  static std::string GetNameForURL(const std::string& url) {
    // Extract the file name without extension.
    int pos1 = static_cast<int>(url.rfind("/"));
    int pos2 = static_cast<int>(url.rfind("."));
    EXPECT_TRUE(pos1 >= 0 && pos2 >= 0 && pos1 < pos2);
    return url.substr(pos1 + 1, pos2 - pos1 - 1);
  }

 private:
  typedef std::vector<MultiQueryManager*> ManagerList;
  // Map of (browser ID, frame ID) to manager.
  typedef std::map<std::pair<int, int64>, MultiQueryManager*> ManagerMap;

  // All managers that have been created.
  ManagerList all_managers_;
  // Managers that have not yet associated with a frame.
  ManagerList pending_managers_;
  // Managers that are currently active.
  ManagerMap manager_map_;

  typedef std::set<Observer*> ObserverSet;
  ObserverSet observer_set_;

  // Set to true after all query managers have been added.
  bool finalized_;
  // Set to true while queries are pending.
  bool running_;

  // Number of managers that have completed.
  int manual_complete_count_;
  int total_complete_count_;
};

// Test multiple queries in a single page load with multiple frames.
class MultiQueryMultiFrameTestHandler : public SingleLoadTestHandler,
                                        public MultiQueryManagerMap::Observer {
 public:
  MultiQueryMultiFrameTestHandler(bool synchronous, bool cancel_with_subnav)
      : synchronous_(synchronous), cancel_with_subnav_(cancel_with_subnav) {
    manager_map_.AddObserver(this);
  }

  void AddOtherResources() override {
    AddSubFrameResource("sub1");
    AddSubFrameResource("sub2");
    AddSubFrameResource("sub3");
    manager_map_.Finalize();

    if (manager_map_.HasAutoQueries()) {
      cancel_url_ = std::string(kTestDomain1) + "cancel.html";
      AddResource(cancel_url_, "<html><body>cancel</body></html>", "text/html");
    }
  }

  std::string GetMainHTML() override { return manager_map_.GetMainHTML(); }

  void OnLoadStart(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   TransitionType transition_type) override {
    AssertMainBrowser(browser);
    if (!frame->IsMain())
      manager_map_.OnLoadStart(browser, frame);
  }

  void OnNotify(CefRefPtr<CefBrowser> browser,
                CefRefPtr<CefFrame> frame,
                const std::string& message) override {
    AssertMainBrowser(browser);
    EXPECT_FALSE(frame->IsMain());

    manager_map_.OnNotify(browser, frame, message);
  }

  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64 query_id,
               const CefString& request,
               bool persistent,
               CefRefPtr<Callback> callback) override {
    AssertMainBrowser(browser);
    EXPECT_FALSE(frame->IsMain());

    return manager_map_.OnQuery(browser, frame, query_id, request, persistent,
                                callback);
  }

  void OnQueryCanceled(CefRefPtr<CefBrowser> browser,
                       CefRefPtr<CefFrame> frame,
                       int64 query_id) override {
    AssertMainBrowser(browser);
    EXPECT_FALSE(frame->IsMain());

    manager_map_.OnQueryCanceled(browser, frame, query_id);
  }

  void OnMapManualQueriesCompleted(MultiQueryManagerMap* map) override {
    EXPECT_EQ(map, &manager_map_);
    if (manager_map_.HasAutoQueries()) {
      CefRefPtr<CefFrame> frame = GetBrowser()->GetMainFrame();

      // Navigate somewhere else to terminate the auto queries.
      if (cancel_with_subnav_) {
        // Navigate each subframe individually.
        const std::string js = "document.getElementById('sub1').src = '" +
                               cancel_url_ +
                               "';"
                               "document.getElementById('sub2').src = '" +
                               cancel_url_ +
                               "';"
                               "document.getElementById('sub3').src = '" +
                               cancel_url_ + "';";

        frame->ExecuteJavaScript(js, frame->GetURL(), 0);
      } else {
        // Navigate the main frame.
        frame->LoadURL(cancel_url_);
      }
    }
  }

  void OnMapAllQueriesCompleted(MultiQueryManagerMap* map) override {
    EXPECT_EQ(map, &manager_map_);
    DestroyTest();
  }

  void DestroyTest() override {
    manager_map_.AssertAllComplete();
    TestHandler::DestroyTest();
  }

 private:
  void AddSubFrameResource(const std::string& name) {
    const std::string& url = std::string(kTestDomain1) + name + ".html";

    MultiQueryManager* manager = manager_map_.CreateManager(url, synchronous_);
    MakeTestQueries(manager, false, 100);

    const std::string& html = manager->GetHTML(false, false);
    AddResource(url, html, "text/html");
  }

  const bool synchronous_;
  const bool cancel_with_subnav_;

  MultiQueryManagerMap manager_map_;

  std::string cancel_url_;
};

}  // namespace

// Test that multiple frames can run many queries successfully in a synchronous
// manner.
TEST(MessageRouterTest, MultiQueryMultiFrameSync) {
  CefRefPtr<MultiQueryMultiFrameTestHandler> handler =
      new MultiQueryMultiFrameTestHandler(true, false);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Test that multiple frames can run many queries successfully in an
// asynchronous manner.
TEST(MessageRouterTest, MultiQueryMultiFrameAsync) {
  CefRefPtr<MultiQueryMultiFrameTestHandler> handler =
      new MultiQueryMultiFrameTestHandler(false, false);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Test that multiple frames can run many queries successfully in a synchronous
// manner. Cancel auto queries with sub-frame navigation.
TEST(MessageRouterTest, MultiQueryMultiFrameSyncSubnavCancel) {
  CefRefPtr<MultiQueryMultiFrameTestHandler> handler =
      new MultiQueryMultiFrameTestHandler(true, true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Test that multiple frames can run many queries successfully in an
// asynchronous manner. Cancel auto queries with sub-frame navigation.
TEST(MessageRouterTest, MultiQueryMultiFrameAsyncSubnavCancel) {
  CefRefPtr<MultiQueryMultiFrameTestHandler> handler =
      new MultiQueryMultiFrameTestHandler(false, true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

namespace {

// Implementation of MRTestHandler that loads multiple pages and/or browsers and
// executes multiple queries.
class MultiQueryMultiLoadTestHandler
    : public MRTestHandler,
      public CefMessageRouterBrowserSide::Handler,
      public MultiQueryManagerMap::Observer,
      public MultiQueryManager::Observer {
 public:
  MultiQueryMultiLoadTestHandler(bool some, bool synchronous)
      : some_(some), synchronous_(synchronous) {
    manager_map_.AddObserver(this);
  }

  void OnLoadStart(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   TransitionType transition_type) override {
    manager_map_.OnLoadStart(browser, frame);
  }

  void OnNotify(CefRefPtr<CefBrowser> browser,
                CefRefPtr<CefFrame> frame,
                const std::string& message) override {
    manager_map_.OnNotify(browser, frame, message);
  }

  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64 query_id,
               const CefString& request,
               bool persistent,
               CefRefPtr<Callback> callback) override {
    return manager_map_.OnQuery(browser, frame, query_id, request, persistent,
                                callback);
  }

  void OnQueryCanceled(CefRefPtr<CefBrowser> browser,
                       CefRefPtr<CefFrame> frame,
                       int64 query_id) override {
    manager_map_.OnQueryCanceled(browser, frame, query_id);
  }

  void OnMapManualQueriesCompleted(MultiQueryManagerMap* map) override {
    EXPECT_EQ(map, &manager_map_);
    if (manager_map_.HasAutoQueries()) {
      // Navigate all browsers somewhere else to terminate the auto queries.
      BrowserMap browser_map;
      GetAllBrowsers(&browser_map);

      BrowserMap::const_iterator it = browser_map.begin();
      for (; it != browser_map.end(); ++it) {
        it->second->GetMainFrame()->LoadURL(cancel_url_);
      }
    }
  }

  void OnMapAllQueriesCompleted(MultiQueryManagerMap* map) override {
    EXPECT_EQ(map, &manager_map_);
    DestroyTest();
  }

  void DestroyTest() override {
    manager_map_.AssertAllComplete();
    TestHandler::DestroyTest();
  }

 protected:
  void AddHandlers(
      CefRefPtr<CefMessageRouterBrowserSide> message_router) override {
    message_router->AddHandler(this, false);
  }

  void AddManagedResource(const std::string& url,
                          bool assert_total,
                          bool assert_browser) {
    MultiQueryManager* manager = manager_map_.CreateManager(url, synchronous_);
    manager->AddObserver(this);
    MakeTestQueries(manager, some_, 75);

    const std::string& html = manager->GetHTML(assert_total, assert_browser);
    AddResource(url, html, "text/html");
  }

  void Finalize() {
    manager_map_.Finalize();

    if (manager_map_.HasAutoQueries()) {
      cancel_url_ = std::string(kTestDomain1) + "cancel.html";
      AddResource(cancel_url_, "<html><body>cancel</body></html>", "text/html");
    }
  }

  MultiQueryManagerMap manager_map_;

 private:
  const bool some_;
  const bool synchronous_;

  std::string cancel_url_;
};

// Test multiple browsers that send queries at the same time.
class MultiQueryMultiBrowserTestHandler
    : public MultiQueryMultiLoadTestHandler {
 public:
  MultiQueryMultiBrowserTestHandler(bool synchronous, bool same_origin)
      : MultiQueryMultiLoadTestHandler(false, synchronous),
        same_origin_(same_origin) {}

 protected:
  void RunMRTest() override {
    const std::string& url1 = std::string(kTestDomain1) + "browser1.html";
    const std::string& url2 =
        std::string(same_origin_ ? kTestDomain1 : kTestDomain2) +
        "browser2.html";
    const std::string& url3 =
        std::string(same_origin_ ? kTestDomain1 : kTestDomain3) +
        "browser3.html";

    AddManagedResource(url1, false, true);
    AddManagedResource(url2, false, true);
    AddManagedResource(url3, false, true);
    Finalize();

    // Create 2 browsers simultaniously.
    CreateBrowser(url1, nullptr);
    CreateBrowser(url2, nullptr);
    CreateBrowser(url3, nullptr);
  }

 private:
  bool same_origin_;
};

}  // namespace

// Test that multiple browsers can query simultaniously from the same origin.
TEST(MessageRouterTest, MultiQueryMultiBrowserSameOriginSync) {
  CefRefPtr<MultiQueryMultiBrowserTestHandler> handler =
      new MultiQueryMultiBrowserTestHandler(true, true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Test that multiple browsers can query simultaniously from the same origin.
TEST(MessageRouterTest, MultiQueryMultiBrowserSameOriginAsync) {
  CefRefPtr<MultiQueryMultiBrowserTestHandler> handler =
      new MultiQueryMultiBrowserTestHandler(false, true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Test that multiple browsers can query simultaniously from different origins.
TEST(MessageRouterTest, MultiQueryMultiBrowserDifferentOriginSync) {
  CefRefPtr<MultiQueryMultiBrowserTestHandler> handler =
      new MultiQueryMultiBrowserTestHandler(true, false);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Test that multiple browsers can query simultaniously from different origins.
TEST(MessageRouterTest, MultiQueryMultiBrowserDifferentOriginAsync) {
  CefRefPtr<MultiQueryMultiBrowserTestHandler> handler =
      new MultiQueryMultiBrowserTestHandler(false, false);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

namespace {

// Test multiple navigations that send queries sequentially.
class MultiQueryMultiNavigateTestHandler
    : public MultiQueryMultiLoadTestHandler {
 public:
  MultiQueryMultiNavigateTestHandler(bool synchronous, bool same_origin)
      : MultiQueryMultiLoadTestHandler(false, synchronous),
        same_origin_(same_origin) {}

  void OnManualQueriesCompleted(MultiQueryManager* manager) override {
    const std::string& url = manager->label();
    if (url == url1_)  // 2. Load the 2nd url.
      GetBrowser()->GetMainFrame()->LoadURL(url2_);
    else if (url == url2_)  // 3. Load the 3rd url.
      GetBrowser()->GetMainFrame()->LoadURL(url3_);
  }

 protected:
  void RunMRTest() override {
    url1_ = std::string(kTestDomain1) + "browser1.html";
    url2_ = std::string(same_origin_ ? kTestDomain1 : kTestDomain2) +
            "browser2.html";
    url3_ = std::string(same_origin_ ? kTestDomain1 : kTestDomain3) +
            "browser3.html";

    // With same-site BFCache enabled a new browser will be created for each
    // same-site navigation in the renderer process, resulting in "total count"
    // values that potentially span multiple navigations.
    const bool should_assert = !(same_origin_ && IsSameSiteBFCacheEnabled());
    AddManagedResource(url1_, should_assert, should_assert);
    AddManagedResource(url2_, should_assert, should_assert);
    AddManagedResource(url3_, should_assert, should_assert);
    Finalize();

    // 1. Load the 1st url.
    CreateBrowser(url1_, nullptr);
  }

 private:
  bool same_origin_;

  std::string url1_;
  std::string url2_;
  std::string url3_;
};

}  // namespace

// Test that multiple navigations can query from the same origin.
TEST(MessageRouterTest, MultiQueryMultiNavigateSameOriginSync) {
  CefRefPtr<MultiQueryMultiNavigateTestHandler> handler =
      new MultiQueryMultiNavigateTestHandler(true, true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Test that multiple navigations can query from the same origin.
TEST(MessageRouterTest, MultiQueryMultiNavigateSameOriginAsync) {
  CefRefPtr<MultiQueryMultiNavigateTestHandler> handler =
      new MultiQueryMultiNavigateTestHandler(false, true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Test that multiple navigations can query from different origins.
TEST(MessageRouterTest, MultiQueryMultiNavigateDifferentOriginSync) {
  CefRefPtr<MultiQueryMultiNavigateTestHandler> handler =
      new MultiQueryMultiNavigateTestHandler(true, false);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Test that multiple navigations can query from different origins.
TEST(MessageRouterTest, MultiQueryMultiNavigateDifferentOriginAsync) {
  CefRefPtr<MultiQueryMultiNavigateTestHandler> handler =
      new MultiQueryMultiNavigateTestHandler(false, false);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}
