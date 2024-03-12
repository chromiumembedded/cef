// Copyright (c) 2022 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFTESTS_MESSAGE_ROUTER_UNITTEST_UTILS_H_
#define CEF_TESTS_CEFTESTS_MESSAGE_ROUTER_UNITTEST_UTILS_H_
#pragma once

#include <cstdlib>

#include "include/base/cef_callback.h"
#include "include/base/cef_weak_ptr.h"
#include "include/cef_v8.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/routing_test_handler.h"
#include "tests/ceftests/test_util.h"
#include "tests/gtest/include/gtest/gtest.h"
#include "tests/shared/renderer/client_app_renderer.h"

using client::ClientAppRenderer;

extern const char kJSQueryFunc[];
extern const char kJSQueryCancelFunc[];

#define S1(N) #N
#define S2(N) S1(N)
#define LINESTR S2(__LINE__)

// Handle the renderer side of the routing implementation.
class MRRenderDelegate : public ClientAppRenderer::Delegate {
 public:
  class V8HandlerImpl : public CefV8Handler {
   public:
    explicit V8HandlerImpl(CefRefPtr<MRRenderDelegate> delegate)
        : delegate_(delegate) {}

    bool Execute(const CefString& name,
                 CefRefPtr<CefV8Value> object,
                 const CefV8ValueList& arguments,
                 CefRefPtr<CefV8Value>& retval,
                 CefString& exception) override;

   private:
    CefRefPtr<MRRenderDelegate> delegate_;

    IMPLEMENT_REFCOUNTING(V8HandlerImpl);
  };

  void OnWebKitInitialized(CefRefPtr<ClientAppRenderer> app) override;

  void OnContextCreated(CefRefPtr<ClientAppRenderer> app,
                        CefRefPtr<CefBrowser> browser,
                        CefRefPtr<CefFrame> frame,
                        CefRefPtr<CefV8Context> context) override;

  void OnContextReleased(CefRefPtr<ClientAppRenderer> app,
                         CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         CefRefPtr<CefV8Context> context) override;

  bool OnProcessMessageReceived(CefRefPtr<ClientAppRenderer> app,
                                CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefProcessId source_process,
                                CefRefPtr<CefProcessMessage> message) override;

 private:
  CefRefPtr<CefMessageRouterRendererSide> message_router_;

  IMPLEMENT_REFCOUNTING(MRRenderDelegate);
};

class MRTestHandler : public TestHandler {
 public:
  void RunTest() override;
  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;
  void OnRenderProcessTerminated(CefRefPtr<CefBrowser> browser,
                                 TerminationStatus status,
                                 int error_code,
                                 const CefString& error_string) override;

  // Only call this method if the navigation isn't canceled.
  bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                      CefRefPtr<CefFrame> frame,
                      CefRefPtr<CefRequest> request,
                      bool user_gesture,
                      bool is_redirect) override;
  // Returns true if the router handled the navigation.
  bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefProcessId source_process,
                                CefRefPtr<CefProcessMessage> message) override;

  CefRefPtr<CefMessageRouterBrowserSide> GetRouter() const;
  void SetMessageSizeThreshold(size_t message_size_treshold);

 protected:
  virtual void RunMRTest() = 0;

  virtual void AddHandlers(
      CefRefPtr<CefMessageRouterBrowserSide> message_router) = 0;

  virtual void OnNotify(CefRefPtr<CefBrowser> browser,
                        CefRefPtr<CefFrame> frame,
                        const std::string& message) = 0;

  bool AssertQueryCount(CefRefPtr<CefBrowser> browser,
                        CefMessageRouterBrowserSide::Handler* handler,
                        int expected_count);
  void AssertMainBrowser(CefRefPtr<CefBrowser> browser);

 private:
  CefRefPtr<CefMessageRouterBrowserSide> message_router_;
  size_t message_size_threshold_ = 0;

  IMPLEMENT_REFCOUNTING(MRTestHandler);
};

// Implementation of MRTestHandler that loads a single page.
class SingleLoadTestHandler : public MRTestHandler,
                              public CefMessageRouterBrowserSide::Handler {
 public:
  SingleLoadTestHandler();
  const std::string& GetMainURL() { return main_url_; }

 protected:
  void RunMRTest() override;
  void AddHandlers(
      CefRefPtr<CefMessageRouterBrowserSide> message_router) override;
  virtual void AddOtherResources() {}
  virtual std::string GetMainHTML() = 0;
  void AssertMainFrame(CefRefPtr<CefFrame> frame);

 private:
  const std::string main_url_;
};

#endif  // CEF_TESTS_CEFTESTS_MESSAGE_ROUTER_UNITTEST_UTILS_H_
