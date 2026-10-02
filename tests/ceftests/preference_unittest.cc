// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/base/cef_callback.h"
#include "include/cef_request_context_handler.h"
#include "include/cef_waitable_event.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_scoped_temp_dir.h"
#include "tests/ceftests/test_handler.h"
#include "tests/ceftests/test_util.h"
#include "tests/gtest/include/gtest/gtest.h"
#include "tests/shared/browser/client_app_browser.h"

namespace {

// Fully qualified preference names.
const char kPrefTest[] = "test";
const char kPrefTestBool[] = "test.bool";
const char kPrefTestInt[] = "test.int";
const char kPrefTestDouble[] = "test.double";
const char kPrefTestString[] = "test.string";
const char kPrefTestList[] = "test.list";
const char kPrefTestDict[] = "test.dict";
const char kPrefTestNoExist[] = "test.noexist";
#if CEF_API_ADDED(CEF_NEXT)
const char kPrefTestOverride[] = "test.override";
const char kSigninAllowed[] = "signin.allowed";
const char kSigninAllowedOnNextStartup[] = "signin.allowed_on_next_startup";
#endif

// Unqualified preference names.
const char kPrefBool[] = "bool";
const char kPrefInt[] = "int";
const char kPrefDouble[] = "double";
const char kPrefString[] = "string";
const char kPrefList[] = "list";
const char kPrefDict[] = "dict";
#if CEF_API_ADDED(CEF_NEXT)
const char kPrefOverride[] = "override";
#endif

CefRefPtr<CefValue> CreateBoolValue(bool value) {
  auto val = CefValue::Create();
  val->SetBool(value);
  return val;
}

CefRefPtr<CefValue> CreateIntValue(int value) {
  auto val = CefValue::Create();
  val->SetInt(value);
  return val;
}

CefRefPtr<CefValue> CreateDoubleValue(double value) {
  auto val = CefValue::Create();
  val->SetDouble(value);
  return val;
}

CefRefPtr<CefValue> CreateStringValue(const std::string& value) {
  auto val = CefValue::Create();
  val->SetString(value);
  return val;
}

CefRefPtr<CefValue> CreateListValue(CefRefPtr<CefListValue> value) {
  auto val = CefValue::Create();
  val->SetList(value);
  return val;
}

CefRefPtr<CefValue> CreateDictionaryValue(CefRefPtr<CefDictionaryValue> value) {
  auto val = CefValue::Create();
  val->SetDictionary(value);
  return val;
}

// Browser-side app delegate.
class PreferenceBrowserTest : public client::ClientAppBrowser::Delegate {
 public:
  PreferenceBrowserTest() = default;

