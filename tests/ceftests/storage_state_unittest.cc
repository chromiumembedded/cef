// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "base/files/file_util.h"
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

class TestStorageStateSuccessReadCallback : public CefStorageStateReadCallback {
 public:
  explicit TestStorageStateSuccessReadCallback(CefRefPtr<CefWaitableEvent> event)
      : event_(event) {}

  void OnComplete(bool success,
                  const CefString& error,
                  CefRefPtr<CefDictionaryValue> result) override {
    EXPECT_TRUE(success);
    EXPECT_TRUE(error.empty());
    ASSERT_TRUE(result);
    EXPECT_FALSE(result->GetString("filename").empty());
    EXPECT_FALSE(result->GetString("path").empty());
    EXPECT_TRUE(result->HasKey("size"));
    EXPECT_TRUE(result->HasKey("modified"));
    EXPECT_STREQ("Storage state scaffolding is not yet implemented.",
                 result->GetString("summary").ToString().c_str());
    event_->Signal();
  }

 private:
  CefRefPtr<CefWaitableEvent> event_;

  IMPLEMENT_REFCOUNTING(TestStorageStateSuccessReadCallback);
};

class TestStorageStateActionCallback : public CefStorageStateActionCallback {
 public:
  TestStorageStateActionCallback(CefRefPtr<CefWaitableEvent> event,
                                 bool expected_success,
                                 const char* expected_error)
      : event_(event),
        expected_success_(expected_success),
        expected_error_(expected_error) {}

  void OnComplete(bool success,
                  const CefString& error,
                  const CefString& path) override {
    EXPECT_EQ(expected_success_, success);
    EXPECT_STREQ(expected_error_.c_str(), error.ToString().c_str());
    if (expected_success_) {
      EXPECT_FALSE(path.empty());
    }
    event_->Signal();
  }

 private:
  CefRefPtr<CefWaitableEvent> event_;
  const bool expected_success_;
  const std::string expected_error_;

  IMPLEMENT_REFCOUNTING(TestStorageStateActionCallback);
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
  auto list_event = CefWaitableEvent::CreateWaitableEvent(true, false);
  auto show_success_event = CefWaitableEvent::CreateWaitableEvent(true, false);
  auto show_missing_event = CefWaitableEvent::CreateWaitableEvent(true, false);
  auto show_invalid_event = CefWaitableEvent::CreateWaitableEvent(true, false);
  auto save_event = CefWaitableEvent::CreateWaitableEvent(true, false);
  auto load_event = CefWaitableEvent::CreateWaitableEvent(true, false);

  CefPostTask(
      TID_UI, base::BindOnce(
                  [](CefRefPtr<CefWaitableEvent> list_event,
                     CefRefPtr<CefWaitableEvent> show_success_event,
                     CefRefPtr<CefWaitableEvent> show_missing_event,
                     CefRefPtr<CefWaitableEvent> show_invalid_event,
                     CefRefPtr<CefWaitableEvent> save_event,
                     CefRefPtr<CefWaitableEvent> load_event) {
                    auto context = CefRequestContext::GetGlobalContext();
                    ASSERT_TRUE(context.get());

                    auto manager = context->GetStorageStateManager(nullptr);
                    ASSERT_TRUE(manager.get());

                    manager->List(new TestStorageStateListCallback(list_event));

                    const auto default_path =
                        base::FilePath(manager->GetDefaultStatePath().ToString());
                    ASSERT_FALSE(default_path.empty());
                    ASSERT_TRUE(base::CreateDirectory(default_path.DirName()));
                    ASSERT_TRUE(base::WriteFile(default_path, "{}"));

                    manager->Show(default_path.BaseName().AsUTF8Unsafe(),
                                  new TestStorageStateSuccessReadCallback(
                                      show_success_event));

                    const auto missing_path = default_path.DirName().AppendASCII(
                        "missing-storage-state.json");
                    manager->Show(missing_path.BaseName().AsUTF8Unsafe(),
                                  new TestStorageStateReadCallback(
                                      show_missing_event,
                                      "State file does not exist."));

                    manager->Show("/tmp/cef-storage-state-does-not-exist.json",
                                  new TestStorageStateReadCallback(
                                      show_invalid_event,
                                      "State path must reference a managed session file."));

                    manager->Save(
                        "../escape.json", nullptr,
                        new TestStorageStateActionCallback(
                            save_event, false,
                            "State path must reference a managed session file."));
                    manager->Load(
                        "../escape.json", nullptr,
                        new TestStorageStateActionCallback(
                            load_event, false,
                            "State path must reference a managed session file."));
                  },
                  list_event, show_success_event, show_missing_event,
                  show_invalid_event, save_event, load_event));

  list_event->Wait();
  show_success_event->Wait();
  show_missing_event->Wait();
  show_invalid_event->Wait();
  save_event->Wait();
  load_event->Wait();
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
