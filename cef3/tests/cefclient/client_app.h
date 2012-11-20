// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
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
                  public CefProxyHandler,
                  public CefRenderProcessHandler {
 public:
  // Interface for browser delegates. All BrowserDelegates must be returned via
  // CreateBrowserDelegates. Do not perform work in the BrowserDelegate
  // constructor.
  class BrowserDelegate : public virtual CefBase {
   public:
    // Called on the browser process UI thread immediately after the CEF context
    // has been initialized.
    virtual void OnContextInitialized(CefRefPtr<ClientApp> app) {
    }

    // Called on the browser process IO thread before a child process is
    // launched. Provides an opportunity to modify the child process command
    // line. Do not keep a reference to |command_line| outside of this method.
    virtual void OnBeforeChildProcessLaunch(
        CefRefPtr<ClientApp> app,
        CefRefPtr<CefCommandLine> command_line) {
    }

    // Called on the browser process IO thread after the main thread has been
    // created for a new render process. Provides an opportunity to specify
    // extra information that will be passed to
    // CefRenderProcessHandler::OnRenderThreadCreated() in the render process.
    // Do not keep a reference to |extra_info| outside of this method.
    virtual void OnRenderProcessThreadCreated(
        CefRefPtr<ClientApp> app,
        CefRefPtr<CefListValue> extra_info) {
    }
  };

  typedef std::set<CefRefPtr<BrowserDelegate> > BrowserDelegateSet;

  // Interface for renderer delegates. All RenderDelegates must be returned via
  // CreateRenderDelegates. Do not perform work in the RenderDelegate
  // constructor.
  class RenderDelegate : public virtual CefBase {
   public:
    // Called after the render process main thread has been created.
    virtual void OnRenderThreadCreated(CefRefPtr<ClientApp> app,
                                       CefRefPtr<CefListValue> extra_info) {
    }

    // Called when WebKit is initialized. Used to register V8 extensions.
    virtual void OnWebKitInitialized(CefRefPtr<ClientApp> app) {
    }

    // Called after a browser has been created.
    virtual void OnBrowserCreated(CefRefPtr<ClientApp> app,
                                  CefRefPtr<CefBrowser> browser) {
    }

    // Called before a browser is destroyed.
    virtual void OnBrowserDestroyed(CefRefPtr<ClientApp> app,
                                    CefRefPtr<CefBrowser> browser) {
    }

    // Called before browser navigation. Return true to cancel the navigation or
    // false to allow the navigation to proceed. The |request| object cannot be
    // modified in this callback.
    virtual bool OnBeforeNavigation(CefRefPtr<ClientApp> app,
                                    CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefFrame> frame,
                                    CefRefPtr<CefRequest> request,
                                    cef_navigation_type_t navigation_type,
                                    bool is_redirect) {
      return false;
    }

    // Called when a V8 context is created. Used to create V8 window bindings
    // and set message callbacks. RenderDelegates should check for unique URLs
    // to avoid interfering with each other.
    virtual void OnContextCreated(CefRefPtr<ClientApp> app,
                                  CefRefPtr<CefBrowser> browser,
                                  CefRefPtr<CefFrame> frame,
                                  CefRefPtr<CefV8Context> context) {
    }

    // Called when a V8 context is released. Used to clean up V8 window
    // bindings. RenderDelegates should check for unique URLs to avoid
    // interfering with each other.
    virtual void OnContextReleased(CefRefPtr<ClientApp> app,
                                   CefRefPtr<CefBrowser> browser,
                                   CefRefPtr<CefFrame> frame,
                                   CefRefPtr<CefV8Context> context) {
    }

    // Global V8 exception handler, disabled by default, to enable set
    // CefSettings.uncaught_exception_stack_size > 0.
    virtual void OnUncaughtException(CefRefPtr<ClientApp> app,
                                     CefRefPtr<CefBrowser> browser,
                                     CefRefPtr<CefFrame> frame,
                                     CefRefPtr<CefV8Context> context,
                                     CefRefPtr<CefV8Exception> exception,
                                     CefRefPtr<CefV8StackTrace> stackTrace) {
    }

    // Called when the focused node in a frame has changed.
    virtual void OnFocusedNodeChanged(CefRefPtr<ClientApp> app,
                                      CefRefPtr<CefBrowser> browser,
                                      CefRefPtr<CefFrame> frame,
                                      CefRefPtr<CefDOMNode> node) {
    }

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

  // Set the proxy configuration. Should only be called during initialization.
  void SetProxyConfig(cef_proxy_type_t proxy_type,
                      const CefString& proxy_config) {
    proxy_type_ = proxy_type;
    proxy_config_ = proxy_config;
  }

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
  virtual CefRefPtr<CefProxyHandler> GetProxyHandler() OVERRIDE { return this; }
  virtual void OnContextInitialized() OVERRIDE;
  virtual void OnBeforeChildProcessLaunch(
      CefRefPtr<CefCommandLine> command_line) OVERRIDE;
  virtual void OnRenderProcessThreadCreated(CefRefPtr<CefListValue> extra_info)
                                            OVERRIDE;

  // CefProxyHandler methods.
  virtual void GetProxyForUrl(const CefString& url,
                              CefProxyInfo& proxy_info) OVERRIDE;

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
  virtual void OnFocusedNodeChanged(CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefFrame> frame,
                                    CefRefPtr<CefDOMNode> node) OVERRIDE;
  virtual bool OnProcessMessageReceived(
      CefRefPtr<CefBrowser> browser,
      CefProcessId source_process,
      CefRefPtr<CefProcessMessage> message) OVERRIDE;

  // Proxy configuration.
  cef_proxy_type_t proxy_type_;
  CefString proxy_config_;

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
