// Copyright (c) 2015 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/chrome_profile_stub.h"

#include "components/variations/variations_client.h"
#include "components/variations/variations_http_header_provider.h"
#include "content/public/browser/resource_context.h"
#include "net/url_request/url_request_context.h"

namespace {

class CefVariationsClient : public variations::VariationsClient {
 public:
  explicit CefVariationsClient(content::BrowserContext* browser_context)
      : browser_context_(browser_context) {}

  ~CefVariationsClient() override = default;

  bool IsIncognito() const override {
    return browser_context_->IsOffTheRecord();
  }

  std::string GetVariationsHeader() const override {
    return variations::VariationsHttpHeaderProvider::GetInstance()
        ->GetClientDataHeader(false /* is_signed_in */);
  }

 private:
  content::BrowserContext* browser_context_;
};

}  // namespace

ChromeProfileStub::ChromeProfileStub() {}

ChromeProfileStub::~ChromeProfileStub() {}

bool ChromeProfileStub::IsOffTheRecord() {
  return false;
}

bool ChromeProfileStub::IsOffTheRecord() const {
  return false;
}

variations::VariationsClient* ChromeProfileStub::GetVariationsClient() {
  if (!variations_client_)
    variations_client_ = std::make_unique<CefVariationsClient>(this);
  return variations_client_.get();
}

scoped_refptr<base::SequencedTaskRunner> ChromeProfileStub::GetIOTaskRunner() {
  NOTREACHED();
  return scoped_refptr<base::SequencedTaskRunner>();
}

std::string ChromeProfileStub::GetProfileUserName() const {
  NOTREACHED();
  return std::string();
}

Profile::ProfileType ChromeProfileStub::GetProfileType() const {
  return REGULAR_PROFILE;
}

Profile* ChromeProfileStub::GetOffTheRecordProfile() {
  NOTREACHED();
  return nullptr;
}

void ChromeProfileStub::DestroyOffTheRecordProfile() {
  NOTREACHED();
}

bool ChromeProfileStub::HasOffTheRecordProfile() {
  return false;
}

Profile* ChromeProfileStub::GetOriginalProfile() {
  return this;
}

const Profile* ChromeProfileStub::GetOriginalProfile() const {
  return this;
}

bool ChromeProfileStub::IsSupervised() const {
  return false;
}

bool ChromeProfileStub::IsChild() const {
  return false;
}

bool ChromeProfileStub::IsLegacySupervised() const {
  return false;
}

ExtensionSpecialStoragePolicy*
ChromeProfileStub::GetExtensionSpecialStoragePolicy() {
  NOTREACHED();
  return nullptr;
}

PrefService* ChromeProfileStub::GetOffTheRecordPrefs() {
  NOTREACHED();
  return nullptr;
}

bool ChromeProfileStub::IsSameProfile(Profile* profile) {
  NOTREACHED();
  return false;
}

base::Time ChromeProfileStub::GetStartTime() const {
  NOTREACHED();
  return base::Time();
}

base::FilePath ChromeProfileStub::last_selected_directory() {
  NOTREACHED();
  return base::FilePath();
}

void ChromeProfileStub::set_last_selected_directory(
    const base::FilePath& path) {
  NOTREACHED();
}

GURL ChromeProfileStub::GetHomePage() {
  NOTREACHED();
  return GURL();
}

bool ChromeProfileStub::WasCreatedByVersionOrLater(const std::string& version) {
  NOTREACHED();
  return false;
}

bool ChromeProfileStub::IsIndependentOffTheRecordProfile() {
  return false;
}

void ChromeProfileStub::SetExitType(ExitType exit_type) {
  NOTREACHED();
}

Profile::ExitType ChromeProfileStub::GetLastSessionExitType() {
  NOTREACHED();
  return EXIT_NORMAL;
}

base::Time ChromeProfileStub::GetCreationTime() const {
  NOTREACHED();
  return base::Time();
}

void ChromeProfileStub::SetCreationTimeForTesting(base::Time creation_time) {
  NOTREACHED();
}
