// Copyright (c) 2020 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/devtools/devtools_manager.h"

#include <memory>

#include "libcef/browser/devtools/devtools_controller.h"
#include "libcef/browser/devtools/devtools_frontend.h"
#include "libcef/features/runtime.h"

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
  bool OnDevToolsMessage(const base::StringPiece& message) override {
    CEF_REQUIRE_UIT();
    return observer_->OnDevToolsMessage(browser_, message.data(),
                                        message.size());
  }

  void OnDevToolsMethodResult(int message_id,
                              bool success,
                              const base::StringPiece& result) override {
    CEF_REQUIRE_UIT();
    observer_->OnDevToolsMethodResult(browser_, message_id, success,
                                      result.data(), result.size());
  }

  void OnDevToolsEvent(const base::StringPiece& method,
                       const base::StringPiece& params) override {
    CEF_REQUIRE_UIT();
    observer_->OnDevToolsEvent(browser_, std::string(method), params.data(),
                               params.size());
  }

  void OnDevToolsAgentAttached() override {
    CEF_REQUIRE_UIT();
    observer_->OnDevToolsAgentAttached(browser_);
  }

  void OnDevToolsAgentDetached() override {
    CEF_REQUIRE_UIT();
    observer_->OnDevToolsAgentDetached(browser_);
  }

  void OnDevToolsControllerDestroyed() override {
    CEF_REQUIRE_UIT();
    browser_ = nullptr;
    controller_.reset();
  }

  CefRefPtr<CefDevToolsMessageObserver> observer_;

  CefBrowserHostBase* browser_ = nullptr;
  base::WeakPtr<CefDevToolsController> controller_;

  IMPLEMENT_REFCOUNTING_DELETE_ON_UIT(CefDevToolsRegistrationImpl);
};

}  // namespace

CefDevToolsManager::CefDevToolsManager(CefBrowserHostBase* inspected_browser)
    : inspected_browser_(inspected_browser), weak_ptr_factory_(this) {
  CEF_REQUIRE_UIT();
}

CefDevToolsManager::~CefDevToolsManager() {
  CEF_REQUIRE_UIT();
}

void CefDevToolsManager::ShowDevTools(const CefWindowInfo& windowInfo,
                                      CefRefPtr<CefClient> client,
                                      const CefBrowserSettings& settings,
                                      const CefPoint& inspect_element_at) {
  CEF_REQUIRE_UIT();
  if (devtools_frontend_) {
    if (!inspect_element_at.IsEmpty()) {
      devtools_frontend_->InspectElementAt(inspect_element_at.x,
                                           inspect_element_at.y);
    }
    devtools_frontend_->Focus();
    return;
  }

  if (cef::IsChromeRuntimeEnabled()) {
    NOTIMPLEMENTED();
  } else {
    auto alloy_browser = static_cast<AlloyBrowserHostImpl*>(inspected_browser_);
    devtools_frontend_ = CefDevToolsFrontend::Show(
        alloy_browser, windowInfo, client, settings, inspect_element_at,
        base::BindOnce(&CefDevToolsManager::OnFrontEndDestroyed,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void CefDevToolsManager::CloseDevTools() {
  CEF_REQUIRE_UIT();
  if (!devtools_frontend_) {
    return;
  }
  devtools_frontend_->Close();
}

bool CefDevToolsManager::HasDevTools() {
  CEF_REQUIRE_UIT();
  return !!devtools_frontend_;
}

bool CefDevToolsManager::SendDevToolsMessage(const void* message,
                                             size_t message_size) {
  CEF_REQUIRE_UIT();
  if (!message || message_size == 0) {
    return false;
  }

  if (!EnsureController()) {
    return false;
  }

  return devtools_controller_->SendDevToolsMessage(
      base::StringPiece(static_cast<const char*>(message), message_size));
}

int CefDevToolsManager::ExecuteDevToolsMethod(
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
CefRefPtr<CefRegistration> CefDevToolsManager::CreateRegistration(
    CefRefPtr<CefDevToolsMessageObserver> observer) {
  DCHECK(observer);
  return new CefDevToolsRegistrationImpl(observer);
}

void CefDevToolsManager::InitializeRegistrationOnUIThread(
    CefRefPtr<CefRegistration> registration) {
  CEF_REQUIRE_UIT();

  if (!EnsureController()) {
    return;
  }

  static_cast<CefDevToolsRegistrationImpl*>(registration.get())
      ->Initialize(inspected_browser_, devtools_controller_->GetWeakPtr());
}

void CefDevToolsManager::OnFrontEndDestroyed() {
  devtools_frontend_ = nullptr;
}

bool CefDevToolsManager::EnsureController() {
  if (!devtools_controller_) {
    devtools_controller_ = std::make_unique<CefDevToolsController>(
        inspected_browser_->contents_delegate()->web_contents());
  }
  return true;
}