  void OnRegisterCustomPreferences(
      CefRefPtr<client::ClientAppBrowser> app,
      cef_preferences_type_t type,
      CefRawPtr<CefPreferenceRegistrar> registrar) override {
    // Register test preferences.
    registrar->AddPreference(kPrefTestBool, CreateBoolValue(true));
    registrar->AddPreference(kPrefTestInt, CreateIntValue(2));
    registrar->AddPreference(kPrefTestDouble, CreateDoubleValue(5.0));
    registrar->AddPreference(kPrefTestString, CreateStringValue("default"));
    registrar->AddPreference(kPrefTestList,
                             CreateListValue(CefListValue::Create()));
    registrar->AddPreference(
        kPrefTestDict, CreateDictionaryValue(CefDictionaryValue::Create()));
#if CEF_API_ADDED(CEF_NEXT)
    const bool added_override =
        registrar->AddPreference(kPrefTestOverride, CreateBoolValue(true));
    const auto* test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    const bool run_registration_tests =
        type == CEF_PREFERENCES_TYPE_REQUEST_CONTEXT && test_info &&
        std::string(test_info->test_suite_name()) == "PreferenceTest" &&
        std::string(test_info->name()) == "OverriddenRequestContextDefault";
    if (!run_registration_tests) {
      // Establish test defaults without running registration assertions during
      // startup or while unrelated tests create request contexts.
      CefString error;
      registrar->SetDefaultPreference(kPrefTestOverride, CreateBoolValue(false),
                                      error);
      return;
    }

    ASSERT_TRUE(added_override);
    auto original_default = registrar->GetDefaultPreference(kPrefTestOverride);
    ASSERT_TRUE(original_default);
    EXPECT_EQ(VTYPE_BOOL, original_default->GetType());
    EXPECT_TRUE(original_default->GetBool());
    EXPECT_FALSE(registrar->GetDefaultPreference(kPrefTestNoExist));
    CefString error;
    EXPECT_TRUE(registrar->SetDefaultPreference(kPrefTestOverride,
                                                CreateBoolValue(false), error));
    EXPECT_TRUE(error.empty());
    auto overridden_default =
        registrar->GetDefaultPreference(kPrefTestOverride);
    ASSERT_TRUE(overridden_default);
    EXPECT_EQ(VTYPE_BOOL, overridden_default->GetType());
    EXPECT_FALSE(overridden_default->GetBool());
    EXPECT_FALSE(registrar->SetDefaultPreference(kPrefTestOverride,
                                                 CreateIntValue(1), error));
    EXPECT_STREQ("Wrong type for preference: test.override",
                 error.ToString().c_str());
    error.clear();
    EXPECT_FALSE(registrar->SetDefaultPreference(kPrefTestNoExist,
                                                 CreateBoolValue(true), error));
    EXPECT_STREQ("Trying to modify an unregistered preference",
                 error.ToString().c_str());
    error.clear();
    EXPECT_FALSE(registrar->SetDefaultPreference(kPrefTestOverride,
                                                 CefValue::Create(), error));
    EXPECT_STREQ("A valid value is required", error.ToString().c_str());

    EXPECT_TRUE(registrar->SetDefaultPreference(kPrefTestOverride,
                                                original_default, error));
    EXPECT_TRUE(registrar->GetDefaultPreference(kPrefTestOverride)->GetBool());
    EXPECT_TRUE(registrar->SetDefaultPreference(kPrefTestOverride,
                                                CreateBoolValue(false), error));
    EXPECT_TRUE(error.empty());

    if (type == CEF_PREFERENCES_TYPE_REQUEST_CONTEXT) {
      // Registered by a Chrome keyed service, after RegisterUserProfilePrefs.
      EXPECT_FALSE(registrar->AddPreference(kSigninAllowedOnNextStartup,
                                            CreateBoolValue(true)));
      auto signin_default =
          registrar->GetDefaultPreference(kSigninAllowedOnNextStartup);
      ASSERT_TRUE(signin_default);
      EXPECT_EQ(VTYPE_BOOL, signin_default->GetType());
      EXPECT_FALSE(signin_default->GetBool());

      // Applications can opt in before the profile is created.
      EXPECT_TRUE(registrar->SetDefaultPreference(
          kSigninAllowedOnNextStartup, CreateBoolValue(true), error));
      EXPECT_TRUE(error.empty());
      EXPECT_TRUE(registrar->GetDefaultPreference(kSigninAllowedOnNextStartup)
                      ->GetBool());
      EXPECT_TRUE(registrar->SetDefaultPreference(kSigninAllowedOnNextStartup,
                                                  signin_default, error));
      EXPECT_TRUE(error.empty());
      EXPECT_FALSE(registrar->GetDefaultPreference(kSigninAllowedOnNextStartup)
                       ->GetBool());
    }
#endif
  }

