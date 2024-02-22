// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/extensions/component_extension_resource_manager.h"

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/values.h"
#include "chrome/browser/pdf/pdf_extension_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/grit/component_extension_resources_map.h"
#include "chrome/grit/pdf_resources_map.h"
#include "extensions/common/constants.h"

namespace extensions {

CefComponentExtensionResourceManager::CefComponentExtensionResourceManager() {
  AddComponentResourceEntries(kComponentExtensionResources,
                              kComponentExtensionResourcesSize);
  AddComponentResourceEntries(kPdfResources, kPdfResourcesSize);

  base::Value::Dict dict;
  pdf_extension_util::AddStrings(
      pdf_extension_util::PdfViewerContext::kPdfViewer, &dict);
  pdf_extension_util::AddAdditionalData(/*enable_printing=*/true,
                                        /*enable_annotations=*/true, &dict);

  ui::TemplateReplacements pdf_viewer_replacements;
  ui::TemplateReplacementsFromDictionaryValue(dict, &pdf_viewer_replacements);
  template_replacements_[extension_misc::kPdfExtensionId] =
      std::move(pdf_viewer_replacements);
}

CefComponentExtensionResourceManager::~CefComponentExtensionResourceManager() =
    default;

bool CefComponentExtensionResourceManager::IsComponentExtensionResource(
    const base::FilePath& extension_path,
    const base::FilePath& resource_path,
    int* resource_id) const {
  base::FilePath directory_path = extension_path;
  base::FilePath resources_dir;
  base::FilePath relative_path;
  if (!base::PathService::Get(chrome::DIR_RESOURCES, &resources_dir) ||
      !resources_dir.AppendRelativePath(directory_path, &relative_path)) {
    return false;
  }
  relative_path = relative_path.Append(resource_path);
  relative_path = relative_path.NormalizePathSeparators();

  auto entry = path_to_resource_info_.find(relative_path);
  if (entry != path_to_resource_info_.end()) {
    *resource_id = entry->second;
    return true;
  }

  return false;
}

const ui::TemplateReplacements*
CefComponentExtensionResourceManager::GetTemplateReplacementsForExtension(
    const ExtensionId& extension_id) const {
  auto it = template_replacements_.find(extension_id);
  return it != template_replacements_.end() ? &it->second : nullptr;
}

void CefComponentExtensionResourceManager::AddComponentResourceEntries(
    const webui::ResourcePath* entries,
    size_t size) {
  for (size_t i = 0; i < size; ++i) {
    base::FilePath resource_path =
        base::FilePath().AppendASCII(entries[i].path);
    resource_path = resource_path.NormalizePathSeparators();

    DCHECK(!base::Contains(path_to_resource_info_, resource_path));
    path_to_resource_info_[resource_path] = entries[i].id;
  }
}

}  // namespace extensions
