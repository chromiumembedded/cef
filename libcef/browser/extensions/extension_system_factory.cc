// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/extensions/extension_system_factory.h"

#include "libcef/browser/extensions/extension_system.h"

#include "chrome/browser/profiles/incognito_helpers.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry_factory.h"

using content::BrowserContext;

namespace extensions {

ExtensionSystem* CefExtensionSystemFactory::GetForBrowserContext(
    BrowserContext* context) {
  return static_cast<CefExtensionSystem*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
CefExtensionSystemFactory* CefExtensionSystemFactory::GetInstance() {
  return base::Singleton<CefExtensionSystemFactory>::get();
}

CefExtensionSystemFactory::CefExtensionSystemFactory()
    : ExtensionSystemProvider("CefExtensionSystem",
                              BrowserContextDependencyManager::GetInstance()) {
  // Other factories that this factory depends on. See
  // libcef/common/extensions/api/README.txt for additional details.
  DependsOn(ExtensionPrefsFactory::GetInstance());
  DependsOn(ExtensionRegistryFactory::GetInstance());
}

CefExtensionSystemFactory::~CefExtensionSystemFactory() {
}

KeyedService* CefExtensionSystemFactory::BuildServiceInstanceFor(
    BrowserContext* context) const {
  return new CefExtensionSystem(context);
}

BrowserContext* CefExtensionSystemFactory::GetBrowserContextToUse(
    BrowserContext* context) const {
  // Use a separate instance for incognito.
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

bool CefExtensionSystemFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace extensions