 private:
  IMPLEMENT_REFCOUNTING(PreferenceBrowserTest);
};

void ValidateReset(CefRefPtr<CefPreferenceManager> context, const char* name) {
  EXPECT_TRUE(context->HasPreference(name));
  EXPECT_TRUE(context->CanSetPreference(name));

  CefString error;
  EXPECT_TRUE(context->SetPreference(name, nullptr, error));
  EXPECT_TRUE(error.empty());
}

void ValidateBool(CefRefPtr<CefPreferenceManager> context,
                  bool set,
                  bool expected,
                  const char* action,
                  const char* name = kPrefTestBool) {
  EXPECT_TRUE(context->HasPreference(name));
  EXPECT_TRUE(context->CanSetPreference(name));

  if (set) {
    CefString error;
    EXPECT_TRUE(context->SetPreference(name, CreateBoolValue(expected), error));
    EXPECT_TRUE(error.empty());
  }

  auto value = context->GetPreference(name);
  EXPECT_TRUE(value.get());
  EXPECT_EQ(VTYPE_BOOL, value->GetType());
  EXPECT_EQ(expected, value->GetBool()) << action;
}

void ValidateInt(CefRefPtr<CefPreferenceManager> context,
                 bool set,
                 int expected,
                 const char* action,
                 const char* name = kPrefTestInt) {
  EXPECT_TRUE(context->HasPreference(name));
  EXPECT_TRUE(context->CanSetPreference(name));

  if (set) {
    CefString error;
    EXPECT_TRUE(context->SetPreference(name, CreateIntValue(expected), error));
    EXPECT_TRUE(error.empty());
  }

  auto value = context->GetPreference(name);
  EXPECT_TRUE(value.get());
  EXPECT_EQ(VTYPE_INT, value->GetType());
  EXPECT_EQ(expected, value->GetInt()) << action;
}

void ValidateDouble(CefRefPtr<CefPreferenceManager> context,
                    bool set,
                    double expected,
                    const char* action,
                    const char* name = kPrefTestDouble) {
  EXPECT_TRUE(context->HasPreference(name));
  EXPECT_TRUE(context->CanSetPreference(name));

  if (set) {
    CefString error;
    EXPECT_TRUE(
        context->SetPreference(name, CreateDoubleValue(expected), error));
    EXPECT_TRUE(error.empty());
  }

  auto value = context->GetPreference(name);
  EXPECT_TRUE(value.get());
  EXPECT_EQ(VTYPE_DOUBLE, value->GetType());
  EXPECT_EQ(expected, value->GetDouble()) << action;
}

void ValidateString(CefRefPtr<CefPreferenceManager> context,
                    bool set,
                    const std::string& expected,
                    const char* action,
                    const char* name = kPrefTestString) {
  EXPECT_TRUE(context->HasPreference(name));
  EXPECT_TRUE(context->CanSetPreference(name));

  if (set) {
    CefString error;
    EXPECT_TRUE(
        context->SetPreference(name, CreateStringValue(expected), error));
    EXPECT_TRUE(error.empty());
  }

  auto value = context->GetPreference(name);
  EXPECT_TRUE(value.get());
  EXPECT_EQ(VTYPE_STRING, value->GetType());
  EXPECT_STREQ(expected.c_str(), value->GetString().ToString().c_str())
      << action;
}

void ValidateList(CefRefPtr<CefPreferenceManager> context,
                  bool set,
                  CefRefPtr<CefListValue> expected,
                  const char* name = kPrefTestList) {
  EXPECT_TRUE(context->HasPreference(name));
  EXPECT_TRUE(context->CanSetPreference(name));

  if (set) {
    CefString error;
    EXPECT_TRUE(context->SetPreference(name, CreateListValue(expected), error));
    EXPECT_TRUE(error.empty());
  }

  auto value = context->GetPreference(name);
  EXPECT_TRUE(value.get());
  EXPECT_EQ(VTYPE_LIST, value->GetType());
  CefRefPtr<CefListValue> list_val = value->GetList();
  EXPECT_TRUE(list_val);
  TestListEqual(expected, list_val);
}

void ValidateDict(CefRefPtr<CefPreferenceManager> context,
                  bool set,
                  CefRefPtr<CefDictionaryValue> expected,
                  const char* name = kPrefTestDict) {
  EXPECT_TRUE(context->HasPreference(name));
  EXPECT_TRUE(context->CanSetPreference(name));

  if (set) {
    CefString error;
    EXPECT_TRUE(
        context->SetPreference(name, CreateDictionaryValue(expected), error));
    EXPECT_TRUE(error.empty());
  }

  auto value = context->GetPreference(name);
  EXPECT_TRUE(value.get());
  EXPECT_EQ(VTYPE_DICTIONARY, value->GetType());
  CefRefPtr<CefDictionaryValue> dict_val = value->GetDictionary();
  EXPECT_TRUE(dict_val);
  TestDictionaryEqual(expected, dict_val);
}

void ValidateNoExist(CefRefPtr<CefPreferenceManager> context,
                     bool set,
                     const char* action,
                     const char* name = kPrefTestNoExist) {
  EXPECT_FALSE(context->HasPreference(name));
  EXPECT_FALSE(context->CanSetPreference(name));

  if (set) {
    CefString error;
    EXPECT_FALSE(context->SetPreference(name, CreateBoolValue(false), error));
    EXPECT_FALSE(error.empty());
  }

  auto value = context->GetPreference(name);
  EXPECT_FALSE(value.get()) << action;
}

void PopulateRootDefaults(CefRefPtr<CefDictionaryValue> val) {
  // Should match the values in OnRegisterCustomPreferences.
  val->SetBool(kPrefBool, true);
  val->SetInt(kPrefInt, 2);
  val->SetDouble(kPrefDouble, 5.0);
  val->SetString(kPrefString, "default");
  val->SetList(kPrefList, CefListValue::Create());
  val->SetDictionary(kPrefDict, CefDictionaryValue::Create());
#if CEF_API_ADDED(CEF_NEXT)
  val->SetBool(kPrefOverride, false);
#endif
}

void ValidateRoot(CefRefPtr<CefDictionaryValue> root,
                  CefRefPtr<CefDictionaryValue> expected,
                  const char* name = kPrefTest) {
  EXPECT_TRUE(root->HasKey(kPrefTest));
  EXPECT_EQ(VTYPE_DICTIONARY, root->GetType(kPrefTest));

  CefRefPtr<CefDictionaryValue> actual = root->GetDictionary(kPrefTest);
  TestDictionaryEqual(expected, actual);
}

// Validate getting default values.
void ValidateDefaults(CefRefPtr<CefPreferenceManager> context,
                      bool reset,
                      CefRefPtr<CefWaitableEvent> event,
                      const char* action = "") {
  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(TID_UI, base::BindOnce(ValidateDefaults, context, reset, event,
                                       action));
    return;
  }

