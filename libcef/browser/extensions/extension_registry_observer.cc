// Copyright 2025 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/browser/extensions/extension_registry_observer.h"

#include <utility>

#include "base/functional/bind.h"
#include "cef/include/cef_extension.h"
#include "cef/include/cef_extension_handler.h"
#include "cef/libcef/browser/browser_host_base.h"
#include "cef/libcef/browser/extensions/extension_impl.h"
#include "cef/libcef/browser/request_context_impl.h"
#include "cef/libcef/browser/thread_util.h"
#include "extensions/browser/crx_installer.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/browser/unpacked_installer.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"

namespace cef {

CefExtensionRegistryObserver::CefExtensionRegistryObserver(
    CefRequestContextImpl* request_context)
    : request_context_(request_context) {}

CefExtensionRegistryObserver::~CefExtensionRegistryObserver() = default;

void CefExtensionRegistryObserver::StartObserving(
    content::BrowserContext* browser_context) {
  CEF_REQUIRE_UIT();
  browser_context_ = browser_context;
  if (auto* registry = GetRegistry()) {
    registry_observation_.Observe(registry);
  }
  // Also observe toolbar-action changes (chrome.action.setIcon/setTitle/...) so
  // a UI showing the action icon can refresh when an extension updates it.
  if (auto* dispatcher =
          extensions::ExtensionActionDispatcher::Get(browser_context_)) {
    action_observation_.Observe(dispatcher);
  }
}

void CefExtensionRegistryObserver::LoadUnpackedExtension(
    const base::FilePath& root_directory,
    CefRefPtr<CefExtensionHandler> handler) {
  CEF_REQUIRE_UIT();
  if (!browser_context_) {
    if (handler) {
      handler->OnExtensionLoadFailed(ERR_FAILED);
    }
    return;
  }

  scoped_refptr<extensions::UnpackedInstaller> installer =
      extensions::UnpackedInstaller::Create(browser_context_);
  installer->set_completion_callback(
      base::BindOnce(&CefExtensionRegistryObserver::OnUnpackedLoaded,
                     weak_ptr_factory_.GetWeakPtr(), handler));
  // Load via the same path as Chrome's --load-extension switch
  // (ManifestLocation::kCommandLine) rather than Load()'s kUnpacked. Both load
  // an unpacked extension from disk, but Chrome's InstalledLoader deliberately
  // skips kCommandLine entries when restoring extensions at startup, so the
  // extension is ephemeral and does not persist across restarts. Success and
  // failure are still delivered through the completion callback set above (the
  // synchronous-failure paths invoke it before returning). |only_allow_apps|
  // is false so ordinary extensions are permitted.
  std::string extension_id;
  installer->LoadFromCommandLine(root_directory, &extension_id,
                                 /*only_allow_apps=*/false);
}

void CefExtensionRegistryObserver::InstallUnpackedExtension(
    const base::FilePath& root_directory,
    CefRefPtr<CefExtensionHandler> handler) {
  CEF_REQUIRE_UIT();
  if (!browser_context_) {
    if (handler) {
      handler->OnExtensionLoadFailed(ERR_FAILED);
    }
    return;
  }

  scoped_refptr<extensions::UnpackedInstaller> installer =
      extensions::UnpackedInstaller::Create(browser_context_);
  installer->set_completion_callback(
      base::BindOnce(&CefExtensionRegistryObserver::OnUnpackedLoaded,
                     weak_ptr_factory_.GetWeakPtr(), handler));
  // Load via UnpackedInstaller::Load (ManifestLocation::kUnpacked), the same
  // path as the chrome://extensions "Load unpacked" button. Unlike
  // LoadUnpackedExtension's kCommandLine path, kUnpacked extensions are
  // recorded in the profile's extension prefs and restored by InstalledLoader
  // at the next startup, so the extension persists across restarts on a
  // persistent profile. Success and failure are delivered through the
  // completion callback set above.
  installer->Load(root_directory);
}

void CefExtensionRegistryObserver::InstallCrxExtension(
    const base::FilePath& crx_path,
    CefRefPtr<CefExtensionHandler> handler) {
  CEF_REQUIRE_UIT();
  if (!browser_context_) {
    if (handler) {
      handler->OnExtensionLoadFailed(ERR_FAILED);
    }
    return;
  }

  scoped_refptr<extensions::CrxInstaller> installer =
      extensions::CrxInstaller::CreateSilent(browser_context_);
  installer->set_off_store_install_allow_reason(
      extensions::CrxInstaller::OffStoreInstallAllowedBecausePref);
  installer->set_allow_silent_install(true);
  // |installer| is captured into its own callback to keep it alive until the
  // install completes; CrxInstaller clears its callback list afterwards so the
  // reference cycle is broken.
  installer->AddInstallerCallback(
      base::BindOnce(&CefExtensionRegistryObserver::OnCrxInstalled,
                     weak_ptr_factory_.GetWeakPtr(), handler, installer));
  installer->InstallCrx(crx_path);
}

void CefExtensionRegistryObserver::SetGlobalHandler(
    CefRefPtr<CefExtensionHandler> handler) {
  CEF_REQUIRE_UIT();
  global_handler_ = handler;
}

bool CefExtensionRegistryObserver::HasExtension(
    const std::string& extension_id) {
  CEF_REQUIRE_UIT();
  auto* registry = GetRegistry();
  return registry && registry->GetExtensionById(
                         extension_id,
                         extensions::ExtensionRegistry::EVERYTHING) != nullptr;
}

std::vector<CefString> CefExtensionRegistryObserver::GetExtensionIds() {
  CEF_REQUIRE_UIT();
  std::vector<CefString> ids;
  auto* registry = GetRegistry();
  if (!registry) {
    return ids;
  }
  for (const auto& extension : registry->enabled_extensions()) {
    ids.push_back(extension->id());
  }
  for (const auto& extension : registry->disabled_extensions()) {
    ids.push_back(extension->id());
  }
  return ids;
}

CefRefPtr<CefExtension> CefExtensionRegistryObserver::GetExtension(
    const std::string& extension_id) {
  CEF_REQUIRE_UIT();
  auto* registry = GetRegistry();
  if (!registry) {
    return nullptr;
  }
  const extensions::Extension* extension = registry->GetExtensionById(
      extension_id, extensions::ExtensionRegistry::EVERYTHING);
  if (!extension) {
    return nullptr;
  }
  return MakeExtension(extension);
}

void CefExtensionRegistryObserver::SetExtensionEnabled(
    const std::string& extension_id,
    bool enable) {
  CEF_REQUIRE_UIT();
  if (!browser_context_) {
    return;
  }
  auto* registrar = extensions::ExtensionRegistrar::Get(browser_context_);
  if (!registrar) {
    return;
  }
  if (enable) {
    registrar->EnableExtension(extension_id);
  } else {
    registrar->DisableExtension(
        extension_id, {extensions::disable_reason::DISABLE_USER_ACTION});
  }
}

void CefExtensionRegistryObserver::UninstallExtension(
    const std::string& extension_id) {
  CEF_REQUIRE_UIT();
  if (!browser_context_) {
    return;
  }
  auto* registrar = extensions::ExtensionRegistrar::Get(browser_context_);
  if (!registrar) {
    return;
  }
  std::u16string error;
  registrar->UninstallExtension(
      extension_id, extensions::UNINSTALL_REASON_USER_INITIATED, &error);
}

void CefExtensionRegistryObserver::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  CEF_REQUIRE_UIT();
  announced_.insert(extension->id());

