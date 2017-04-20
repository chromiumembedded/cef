// Copyright 2015 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_EXTENSIONS_EXTENSION_SYSTEM_H_
#define CEF_LIBCEF_BROWSER_EXTENSIONS_EXTENSION_SYSTEM_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/one_shot_event.h"

class BrowserContextKeyedServiceFactory;

namespace base {
class FilePath;
}

namespace content {
class BrowserContext;
}

namespace extensions {

class ExtensionRegistry;
class InfoMap;
class ProcessManager;
class RendererStartupHelper;
class SharedUserScriptMaster;
class ValueStoreFactory;

// Used to manage extensions.
class CefExtensionSystem : public ExtensionSystem {
 public:
  explicit CefExtensionSystem(content::BrowserContext* browser_context);
  ~CefExtensionSystem() override;

  // Initializes the extension system.
  void Init();

  // Add an extension. Returns the new Extension object on success or NULL on
  // failure.
  const Extension* AddExtension(const std::string& manifest_contents,
                                const base::FilePath& root_directory);

  // Remove an extension.
  void RemoveExtension(const std::string& extension_id);

  // KeyedService implementation:
  void Shutdown() override;

  // ExtensionSystem implementation:
  void InitForRegularProfile(bool extensions_enabled) override;
  ExtensionService* extension_service() override;
  RuntimeData* runtime_data() override;
  ManagementPolicy* management_policy() override;
  ServiceWorkerManager* service_worker_manager() override;
  SharedUserScriptMaster* shared_user_script_master() override;
  StateStore* state_store() override;
  StateStore* rules_store() override;
  scoped_refptr<ValueStoreFactory> store_factory() override;
  InfoMap* info_map() override;
  QuotaService* quota_service() override;
  AppSorting* app_sorting() override;
  void RegisterExtensionWithRequestContexts(
      const Extension* extension,
      const base::Closure& callback) override;
  void UnregisterExtensionWithRequestContexts(
      const std::string& extension_id,
      const UnloadedExtensionInfo::Reason reason) override;
  const OneShotEvent& ready() const override;
  ContentVerifier* content_verifier() override;
  std::unique_ptr<ExtensionSet> GetDependentExtensions(
      const Extension* extension) override;
  void InstallUpdate(const std::string& extension_id,
                     const base::FilePath& temp_dir) override;

  bool initialized() const { return initialized_; }

 private:
  // Information about a registered component extension.
  struct ComponentExtensionInfo {
    ComponentExtensionInfo(const base::DictionaryValue* manifest,
                           const base::FilePath& root_directory);

    // The parsed contents of the extensions's manifest file.
    const base::DictionaryValue* manifest;

    // Directory where the extension is stored.
    base::FilePath root_directory;

    // The component extension's ID.
    std::string extension_id;
  };

  scoped_refptr<const Extension> CreateExtension(
      const ComponentExtensionInfo& info, std::string* utf8_error);

  // Loads a registered component extension.
  const Extension* LoadExtension(const ComponentExtensionInfo& info);

  // Unload the specified extension.
  void UnloadExtension(
      const std::string& extension_id,
      extensions::UnloadedExtensionInfo::Reason reason);

  // Handles sending notification that |extension| was loaded.
  void NotifyExtensionLoaded(const Extension* extension);

  // Handles sending notification that |extension| was unloaded.
  void NotifyExtensionUnloaded(
      const Extension* extension,
      UnloadedExtensionInfo::Reason reason);

  // Completes extension loading after URLRequestContexts have been updated
  // on the IO thread.
  void OnExtensionRegisteredWithRequestContexts(
      scoped_refptr<const extensions::Extension> extension);

  content::BrowserContext* browser_context_;  // Not owned.

  bool initialized_;

  // Data to be accessed on the IO thread. Must outlive process_manager_.
  scoped_refptr<InfoMap> info_map_;

  std::unique_ptr<ServiceWorkerManager> service_worker_manager_;
  std::unique_ptr<RuntimeData> runtime_data_;
  std::unique_ptr<QuotaService> quota_service_;
  std::unique_ptr<AppSorting> app_sorting_;

  // Signaled when the extension system has completed its startup tasks.
  OneShotEvent ready_;

  // Sets of enabled/disabled/terminated/blacklisted extensions. Not owned.
  ExtensionRegistry* registry_;

  // The associated RendererStartupHelper. Guaranteed to outlive the
  // ExtensionSystem, and thus us.
  extensions::RendererStartupHelper* renderer_helper_;

  // Must be the last member.
  base::WeakPtrFactory<CefExtensionSystem> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CefExtensionSystem);
};

}  // namespace extensions

#endif  // CEF_LIBCEF_BROWSER_EXTENSIONS_EXTENSION_SYSTEM_H_