  if (reset) {
    // Reset default values.
    ValidateReset(context, kPrefTestBool);
    ValidateReset(context, kPrefTestInt);
    ValidateReset(context, kPrefTestDouble);
    ValidateReset(context, kPrefTestString);
    ValidateReset(context, kPrefTestList);
    ValidateReset(context, kPrefTestDict);
  }

  // Test default values.
  // Should match the values in CefBrowserPrefStore::CreateService.
  ValidateBool(context, false, true, action);
  ValidateInt(context, false, 2, action);
  ValidateDouble(context, false, 5.0, action);
  ValidateString(context, false, "default", action);
  ValidateList(context, false, CefListValue::Create());
  ValidateDict(context, false, CefDictionaryValue::Create());
  ValidateNoExist(context, false, action);

  // Expected value of the tests root.
  CefRefPtr<CefDictionaryValue> expected = CefDictionaryValue::Create();
  PopulateRootDefaults(expected);

  // Test all preferences including defaults.
  ValidateRoot(context->GetAllPreferences(true), expected);

  // Test all preferences excluding defaults.
  EXPECT_FALSE(context->GetAllPreferences(false)->HasKey(kPrefTest));

  event->Signal();
}

void PopulateListValue(CefRefPtr<CefListValue> val) {
  // Test list values.
  val->SetInt(0, 54);
  val->SetString(1, "foobar");
  val->SetDouble(2, 99.7643);
}

