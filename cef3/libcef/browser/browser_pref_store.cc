// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/browser_pref_store.h"

#include "base/command_line.h"
#include "base/values.h"
#include "chrome/browser/prefs/command_line_pref_store.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/pref_service_mock_builder.h"
#include "chrome/browser/prefs/proxy_config_dictionary.h"
#include "chrome/common/pref_names.h"

BrowserPrefStore::BrowserPrefStore() {
}

PrefService* BrowserPrefStore::CreateService() {
  PrefServiceMockBuilder builder;
  builder.WithCommandLinePrefs(
      new CommandLinePrefStore(CommandLine::ForCurrentProcess()));
  builder.WithUserPrefs(this);

  PrefService* service = builder.Create();

  // Default settings.
  service->RegisterDictionaryPref(prefs::kProxy,
                                  ProxyConfigDictionary::CreateDirect());

  return service;
}

BrowserPrefStore::~BrowserPrefStore() {
}
