// Copyright 2015 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/common/alloy/alloy_content_client.h"

#include <stdint.h>

#include "include/cef_stream.h"
#include "include/cef_version.h"
#include "libcef/common/app_manager.h"
#include "libcef/common/cef_switches.h"
#include "libcef/common/extensions/extensions_util.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/media/cdm_registration.h"
#include "components/pdf/common/constants.h"
#include "content/public/common/cdm_info.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_plugin_info.h"
#include "content/public/common/content_switches.h"
#include "ppapi/shared_impl/ppapi_permissions.h"
#include "third_party/widevine/cdm/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
#include "libcef/common/cdm_host_file_path.h"
#endif

namespace {

// The following plugin-related methods are from
// chrome/common/chrome_content_client.cc

constexpr char kPDFPluginName[] = "Chromium PDF Plugin";
constexpr char kPDFPluginExtension[] = "pdf";
constexpr char kPDFPluginDescription[] = "Portable Document Format";
constexpr uint32_t kPDFPluginPermissions =
    ppapi::PERMISSION_PDF | ppapi::PERMISSION_DEV;

// Appends the known built-in plugins to the given vector. Some built-in
// plugins are "internal" which means they are compiled into the Chrome binary,
// and some are extra shared libraries distributed with the browser (these are
// not marked internal, aside from being automatically registered, they're just
// regular plugins).
void ComputeBuiltInPlugins(std::vector<content::ContentPluginInfo>* plugins) {
  if (extensions::PdfExtensionEnabled()) {
    content::ContentPluginInfo pdf_info;
    pdf_info.is_internal = true;
    pdf_info.is_out_of_process = true;
    pdf_info.name = kPDFPluginName;
    pdf_info.description = kPDFPluginDescription;
    pdf_info.path = base::FilePath(ChromeContentClient::kPDFInternalPluginPath);
    content::WebPluginMimeType pdf_mime_type(pdf::kInternalPluginMimeType,
                                             kPDFPluginExtension,
                                             kPDFPluginDescription);
    pdf_info.mime_types.push_back(pdf_mime_type);
    pdf_info.permissions = kPDFPluginPermissions;
    plugins->push_back(pdf_info);
  }
}

}  // namespace

AlloyContentClient::AlloyContentClient() = default;
AlloyContentClient::~AlloyContentClient() = default;

void AlloyContentClient::AddPlugins(
    std::vector<content::ContentPluginInfo>* plugins) {
  ComputeBuiltInPlugins(plugins);
}

void AlloyContentClient::AddContentDecryptionModules(
    std::vector<content::CdmInfo>* cdms,
    std::vector<media::CdmHostFilePath>* cdm_host_file_paths) {
  if (cdms) {
    RegisterCdmInfo(cdms);
  }

#if BUILDFLAG(ENABLE_CDM_HOST_VERIFICATION)
  if (cdm_host_file_paths) {
    cef::AddCdmHostFilePaths(cdm_host_file_paths);
  }
#endif
}

void AlloyContentClient::AddAdditionalSchemes(Schemes* schemes) {
  CefAppManager::Get()->AddAdditionalSchemes(schemes);
}

std::u16string AlloyContentClient::GetLocalizedString(int message_id) {
  std::u16string value =
      ui::ResourceBundle::GetSharedInstance().GetLocalizedString(message_id);
  if (value.empty()) {
    LOG(ERROR) << "No localized string available for id " << message_id;
  }

  return value;
}

std::u16string AlloyContentClient::GetLocalizedString(
    int message_id,
    const std::u16string& replacement) {
  std::u16string value = l10n_util::GetStringFUTF16(message_id, replacement);
  if (value.empty()) {
    LOG(ERROR) << "No localized string available for id " << message_id;
  }

  return value;
}

std::string_view AlloyContentClient::GetDataResource(
    int resource_id,
    ui::ResourceScaleFactor scale_factor) {
  auto value =
      ui::ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
          resource_id, scale_factor);
  if (value.empty()) {
    LOG(ERROR) << "No data resource available for id " << resource_id;
  }

  return value;
}

base::RefCountedMemory* AlloyContentClient::GetDataResourceBytes(
    int resource_id) {
  base::RefCountedMemory* value =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
          resource_id);
  if (!value) {
    LOG(ERROR) << "No data resource bytes available for id " << resource_id;
  }

  return value;
}

gfx::Image& AlloyContentClient::GetNativeImageNamed(int resource_id) {
  gfx::Image& value =
      ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(resource_id);
  if (value.IsEmpty()) {
    LOG(ERROR) << "No native image available for id " << resource_id;
  }

  return value;
}