void PopulateDictValue(CefRefPtr<CefDictionaryValue> val) {
  // Test dictionary values.
  val->SetString("key1", "some string");
  val->SetBool("key2", false);

  CefRefPtr<CefListValue> list_val = CefListValue::Create();
  PopulateListValue(list_val);
  val->SetList("key3", list_val);
}

void PopulateRootSet(CefRefPtr<CefDictionaryValue> val) {
  CefRefPtr<CefListValue> list_val = CefListValue::Create();
  CefRefPtr<CefDictionaryValue> dict_val = CefDictionaryValue::Create();

  PopulateListValue(list_val);
  PopulateDictValue(dict_val);

  // Should match the values in ValidateSetGet and ValidateGet.
  val->SetBool(kPrefBool, true);
  val->SetInt(kPrefInt, 65);
  val->SetDouble(kPrefDouble, 54.5443);
  val->SetString(kPrefString, "My test string");
  val->SetList(kPrefList, list_val);
  val->SetDictionary(kPrefDict, dict_val);
#if CEF_API_ADDED(CEF_NEXT)
  val->SetBool(kPrefOverride, false);
#endif
}

// Validate getting and setting values.
void ValidateSetGet(CefRefPtr<CefPreferenceManager> context,
                    CefRefPtr<CefWaitableEvent> event,
                    const char* action = "") {
  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(TID_UI, base::BindOnce(ValidateSetGet, context, event, action));
    return;
  }

  CefRefPtr<CefListValue> list_val = CefListValue::Create();
  CefRefPtr<CefDictionaryValue> dict_val = CefDictionaryValue::Create();

  PopulateListValue(list_val);
  PopulateDictValue(dict_val);

  // Test setting/getting values.
  // Should match the values in PopulateRootSet and ValidateGet.
  ValidateBool(context, true, true, action);
  ValidateInt(context, true, 65, action);
  ValidateDouble(context, true, 54.5443, action);
  ValidateString(context, true, "My test string", action);
  ValidateList(context, true, list_val);
  ValidateDict(context, true, dict_val);
  ValidateNoExist(context, true, action);

  // Expected value of the tests root.
  CefRefPtr<CefDictionaryValue> expected = CefDictionaryValue::Create();
  PopulateRootSet(expected);

  // Validate all preferences including defaults.
  ValidateRoot(context->GetAllPreferences(true), expected);

  // Validate all preferences excluding defaults.
#if CEF_API_ADDED(CEF_NEXT)
  expected->Remove(kPrefOverride);
#endif
  ValidateRoot(context->GetAllPreferences(false), expected);

  event->Signal();
}

// Validate getting values.
void ValidateGet(CefRefPtr<CefPreferenceManager> context,
                 CefRefPtr<CefWaitableEvent> event,
                 const char* action) {
  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(TID_UI, base::BindOnce(ValidateGet, context, event, action));
    return;
  }

  CefRefPtr<CefListValue> list_val = CefListValue::Create();
  CefRefPtr<CefDictionaryValue> dict_val = CefDictionaryValue::Create();

  PopulateListValue(list_val);
  PopulateDictValue(dict_val);

  // Test getting values.
  // Should match the values in PopulateRootSet and ValidateSetGet.
  ValidateBool(context, false, true, action);
  ValidateInt(context, false, 65, action);
  ValidateDouble(context, false, 54.5443, action);
  ValidateString(context, false, "My test string", action);
  ValidateList(context, false, list_val);
  ValidateDict(context, false, dict_val);
  ValidateNoExist(context, false, action);

  // Expected value of the tests root.
  CefRefPtr<CefDictionaryValue> expected = CefDictionaryValue::Create();
  PopulateRootSet(expected);

  // Validate all preferences including defaults.
  ValidateRoot(context->GetAllPreferences(true), expected);

  // Validate all preferences excluding defaults.
