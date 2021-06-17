// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_SHARED_RENDERER_CLIENT_APP_RENDERER_H_
#define CEF_TESTS_SHARED_RENDERER_CLIENT_APP_RENDERER_H_
#pragma once

#include <set>

#include "tests/shared/common/client_app.h"

namespace client {

// Client app implementation for the renderer process.
class ClientAppRenderer : public ClientApp, public CefRenderProcessHandler {
 public:
  // Interface for renderer delegates. All Delegates must be returned via
  // CreateDelegates. Do not perform work in the Delegate
  // constructor. See CefRenderProcessHandler for documentation.
  class Delegate : public virtual CefBaseRefCounted {
   public:
    virtual void OnWebKitInitialized(CefRefPtr<ClientAppRenderer> app) {}

    virtual void OnBrowserCreated(CefRefPtr<ClientAppRenderer> app,
                                  CefRefPtr<CefBrowser> browser,
                                  CefRefPtr<CefDictionaryValue> extra_info) {}

    virtual void OnBrowserDestroyed(CefRefPtr<ClientAppRenderer> app,
                                    CefRefPtr<CefBrowser> browser) {}

    virtual CefRefPtr<CefLoadHandler> GetLoadHandler(
        CefRefPtr<ClientAppRenderer> app) {
      return nullptr;
    }

    virtual void OnContextCreated(CefRefPtr<ClientAppRenderer> app,
                                  CefRefPtr<CefBrowser> browser,
                                  CefRefPtr<CefFrame> frame,
                                  CefRefPtr<CefV8Context> context) {}

    virtual void OnContextReleased(CefRefPtr<ClientAppRenderer> app,
                                   CefRefPtr<CefBrowser> browser,
                                   CefRefPtr<CefFrame> frame,
                                   CefRefPtr<CefV8Context> context) {}

    virtual void OnUncaughtException(CefRefPtr<ClientAppRenderer> app,
                                     CefRefPtr<CefBrowser> browser,
                                     CefRefPtr<CefFrame> frame,
                                     CefRefPtr<CefV8Context> context,
                                     CefRefPtr<CefV8Exception> exception,
                                     CefRefPtr<CefV8StackTrace> stackTrace) {}

    virtual void OnFocusedNodeChanged(CefRefPtr<ClientAppRenderer> app,
                                      CefRefPtr<CefBrowser> browser,
                                      CefRefPtr<CefFrame> frame,
                                      CefRefPtr<CefDOMNode> node) {}

    // Called when a process message is received. Return true if the message was
    // handled and should not be passed on to other handlers. Delegates
    // should check for unique message names to avoid interfering with each
    // other.
    virtual bool OnProcessMessageReceived(
        CefRefPtr<ClientAppRenderer> app,
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefProcessId source_process,
        CefRefPtr<CefProcessMessage> message) {
      return false;
    }
  };

  typedef std::set<CefRefPtr<Delegate>> DelegateSet;

  ClientAppRenderer();

 private:
  // Creates all of the Delegate objects. Implemented by cefclient in
  // client_app_delegates_renderer.cc
  static void CreateDelegates(DelegateSet& delegates);

  // CefApp methods.
  CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override {
    return this;
  }

  // CefRenderProcessHandler methods.
  void OnWebKitInitialized() override;
  void OnBrowserCreated(CefRefPtr<CefBrowser> browser,
                        CefRefPtr<CefDictionaryValue> extra_info) override;
  void OnBrowserDestroyed(CefRefPtr<CefBrowser> browser) override;
  CefRefPtr<CefLoadHandler> GetLoadHandler() override;
  void OnContextCreated(CefRefPtr<CefBrowser> browser,
                        CefRefPtr<CefFrame> frame,
                        CefRefPtr<CefV8Context> context) override;
  void OnContextReleased(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         CefRefPtr<CefV8Context> context) override;
  void OnUncaughtException(CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefFrame> frame,
                           CefRefPtr<CefV8Context> context,
                           CefRefPtr<CefV8Exception> exception,
                           CefRefPtr<CefV8StackTrace> stackTrace) override;
  void OnFocusedNodeChanged(CefRefPtr<CefBrowser> browser,
                            CefRefPtr<CefFrame> frame,
                            CefRefPtr<CefDOMNode> node) override;
  bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefProcessId source_process,
                                CefRefPtr<CefProcessMessage> message) override;

 private:
  // Set of supported Delegates.
  DelegateSet delegates_;

  IMPLEMENT_REFCOUNTING(ClientAppRenderer);
  DISALLOW_COPY_AND_ASSIGN(ClientAppRenderer);
};

}  // namespace client

#endif  // CEF_TESTS_SHARED_RENDERER_CLIENT_APP_RENDERER_H_
