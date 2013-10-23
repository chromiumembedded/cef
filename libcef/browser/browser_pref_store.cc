// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/browser_pref_store.h"
#include "libcef/browser/media_capture_devices_dispatcher.h"

#include "base/command_line.h"
#include "base/prefs/pref_service_builder.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/values.h"
#include "chrome/browser/net/pref_proxy_config_tracker_impl.h"
#include "chrome/browser/prefs/command_line_pref_store.h"
#include "chrome/browser/prefs/proxy_config_dictionary.h"
#include "chrome/common/pref_names.h"

CefBrowserPrefStore::CefBrowserPrefStore() {
}

PrefService* CefBrowserPrefStore::CreateService() {
  PrefServiceBuilder builder;
  builder.WithCommandLinePrefs(
      new CommandLinePrefStore(CommandLine::ForCurrentProcess()));
  builder.WithUserPrefs(this);

  scoped_refptr<PrefRegistrySimple> registry(new PrefRegistrySimple());

  // Default settings.
  CefMediaCaptureDevicesDispatcher::RegisterPrefs(registry);
  PrefProxyConfigTrackerImpl::RegisterPrefs(registry);

  registry->RegisterBooleanPref(prefs::kPrintingEnabled, true);

  return builder.Create(registry);
}

CefBrowserPrefStore::~CefBrowserPrefStore() {
}