#if CEF_API_ADDED(CEF_NEXT)
  expected->Remove(kPrefOverride);
#endif
  ValidateRoot(context->GetAllPreferences(false), expected);

  event->Signal();
}

// No-op implementation.
class TestRequestContextHandler : public CefRequestContextHandler {
 public:
  TestRequestContextHandler() = default;
  explicit TestRequestContextHandler(CefRefPtr<CefWaitableEvent> event)
      : event_(event) {}

  void OnRequestContextInitialized(
      CefRefPtr<CefRequestContext> context) override {
    if (event_) {
      event_->Signal();
      event_ = nullptr;
    }
  }

 private:
  CefRefPtr<CefWaitableEvent> event_;

  IMPLEMENT_REFCOUNTING(TestRequestContextHandler);
};

}  // namespace

// Verify default preference values on the global state.
TEST(PreferenceTest, GlobalDefaults) {
  CefRefPtr<CefWaitableEvent> event =
      CefWaitableEvent::CreateWaitableEvent(true, false);

  auto context = CefPreferenceManager::GetGlobalPreferenceManager();
  EXPECT_TRUE(context.get());

  ValidateDefaults(context, false, event);
  event->Wait();
}

// Verify setting/getting preference values on the global state.
TEST(PreferenceTest, GlobalSetGet) {
  CefRefPtr<CefWaitableEvent> event =
      CefWaitableEvent::CreateWaitableEvent(true, false);

  auto context = CefPreferenceManager::GetGlobalPreferenceManager();
  EXPECT_TRUE(context.get());

  ValidateSetGet(context, event);
  event->Wait();

  // Reset to the default values.
  ValidateDefaults(context, true, event);
  event->Wait();
}

// Verify default preference values on the global request context.
TEST(PreferenceTest, RequestContextGlobalDefaults) {
  CefRefPtr<CefWaitableEvent> event =
      CefWaitableEvent::CreateWaitableEvent(true, false);

  CefRefPtr<CefRequestContext> context = CefRequestContext::GetGlobalContext();
  EXPECT_TRUE(context.get());

  ValidateDefaults(context, false, event);
  event->Wait();
}

// Verify setting/getting preference values on the global request context.
TEST(PreferenceTest, RequestContextGlobalSetGet) {
  CefRefPtr<CefWaitableEvent> event =
      CefWaitableEvent::CreateWaitableEvent(true, false);

  CefRefPtr<CefRequestContext> context = CefRequestContext::GetGlobalContext();
  EXPECT_TRUE(context.get());

  ValidateSetGet(context, event);
  event->Wait();

  // Reset to the default values.
  ValidateDefaults(context, true, event);
  event->Wait();
}

// Verify setting/getting preference values on shared global request contexts.
TEST(PreferenceTest, RequestContextGlobalSetGetShared) {
  CefRefPtr<CefWaitableEvent> event =
      CefWaitableEvent::CreateWaitableEvent(true, false);

  CefRefPtr<CefRequestContext> context = CefRequestContext::GetGlobalContext();
  EXPECT_TRUE(context.get());

  // Sharing storage.
  CefRefPtr<CefRequestContext> context2 =
      CefRequestContext::CreateContext(context, nullptr);
  EXPECT_TRUE(context2.get());

  // Sharing storage.
  CefRefPtr<CefRequestContext> context3 =
      CefRequestContext::CreateContext(context, new TestRequestContextHandler);
  EXPECT_TRUE(context3.get());

  // Unassociated context.
  CefRequestContextSettings settings;
  CefRefPtr<CefRequestContext> context4 = CefRequestContext::CreateContext(
      settings, new TestRequestContextHandler(event));
  EXPECT_TRUE(context4.get());
  // Wait for the context to be fully initialized.
  event->Wait();

  // Set/get the values on the first context.
  ValidateSetGet(context, event, "Set/get the values on the first context");
  event->Wait();

  // Get the values from the 2nd and 3rd contexts. They should be the same.
  ValidateGet(context2, event, "Get the values from the 2nd context.");
  event->Wait();
  ValidateGet(context3, event, "Get the values from the 3rd context.");
  event->Wait();

  // Get the values from the 4th context.
  // Prefs set via an incognito profile will become an overlay on top of the
  // global (parent) profile. The incognito profile shares the prefs in this
  // case because they were set via the global profile.
  ValidateGet(context4, event, "Get the values from the 4th context.");

  event->Wait();

  // Reset to the default values.
  ValidateDefaults(context, true, event, "Reset to the default values.");
  event->Wait();
}

