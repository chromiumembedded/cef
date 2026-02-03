// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "cef/libcef/browser/component_updater_impl.h"

#include "base/functional/bind.h"
#include "cef/libcef/browser/context.h"
#include "cef/libcef/browser/thread_util.h"
#include "chrome/browser/browser_process.h"
#include "components/component_updater/component_updater_service.h"
#include "components/update_client/crx_update_item.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"

#if CEF_API_ADDED(CEF_NEXT)

namespace {

cef_component_state_t MapComponentState(update_client::ComponentState state) {
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
  return CEF_COMPONENT_STATE_NEW;
}

cef_component_update_error_t MapUpdateClientError(update_client::Error error) {
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
      // Treat MAX_VALUE as a service error
      return CEF_COMPONENT_UPDATE_ERROR_SERVICE_ERROR;
  }
  return CEF_COMPONENT_UPDATE_ERROR_SERVICE_ERROR;
}

component_updater::OnDemandUpdater::Priority ToChromeUpdatePriority(
    cef_component_update_priority_t priority) {
  switch (priority) {
    case CEF_COMPONENT_UPDATE_PRIORITY_BACKGROUND:
      return component_updater::OnDemandUpdater::Priority::BACKGROUND;
    case CEF_COMPONENT_UPDATE_PRIORITY_FOREGROUND:
      return component_updater::OnDemandUpdater::Priority::FOREGROUND;
  }
  return component_updater::OnDemandUpdater::Priority::BACKGROUND;
}

void OnUpdateComplete(CefRefPtr<CefComponentUpdateCallback> callback,
                      update_client::Error error) {
  CEF_REQUIRE_UIT();
  if (callback) {
    callback->OnComplete(MapUpdateClientError(error));
  }
}

}  // namespace

CefComponentImpl::CefComponentImpl(const std::string& id,
                                   const std::string& name,
                                   const std::string& version,
                                   cef_component_state_t state)
    : id_(id), name_(name), version_(version), state_(state) {}

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

CefComponentUpdaterImpl::CefComponentUpdaterImpl(
    component_updater::ComponentUpdateService* component_updater)
    : component_updater_(component_updater) {
  DCHECK(component_updater_);
}

size_t CefComponentUpdaterImpl::GetComponentCount() {
  CEF_REQUIRE_UIT();
  return component_updater_->GetComponentIDs().size();
}

void CefComponentUpdaterImpl::GetComponents(
    std::vector<CefRefPtr<CefComponent>>& components) {
  CEF_REQUIRE_UIT();

  components.clear();

  for (const auto& component_id : component_updater_->GetComponentIDs()) {
    update_client::CrxUpdateItem item;
    if (component_updater_->GetComponentDetails(component_id, &item)) {
      std::string name;
      std::string version;

      // component is std::optional - may not be present if not installed
      if (item.component) {
        name = item.component->name;
        version = item.component->version.GetString();
      }

      components.push_back(new CefComponentImpl(component_id, name, version,
                                                MapComponentState(item.state)));
    }
  }
}

CefRefPtr<CefComponent> CefComponentUpdaterImpl::GetComponentByID(
    const CefString& component_id) {
  CEF_REQUIRE_UIT();

  std::string id = component_id.ToString();
  update_client::CrxUpdateItem item;

  if (component_updater_->GetComponentDetails(id, &item)) {
    std::string name;
    std::string version;

    if (item.component) {
      name = item.component->name;
      version = item.component->version.GetString();
    }

    return new CefComponentImpl(id, name, version,
                                MapComponentState(item.state));
  }

  return nullptr;
}

void CefComponentUpdaterImpl::Update(
    const CefString& component_id,
    cef_component_update_priority_t priority,
    CefRefPtr<CefComponentUpdateCallback> callback) {
  CEF_REQUIRE_UIT();

  component_updater_->GetOnDemandUpdater().OnDemandUpdate(
      component_id.ToString(), ToChromeUpdatePriority(priority),
      base::BindOnce(&OnUpdateComplete, callback));
}

// Static
CefRefPtr<CefComponentUpdater> CefComponentUpdater::GetComponentUpdater() {
  if (!CONTEXT_STATE_VALID()) {
    DCHECK(false) << "context not valid";
    return nullptr;
  }

  CEF_REQUIRE_UIT_RETURN(nullptr);

  auto* component_updater = g_browser_process->component_updater();
  if (!component_updater) {
    return nullptr;
  }

  return new CefComponentUpdaterImpl(component_updater);
}

#endif  // CEF_API_ADDED(CEF_NEXT)