  CefRefPtr<CefExtension> wrapper = MakeExtension(extension);
  auto it = extension_handlers_.find(extension->id());
  if (it != extension_handlers_.end()) {
    it->second->OnExtensionLoaded(wrapper);
  }
  if (global_handler_) {
    global_handler_->OnExtensionLoaded(wrapper);
  }
}

void CefExtensionRegistryObserver::OnUnpackedLoaded(
    CefRefPtr<CefExtensionHandler> handler,
    const extensions::Extension* extension,
    const base::FilePath& root_directory,
    const std::u16string& error) {
  CEF_REQUIRE_UIT();
  if (!extension) {
    if (handler) {
      handler->OnExtensionLoadFailed(ERR_FAILED);
    }
    return;
  }
  if (!handler) {
    return;
  }
  extension_handlers_[extension->id()] = handler;
  // If the registry already reported the load before this callback ran, the
  // per-extension handler missed its OnExtensionLoaded; deliver it now.
  if (announced_.count(extension->id())) {
    handler->OnExtensionLoaded(MakeExtension(extension));
  }
}

void CefExtensionRegistryObserver::OnCrxInstalled(
    CefRefPtr<CefExtensionHandler> handler,
    scoped_refptr<extensions::CrxInstaller> installer,
    const std::optional<extensions::CrxInstallError>& error) {
  CEF_REQUIRE_UIT();
  const extensions::Extension* extension =
      installer ? installer->extension() : nullptr;
  if (error.has_value() || !extension) {
    if (handler) {
      handler->OnExtensionLoadFailed(ERR_FAILED);
    }
    return;
  }
  if (!handler) {
    return;
  }
  extension_handlers_[extension->id()] = handler;
  if (announced_.count(extension->id())) {
    handler->OnExtensionLoaded(MakeExtension(extension));
  }
}

