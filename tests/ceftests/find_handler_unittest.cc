// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <vector>

#include "include/base/cef_callback.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/test_handler.h"
#include "tests/gtest/include/gtest/gtest.h"

namespace {

const char kFindUrl[] = "https://tests-find/find.html";

class FindTestHandler : public TestHandler, public CefFindHandler {
 public:
  struct FindResult {
    int identifier = 0;
    int count = 0;
    CefRect selection_rect;
    int active_match_ordinal = 0;
    bool final_update = false;
  };

  FindTestHandler() = default;

  FindTestHandler(const FindTestHandler&) = delete;
  FindTestHandler& operator=(const FindTestHandler&) = delete;

  void RunTest() override {
    const std::string html =
        "<html>"
        "<head><title>Find Test</title></head>"
        "<body>"
        "<p>The quick brown fox jumps over the lazy dog.</p>"
        "<p>The Fox and the Hound.</p>"
        "<p>Unique text here for testing.</p>"
        "</body>"
        "</html>";

    AddResource(kFindUrl, html, "text/html");
    CreateBrowser(kFindUrl);
    SetTestTimeout();
  }

  CefRefPtr<CefFindHandler> GetFindHandler() override { return this; }

  void OnDocumentAvailableInMainFrame(CefRefPtr<CefBrowser> browser) override {
    got_document_available_.yes();

    // Delay the find operation to ensure DOM is fully rendered and searchable
    CefPostDelayedTask(
        TID_UI, base::BindOnce(&FindTestHandler::PerformFind, this, browser),
        100);
  }

  void OnFindResult(CefRefPtr<CefBrowser> browser,
                    int identifier,
                    int count,
                    const CefRect& selectionRect,
                    int activeMatchOrdinal,
                    bool finalUpdate) override {
    got_on_find_result_.yes();

    EXPECT_TRUE(browser.get());
    EXPECT_TRUE(browser->IsSame(GetBrowser()));
    EXPECT_GE(identifier, 0);
    EXPECT_GE(count, 0);
    EXPECT_GE(activeMatchOrdinal, 0);

    // Store the result
    FindResult result;
    result.identifier = identifier;
    result.count = count;
    result.selection_rect = selectionRect;
    result.active_match_ordinal = activeMatchOrdinal;
    result.final_update = finalUpdate;
    results_.push_back(result);

    if (finalUpdate) {
      got_final_update_.yes();
      OnFindComplete(browser);
    }
  }

  // Override in subclasses to perform specific find operations
  virtual void PerformFind(CefRefPtr<CefBrowser> browser) = 0;

  // Override in subclasses to handle find completion
  virtual void OnFindComplete(CefRefPtr<CefBrowser> browser) = 0;

  void DestroyTest() override {
    EXPECT_TRUE(got_document_available_);
    EXPECT_TRUE(got_on_find_result_);
    EXPECT_TRUE(got_final_update_);

    // Clean up find state before destroying
    if (GetBrowser()) {
      GetBrowser()->GetHost()->StopFinding(true);
    }

    TestHandler::DestroyTest();
  }

 protected:
  std::vector<FindResult> results_;

  TrackCallback got_document_available_;
  TrackCallback got_on_find_result_;
  TrackCallback got_final_update_;
};

// Test finding text with a single match
class FindSingleMatchTestHandler : public FindTestHandler {
 public:
  FindSingleMatchTestHandler() = default;

  FindSingleMatchTestHandler(const FindSingleMatchTestHandler&) = delete;
  FindSingleMatchTestHandler& operator=(const FindSingleMatchTestHandler&) =
      delete;

  void PerformFind(CefRefPtr<CefBrowser> browser) override {
    // Search for "Unique" which appears once
    browser->GetHost()->Find("Unique", true, false, false);
  }

