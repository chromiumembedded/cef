// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_UNITTESTS_TEST_APP_H_
#define CEF_TESTS_UNITTESTS_TEST_APP_H_
#pragma once

#include <map>
#include <set>
#include <string>
#include <utility>
#include "include/cef_app.h"

class TestApp : public CefApp,
                public CefRenderProcessHandler {
 public:
  // Interface for renderer tests. All tests must be returned via CreateTests.
  // Do not perform work in the test constructor as this will slow down every
  // test run.
  class Test : public virtual CefBase {
   public:
    // Called when WebKit is initialized. Used to register V8 extensions.
    virtual void OnWebKitInitialized(CefRefPtr<TestApp> test_app) {
    };

    // Called when a V8 context is created. Used to create V8 window bindings
    // and set message callbacks. Tests should check for unique URLs to avoid
    // interfering with each other.
    virtual void OnContextCreated(CefRefPtr<TestApp> test_app,
                                  CefRefPtr<CefBrowser> browser,
                                  CefRefPtr<CefFrame> frame,
                                  CefRefPtr<CefV8Context> context) {
    };

    // Called when a V8 context is released. Used to clean up V8 window
    // bindings. Tests should check for unique URLs to avoid interfering with
    // each other.
    virtual void OnContextReleased(CefRefPtr<TestApp> test_app,
                                   CefRefPtr<CefBrowser> browser,
                                   CefRefPtr<CefFrame> frame,
                                   CefRefPtr<CefV8Context> context) {
    };

    // Called when a process message is received. Return true if the message was
    // handled and should not be passed on to other handlers. Tests should check
    // for unique message names to avoid interfering with each other.
    virtual bool OnProcessMessageRecieved(
        CefRefPtr<TestApp> test_app,
        CefRefPtr<CefBrowser> browser,
        CefProcessId source_process,
        CefRefPtr<CefProcessMessage> message) {
      return false;
    }
  };

  typedef std::set<CefRefPtr<Test> > TestSet;

  TestApp();

  // Set a JavaScript callback for the specified |message_name| and |browser_id|
  // combination. Will automatically be removed when the associated context is
  // released. Callbacks can also be set in JavaScript using the
  // test_app.setMessageCallback function.
  void SetMessageCallback(const std::string& message_name,
                          int browser_id,
                          CefRefPtr<CefV8Context> context,
                          CefRefPtr<CefV8Value> function);

  // Removes the JavaScript callback for the specified |message_name| and
  // |browser_id| combination. Returns true if a callback was removed. Callbacks
  // can also be removed in JavaScript using the test_app.removeMessageCallback
  // function.
  bool RemoveMessageCallback(const std::string& message_name,
                             int browser_id);

  // Returns true if the currently running test has failed.
  static bool TestFailed();

 private:
  // Creates all of the test objects. Implemented in test_app_tests.cc.
  static void CreateTests(TestSet& tests);

  // CefApp methods.
  virtual CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler()
      OVERRIDE { return this; }

  // CefRenderProcessHandler methods.
  virtual void OnWebKitInitialized() OVERRIDE;
  virtual void OnContextCreated(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefRefPtr<CefV8Context> context) OVERRIDE;
  virtual void OnContextReleased(CefRefPtr<CefBrowser> browser,
                                 CefRefPtr<CefFrame> frame,
                                 CefRefPtr<CefV8Context> context) OVERRIDE;
  virtual bool OnProcessMessageRecieved(
      CefRefPtr<CefBrowser> browser,
      CefProcessId source_process,
      CefRefPtr<CefProcessMessage> message) OVERRIDE;

  // Map of message callbacks.
  typedef std::map<std::pair<std::string, int>,
                   std::pair<CefRefPtr<CefV8Context>, CefRefPtr<CefV8Value> > >
                   CallbackMap;
  CallbackMap callback_map_;

  // Set of supported tests.
  TestSet tests_;

  IMPLEMENT_REFCOUNTING(TestApp);
};

#endif  // CEF_TESTS_UNITTESTS_TEST_APP_H_
