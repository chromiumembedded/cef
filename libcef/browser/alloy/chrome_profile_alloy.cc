// Copyright (c) 2015 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/alloy/chrome_profile_alloy.h"

#include "base/no_destructor.h"
#include "components/profile_metrics/browser_profile_type.h"
#include "components/variations/variations_client.h"
#include "components/variations/variations_ids_provider.h"
#include "net/url_request/url_request_context.h"

namespace {

class CefVariationsClient : public variations::VariationsClient {
 public:
  explicit CefVariationsClient(content::BrowserContext* browser_context)
      : browser_context_(browser_context) {}

  ~CefVariationsClient() override = default;

  bool IsOffTheRecord() const override {
    return browser_context_->IsOffTheRecord();
  }

  variations::mojom::VariationsHeadersPtr GetVariationsHeaders()
      const override {
    return variations::VariationsIdsProvider::GetInstance()
        ->GetClientDataHeaders(false /* is_signed_in */);
  }

 private:
  content::BrowserContext* browser_context_;
};

}  // namespace

ChromeProfileAlloy::ChromeProfileAlloy() {
  profile_metrics::SetBrowserProfileType(
      this, profile_metrics::BrowserProfileType::kRegular);
}

ChromeProfileAlloy::~ChromeProfileAlloy() = default;

bool ChromeProfileAlloy::IsOffTheRecord() {
  return false;
}

bool ChromeProfileAlloy::IsOffTheRecord() const {
  // Alloy contexts are never flagged as off-the-record. It causes problems
  // for the extension system.
  return false;
}

const Profile::OTRProfileID& ChromeProfileAlloy::GetOTRProfileID() const {
  DCHECK(false);
  static base::NoDestructor<Profile::OTRProfileID> otr_profile_id(
      Profile::OTRProfileID::PrimaryID());
  return *otr_profile_id;
}

variations::VariationsClient* ChromeProfileAlloy::GetVariationsClient() {
  if (!variations_client_) {
    variations_client_ = std::make_unique<CefVariationsClient>(this);
  }
  return variations_client_.get();
}

scoped_refptr<base::SequencedTaskRunner> ChromeProfileAlloy::GetIOTaskRunner() {
  DCHECK(false);
  return scoped_refptr<base::SequencedTaskRunner>();
}

std::string ChromeProfileAlloy::GetProfileUserName() const {
  DCHECK(false);
  return std::string();
}

Profile* ChromeProfileAlloy::GetOffTheRecordProfile(
    const Profile::OTRProfileID& otr_profile_id,
    bool create_if_needed) {
  DCHECK(false);
  return nullptr;
}

std::vector<Profile*> ChromeProfileAlloy::GetAllOffTheRecordProfiles() {
  return {};
}

void ChromeProfileAlloy::DestroyOffTheRecordProfile(Profile* otr_profile) {
  DCHECK(false);
}

bool ChromeProfileAlloy::HasOffTheRecordProfile(
    const Profile::OTRProfileID& otr_profile_id) {
  return false;
}

bool ChromeProfileAlloy::HasAnyOffTheRecordProfile() {
  return false;
}

Profile* ChromeProfileAlloy::GetOriginalProfile() {
  return this;
}

const Profile* ChromeProfileAlloy::GetOriginalProfile() const {
  return this;
}

bool ChromeProfileAlloy::IsChild() const {
  return false;
}

ExtensionSpecialStoragePolicy*
ChromeProfileAlloy::GetExtensionSpecialStoragePolicy() {
  DCHECK(false);
  return nullptr;
}

bool ChromeProfileAlloy::IsSameOrParent(Profile* profile) {
  DCHECK(false);
  return false;
}

base::Time ChromeProfileAlloy::GetStartTime() const {
  DCHECK(false);
  return base::Time();
}

base::FilePath ChromeProfileAlloy::last_selected_directory() {
  return last_selected_directory_;
}

void ChromeProfileAlloy::set_last_selected_directory(
    const base::FilePath& path) {
  last_selected_directory_ = path;
}

GURL ChromeProfileAlloy::GetHomePage() {
  DCHECK(false);
  return GURL();
}

bool ChromeProfileAlloy::WasCreatedByVersionOrLater(
    const std::string& version) {
  DCHECK(false);
  return false;
}

base::Time ChromeProfileAlloy::GetCreationTime() const {
  DCHECK(false);
  return base::Time();
}

void ChromeProfileAlloy::SetCreationTimeForTesting(base::Time creation_time) {
  DCHECK(false);
}

void ChromeProfileAlloy::RecordPrimaryMainFrameNavigation() {
  DCHECK(false);
}

bool ChromeProfileAlloy::IsSignedIn() {
  DCHECK(false);
  return false;
}
