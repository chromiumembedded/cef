// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/config_test.h"

#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "include/base/cef_logging.h"
#include "include/cef_parser.h"
#include "include/cef_request_context.h"
#include "tests/cefclient/browser/test_runner.h"

namespace client::config_test {

namespace {

const char kTestUrlPath[] = "/config";

// Application-specific error codes.
const int kMessageFormatError = 1;
const int kRequestFailedError = 2;

// Common to all messages.
const char kNameKey[] = "name";
const char kNameGlobalConfig[] = "global_config";
const char kNameSubscribe[] = "subscribe";

// Convert a dictionary value to a JSON string.
CefString GetJSON(CefRefPtr<CefDictionaryValue> dictionary) {
  CefRefPtr<CefValue> value = CefValue::Create();
  value->SetDictionary(dictionary);
  return CefWriteJSON(value, JSON_WRITER_DEFAULT);
}

using CallbackType = CefMessageRouterBrowserSide::Callback;

void SendSuccess(CefRefPtr<CallbackType> callback,
                 CefRefPtr<CefDictionaryValue> result) {
  callback->Success(GetJSON(result));
}

void SendFailure(CefRefPtr<CallbackType> callback,
                 int error_code,
                 const std::string& error_message) {
  callback->Failure(error_code, error_message);
}

class PreferenceObserver : public CefPreferenceObserver {
 public:
  PreferenceObserver(CefRefPtr<CefPreferenceManager> manager,
                     bool global,
                     CefRefPtr<CallbackType> callback)
      : manager_(manager), global_(global), callback_(callback) {}

  PreferenceObserver(const PreferenceObserver&) = delete;
  PreferenceObserver& operator=(const PreferenceObserver&) = delete;

  void OnPreferenceChanged(const CefString& name) override {
    CEF_REQUIRE_UI_THREAD();
    auto value = manager_->GetPreference(name);

    auto payload = CefDictionaryValue::Create();
    payload->SetString("type", "preference");
    payload->SetBool("global", global_);
    payload->SetString("name", name);
    if (value) {
      payload->SetInt("value_type", value->GetType());
      payload->SetValue("value", value);
    } else {
      payload->SetInt("value_type", VTYPE_NULL);
      payload->SetNull("value");
    }

    SendSuccess(callback_, payload);
  }

 private:
  const CefRefPtr<CefPreferenceManager> manager_;
  const bool global_;
  const CefRefPtr<CallbackType> callback_;

  IMPLEMENT_REFCOUNTING(PreferenceObserver);
};

class SettingObserver : public CefSettingObserver {
 public:
  SettingObserver(CefRefPtr<CefRequestContext> context,
                  CefRefPtr<CallbackType> callback)
      : context_(context), callback_(callback) {}

  SettingObserver(const SettingObserver&) = delete;
  SettingObserver& operator=(const SettingObserver&) = delete;

  void OnSettingChanged(const CefString& requesting_url,
                        const CefString& top_level_url,
                        cef_content_setting_types_t content_type) override {
    CEF_REQUIRE_UI_THREAD();
    auto value = context_->GetWebsiteSetting(requesting_url, top_level_url,
                                             content_type);

    auto payload = CefDictionaryValue::Create();
    payload->SetString("type", "setting");
    payload->SetString("requesting_url", requesting_url);
    payload->SetString("top_level_url", top_level_url);
    payload->SetInt("content_type", static_cast<int>(content_type));
    if (value) {
      payload->SetInt("value_type", value->GetType());
      payload->SetValue("value", value);
    } else {
      payload->SetInt("value_type", VTYPE_NULL);
      payload->SetNull("value");
    }

    SendSuccess(callback_, payload);
  }

 private:
  const CefRefPtr<CefRequestContext> context_;
  const CefRefPtr<CallbackType> callback_;

  IMPLEMENT_REFCOUNTING(SettingObserver);
};

// Handle messages in the browser process. Only accessed on the UI thread.
class Handler : public CefMessageRouterBrowserSide::Handler {
 public:
  typedef std::vector<std::string> NameVector;

  Handler() { CEF_REQUIRE_UI_THREAD(); }

  Handler(const Handler&) = delete;
  Handler& operator=(const Handler&) = delete;

  ~Handler() override {
    for (auto& pair : subscription_state_map_) {
      delete pair.second;
    }
  }

  // Called due to cefQuery execution in config.html.
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

    // Parse |request| as a JSON dictionary.
    CefRefPtr<CefDictionaryValue> request_dict = ParseJSON(request);
    if (!request_dict) {
      callback->Failure(kMessageFormatError, "Incorrect message format");
      return true;
    }

    // Verify the "name" key.
    if (!VerifyKey(request_dict, kNameKey, VTYPE_STRING, callback)) {
      return true;
    }

