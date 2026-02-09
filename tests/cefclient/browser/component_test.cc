// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/component_test.h"

#include "include/cef_component_updater.h"
#include "include/cef_parser.h"
#include "include/wrapper/cef_helpers.h"

namespace client::component_test {

namespace {

const char kTestUrlPath[] = "/components";

const char* GetErrorName(cef_component_update_error_t error) {
  switch (error) {
    case CEF_COMPONENT_UPDATE_ERROR_NONE:
      return "None";
    case CEF_COMPONENT_UPDATE_ERROR_UPDATE_IN_PROGRESS:
      return "Update in progress";
    case CEF_COMPONENT_UPDATE_ERROR_UPDATE_CANCELED:
      return "Update canceled";
    case CEF_COMPONENT_UPDATE_ERROR_RETRY_LATER:
      return "Retry later";
    case CEF_COMPONENT_UPDATE_ERROR_SERVICE_ERROR:
      return "Service error";
    case CEF_COMPONENT_UPDATE_ERROR_UPDATE_CHECK_ERROR:
      return "Update check error";
    case CEF_COMPONENT_UPDATE_ERROR_CRX_NOT_FOUND:
      return "Component not found";
    case CEF_COMPONENT_UPDATE_ERROR_INVALID_ARGUMENT:
      return "Invalid argument";
    case CEF_COMPONENT_UPDATE_ERROR_BAD_CRX_DATA_CALLBACK:
      return "Bad CRX data callback";
  }
  return "Unknown error";
}

const char* GetStateName(cef_component_state_t state) {
  switch (state) {
    case CEF_COMPONENT_STATE_NEW:
      return "New";
    case CEF_COMPONENT_STATE_CHECKING:
      return "Checking for update...";
    case CEF_COMPONENT_STATE_CAN_UPDATE:
      return "Update available";
    case CEF_COMPONENT_STATE_DOWNLOADING:
      return "Downloading...";
    case CEF_COMPONENT_STATE_DECOMPRESSING:
      return "Decompressing...";
    case CEF_COMPONENT_STATE_PATCHING:
      return "Patching...";
    case CEF_COMPONENT_STATE_UPDATING:
      return "Updating...";
    case CEF_COMPONENT_STATE_UPDATED:
      return "Updated";
    case CEF_COMPONENT_STATE_UP_TO_DATE:
      return "Up to date";
    case CEF_COMPONENT_STATE_UPDATE_ERROR:
      return "Update error";
    case CEF_COMPONENT_STATE_RUN:
      return "Running action";
  }
  return "Unknown";
}

// Callback for component update operations
class ComponentUpdateCallbackImpl : public CefComponentUpdateCallback {
 public:
  explicit ComponentUpdateCallbackImpl(
      CefRefPtr<CefMessageRouterBrowserSide::Callback> callback)
      : callback_(callback) {}

  void OnComplete(const CefString& component_id,
                  cef_component_update_error_t error) override {
    CEF_REQUIRE_UI_THREAD();

    CefRefPtr<CefDictionaryValue> result = CefDictionaryValue::Create();
    result->SetString("componentId", component_id);
    result->SetInt("error", static_cast<int>(error));
    result->SetString("errorName", GetErrorName(error));

    CefRefPtr<CefValue> value = CefValue::Create();
    value->SetDictionary(result);
    CefString json = CefWriteJSON(value, JSON_WRITER_DEFAULT);
    callback_->Success(json);
  }

 private:
  CefRefPtr<CefMessageRouterBrowserSide::Callback> callback_;

  IMPLEMENT_REFCOUNTING(ComponentUpdateCallbackImpl);
};

// Message handler for component test page
class Handler : public CefMessageRouterBrowserSide::Handler {
 public:
  Handler() = default;
  Handler(const Handler&) = delete;
  Handler& operator=(const Handler&) = delete;

  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64_t query_id,
               const CefString& request,
               bool persistent,
               CefRefPtr<Callback> callback) override {
    CEF_REQUIRE_UI_THREAD();

    // Only handle messages from the test URL.
    const std::string& url = frame->GetURL();
    if (!test_runner::IsTestURL(url, kTestUrlPath)) {
      return false;
    }

    // Parse the JSON request
    CefRefPtr<CefValue> value =
        CefParseJSON(request, JSON_PARSER_ALLOW_TRAILING_COMMAS);
    if (!value || value->GetType() != VTYPE_DICTIONARY) {
      callback->Failure(1, "Invalid request format");
      return true;
    }

    CefRefPtr<CefDictionaryValue> dict = value->GetDictionary();
    CefString action = dict->GetString("action");

    if (action == "getComponents") {
      return HandleGetComponents(callback);
    } else if (action == "updateComponent") {
      CefString componentId = dict->GetString("componentId");
      return HandleUpdateComponent(componentId, callback);
    }

    return false;  // Not handled
  }

 private:
  bool HandleGetComponents(CefRefPtr<Callback> callback) {
    CefRefPtr<CefComponentUpdater> updater =
        CefComponentUpdater::GetComponentUpdater();
    if (!updater) {
      callback->Failure(2, "Component updater not available");
      return true;
    }

    std::vector<CefRefPtr<CefComponent>> components;
    updater->GetComponents(components);

    CefRefPtr<CefListValue> componentList = CefListValue::Create();
    for (size_t i = 0; i < components.size(); ++i) {
      CefRefPtr<CefComponent> component = components[i];
      CefRefPtr<CefDictionaryValue> componentDict =
          CefDictionaryValue::Create();
      componentDict->SetString("id", component->GetID());
      componentDict->SetString("name", component->GetName());
      componentDict->SetString("version", component->GetVersion());
      componentDict->SetString("status", GetStateName(component->GetState()));
      componentList->SetDictionary(i, componentDict);
    }

    CefRefPtr<CefValue> value = CefValue::Create();
    value->SetList(componentList);
    CefString json = CefWriteJSON(value, JSON_WRITER_DEFAULT);
    callback->Success(json);
    return true;
  }

  bool HandleUpdateComponent(const CefString& componentId,
                             CefRefPtr<Callback> callback) {
    if (componentId.empty()) {
      callback->Failure(3, "Component ID is required");
      return true;
    }

    CefRefPtr<CefComponentUpdater> updater =
        CefComponentUpdater::GetComponentUpdater();
    if (!updater) {
      callback->Failure(2, "Component updater not available");
      return true;
    }

    // Request update directly via the updater with component ID.
    updater->Update(componentId, CEF_COMPONENT_UPDATE_PRIORITY_FOREGROUND,
                    new ComponentUpdateCallbackImpl(callback));
    return true;
  }
};

}  // namespace

void CreateMessageHandlers(test_runner::MessageHandlerSet& handlers) {
  handlers.insert(new Handler());
}

}  // namespace client::component_test