// Verify default preference values on a custom request context.
TEST(PreferenceTest, RequestContextCustomDefaults) {
  CefRefPtr<CefWaitableEvent> event =
      CefWaitableEvent::CreateWaitableEvent(true, false);

  CefRequestContextSettings settings;
  CefRefPtr<CefRequestContext> context = CefRequestContext::CreateContext(
      settings, new TestRequestContextHandler(event));
  EXPECT_TRUE(context.get());
  // Wait for the context to be fully initialized.
  event->Wait();

  ValidateDefaults(context, false, event);
  event->Wait();
}

// Verify setting/getting preference values on a custom request context.
TEST(PreferenceTest, RequestContextCustomSetGet) {
  CefRefPtr<CefWaitableEvent> event =
      CefWaitableEvent::CreateWaitableEvent(true, false);

  CefRequestContextSettings settings;
  CefRefPtr<CefRequestContext> context = CefRequestContext::CreateContext(
      settings, new TestRequestContextHandler(event));
  EXPECT_TRUE(context.get());
  // Wait for the context to be fully initialized.
  event->Wait();

  ValidateSetGet(context, event);
  event->Wait();

  // Reset to the default values.
  ValidateDefaults(context, true, event);
  event->Wait();
}

// Verify setting/getting preference values on shared custom request contexts.
TEST(PreferenceTest, RequestContextCustomSetGetShared) {
  CefRefPtr<CefWaitableEvent> event =
      CefWaitableEvent::CreateWaitableEvent(true, false);

  CefRequestContextSettings settings;
  CefRefPtr<CefRequestContext> context = CefRequestContext::CreateContext(
      settings, new TestRequestContextHandler(event));
  EXPECT_TRUE(context.get());
  // Wait for the context to be fully initialized.
  event->Wait();

  // Sharing storage.
  CefRefPtr<CefRequestContext> context2 =
      CefRequestContext::CreateContext(context, nullptr);
  EXPECT_TRUE(context2.get());

  // Sharing storage.
  CefRefPtr<CefRequestContext> context3 =
      CefRequestContext::CreateContext(context, new TestRequestContextHandler);
  EXPECT_TRUE(context3.get());

  // Unassociated context.
  CefRefPtr<CefRequestContext> context4 = CefRequestContext::CreateContext(
      settings, new TestRequestContextHandler(event));
  EXPECT_TRUE(context4.get());
  // Wait for the context to be fully initialized.
  event->Wait();

  // Set/get the values on the first context.
  ValidateSetGet(context, event, "Set/get the values on the first context");
  event->Wait();

  // Get the values from the 2nd and 3d contexts. They should be the same.
  ValidateGet(context2, event, "Get the values from the 2nd context.");
  event->Wait();
  ValidateGet(context3, event, "Get the values from the 3rd context.");
  event->Wait();

  // Get the values from the 4th context. They should be at the default.
  // This works with Chrome style because the preference changes only
  // exist in the other incognito profile's overlay.
  ValidateDefaults(context4, false, event,
                   "Get the values from the 4th context.");
  event->Wait();

  // Reset to the default values.
  ValidateDefaults(context, true, event, "Reset to the default values.");
  event->Wait();
}