    const std::string& message_name = request_dict->GetString(kNameKey);
    if (message_name == kNameGlobalConfig) {
      // JavaScript is requesting a JSON representation of the global config.

      SendGlobalConfig(callback);
      return true;
    } else if (message_name == kNameSubscribe) {
      // Subscribe to notifications from observers.

      if (!persistent) {
        SendFailure(callback, kMessageFormatError,
                    "Subscriptions must be persistent");
        return true;
      }

      if (!CreateSubscription(browser, query_id, callback)) {
        SendFailure(callback, kRequestFailedError,
                    "Browser is already subscribed");
      }
      return true;
    }

    return false;
  }

  void OnQueryCanceled(CefRefPtr<CefBrowser> browser,
                       CefRefPtr<CefFrame> frame,
                       int64_t query_id) override {
    CEF_REQUIRE_UI_THREAD();
    RemoveSubscription(browser->GetIdentifier(), query_id);
  }

 private:
  // Convert a JSON string to a dictionary value.
  static CefRefPtr<CefDictionaryValue> ParseJSON(const CefString& string) {
    CefRefPtr<CefValue> value = CefParseJSON(string, JSON_PARSER_RFC);
    if (value.get() && value->GetType() == VTYPE_DICTIONARY) {
      return value->GetDictionary();
    }
    return nullptr;
  }

  // Verify that |key| exists in |dictionary| and has type |value_type|. Fails
  // |callback| and returns false on failure.
  static bool VerifyKey(CefRefPtr<CefDictionaryValue> dictionary,
                        const char* key,
                        cef_value_type_t value_type,
                        CefRefPtr<Callback> callback) {
    if (!dictionary->HasKey(key) || dictionary->GetType(key) != value_type) {
      callback->Failure(
          kMessageFormatError,
          "Missing or incorrectly formatted message key: " + std::string(key));
      return false;
    }
    return true;
  }

  static CefRefPtr<CefListValue> MakeListValue(std::vector<CefString> vec) {
    if (vec.empty()) {
      return nullptr;
    }
    auto list = CefListValue::Create();
    list->SetSize(vec.size());
    size_t idx = 0;
    for (const auto& val : vec) {
      list->SetString(idx++, val);
    }
    return list;
  }

  void SendGlobalConfig(CefRefPtr<Callback> callback) {
    std::vector<CefString> switches;
    CefPreferenceManager::GetChromeVariationsAsSwitches(switches);
    std::vector<CefString> strings;
    CefPreferenceManager::GetChromeVariationsAsStrings(strings);

    auto payload = CefDictionaryValue::Create();

    if (auto list = MakeListValue(switches)) {
      payload->SetList("switches", list);
    } else {
      payload->SetNull("switches");
    }

    if (auto list = MakeListValue(strings)) {
      payload->SetList("strings", list);
    } else {
      payload->SetNull("strings");
    }

    SendSuccess(callback, payload);
  }

  // Subscription state associated with a single browser.
  struct SubscriptionState {
    int64_t query_id;
    CefRefPtr<PreferenceObserver> global_pref_observer;
    CefRefPtr<CefRegistration> global_pref_registration;
    CefRefPtr<PreferenceObserver> context_pref_observer;
    CefRefPtr<CefRegistration> context_pref_registration;
    CefRefPtr<SettingObserver> context_setting_observer;
    CefRefPtr<CefRegistration> context_setting_registration;
  };

  bool CreateSubscription(CefRefPtr<CefBrowser> browser,
                          int64_t query_id,
                          CefRefPtr<Callback> callback) {
    const int browser_id = browser->GetIdentifier();
    if (subscription_state_map_.find(browser_id) !=
        subscription_state_map_.end()) {
      // An subscription already exists for this browser.
      return false;
    }

    auto global_pref_manager =
        CefPreferenceManager::GetGlobalPreferenceManager();
    auto request_context = browser->GetHost()->GetRequestContext();

    SubscriptionState* state = new SubscriptionState();
    state->query_id = query_id;

    state->global_pref_observer =
        new PreferenceObserver(global_pref_manager, /*global=*/true, callback);
    state->global_pref_registration =
        global_pref_manager->AddPreferenceObserver(CefString(),
                                                   state->global_pref_observer);

    state->context_pref_observer =
        new PreferenceObserver(request_context, /*global=*/false, callback);
    state->context_pref_registration = request_context->AddPreferenceObserver(
        CefString(), state->context_pref_observer);

    state->context_setting_observer =
        new SettingObserver(request_context, callback);
    state->context_setting_registration =
        request_context->AddSettingObserver(state->context_setting_observer);

    subscription_state_map_[browser_id] = state;

    return true;
  }

  void RemoveSubscription(int browser_id, int64_t query_id) {
    SubscriptionStateMap::iterator it =
        subscription_state_map_.find(browser_id);
    if (it != subscription_state_map_.end() &&
        it->second->query_id == query_id) {
      delete it->second;
      subscription_state_map_.erase(it);
    }
  }

  // Map of browser ID to SubscriptionState object.
  typedef std::map<int, SubscriptionState*> SubscriptionStateMap;
  SubscriptionStateMap subscription_state_map_;
};

}  // namespace

void CreateMessageHandlers(test_runner::MessageHandlerSet& handlers) {
  handlers.insert(new Handler());
}

}  // namespace client::config_test
