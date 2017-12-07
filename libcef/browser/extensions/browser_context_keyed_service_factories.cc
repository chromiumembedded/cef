// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/extensions/browser_context_keyed_service_factories.h"

#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/extensions/api/streams_private/streams_private_api.h"
#include "chrome/browser/plugins/plugin_prefs_factory.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "extensions/browser/api/alarms/alarm_manager.h"
#include "extensions/browser/api/storage/storage_frontend.h"
#include "extensions/browser/renderer_startup_helper.h"

namespace extensions {
namespace cef {

void EnsureBrowserContextKeyedServiceFactoriesBuilt() {
  AlarmManager::GetFactoryInstance();
  CookieSettingsFactory::GetInstance();
  PluginPrefsFactory::GetInstance();
  PrefsTabHelper::GetServiceInstance();
  RendererStartupHelperFactory::GetInstance();
  SpellcheckServiceFactory::GetInstance();
  StorageFrontend::GetFactoryInstance();
  StreamsPrivateAPI::GetFactoryInstance();
  ThemeServiceFactory::GetInstance();
}

}  // namespace cef
}  // namespace extensions
