// Copyright (c) 2015 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/chrome_profile_stub.h"

ChromeProfileStub::ChromeProfileStub() {
}

ChromeProfileStub::~ChromeProfileStub() {
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
  return NULL;
}

void ChromeProfileStub::DestroyOffTheRecordProfile() {
  NOTREACHED();
}

bool ChromeProfileStub::HasOffTheRecordProfile() {
  NOTREACHED();
  return false;
}

Profile* ChromeProfileStub::GetOriginalProfile() {
  return this;
}

bool ChromeProfileStub::IsSupervised() const {
  return false;
}

bool ChromeProfileStub::IsChild() const {
  return false;
}

bool ChromeProfileStub::IsLegacySupervised() const {
  NOTREACHED();
  return false;
}

ExtensionSpecialStoragePolicy*
    ChromeProfileStub::GetExtensionSpecialStoragePolicy() {
  NOTREACHED();
  return NULL;
}

PrefService* ChromeProfileStub::GetOffTheRecordPrefs() {
  NOTREACHED();
  return NULL;
}

net::URLRequestContextGetter*
    ChromeProfileStub::GetRequestContextForExtensions() {
  // TODO(cef): Consider creating a separate context for extensions to match
  // Chrome behavior.
  return GetRequestContext();
}

net::SSLConfigService* ChromeProfileStub::GetSSLConfigService() {
  NOTREACHED();
  return NULL;
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

PrefProxyConfigTracker* ChromeProfileStub::GetProxyConfigTracker() {
  NOTREACHED();
  return NULL;
}

chrome_browser_net::Predictor* ChromeProfileStub::GetNetworkPredictor() {
  NOTREACHED();
  return NULL;
}

DevToolsNetworkControllerHandle*
    ChromeProfileStub::GetDevToolsNetworkControllerHandle() {
  NOTREACHED();
  return NULL;
}

void ChromeProfileStub::ClearNetworkingHistorySince(
    base::Time time,
    const base::Closure& completion) {
  NOTREACHED();
}

GURL ChromeProfileStub::GetHomePage() {
  NOTREACHED();
  return GURL();
}

bool ChromeProfileStub::WasCreatedByVersionOrLater(
    const std::string& version) {
  NOTREACHED();
  return false;
}

void ChromeProfileStub::SetExitType(ExitType exit_type) {
  NOTREACHED();
}

Profile::ExitType ChromeProfileStub::GetLastSessionExitType() {
  NOTREACHED();
  return EXIT_NORMAL;
}
