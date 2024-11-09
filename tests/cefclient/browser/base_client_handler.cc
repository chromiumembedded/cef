// Copyright (c) 2024 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/base_client_handler.h"

#include "include/cef_command_line.h"
#include "tests/cefclient/browser/main_context.h"
#include "tests/cefclient/browser/root_window_manager.h"
#include "tests/shared/common/client_switches.h"

namespace client {

BaseClientHandler::BaseClientHandler() {
  resource_manager_ = new CefResourceManager();
  test_runner::SetupResourceManager(resource_manager_, &string_resource_map_);
}

// static
CefRefPtr<BaseClientHandler> BaseClientHandler::GetForBrowser(
    CefRefPtr<CefBrowser> browser) {
  return GetForClient(browser->GetHost()->GetClient());
}

// static
CefRefPtr<BaseClientHandler> BaseClientHandler::GetForClient(
    CefRefPtr<CefClient> client) {
  return static_cast<BaseClientHandler*>(client.get());
}

bool BaseClientHandler::OnProcessMessageReceived(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefProcessId source_process,
    CefRefPtr<CefProcessMessage> message) {
  CEF_REQUIRE_UI_THREAD();
  return message_router_->OnProcessMessageReceived(browser, frame,
                                                   source_process, message);
}

bool BaseClientHandler::OnSetFocus(CefRefPtr<CefBrowser> browser,
                                   FocusSource source) {
  return !ShouldRequestFocus();
}

void BaseClientHandler::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  browser_count_++;

  if (!message_router_) {
    // Create the browser-side router for query handling.
    CefMessageRouterConfig config;
    message_router_ = CefMessageRouterBrowserSide::Create(config);

    // Register handlers with the router.
    test_runner::CreateMessageHandlers(message_handler_set_);
    for (auto* message_handler : message_handler_set_) {
      message_router_->AddHandler(message_handler, false);
    }
  }

  if (track_as_other_browser_) {
    MainContext::Get()->GetRootWindowManager()->OtherBrowserCreated(
        browser->GetIdentifier(), browser->GetHost()->GetOpenerIdentifier());
  }
}

void BaseClientHandler::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UI_THREAD();

  if (--browser_count_ == 0) {
    // Remove and delete message router handlers.
    for (auto* message_handler : message_handler_set_) {
      message_router_->RemoveHandler(message_handler);
      delete message_handler;
    }
    message_handler_set_.clear();
    message_router_ = nullptr;
  }

  if (track_as_other_browser_) {
    MainContext::Get()->GetRootWindowManager()->OtherBrowserClosed(
        browser->GetIdentifier(), browser->GetHost()->GetOpenerIdentifier());
  }
}

void BaseClientHandler::OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                                             bool isLoading,
                                             bool canGoBack,
                                             bool canGoForward) {
  CEF_REQUIRE_UI_THREAD();

  if (!isLoading && initial_navigation_) {
    initial_navigation_ = false;
  }
}

bool BaseClientHandler::OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                                       CefRefPtr<CefFrame> frame,
                                       CefRefPtr<CefRequest> request,
                                       bool user_gesture,
                                       bool is_redirect) {
  CEF_REQUIRE_UI_THREAD();
  message_router_->OnBeforeBrowse(browser, frame);
  return false;
}

bool BaseClientHandler::OnRenderProcessUnresponsive(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefUnresponsiveProcessCallback> callback) {
  switch (hang_action_) {
    case HangAction::kDefault:
      return false;
    case HangAction::kWait:
      callback->Wait();
      break;
    case HangAction::kTerminate:
      callback->Terminate();
      break;
  }
  return true;
}

void BaseClientHandler::OnRenderProcessTerminated(
    CefRefPtr<CefBrowser> browser,
    TerminationStatus status,
    int error_code,
    const CefString& error_string) {
  CEF_REQUIRE_UI_THREAD();
  message_router_->OnRenderProcessTerminated(browser);
}

cef_return_value_t BaseClientHandler::OnBeforeResourceLoad(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefRequest> request,
    CefRefPtr<CefCallback> callback) {
  CEF_REQUIRE_IO_THREAD();

  return resource_manager_->OnBeforeResourceLoad(browser, frame, request,
                                                 callback);
}

CefRefPtr<CefResourceHandler> BaseClientHandler::GetResourceHandler(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefRequest> request) {
  CEF_REQUIRE_IO_THREAD();

  return resource_manager_->GetResourceHandler(browser, frame, request);
}

CefRefPtr<CefResponseFilter> BaseClientHandler::GetResourceResponseFilter(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefRequest> request,
    CefRefPtr<CefResponse> response) {
  CEF_REQUIRE_IO_THREAD();

  return test_runner::GetResourceResponseFilter(browser, frame, request,
                                                response);
}

int BaseClientHandler::GetBrowserCount() const {
  CEF_REQUIRE_UI_THREAD();
  return browser_count_;
}

void BaseClientHandler::SetStringResource(const std::string& page,
                                          const std::string& data) {
  if (!CefCurrentlyOn(TID_IO)) {
    CefPostTask(TID_IO, base::BindOnce(&BaseClientHandler::SetStringResource,
                                       this, page, data));
    return;
  }

  string_resource_map_[page] = data;
}

void BaseClientHandler::SetHangAction(HangAction action) {
  CEF_REQUIRE_UI_THREAD();
  hang_action_ = action;
}

BaseClientHandler::HangAction BaseClientHandler::GetHangAction() const {
  CEF_REQUIRE_UI_THREAD();
  return hang_action_;
}

bool BaseClientHandler::ShouldRequestFocus() {
  CEF_REQUIRE_UI_THREAD();

  if (initial_navigation_) {
    CefRefPtr<CefCommandLine> command_line =
        CefCommandLine::GetGlobalCommandLine();
    if (command_line->HasSwitch(switches::kNoActivate)) {
      // Don't give focus to the browser on creation.
      return false;
    }
  }

  return true;
}

}  // namespace client
