// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "cef/libcef/browser/component_updater_impl.h"

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "cef/libcef/browser/context.h"
#include "cef/libcef/browser/thread_util.h"
#include "cef/libcef/common/api_version_util.h"
#include "chrome/browser/browser_process.h"
#include "components/component_updater/component_updater_service.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"

namespace {

cef_component_state_t ToCefComponentState(update_client::ComponentState state) {
  switch (state) {
    case update_client::ComponentState::kNew:
      return CEF_COMPONENT_STATE_NEW;
    case update_client::ComponentState::kChecking:
      return CEF_COMPONENT_STATE_CHECKING;
    case update_client::ComponentState::kCanUpdate:
      return CEF_COMPONENT_STATE_CAN_UPDATE;
    case update_client::ComponentState::kDownloading:
      return CEF_COMPONENT_STATE_DOWNLOADING;
    case update_client::ComponentState::kDecompressing:
      return CEF_COMPONENT_STATE_DECOMPRESSING;
    case update_client::ComponentState::kPatching:
      return CEF_COMPONENT_STATE_PATCHING;
    case update_client::ComponentState::kUpdating:
      return CEF_COMPONENT_STATE_UPDATING;
    case update_client::ComponentState::kUpdated:
      return CEF_COMPONENT_STATE_UPDATED;
    case update_client::ComponentState::kUpToDate:
      return CEF_COMPONENT_STATE_UP_TO_DATE;
    case update_client::ComponentState::kUpdateError:
      return CEF_COMPONENT_STATE_UPDATE_ERROR;
    case update_client::ComponentState::kRun:
      return CEF_COMPONENT_STATE_RUN;
  }
  NOTREACHED() << "Unexpected ComponentState: " << static_cast<int>(state);
}

cef_component_update_error_t ToCefUpdateError(update_client::Error error) {
  switch (error) {
    case update_client::Error::NONE:
      return CEF_COMPONENT_UPDATE_ERROR_NONE;
    case update_client::Error::UPDATE_IN_PROGRESS:
      return CEF_COMPONENT_UPDATE_ERROR_UPDATE_IN_PROGRESS;
    case update_client::Error::UPDATE_CANCELED:
      return CEF_COMPONENT_UPDATE_ERROR_UPDATE_CANCELED;
    case update_client::Error::RETRY_LATER:
      return CEF_COMPONENT_UPDATE_ERROR_RETRY_LATER;
    case update_client::Error::SERVICE_ERROR:
      return CEF_COMPONENT_UPDATE_ERROR_SERVICE_ERROR;
    case update_client::Error::UPDATE_CHECK_ERROR:
      return CEF_COMPONENT_UPDATE_ERROR_UPDATE_CHECK_ERROR;
    case update_client::Error::CRX_NOT_FOUND:
      return CEF_COMPONENT_UPDATE_ERROR_CRX_NOT_FOUND;
    case update_client::Error::INVALID_ARGUMENT:
      return CEF_COMPONENT_UPDATE_ERROR_INVALID_ARGUMENT;
    case update_client::Error::BAD_CRX_DATA_CALLBACK:
      return CEF_COMPONENT_UPDATE_ERROR_BAD_CRX_DATA_CALLBACK;
    case update_client::Error::MAX_VALUE:
      return CEF_COMPONENT_UPDATE_ERROR_SERVICE_ERROR;
  }
  NOTREACHED() << "Unexpected update_client::Error: "
               << static_cast<int>(error);
}

component_updater::OnDemandUpdater::Priority ToChromiumPriority(
    cef_component_update_priority_t priority) {
  switch (priority) {
    case CEF_COMPONENT_UPDATE_PRIORITY_BACKGROUND:
      return component_updater::OnDemandUpdater::Priority::BACKGROUND;
    case CEF_COMPONENT_UPDATE_PRIORITY_FOREGROUND:
      return component_updater::OnDemandUpdater::Priority::FOREGROUND;
  }
  NOTREACHED() << "Unexpected update priority: " << static_cast<int>(priority);
}

void OnUpdateComplete(CefRefPtr<CefComponentUpdateCallback> callback,
                      const std::string& component_id,
                      update_client::Error error) {
  CEF_REQUIRE_UIT();
  if (callback) {
    callback->OnComplete(component_id, ToCefUpdateError(error));
  }
}

CefRefPtr<CefComponentImpl> CreateComponentFromItem(
    std::string id,
    const update_client::CrxUpdateItem& item) {
  std::string name;
  std::string version;
  if (item.component) {
    name = item.component->name;
    version = item.component->version.GetString();
  }
  return new CefComponentImpl(std::move(id), std::move(name),
                              std::move(version),
                              ToCefComponentState(item.state));
}

component_updater::ComponentUpdateService* GetComponentUpdateService() {
  if (!CONTEXT_STATE_VALID()) {
    return nullptr;
  }
  CEF_REQUIRE_UIT_RETURN(nullptr);
  return g_browser_process->component_updater();
}

}  // namespace

CefComponentImpl::CefComponentImpl(std::string id,
                                   std::string name,
                                   std::string version,
                                   cef_component_state_t state)
    : id_(std::move(id)),
      name_(std::move(name)),
      version_(std::move(version)),
      state_(state) {}

CefString CefComponentImpl::GetID() {
  return id_;
}

CefString CefComponentImpl::GetName() {
  return name_;
}

CefString CefComponentImpl::GetVersion() {
  return version_;
}

cef_component_state_t CefComponentImpl::GetState() {
  return state_;
}

size_t CefComponentUpdaterImpl::GetComponentCount() {
  auto* service = GetComponentUpdateService();
  if (!service) {
    return 0;
  }
  return service->GetComponentIDs().size();
}

void CefComponentUpdaterImpl::GetComponents(
    std::vector<CefRefPtr<CefComponent>>& components) {
  components.clear();
  auto* service = GetComponentUpdateService();
  if (!service) {
    return;
  }

  const auto& component_ids = service->GetComponentIDs();
  components.reserve(component_ids.size());

  for (const auto& component_id : component_ids) {
    update_client::CrxUpdateItem item;
    CHECK(service->GetComponentDetails(component_id, &item));
    components.push_back(CreateComponentFromItem(component_id, item));
  }
}

CefRefPtr<CefComponent> CefComponentUpdaterImpl::GetComponentByID(
    const CefString& component_id) {
  auto* service = GetComponentUpdateService();
  if (!service) {
    return nullptr;
  }

  std::string id = component_id.ToString();
  update_client::CrxUpdateItem item;

  if (service->GetComponentDetails(id, &item)) {
    return CreateComponentFromItem(std::move(id), item);
  }
  return nullptr;
}

void CefComponentUpdaterImpl::Update(
    const CefString& component_id,
    cef_component_update_priority_t priority,
    CefRefPtr<CefComponentUpdateCallback> callback) {
  auto* service = GetComponentUpdateService();
  if (!service) {
    return;
  }

  std::string id = component_id.ToString();
  service->GetOnDemandUpdater().OnDemandUpdate(
      id, ToChromiumPriority(priority),
      base::BindOnce(&OnUpdateComplete, callback, id));
}

// static
CefRefPtr<CefComponentUpdater> CefComponentUpdater::GetComponentUpdater() {
  CEF_API_REQUIRE_ADDED(CEF_NEXT);
  CEF_REQUIRE_UIT_RETURN(nullptr);

  static base::NoDestructor<CefRefPtr<CefComponentUpdaterImpl>> instance(
      new CefComponentUpdaterImpl());
  return instance->get();
}
