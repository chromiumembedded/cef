// Copyright (c) 2020 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cef/libcef/browser/devtools/devtools_protocol_manager.h"

#include "base/memory/raw_ptr.h"
#include "cef/libcef/browser/browser_host_base.h"
#include "cef/libcef/browser/devtools/devtools_controller.h"
#include "cef/libcef/browser/thread_util.h"
#include "content/public/browser/web_contents.h"

namespace {

// May be created on any thread but will be destroyed on the UI thread.
class CefDevToolsRegistrationImpl : public CefRegistration,
                                    public CefDevToolsController::Observer {
 public:
  explicit CefDevToolsRegistrationImpl(
      CefRefPtr<CefDevToolsMessageObserver> observer)
      : observer_(observer) {
    DCHECK(observer_);
  }

  CefDevToolsRegistrationImpl(const CefDevToolsRegistrationImpl&) = delete;
  CefDevToolsRegistrationImpl& operator=(const CefDevToolsRegistrationImpl&) =
      delete;

  ~CefDevToolsRegistrationImpl() override {
    CEF_REQUIRE_UIT();

    // May be null if OnDevToolsControllerDestroyed was called.
    if (!controller_) {
      return;
    }

    controller_->RemoveObserver(this);
  }

  void Initialize(CefBrowserHostBase* browser,
                  base::WeakPtr<CefDevToolsController> controller) {
    CEF_REQUIRE_UIT();
    DCHECK(browser && controller);
    DCHECK(!browser_ && !controller_);
    browser_ = browser;
    controller_ = controller;

    controller_->AddObserver(this);
  }

 private:
  // CefDevToolsController::Observer methods:
  bool OnDevToolsMessage(const std::string_view& message) override {
    CEF_REQUIRE_UIT();
    return observer_->OnDevToolsMessage(browser_.get(), message.data(),
                                        message.size());
  }

  void OnDevToolsMethodResult(int message_id,
                              bool success,
                              const std::string_view& result) override {
    CEF_REQUIRE_UIT();
    observer_->OnDevToolsMethodResult(browser_.get(), message_id, success,
                                      result.data(), result.size());
  }

  void OnDevToolsEvent(const std::string_view& method,
                       const std::string_view& params) override {
    CEF_REQUIRE_UIT();
    observer_->OnDevToolsEvent(browser_.get(), std::string(method),
                               params.data(), params.size());
  }

  void OnDevToolsAgentAttached() override {
    CEF_REQUIRE_UIT();
    observer_->OnDevToolsAgentAttached(browser_.get());
  }

  void OnDevToolsAgentDetached() override {
    CEF_REQUIRE_UIT();
    observer_->OnDevToolsAgentDetached(browser_.get());
  }

  void OnDevToolsControllerDestroyed() override {
    CEF_REQUIRE_UIT();
    browser_ = nullptr;
    controller_.reset();
  }

  CefRefPtr<CefDevToolsMessageObserver> observer_;

  raw_ptr<CefBrowserHostBase> browser_ = nullptr;
  base::WeakPtr<CefDevToolsController> controller_;

  IMPLEMENT_REFCOUNTING_DELETE_ON_UIT(CefDevToolsRegistrationImpl);
};

}  // namespace

CefDevToolsProtocolManager::CefDevToolsProtocolManager(
    CefBrowserHostBase* inspected_browser)
    : inspected_browser_(inspected_browser) {
  CEF_REQUIRE_UIT();
}

CefDevToolsProtocolManager::~CefDevToolsProtocolManager() {
  CEF_REQUIRE_UIT();
}

bool CefDevToolsProtocolManager::SendDevToolsMessage(const void* message,
                                                     size_t message_size) {
  CEF_REQUIRE_UIT();
  if (!message || message_size == 0) {
    return false;
  }

  if (!EnsureController()) {
    return false;
  }

  return devtools_controller_->SendDevToolsMessage(
      std::string_view(static_cast<const char*>(message), message_size));
}

int CefDevToolsProtocolManager::ExecuteDevToolsMethod(
    int message_id,
    const CefString& method,
    CefRefPtr<CefDictionaryValue> params) {
  CEF_REQUIRE_UIT();
  if (method.empty()) {
    return 0;
  }

  if (!EnsureController()) {
    return 0;
  }

  if (params && params->IsValid()) {
    CefDictionaryValueImpl* impl =
        static_cast<CefDictionaryValueImpl*>(params.get());
    CefValueController::AutoLock lock_scope(impl->controller());
    return devtools_controller_->ExecuteDevToolsMethod(
        message_id, method, impl->GetValueUnsafe()->GetIfDict());
  } else {
    return devtools_controller_->ExecuteDevToolsMethod(message_id, method,
                                                       nullptr);
  }
}

// static
CefRefPtr<CefRegistration> CefDevToolsProtocolManager::CreateRegistration(
    CefRefPtr<CefDevToolsMessageObserver> observer) {
  DCHECK(observer);
  return new CefDevToolsRegistrationImpl(observer);
}

void CefDevToolsProtocolManager::InitializeRegistrationOnUIThread(
    CefRefPtr<CefRegistration> registration) {
  CEF_REQUIRE_UIT();

  if (!EnsureController()) {
    return;
  }

  static_cast<CefDevToolsRegistrationImpl*>(registration.get())
      ->Initialize(inspected_browser_, devtools_controller_->GetWeakPtr());
}

bool CefDevToolsProtocolManager::EnsureController() {
  if (!devtools_controller_) {
    devtools_controller_ = std::make_unique<CefDevToolsController>(
        inspected_browser_->contents_delegate()->web_contents());
  }
  return true;
}
