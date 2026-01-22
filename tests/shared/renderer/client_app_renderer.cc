// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/shared/renderer/client_app_renderer.h"

#include "include/base/cef_logging.h"

namespace client {

ClientAppRenderer::ClientAppRenderer() {
  CreateDelegates(delegates_);
}

void ClientAppRenderer::OnWebKitInitialized() {
  for (auto& delegate : delegates_) {
    delegate->OnWebKitInitialized(this);
  }
}

void ClientAppRenderer::OnBrowserCreated(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefDictionaryValue> extra_info) {
  for (auto& delegate : delegates_) {
    delegate->OnBrowserCreated(this, browser, extra_info);
  }
}

void ClientAppRenderer::OnBrowserDestroyed(CefRefPtr<CefBrowser> browser) {
  for (auto& delegate : delegates_) {
    delegate->OnBrowserDestroyed(this, browser);
  }
}

CefRefPtr<CefLoadHandler> ClientAppRenderer::GetLoadHandler() {
  CefRefPtr<CefLoadHandler> load_handler;
  DelegateSet::iterator it = delegates_.begin();
  for (; it != delegates_.end() && !load_handler.get(); ++it) {
    load_handler = (*it)->GetLoadHandler(this);
  }

  return load_handler;
}

void ClientAppRenderer::OnContextCreated(CefRefPtr<CefBrowser> browser,
                                         CefRefPtr<CefFrame> frame,
                                         CefRefPtr<CefV8Context> context) {
  for (auto& delegate : delegates_) {
    delegate->OnContextCreated(this, browser, frame, context);
  }
}

void ClientAppRenderer::OnContextReleased(CefRefPtr<CefBrowser> browser,
                                          CefRefPtr<CefFrame> frame,
                                          CefRefPtr<CefV8Context> context) {
  for (auto& delegate : delegates_) {
    delegate->OnContextReleased(this, browser, frame, context);
  }
}

void ClientAppRenderer::OnUncaughtException(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefV8Context> context,
    CefRefPtr<CefV8Exception> exception,
    CefRefPtr<CefV8StackTrace> stackTrace) {
  for (auto& delegate : delegates_) {
    delegate->OnUncaughtException(this, browser, frame, context, exception,
                                  stackTrace);
  }
}

void ClientAppRenderer::OnFocusedNodeChanged(CefRefPtr<CefBrowser> browser,
                                             CefRefPtr<CefFrame> frame,
                                             CefRefPtr<CefDOMNode> node) {
  for (auto& delegate : delegates_) {
    delegate->OnFocusedNodeChanged(this, browser, frame, node);
  }
}

bool ClientAppRenderer::OnProcessMessageReceived(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefProcessId source_process,
    CefRefPtr<CefProcessMessage> message) {
  DCHECK_EQ(source_process, PID_BROWSER);

  bool handled = false;

  DelegateSet::iterator it = delegates_.begin();
  for (; it != delegates_.end() && !handled; ++it) {
    handled = (*it)->OnProcessMessageReceived(this, browser, frame,
                                              source_process, message);
  }

  return handled;
}

}  // namespace client
