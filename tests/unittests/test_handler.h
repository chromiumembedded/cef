// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_UNITTESTS_TEST_HANDLER_H_
#define CEF_TESTS_UNITTESTS_TEST_HANDLER_H_
#pragma once

#include <list>
#include <map>
#include <string>
#include <utility>

#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_frame.h"
#include "include/cef_task.h"
#include "base/synchronization/waitable_event.h"
#include "testing/gtest/include/gtest/gtest.h"

class TrackCallback {
 public:
  TrackCallback(): gotit_(false) {}
  void yes() { gotit_ = true; }
  bool isSet() { return gotit_; }
  void reset() { gotit_ = false; }
  operator bool() const { return gotit_; }
 protected:
  bool gotit_;
};


// Base implementation of CefClient for unit tests. Add new interfaces as needed
// by test cases.
class TestHandler : public CefClient,
                    public CefDialogHandler,
                    public CefDisplayHandler,
                    public CefDownloadHandler,
                    public CefGeolocationHandler,
                    public CefJSDialogHandler,
                    public CefLifeSpanHandler,
                    public CefLoadHandler,
                    public CefRequestHandler {
 public:
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
    int count_;

    // Handle used to notify when the test is complete
    base::WaitableEvent event_;
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

  // The |completion_state| object if specified must outlive this class.
  explicit TestHandler(CompletionState* completion_state = NULL);
  virtual ~TestHandler();

  // Implement this method to set up the test. Only used in combination with a
  // Collection. Call SetupComplete() once the setup is complete.
  virtual void SetupTest() {}

  // Implement this method to run the test. Call DestroyTest() once the test is
  // complete.
  virtual void RunTest() =0;

  // CefClient methods. Add new methods as needed by test cases.
  virtual CefRefPtr<CefDialogHandler> GetDialogHandler() OVERRIDE {
    return this;
  }
  virtual CefRefPtr<CefDisplayHandler> GetDisplayHandler() OVERRIDE {
    return this;
  }
  virtual CefRefPtr<CefDownloadHandler> GetDownloadHandler() OVERRIDE {
    return this;
  }
  virtual CefRefPtr<CefGeolocationHandler> GetGeolocationHandler() OVERRIDE {
    return this;
  }
  virtual CefRefPtr<CefJSDialogHandler> GetJSDialogHandler() OVERRIDE {
    return this;
  }
  virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() OVERRIDE {
    return this;
  }
  virtual CefRefPtr<CefLoadHandler> GetLoadHandler() OVERRIDE {
    return this;
  }
  virtual CefRefPtr<CefRequestHandler> GetRequestHandler() OVERRIDE {
    return this;
  }

  // CefDownloadHandler methods
  virtual void OnBeforeDownload(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefDownloadItem> download_item,
      const CefString& suggested_name,
      CefRefPtr<CefBeforeDownloadCallback> callback) OVERRIDE {}

  // CefLifeSpanHandler methods
  virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) OVERRIDE;
  virtual void OnBeforeClose(CefRefPtr<CefBrowser> browser) OVERRIDE;

  // CefRequestHandler methods
  virtual CefRefPtr<CefResourceHandler> GetResourceHandler(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request) OVERRIDE;

  CefRefPtr<CefBrowser> GetBrowser() { return browser_; }
  int GetBrowserId() { return browser_id_; }

  // Called by the test function to execute the test. This method blocks until
  // the test is complete. Do not reference the object after this method
  // returns. Do not use this method if the CompletionState object is shared by
  // multiple handlers or when using a Collection object.
  void ExecuteTest();

  // Returns true if a browser currently exists.
  static bool HasBrowser() { return browser_count_ > 0; }

 protected:
  // Indicate that test setup is complete. Only used in combination with a
  // Collection.
  virtual void SetupComplete();

  // Destroy the browser window. Once the window is destroyed test completion
  // will be signaled.
  virtual void DestroyTest();

  void CreateBrowser(const CefString& url,
                     CefRefPtr<CefRequestContext> request_context = NULL);

  void AddResource(const std::string& url,
                   const std::string& content,
                   const std::string& mimeType);
  void ClearResources();

 private:
  // The child browser window
  CefRefPtr<CefBrowser> browser_;

  // The browser window identifier
  int browser_id_;

  // Used to notify when the test is complete
  CompletionState* completion_state_;
  bool completion_state_owned_;

  // Map of resources that can be automatically loaded
  typedef std::map<std::string, std::pair<std::string, std::string> >
      ResourceMap;
  ResourceMap resource_map_;

  // Include the default reference counting implementation.
  IMPLEMENT_REFCOUNTING(TestHandler);
  // Include the default locking implementation.
  IMPLEMENT_LOCKING(TestHandler);

  // Used to track the number of currently existing browser windows.
  static int browser_count_;
};


// Post a task to the specified thread and wait for the task to execute as
// indication that all previously pending tasks on that thread have completed.
void WaitForThread(CefThreadId thread_id);
void WaitForThread(CefRefPtr<CefTaskRunner> task_runner);

#define WaitForIOThread() WaitForThread(TID_IO)
#define WaitForUIThread() WaitForThread(TID_UI)

// Returns true if the currently running test has failed.
bool TestFailed();

#endif  // CEF_TESTS_UNITTESTS_TEST_HANDLER_H_
