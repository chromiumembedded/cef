// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_CLIENT_APP_H_
#define CEF_TESTS_CEFCLIENT_CLIENT_APP_H_
#pragma once

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "include/cef_app.h"

class ClientApp : public CefApp,
                  public CefBrowserProcessHandler,
                  public CefRenderProcessHandler {
 public:
  // Interface for browser delegates. All BrowserDelegates must be returned via
  // CreateBrowserDelegates. Do not perform work in the BrowserDelegate
  // constructor. See CefBrowserProcessHandler for documentation.
  class BrowserDelegate : public virtual CefBase {
   public:
    virtual void OnContextInitialized(CefRefPtr<ClientApp> app) {}

    virtual void OnBeforeChildProcessLaunch(
        CefRefPtr<ClientApp> app,
        CefRefPtr<CefCommandLine> command_line) {}

    virtual void OnRenderProcessThreadCreated(
        CefRefPtr<ClientApp> app,
        CefRefPtr<CefListValue> extra_info) {}
  };

  typedef std::set<CefRefPtr<BrowserDelegate> > BrowserDelegateSet;

  // Interface for renderer delegates. All RenderDelegates must be returned via
  // CreateRenderDelegates. Do not perform work in the RenderDelegate
  // constructor. See CefRenderProcessHandler for documentation.
  class RenderDelegate : public virtual CefBase {
   public:
    virtual void OnRenderThreadCreated(CefRefPtr<ClientApp> app,
                                       CefRefPtr<CefListValue> extra_info) {}

    virtual void OnWebKitInitialized(CefRefPtr<ClientApp> app) {}

    virtual void OnBrowserCreated(CefRefPtr<ClientApp> app,
                                  CefRefPtr<CefBrowser> browser) {}

    virtual void OnBrowserDestroyed(CefRefPtr<ClientApp> app,
                                    CefRefPtr<CefBrowser> browser) {}

    virtual bool OnBeforeNavigation(CefRefPtr<ClientApp> app,
                                    CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefFrame> frame,
                                    CefRefPtr<CefRequest> request,
                                    cef_navigation_type_t navigation_type,
                                    bool is_redirect) {
      return false;
    }

    virtual void OnContextCreated(CefRefPtr<ClientApp> app,
                                  CefRefPtr<CefBrowser> browser,
                                  CefRefPtr<CefFrame> frame,
                                  CefRefPtr<CefV8Context> context) {}

    virtual void OnContextReleased(CefRefPtr<ClientApp> app,
                                   CefRefPtr<CefBrowser> browser,
                                   CefRefPtr<CefFrame> frame,
                                   CefRefPtr<CefV8Context> context) {}

    virtual void OnUncaughtException(CefRefPtr<ClientApp> app,
                                     CefRefPtr<CefBrowser> browser,
                                     CefRefPtr<CefFrame> frame,
                                     CefRefPtr<CefV8Context> context,
                                     CefRefPtr<CefV8Exception> exception,
                                     CefRefPtr<CefV8StackTrace> stackTrace) {}

    virtual void OnWorkerContextCreated(CefRefPtr<ClientApp> app,
                                        int worker_id,
                                        const CefString& url,
                                        CefRefPtr<CefV8Context> context) {}

    virtual void OnWorkerContextReleased(CefRefPtr<ClientApp> app,
                                         int worker_id,
                                         const CefString& url,
                                         CefRefPtr<CefV8Context> context) {}

    virtual void OnWorkerUncaughtException(
        CefRefPtr<ClientApp> app,
        int worker_id,
        const CefString& url,
        CefRefPtr<CefV8Context> context,
        CefRefPtr<CefV8Exception> exception,
        CefRefPtr<CefV8StackTrace> stackTrace) {}

    virtual void OnFocusedNodeChanged(CefRefPtr<ClientApp> app,
                                      CefRefPtr<CefBrowser> browser,
                                      CefRefPtr<CefFrame> frame,
                                      CefRefPtr<CefDOMNode> node) {}

    // Called when a process message is received. Return true if the message was
    // handled and should not be passed on to other handlers. RenderDelegates
    // should check for unique message names to avoid interfering with each
    // other.
    virtual bool OnProcessMessageReceived(
        CefRefPtr<ClientApp> app,
        CefRefPtr<CefBrowser> browser,
        CefProcessId source_process,
        CefRefPtr<CefProcessMessage> message) {
      return false;
    }
  };

  typedef std::set<CefRefPtr<RenderDelegate> > RenderDelegateSet;

  ClientApp();

  // Set a JavaScript callback for the specified |message_name| and |browser_id|
  // combination. Will automatically be removed when the associated context is
  // released. Callbacks can also be set in JavaScript using the
  // app.setMessageCallback function.
  void SetMessageCallback(const std::string& message_name,
                          int browser_id,
                          CefRefPtr<CefV8Context> context,
                          CefRefPtr<CefV8Value> function);

  // Removes the JavaScript callback for the specified |message_name| and
  // |browser_id| combination. Returns true if a callback was removed. Callbacks
  // can also be removed in JavaScript using the app.removeMessageCallback
  // function.
  bool RemoveMessageCallback(const std::string& message_name,
                             int browser_id);

 private:
  // Creates all of the BrowserDelegate objects. Implemented in
  // client_app_delegates.
  static void CreateBrowserDelegates(BrowserDelegateSet& delegates);

  // Creates all of the RenderDelegate objects. Implemented in
  // client_app_delegates.
  static void CreateRenderDelegates(RenderDelegateSet& delegates);

  // Registers custom schemes. Implemented in client_app_delegates.
  static void RegisterCustomSchemes(CefRefPtr<CefSchemeRegistrar> registrar,
                                    std::vector<CefString>& cookiable_schemes);

  // CefApp methods.
  virtual void OnRegisterCustomSchemes(
      CefRefPtr<CefSchemeRegistrar> registrar) OVERRIDE {
    RegisterCustomSchemes(registrar, cookieable_schemes_);
  }
  virtual CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler()
      OVERRIDE { return this; }
  virtual CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler()
      OVERRIDE { return this; }

  // CefBrowserProcessHandler methods.
  virtual void OnContextInitialized() OVERRIDE;
  virtual void OnBeforeChildProcessLaunch(
      CefRefPtr<CefCommandLine> command_line) OVERRIDE;
  virtual void OnRenderProcessThreadCreated(CefRefPtr<CefListValue> extra_info)
                                            OVERRIDE;

  // CefRenderProcessHandler methods.
  virtual void OnRenderThreadCreated(CefRefPtr<CefListValue> extra_info)
                                     OVERRIDE;
  virtual void OnWebKitInitialized() OVERRIDE;
  virtual void OnBrowserCreated(CefRefPtr<CefBrowser> browser) OVERRIDE;
  virtual void OnBrowserDestroyed(CefRefPtr<CefBrowser> browser) OVERRIDE;
  virtual bool OnBeforeNavigation(CefRefPtr<CefBrowser> browser,
                                  CefRefPtr<CefFrame> frame,
                                  CefRefPtr<CefRequest> request,
                                  NavigationType navigation_type,
                                  bool is_redirect) OVERRIDE;
  virtual void OnContextCreated(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefRefPtr<CefV8Context> context) OVERRIDE;
  virtual void OnContextReleased(CefRefPtr<CefBrowser> browser,
                                 CefRefPtr<CefFrame> frame,
                                 CefRefPtr<CefV8Context> context) OVERRIDE;
  virtual void OnUncaughtException(CefRefPtr<CefBrowser> browser,
                                   CefRefPtr<CefFrame> frame,
                                   CefRefPtr<CefV8Context> context,
                                   CefRefPtr<CefV8Exception> exception,
                                   CefRefPtr<CefV8StackTrace> stackTrace)
                                   OVERRIDE;
  virtual void OnWorkerContextCreated(
      int worker_id,
      const CefString& url,
      CefRefPtr<CefV8Context> context) OVERRIDE;
  virtual void OnWorkerContextReleased(
      int worker_id,
      const CefString& url,
      CefRefPtr<CefV8Context> context) OVERRIDE;
  virtual void OnWorkerUncaughtException(
      int worker_id,
      const CefString& url,
      CefRefPtr<CefV8Context> context,
      CefRefPtr<CefV8Exception> exception,
      CefRefPtr<CefV8StackTrace> stackTrace)
      OVERRIDE;
  virtual void OnFocusedNodeChanged(CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefFrame> frame,
                                    CefRefPtr<CefDOMNode> node) OVERRIDE;
  virtual bool OnProcessMessageReceived(
      CefRefPtr<CefBrowser> browser,
      CefProcessId source_process,
      CefRefPtr<CefProcessMessage> message) OVERRIDE;

  // Map of message callbacks.
  typedef std::map<std::pair<std::string, int>,
                   std::pair<CefRefPtr<CefV8Context>, CefRefPtr<CefV8Value> > >
                   CallbackMap;
  CallbackMap callback_map_;

  // Set of supported BrowserDelegates.
  BrowserDelegateSet browser_delegates_;

  // Set of supported RenderDelegates.
  RenderDelegateSet render_delegates_;

  // Schemes that will be registered with the global cookie manager.
  std::vector<CefString> cookieable_schemes_;

  IMPLEMENT_REFCOUNTING(ClientApp);
};

#endif  // CEF_TESTS_CEFCLIENT_CLIENT_APP_H_
