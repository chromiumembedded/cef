// Copyright 2025 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_EXTENSIONS_EXTENSION_REGISTRY_OBSERVER_H_
#define CEF_LIBCEF_BROWSER_EXTENSIONS_EXTENSION_REGISTRY_OBSERVER_H_
#pragma once

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "cef/include/cef_base.h"
#include "chrome/browser/extensions/extension_action_dispatcher.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/install/crx_install_error.h"

class CefExtension;
class CefExtensionHandler;
class CefRequestContext;
class CefRequestContextImpl;

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace extensions {
class CrxInstaller;
class Extension;
class ExtensionAction;
}  // namespace extensions

namespace cef {

// Bridges Chromium's extension subsystem to the CEF extension API for a single
// CefRequestContext. This is intentionally a thin adapter:
//   - load  -> extensions::UnpackedInstaller (via the --load-extension switch's
//              kCommandLine path, so loaded extensions are ephemeral and do not
//              persist across restarts)
//   - install -> extensions::CrxInstaller
//   - enable/disable/uninstall -> extensions::ExtensionRegistrar
//   - queries -> extensions::ExtensionRegistry
// Lifecycle notifications are taken directly from Chrome's
// ExtensionRegistryObserver and forwarded to the registered CefExtension
// handler(s). Owned by CefRequestContextImpl; all methods run on the browser
// process UI thread.
class CefExtensionRegistryObserver
    : public extensions::ExtensionRegistryObserver,
      public extensions::ExtensionActionDispatcher::Observer {
 public:
  explicit CefExtensionRegistryObserver(CefRequestContextImpl* request_context);

  CefExtensionRegistryObserver(const CefExtensionRegistryObserver&) = delete;
  CefExtensionRegistryObserver& operator=(const CefExtensionRegistryObserver&) =
      delete;

  ~CefExtensionRegistryObserver() override;

  // Begin observing the ExtensionRegistry associated with |browser_context|.
  void StartObserving(content::BrowserContext* browser_context);

  // Load an unpacked extension from |root_directory| ephemerally (--load-
  // extension semantics; does not persist across restarts). |handler| (may be
  // null) receives callbacks for this extension only.
  void LoadUnpackedExtension(const base::FilePath& root_directory,
                             CefRefPtr<CefExtensionHandler> handler);

  // Install an unpacked extension from |root_directory| persistently (the
  // chrome://extensions "Load unpacked" semantics; reloaded across restarts on
  // a persistent profile). |handler| (may be null) receives callbacks for this
  // extension only.
  void InstallUnpackedExtension(const base::FilePath& root_directory,
                                CefRefPtr<CefExtensionHandler> handler);

  // Install a packed (.crx) extension from |crx_path|. |handler| (may be null)
  // receives callbacks for this extension only.
  void InstallCrxExtension(const base::FilePath& crx_path,
                           CefRefPtr<CefExtensionHandler> handler);

  // Register (or, with null, detach) the context-wide handler. On registration
  // the handler is sent OnExtensionLoaded for every currently-loaded extension.
  void SetGlobalHandler(CefRefPtr<CefExtensionHandler> handler);

  // Queries against the live ExtensionRegistry.
  bool HasExtension(const std::string& extension_id);
  std::vector<CefString> GetExtensionIds();
  CefRefPtr<CefExtension> GetExtension(const std::string& extension_id);

  // Mutations routed through ExtensionRegistrar.
  void SetExtensionEnabled(const std::string& extension_id, bool enable);
  void UninstallExtension(const std::string& extension_id);

  // extensions::ExtensionRegistryObserver methods:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override;
  void OnShutdown(extensions::ExtensionRegistry* registry) override;

  // extensions::ExtensionActionDispatcher::Observer methods:
  void OnExtensionActionUpdated(
      extensions::ExtensionAction* extension_action,
      content::WebContents* web_contents,
      content::BrowserContext* browser_context) override;
  void OnShuttingDown() override;

 private:
  extensions::ExtensionRegistry* GetRegistry() const;

  // Builds a CefExtension snapshot wrapper for |extension|.
  CefRefPtr<CefExtension> MakeExtension(const extensions::Extension* extension);

  // Installer completion callbacks.
  void OnUnpackedLoaded(CefRefPtr<CefExtensionHandler> handler,
                        const extensions::Extension* extension,
                        const base::FilePath& root_directory,
                        const std::u16string& error);
  void OnCrxInstalled(CefRefPtr<CefExtensionHandler> handler,
                      scoped_refptr<extensions::CrxInstaller> installer,
                      const std::optional<extensions::CrxInstallError>& error);

  // Owns us; always outlives this object.
  raw_ptr<CefRequestContextImpl> request_context_;
  raw_ptr<content::BrowserContext> browser_context_ = nullptr;

  base::ScopedObservation<extensions::ExtensionRegistry,
                          extensions::ExtensionRegistryObserver>
      registry_observation_{this};

  base::ScopedObservation<extensions::ExtensionActionDispatcher,
                          extensions::ExtensionActionDispatcher::Observer>
      action_observation_{this};

  // The context-wide handler, plus per-extension handlers registered via
  // LoadUnpackedExtension / InstallExtension (keyed by extension id).
  CefRefPtr<CefExtensionHandler> global_handler_;
  std::map<std::string, CefRefPtr<CefExtensionHandler>> extension_handlers_;

  // Ids of extensions for which a "loaded" notification has been delivered to
  // the handler(s) and not yet balanced by an "unloaded" one. Two uses:
  //   - dedupe the initial explicit load (both the installer completion
  //     callback and the registry observer would otherwise report it); and
  //   - gate OnExtensionUnloaded so only previously-announced extensions are
  //     reported as unloaded.
  // SetGlobalHandler records the extensions it replays here too, so extensions
  // already loaded before observation began (e.g. persisted ones restored at
  // startup) still get a balancing unload notification.
  std::set<std::string> announced_;

  base::WeakPtrFactory<CefExtensionRegistryObserver> weak_ptr_factory_{this};
};

}  // namespace cef

#endif  // CEF_LIBCEF_BROWSER_EXTENSIONS_EXTENSION_REGISTRY_OBSERVER_H_