  void OnFindComplete(CefRefPtr<CefBrowser> browser) override {
    EXPECT_GE(results_.size(), 1U);

    if (!results_.empty()) {
      const FindResult& final_result = results_.back();
      EXPECT_TRUE(final_result.final_update);
      EXPECT_EQ(1, final_result.count);
      EXPECT_EQ(0, final_result.active_match_ordinal);
    }

    DestroyTest();
  }

 private:
  IMPLEMENT_REFCOUNTING(FindSingleMatchTestHandler);
};

// Test finding text with multiple matches
class FindMultipleMatchTestHandler : public FindTestHandler {
 public:
  FindMultipleMatchTestHandler() = default;

  FindMultipleMatchTestHandler(const FindMultipleMatchTestHandler&) = delete;
  FindMultipleMatchTestHandler& operator=(const FindMultipleMatchTestHandler&) =
      delete;

  void PerformFind(CefRefPtr<CefBrowser> browser) override {
    // Search for "the" which appears multiple times (case insensitive)
    browser->GetHost()->Find("the", true, false, false);
  }

  void OnFindComplete(CefRefPtr<CefBrowser> browser) override {
    EXPECT_GE(results_.size(), 1U);

    if (!results_.empty()) {
      const FindResult& final_result = results_.back();
      EXPECT_TRUE(final_result.final_update);
      EXPECT_GT(final_result.count, 1);
      EXPECT_GE(final_result.active_match_ordinal, 0);
      EXPECT_LT(final_result.active_match_ordinal, final_result.count);
    }

    DestroyTest();
  }

 private:
  IMPLEMENT_REFCOUNTING(FindMultipleMatchTestHandler);
};

// Test finding text with no matches
class FindNoMatchTestHandler : public FindTestHandler {
 public:
  FindNoMatchTestHandler() = default;

  FindNoMatchTestHandler(const FindNoMatchTestHandler&) = delete;
  FindNoMatchTestHandler& operator=(const FindNoMatchTestHandler&) = delete;

  void PerformFind(CefRefPtr<CefBrowser> browser) override {
    // Search for text that doesn't exist
    browser->GetHost()->Find("xyznotfound", true, false, false);
  }

  void OnFindComplete(CefRefPtr<CefBrowser> browser) override {
    EXPECT_GE(results_.size(), 1U);

    if (!results_.empty()) {
      const FindResult& final_result = results_.back();
      EXPECT_TRUE(final_result.final_update);
      EXPECT_EQ(0, final_result.count);
      EXPECT_EQ(0, final_result.active_match_ordinal);
    }

    DestroyTest();
  }

 private:
  IMPLEMENT_REFCOUNTING(FindNoMatchTestHandler);
};

// Test case-sensitive search
class FindCaseSensitiveTestHandler : public FindTestHandler {
 public:
  FindCaseSensitiveTestHandler() = default;

  FindCaseSensitiveTestHandler(const FindCaseSensitiveTestHandler&) = delete;
  FindCaseSensitiveTestHandler& operator=(const FindCaseSensitiveTestHandler&) =
      delete;

  void PerformFind(CefRefPtr<CefBrowser> browser) override {
    // Search for "Fox" (capital F) with case-sensitive matching
    browser->GetHost()->Find("Fox", true, true, false);
  }

  void OnFindComplete(CefRefPtr<CefBrowser> browser) override {
    EXPECT_GE(results_.size(), 1U);

    if (!results_.empty()) {
      const FindResult& final_result = results_.back();
      EXPECT_TRUE(final_result.final_update);
      // Should find only 1 match ("Fox" with capital F)
      EXPECT_EQ(1, final_result.count);
      EXPECT_EQ(0, final_result.active_match_ordinal);
    }

    DestroyTest();
  }

 private:
  IMPLEMENT_REFCOUNTING(FindCaseSensitiveTestHandler);
};

// Test case-insensitive search
class FindCaseInsensitiveTestHandler : public FindTestHandler {
 public:
  FindCaseInsensitiveTestHandler() = default;

