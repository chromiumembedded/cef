// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_SHARED_BROWSER_CLIENT_APP_BROWSER_H_
#define CEF_TESTS_SHARED_BROWSER_CLIENT_APP_BROWSER_H_
#pragma once

#include <set>

#include "tests/shared/common/client_app.h"

namespace client {

// Client app implementation for the browser process.
class ClientAppBrowser : public ClientApp, public CefBrowserProcessHandler {
 public:
  // Interface for browser delegates. All Delegates must be returned via
  // CreateDelegates. Do not perform work in the Delegate
  // constructor. See CefBrowserProcessHandler for documentation.
  class Delegate : public virtual CefBaseRefCounted {
   public:
    virtual void OnBeforeCommandLineProcessing(
        CefRefPtr<ClientAppBrowser> app,
        CefRefPtr<CefCommandLine> command_line) {}

    virtual void OnContextInitialized(CefRefPtr<ClientAppBrowser> app) {}

    virtual void OnBeforeChildProcessLaunch(
        CefRefPtr<ClientAppBrowser> app,
        CefRefPtr<CefCommandLine> command_line) {}
  };

  typedef std::set<CefRefPtr<Delegate>> DelegateSet;

  ClientAppBrowser();

  // Called to populate |settings| based on |command_line| and other global
  // state.
  static void PopulateSettings(CefRefPtr<CefCommandLine> command_line,
                               CefSettings& settings);

 private:
  // Registers cookieable schemes. Implemented by cefclient in
  // client_app_delegates_browser.cc
  static void RegisterCookieableSchemes(
      std::vector<std::string>& cookieable_schemes);

  // Creates all of the Delegate objects. Implemented by cefclient in
  // client_app_delegates_browser.cc
  static void CreateDelegates(DelegateSet& delegates);

  // CefApp methods.
  void OnBeforeCommandLineProcessing(
      const CefString& process_type,
      CefRefPtr<CefCommandLine> command_line) override;
  CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
    return this;
  }

  // CefBrowserProcessHandler methods.
  void OnContextInitialized() override;
  void OnBeforeChildProcessLaunch(
      CefRefPtr<CefCommandLine> command_line) override;
  void OnScheduleMessagePumpWork(int64 delay) override;

  // Set of supported Delegates.
  DelegateSet delegates_;

  IMPLEMENT_REFCOUNTING(ClientAppBrowser);
  DISALLOW_COPY_AND_ASSIGN(ClientAppBrowser);
};

}  // namespace client

#endif  // CEF_TESTS_SHARED_BROWSER_CLIENT_APP_BROWSER_H_