void CefExtensionRegistryObserver::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  CEF_REQUIRE_UIT();
  // Drop the load-dedup record (see announced_) but deliver unconditionally:
  // unloads are fire-and-forget, so a handler registered after an extension
  // was already loaded still hears about its removal.
  announced_.erase(extension->id());

  CefRefPtr<CefExtension> wrapper = MakeExtension(extension);
  auto it = extension_handlers_.find(extension->id());
  if (it != extension_handlers_.end()) {
    it->second->OnExtensionUnloaded(wrapper);
  }
  if (global_handler_) {
    global_handler_->OnExtensionUnloaded(wrapper);
  }
}

void CefExtensionRegistryObserver::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  CEF_REQUIRE_UIT();
  // The matching unload was already reported via OnExtensionUnloaded; just drop
  // the per-extension handler now that the extension is gone.
  extension_handlers_.erase(extension->id());
}

void CefExtensionRegistryObserver::OnShutdown(
    extensions::ExtensionRegistry* registry) {
  registry_observation_.Reset();
  browser_context_ = nullptr;
}

void CefExtensionRegistryObserver::OnExtensionActionUpdated(
    extensions::ExtensionAction* extension_action,
    content::WebContents* web_contents,
    content::BrowserContext* browser_context) {
  CEF_REQUIRE_UIT();
  if (!extension_action) {
    return;
  }

  // Re-snapshot the extension so handlers see current state, and skip updates
  // for extensions this context no longer tracks.
  CefRefPtr<CefExtension> wrapper =
      GetExtension(extension_action->extension_id());
  if (!wrapper) {
    return;
  }

  // |web_contents| is the affected tab, or null for a change that applies to
  // all tabs (e.g. a default-icon change).
  CefRefPtr<CefBrowser> browser;
  if (web_contents) {
    if (auto host = CefBrowserHostBase::GetBrowserForContents(web_contents)) {
      browser = host.get();
    }
  }

  auto it = extension_handlers_.find(extension_action->extension_id());
  if (it != extension_handlers_.end()) {
    it->second->OnExtensionActionUpdated(wrapper, browser);
  }
  if (global_handler_) {
    global_handler_->OnExtensionActionUpdated(wrapper, browser);
  }
}

void CefExtensionRegistryObserver::OnShuttingDown() {
  action_observation_.Reset();
}

extensions::ExtensionRegistry* CefExtensionRegistryObserver::GetRegistry()
    const {
  return browser_context_ ? extensions::ExtensionRegistry::Get(browser_context_)
                          : nullptr;
}

CefRefPtr<CefExtension> CefExtensionRegistryObserver::MakeExtension(
    const extensions::Extension* extension) {
  auto* registry = GetRegistry();
  const bool enabled =
      registry && registry->enabled_extensions().Contains(extension->id());
  return new CefExtensionImpl(extension, enabled,
                              CefRefPtr<CefRequestContext>(request_context_));
}

}  // namespace cef