  FindCaseInsensitiveTestHandler(const FindCaseInsensitiveTestHandler&) =
      delete;
  FindCaseInsensitiveTestHandler& operator=(
      const FindCaseInsensitiveTestHandler&) = delete;

  void PerformFind(CefRefPtr<CefBrowser> browser) override {
    // Search for "fox" (lowercase) with case-insensitive matching
    browser->GetHost()->Find("fox", true, false, false);
  }

  void OnFindComplete(CefRefPtr<CefBrowser> browser) override {
    EXPECT_GE(results_.size(), 1U);

    if (!results_.empty()) {
      const FindResult& final_result = results_.back();
      EXPECT_TRUE(final_result.final_update);
      // Should find 2 matches ("fox" and "Fox")
      EXPECT_EQ(2, final_result.count);
      EXPECT_GE(final_result.active_match_ordinal, 0);
      EXPECT_LT(final_result.active_match_ordinal, final_result.count);
    }

    DestroyTest();
  }

 private:
  IMPLEMENT_REFCOUNTING(FindCaseInsensitiveTestHandler);
};

// Test stopping find operation
class FindStopTestHandler : public FindTestHandler {
 public:
  FindStopTestHandler() = default;

  FindStopTestHandler(const FindStopTestHandler&) = delete;
  FindStopTestHandler& operator=(const FindStopTestHandler&) = delete;

  void PerformFind(CefRefPtr<CefBrowser> browser) override {
    // Start a search
    browser->GetHost()->Find("the", true, false, false);
  }

  void OnFindResult(CefRefPtr<CefBrowser> browser,
                    int identifier,
                    int count,
                    const CefRect& selectionRect,
                    int activeMatchOrdinal,
                    bool finalUpdate) override {
    // Call base implementation first
    FindTestHandler::OnFindResult(browser, identifier, count, selectionRect,
                                  activeMatchOrdinal, finalUpdate);

    // Stop the search after receiving the first result
    if (!got_stop_find_) {
      got_stop_find_.yes();
      browser->GetHost()->StopFinding(true);
      // DestroyTest after stopping since we won't get a final update
      DestroyTest();
    }
  }

  void OnFindComplete(CefRefPtr<CefBrowser> browser) override {
    // Should not be called since we stop the find operation
    ADD_FAILURE() << "OnFindComplete should not be called after StopFinding";
  }

  void DestroyTest() override {
    EXPECT_TRUE(got_document_available_);
    EXPECT_TRUE(got_on_find_result_);
    EXPECT_TRUE(got_stop_find_);
    // Don't check got_final_update_ since StopFinding interrupts it

    // Already called StopFinding in OnFindResult
    // Call TestHandler::DestroyTest to skip FindTestHandler cleanup
    TestHandler::DestroyTest();
  }

 private:
  TrackCallback got_stop_find_;

  IMPLEMENT_REFCOUNTING(FindStopTestHandler);
};

}  // namespace

TEST(FindHandlerTest, SingleMatch) {
  CefRefPtr<FindSingleMatchTestHandler> handler =
      new FindSingleMatchTestHandler();
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(FindHandlerTest, MultipleMatches) {
  CefRefPtr<FindMultipleMatchTestHandler> handler =
      new FindMultipleMatchTestHandler();
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(FindHandlerTest, NoMatch) {
  CefRefPtr<FindNoMatchTestHandler> handler = new FindNoMatchTestHandler();
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(FindHandlerTest, CaseSensitive) {
  CefRefPtr<FindCaseSensitiveTestHandler> handler =
      new FindCaseSensitiveTestHandler();
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(FindHandlerTest, CaseInsensitive) {
  CefRefPtr<FindCaseInsensitiveTestHandler> handler =
      new FindCaseInsensitiveTestHandler();
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(FindHandlerTest, StopFinding) {
  CefRefPtr<FindStopTestHandler> handler = new FindStopTestHandler();
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}
