// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "base/files/file_util.h"
#include "include/cef_auth_vault.h"
#include "include/cef_parser.h"
#include "include/cef_waitable_event.h"
#include "tests/gtest/include/gtest/gtest.h"

namespace {

class VaultActionCallback : public CefAuthVaultActionCallback {
 public:
  explicit VaultActionCallback(CefRefPtr<CefWaitableEvent> event)
      : event_(event) {}

  void OnComplete(bool success,
                  const CefString& path,
                  const CefString& error) override {
    success_ = success;
    path_ = path;
    error_ = error;
    event_->Signal();
  }

  bool success() const { return success_; }
  const CefString& path() const { return path_; }
  const CefString& error() const { return error_; }

 private:
  CefRefPtr<CefWaitableEvent> event_;
  bool success_ = false;
  CefString path_;
  CefString error_;

  IMPLEMENT_REFCOUNTING(VaultActionCallback);
};

class VaultReadCallback : public CefAuthVaultReadCallback {
 public:
  explicit VaultReadCallback(CefRefPtr<CefWaitableEvent> event)
      : event_(event) {}

  void OnComplete(bool success,
                  CefRefPtr<CefDictionaryValue> profile,
                  const CefString& error) override {
    success_ = success;
    profile_ = profile;
    error_ = error;
    event_->Signal();
  }

  bool success() const { return success_; }
  CefRefPtr<CefDictionaryValue> profile() const { return profile_; }
  const CefString& error() const { return error_; }

 private:
  CefRefPtr<CefWaitableEvent> event_;
  bool success_ = false;
  CefRefPtr<CefDictionaryValue> profile_;
  CefString error_;

  IMPLEMENT_REFCOUNTING(VaultReadCallback);
};

}  // namespace

TEST(AuthVaultTest, BasicAccessors) {
  CefRefPtr<CefAuthVault> vault = CefAuthVault::GetGlobalVault();
  ASSERT_TRUE(vault);
  EXPECT_FALSE(vault->GetVaultPath().empty());
  EXPECT_FALSE(vault->GetEncryptionKeyPath().empty());
}

TEST(AuthVaultTest, SaveReadDeleteProfile) {
  CefRefPtr<CefAuthVault> vault = CefAuthVault::GetGlobalVault();
  ASSERT_TRUE(vault);

  const CefString name("cef-auth-vault-test-profile");

  CefRefPtr<CefDictionaryValue> profile = CefDictionaryValue::Create();
  profile->SetString("name", name);
  profile->SetString("url", "https://example.com/login");
  profile->SetString("username", "user");
  profile->SetString("password", "secret");
  profile->SetString("username_selector", "#email");

  auto save_event = CefWaitableEvent::CreateWaitableEvent(true, false);
  CefRefPtr<VaultActionCallback> save_callback =
      new VaultActionCallback(save_event);
  vault->SaveProfile(profile, save_callback);
  save_event->Wait();
  EXPECT_TRUE(save_callback->success());
  EXPECT_FALSE(save_callback->path().empty());
  EXPECT_TRUE(base::PathExists(base::FilePath(save_callback->path().ToString())));

  std::string encrypted_payload;
  EXPECT_TRUE(base::ReadFileToString(
      base::FilePath(save_callback->path().ToString()), &encrypted_payload));
  CefRefPtr<CefValue> payload_value =
      CefParseJSON(encrypted_payload, JSON_PARSER_RFC);
  ASSERT_TRUE(payload_value.get());
  CefRefPtr<CefDictionaryValue> payload = payload_value->GetDictionary();
  ASSERT_TRUE(payload.get());
  EXPECT_TRUE(payload->GetBool("encrypted"));
  EXPECT_EQ(1, payload->GetInt("version"));
  EXPECT_FALSE(payload->GetString("iv").empty());
  EXPECT_FALSE(payload->GetString("authTag").empty());
  EXPECT_FALSE(payload->GetString("data").empty());
  EXPECT_TRUE(
      base::PathExists(base::FilePath(vault->GetEncryptionKeyPath().ToString())));

  auto read_event = CefWaitableEvent::CreateWaitableEvent(true, false);
  CefRefPtr<VaultReadCallback> read_callback =
      new VaultReadCallback(read_event);
  vault->ReadProfile(name, read_callback);
  read_event->Wait();
  ASSERT_TRUE(read_callback->success());
  ASSERT_TRUE(read_callback->profile());
  EXPECT_STREQ(name.ToString().c_str(),
               read_callback->profile()->GetString("name").ToString().c_str());
  EXPECT_STREQ(
      "user",
      read_callback->profile()->GetString("username").ToString().c_str());
  EXPECT_STREQ("https://example.com/login",
               read_callback->profile()->GetString("url").ToString().c_str());
  EXPECT_TRUE(read_callback->profile()->GetBool("encrypted"));
  EXPECT_FALSE(read_callback->profile()->GetString("path").empty());

  auto delete_event = CefWaitableEvent::CreateWaitableEvent(true, false);
  CefRefPtr<VaultActionCallback> delete_callback =
      new VaultActionCallback(delete_event);
  vault->DeleteProfile(name, delete_callback);
  delete_event->Wait();
  EXPECT_TRUE(delete_callback->success());
}