#if CEF_API_ADDED(CEF_NEXT)
void ValidateOverriddenDefault(CefRefPtr<CefPreferenceManager> context,
                               CefRefPtr<CefWaitableEvent> event) {
  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(TID_UI,
                base::BindOnce(ValidateOverriddenDefault, context, event));
    return;
  }

  // Verify the default set by OnRegisterCustomPreferences is the effective
  // value before a user preference is set.
  auto value = context->GetPreference(kPrefTestOverride);
  EXPECT_TRUE(value);
  if (!value) {
    event->Signal();
    return;
  }
  EXPECT_EQ(VTYPE_BOOL, value->GetType());
  EXPECT_FALSE(value->GetBool());

  CefString error;
  EXPECT_TRUE(
      context->SetPreference(kPrefTestOverride, CreateBoolValue(true), error));
  EXPECT_TRUE(error.empty());
  EXPECT_TRUE(context->GetPreference(kPrefTestOverride)->GetBool());
  EXPECT_TRUE(context->SetPreference(kPrefTestOverride, nullptr, error));
  EXPECT_FALSE(context->GetPreference(kPrefTestOverride)->GetBool());
  event->Signal();
}

TEST(PreferenceTest, OverriddenGlobalDefault) {
  auto event = CefWaitableEvent::CreateWaitableEvent(true, false);
  ValidateOverriddenDefault(CefPreferenceManager::GetGlobalPreferenceManager(),
                            event);
  event->Wait();
}

TEST(PreferenceTest, OverriddenRequestContextDefault) {
  // An in-memory context shares an existing profile's registered defaults.
  // Use a fresh profile so this test runs the registration-time assertions.
  CefScopedTempDir tempdir;
  ASSERT_TRUE(tempdir.CreateUniqueTempDirUnderPath(
      CefTestSuite::GetInstance()->root_cache_path()));
  // Take ownership so the directory isn't deleted while the profile is still
  // initializing on-disk storage (e.g. the WebDatabase) in the background. It
  // will be cleaned up when the root_cache_path is deleted during test
  // shutdown. See https://crbug.com/416755456.
  const std::string cache_path = tempdir.Take();
  auto init_event = CefWaitableEvent::CreateWaitableEvent(true, false);
  CefRequestContextSettings settings;
  CefString(&settings.cache_path) = cache_path;
  auto context = CefRequestContext::CreateContext(
      settings, new TestRequestContextHandler(init_event));
  init_event->Wait();
  auto event = CefWaitableEvent::CreateWaitableEvent(true, false);
  ValidateOverriddenDefault(context, event);
  event->Wait();
}

void ValidateSigninDisabled(CefRefPtr<CefRequestContext> context,
                            CefRefPtr<CefWaitableEvent> event) {
  if (!CefCurrentlyOn(TID_UI)) {
    CefPostTask(TID_UI, base::BindOnce(ValidateSigninDisabled, context, event));
    return;
  }

  // The registration default is applied before account consistency initializes.
  auto next_startup = context->GetPreference(kSigninAllowedOnNextStartup);
  EXPECT_TRUE(next_startup);
  if (next_startup) {
    EXPECT_EQ(VTYPE_BOOL, next_startup->GetType());
    EXPECT_FALSE(next_startup->GetBool());
  }

  auto allowed = context->GetPreference(kSigninAllowed);
  EXPECT_TRUE(allowed);
  if (allowed) {
    EXPECT_EQ(VTYPE_BOOL, allowed->GetType());
    EXPECT_FALSE(allowed->GetBool());
  }
  event->Signal();
}

TEST(PreferenceTest, RequestContextSigninDisabledByDefault) {
  auto event = CefWaitableEvent::CreateWaitableEvent(true, false);
  ValidateSigninDisabled(CefRequestContext::GetGlobalContext(), event);
  event->Wait();
}
#endif  // CEF_API_ADDED(CEF_NEXT)

// Entry point for creating preference browser test objects.
// Called from client_app_delegates.cc.
void CreatePreferenceBrowserTests(
    client::ClientAppBrowser::DelegateSet& delegates) {
  delegates.insert(new PreferenceBrowserTest);
}
