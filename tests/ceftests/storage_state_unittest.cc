// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/base/cef_callback.h"
#include "include/cef_browser_security.h"
#include "include/cef_request_context.h"
#include "include/cef_storage_state.h"
#include "include/cef_task.h"
#include "include/cef_waitable_event.h"
#include "tests/gtest/include/gtest/gtest.h"

namespace {

class TestStorageStateListCallback : public CefStorageStateListCallback {
 public:
  explicit TestStorageStateListCallback(CefRefPtr<CefWaitableEvent> event)
      : event_(event) {}

  void OnComplete(bool success,
                  const CefString& error,
                  CefRefPtr<CefListValue> entries,
                  const CefString& directory) override {
    EXPECT_TRUE(success);
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(entries.get());
    EXPECT_FALSE(directory.empty());
    event_->Signal();
  }

 private:
  CefRefPtr<CefWaitableEvent> event_;

  IMPLEMENT_REFCOUNTING(TestStorageStateListCallback);
};

class TestStorageStateReadCallback : public CefStorageStateReadCallback {
 public:
  TestStorageStateReadCallback(CefRefPtr<CefWaitableEvent> event,
                               const char* expected_error)
      : event_(event), expected_error_(expected_error) {}

  void OnComplete(bool success,
                  const CefString& error,
                  CefRefPtr<CefDictionaryValue> result) override {
    EXPECT_FALSE(success);
    EXPECT_STREQ(expected_error_.c_str(), error.ToString().c_str());
    EXPECT_FALSE(result.get());
    event_->Signal();
  }

 private:
  CefRefPtr<CefWaitableEvent> event_;
  const std::string expected_error_;

  IMPLEMENT_REFCOUNTING(TestStorageStateReadCallback);
};

class TestSecurityCallback : public CefBrowserSecurityCallback {
 public:
  explicit TestSecurityCallback(CefRefPtr<CefWaitableEvent> event)
      : event_(event) {}

  void OnComplete(bool success, const CefString& message) override {
    EXPECT_TRUE(success);
    EXPECT_FALSE(message.empty());
    event_->Signal();
  }

 private:
  CefRefPtr<CefWaitableEvent> event_;

  IMPLEMENT_REFCOUNTING(TestSecurityCallback);
};

}  // namespace

TEST(StorageStateTest, RequestContextManagerAccessAndDefaults) {
  auto event = CefWaitableEvent::CreateWaitableEvent(true, false);
  CefPostTask(
      TID_UI, base::BindOnce(
                  [](CefRefPtr<CefWaitableEvent> event) {
                    auto context = CefRequestContext::GetGlobalContext();
                    ASSERT_TRUE(context.get());

                    auto manager = context->GetStorageStateManager(nullptr);
                    ASSERT_TRUE(manager.get());

                    EXPECT_TRUE(manager->GetSessionName().empty());
                    EXPECT_FALSE(manager->GetDefaultStateDirectory().empty());
                    EXPECT_FALSE(manager->GetDefaultStatePath().empty());

                    manager->SetSessionName("scaffold-session");
                    EXPECT_STREQ("scaffold-session",
                                 manager->GetSessionName().ToString().c_str());
                    EXPECT_NE(std::string::npos,
                              manager->GetDefaultStatePath()
                                  .ToString()
                                  .find("scaffold-session"));
                    event->Signal();
                  },
                  event));
  event->Wait();
}

TEST(StorageStateTest, RequestContextListAndShowScaffoldBehavior) {
  auto context = CefRequestContext::GetGlobalContext();
  ASSERT_TRUE(context.get());

  auto manager = context->GetStorageStateManager(nullptr);
  ASSERT_TRUE(manager.get());

  auto list_event = CefWaitableEvent::CreateWaitableEvent(true, false);
  manager->List(new TestStorageStateListCallback(list_event));
  list_event->Wait();

  auto show_event = CefWaitableEvent::CreateWaitableEvent(true, false);
  manager->Show("/tmp/cef-storage-state-does-not-exist.json",
                new TestStorageStateReadCallback(
                    show_event, "State file does not exist."));
  show_event->Wait();
}

TEST(StorageStateTest, BrowserSecurityPolicyRoundTrip) {
  auto context = CefRequestContext::GetGlobalContext();
  ASSERT_TRUE(context.get());

  auto policy = context->GetSecurityPolicy();
  ASSERT_TRUE(policy.get());

  CefBrowserSecuritySettings settings = {};
  settings.content_boundaries = true;
  settings.max_output_chars = 4096;
  CefString(&settings.allowed_domains) = "example.com,*.example.com";
  CefString(&settings.action_policy_path) = "/tmp/policy.json";
  CefString(&settings.confirm_actions) = "eval,download";
  settings.confirm_interactive = true;

  auto callback_event = CefWaitableEvent::CreateWaitableEvent(true, false);
  policy->SetSettings(settings, new TestSecurityCallback(callback_event));
  callback_event->Wait();

  CefBrowserSecuritySettings actual = {};
  policy->GetSettings(actual);
  EXPECT_TRUE(!!actual.content_boundaries);
  EXPECT_EQ(4096, actual.max_output_chars);
  EXPECT_STREQ("example.com,*.example.com",
               CefString(&actual.allowed_domains).ToString().c_str());
  EXPECT_STREQ("/tmp/policy.json",
               CefString(&actual.action_policy_path).ToString().c_str());
  EXPECT_STREQ("eval,download",
               CefString(&actual.confirm_actions).ToString().c_str());
  EXPECT_TRUE(!!actual.confirm_interactive);
  EXPECT_STREQ("example.com,*.example.com",
               policy->GetAllowedDomains().ToString().c_str());
  EXPECT_STREQ("eval,download",
               policy->GetConfirmActionCategories().ToString().c_str());
}
