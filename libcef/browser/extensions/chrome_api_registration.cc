// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// APIs must also be registered in
// libcef/common/extensions/api/_*_features.json files and possibly
// CefExtensionsDispatcherDelegate::PopulateSourceMap. See
// libcef/common/extensions/api/README.txt for additional details.

#include "libcef/browser/extensions/chrome_api_registration.h"

#include "libcef/browser/extensions/api/tabs/tabs_api.h"

#include "chrome/browser/extensions/api/content_settings/content_settings_api.h"
#include "chrome/browser/extensions/api/pdf_viewer_private/pdf_viewer_private_api.h"
#include "chrome/browser/extensions/api/resources_private/resources_private_api.h"
#include "extensions/browser/api/alarms/alarms_api.h"
#include "extensions/browser/api/storage/storage_api.h"
#include "extensions/browser/extension_function_registry.h"

namespace extensions::api::cef {

namespace cefimpl = extensions::cef;

#define EXTENSION_FUNCTION_NAME(classname) classname::static_function_name()

// Maintain the same order as https://developer.chrome.com/extensions/api_index
// so chrome://extensions-support looks nice.
const char* const kSupportedAPIs[] = {
    "alarms",
    EXTENSION_FUNCTION_NAME(AlarmsCreateFunction),
    EXTENSION_FUNCTION_NAME(AlarmsGetFunction),
    EXTENSION_FUNCTION_NAME(AlarmsGetAllFunction),
    EXTENSION_FUNCTION_NAME(AlarmsClearFunction),
    EXTENSION_FUNCTION_NAME(AlarmsClearAllFunction),
    "contentSettings",
    EXTENSION_FUNCTION_NAME(ContentSettingsContentSettingClearFunction),
    EXTENSION_FUNCTION_NAME(ContentSettingsContentSettingGetFunction),
    EXTENSION_FUNCTION_NAME(ContentSettingsContentSettingSetFunction),
    EXTENSION_FUNCTION_NAME(
        ContentSettingsContentSettingGetResourceIdentifiersFunction),
    "pdfViewerPrivate",
    EXTENSION_FUNCTION_NAME(PdfViewerPrivateIsAllowedLocalFileAccessFunction),
    EXTENSION_FUNCTION_NAME(PdfViewerPrivateIsPdfOcrAlwaysActiveFunction),
    "resourcesPrivate",
    EXTENSION_FUNCTION_NAME(ResourcesPrivateGetStringsFunction),
    "storage",
    EXTENSION_FUNCTION_NAME(StorageStorageAreaGetFunction),
    EXTENSION_FUNCTION_NAME(StorageStorageAreaSetFunction),
    EXTENSION_FUNCTION_NAME(StorageStorageAreaRemoveFunction),
    EXTENSION_FUNCTION_NAME(StorageStorageAreaClearFunction),
    EXTENSION_FUNCTION_NAME(StorageStorageAreaGetBytesInUseFunction),
    "tabs",
    EXTENSION_FUNCTION_NAME(cefimpl::TabsGetFunction),
    EXTENSION_FUNCTION_NAME(cefimpl::TabsCreateFunction),
    EXTENSION_FUNCTION_NAME(cefimpl::TabsUpdateFunction),
    EXTENSION_FUNCTION_NAME(cefimpl::TabsExecuteScriptFunction),
    EXTENSION_FUNCTION_NAME(cefimpl::TabsInsertCSSFunction),
    EXTENSION_FUNCTION_NAME(cefimpl::TabsRemoveCSSFunction),
    EXTENSION_FUNCTION_NAME(cefimpl::TabsSetZoomFunction),
    EXTENSION_FUNCTION_NAME(cefimpl::TabsGetZoomFunction),
    EXTENSION_FUNCTION_NAME(cefimpl::TabsSetZoomSettingsFunction),
    EXTENSION_FUNCTION_NAME(cefimpl::TabsGetZoomSettingsFunction),
    nullptr,  // Indicates end of array.
};

// Only add APIs to this list that have been tested in CEF.
// static
bool ChromeFunctionRegistry::IsSupported(const std::string& name) {
  for (size_t i = 0; kSupportedAPIs[i] != nullptr; ++i) {
    if (name == kSupportedAPIs[i]) {
      return true;
    }
  }
  return false;
}

// Only add APIs to this list that have been tested in CEF.
// static
void ChromeFunctionRegistry::RegisterAll(ExtensionFunctionRegistry* registry) {
  registry->RegisterFunction<AlarmsCreateFunction>();
  registry->RegisterFunction<AlarmsGetFunction>();
  registry->RegisterFunction<AlarmsGetAllFunction>();
  registry->RegisterFunction<AlarmsClearFunction>();
  registry->RegisterFunction<AlarmsClearAllFunction>();
  registry->RegisterFunction<ContentSettingsContentSettingClearFunction>();
  registry->RegisterFunction<ContentSettingsContentSettingGetFunction>();
  registry->RegisterFunction<ContentSettingsContentSettingSetFunction>();
  registry->RegisterFunction<
      ContentSettingsContentSettingGetResourceIdentifiersFunction>();
  registry
      ->RegisterFunction<PdfViewerPrivateIsAllowedLocalFileAccessFunction>();
  registry->RegisterFunction<PdfViewerPrivateIsPdfOcrAlwaysActiveFunction>();
  registry->RegisterFunction<ResourcesPrivateGetStringsFunction>();
  registry->RegisterFunction<StorageStorageAreaGetFunction>();
  registry->RegisterFunction<StorageStorageAreaSetFunction>();
  registry->RegisterFunction<StorageStorageAreaRemoveFunction>();
  registry->RegisterFunction<StorageStorageAreaClearFunction>();
  registry->RegisterFunction<StorageStorageAreaGetBytesInUseFunction>();
  registry->RegisterFunction<cefimpl::TabsExecuteScriptFunction>();
  registry->RegisterFunction<cefimpl::TabsInsertCSSFunction>();
  registry->RegisterFunction<cefimpl::TabsRemoveCSSFunction>();
  registry->RegisterFunction<cefimpl::TabsGetFunction>();
  registry->RegisterFunction<cefimpl::TabsCreateFunction>();
  registry->RegisterFunction<cefimpl::TabsUpdateFunction>();
  registry->RegisterFunction<cefimpl::TabsSetZoomFunction>();
  registry->RegisterFunction<cefimpl::TabsGetZoomFunction>();
  registry->RegisterFunction<cefimpl::TabsSetZoomSettingsFunction>();
  registry->RegisterFunction<cefimpl::TabsGetZoomSettingsFunction>();
}

}  // namespace extensions::api::cef
