// Copyright 2025 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/browser/extensions/extension_action_impl.h"

#include "cef/libcef/browser/image_impl.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_action_icon_factory.h"
#include "extensions/browser/extension_action_manager.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

namespace cef {

namespace {

// ExtensionActionIconFactory requires an observer (it notifies when an
// asynchronously-loaded default icon arrives). We only take a one-shot snapshot
// of the current icon, so a no-op observer that outlives the transient factory
// is sufficient.
class NoopIconObserver
    : public extensions::ExtensionActionIconFactory::Observer {
 public:
  void OnIconUpdated() override {}
};

}  // namespace

GURL GetExtensionActionPopupUrl(content::BrowserContext* browser_context,
                                const extensions::Extension& extension,
                                int tab_id) {
  auto* manager = extensions::ExtensionActionManager::Get(browser_context);
  if (!manager) {
    return GURL();
  }
  extensions::ExtensionAction* action = manager->GetExtensionAction(extension);
  if (!action || !action->HasPopup(tab_id)) {
    return GURL();
  }
  return action->GetPopupUrl(tab_id);
}

CefRefPtr<CefImage> GetExtensionActionIcon(
    content::BrowserContext* browser_context,
    const std::string& extension_id,
    int tab_id) {
  auto* registry = extensions::ExtensionRegistry::Get(browser_context);
  if (!registry) {
    return nullptr;
  }

  const extensions::Extension* extension = registry->GetExtensionById(
      extension_id, extensions::ExtensionRegistry::ENABLED);
  if (!extension) {
    return nullptr;
  }

  auto* manager = extensions::ExtensionActionManager::Get(browser_context);
  if (!manager) {
    return nullptr;
  }

  extensions::ExtensionAction* action = manager->GetExtensionAction(*extension);
  if (!action) {
    return nullptr;
  }

  NoopIconObserver observer;
  extensions::ExtensionActionIconFactory icon_factory(extension, action,
                                                      &observer);
  gfx::Image image = icon_factory.GetIcon(tab_id);
  if (image.IsEmpty()) {
    return nullptr;
  }

  return new CefImageImpl(image.AsImageSkia());
}

}  // namespace cef
