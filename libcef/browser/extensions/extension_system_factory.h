// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_EXTENSIONS_EXTENSION_SYSTEM_FACTORY_H_
#define CEF_LIBCEF_BROWSER_EXTENSIONS_EXTENSION_SYSTEM_FACTORY_H_

#include "base/memory/singleton.h"
#include "extensions/browser/extension_system_provider.h"

namespace extensions {

// Factory that provides CefExtensionSystem.
class CefExtensionSystemFactory : public ExtensionSystemProvider {
 public:
  // ExtensionSystemProvider implementation:
  ExtensionSystem* GetForBrowserContext(
      content::BrowserContext* context) override;

  static CefExtensionSystemFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<CefExtensionSystemFactory>;

  CefExtensionSystemFactory();
  ~CefExtensionSystemFactory() override;

  // BrowserContextKeyedServiceFactory implementation:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;

  DISALLOW_COPY_AND_ASSIGN(CefExtensionSystemFactory);
};

}  // namespace extensions

#endif  // CEF_LIBCEF_BROWSER_EXTENSIONS_EXTENSION_SYSTEM_FACTORY_H_
