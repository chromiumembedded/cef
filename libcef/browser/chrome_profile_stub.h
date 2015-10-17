// Copyright (c) 2015 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class gathers state related to a single user profile.

#ifndef CEF_LIBCEF_BROWSER_CHROME_PROFILE_STUB_H_
#define CEF_LIBCEF_BROWSER_CHROME_PROFILE_STUB_H_

#include "chrome/browser/profiles/profile.h"

// This file provides a stub implementation of Chrome's Profile object for use
// as an interop layer between CEF and files that live in chrome/.

class ChromeProfileStub : public Profile {
 public:
  ChromeProfileStub();
  ~ChromeProfileStub() override;

 protected:
  // Profile methods.
  scoped_refptr<base::SequencedTaskRunner> GetIOTaskRunner() override;
  std::string GetProfileUserName() const override;
  ProfileType GetProfileType() const override;
  Profile* GetOffTheRecordProfile() override;
  void DestroyOffTheRecordProfile() override;
  bool HasOffTheRecordProfile() override;
  Profile* GetOriginalProfile() override;
  bool IsSupervised() const override;
  bool IsChild() const override;
  bool IsLegacySupervised() const override;
  ExtensionSpecialStoragePolicy*
      GetExtensionSpecialStoragePolicy() override;
  PrefService* GetOffTheRecordPrefs() override;
  net::URLRequestContextGetter* GetRequestContextForExtensions() override;
  net::SSLConfigService* GetSSLConfigService() override;
  bool IsSameProfile(Profile* profile) override;
  base::Time GetStartTime() const override;
  base::FilePath last_selected_directory() override;
  void set_last_selected_directory(const base::FilePath& path) override;
  PrefProxyConfigTracker* GetProxyConfigTracker() override;
  chrome_browser_net::Predictor* GetNetworkPredictor() override;
  DevToolsNetworkControllerHandle*
      GetDevToolsNetworkControllerHandle() override;
  void ClearNetworkingHistorySince(base::Time time,
                                   const base::Closure& completion) override;
  GURL GetHomePage() override;
  bool WasCreatedByVersionOrLater(const std::string& version) override;
  void SetExitType(ExitType exit_type) override;
  ExitType GetLastSessionExitType() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeProfileStub);
};

#endif  // CEF_LIBCEF_BROWSER_CHROME_PROFILE_STUB_H_
