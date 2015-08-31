// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(dberger): This file is nearly identical to
// chrome/browser/component_updater/widevine_cdm_component_installer.cc,
// the only real difference being how we compute the browser version in
// UpdateCDMAdapter.

#include "libcef/browser/component_updater/widevine_cdm_component_installer.h"

#include "include/cef_version.h"

#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/widevine_cdm_constants.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/default_component_installer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/pepper_plugin_info.h"
#include "media/cdm/ppapi/supported_cdm_versions.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"

#include "widevine_cdm_version.h"  // In SHARED_INTERMEDIATE_DIR. NOLINT

using content::BrowserThread;
using content::PluginService;

namespace component_updater {

#if defined(WIDEVINE_CDM_AVAILABLE) && defined(WIDEVINE_CDM_IS_COMPONENT)

namespace {

// CRX hash. The extension id is: oimompecagnajdejgnnjijobebaeigek.
const uint8_t kSha2Hash[] = {0xe8, 0xce, 0xcf, 0x42, 0x06, 0xd0, 0x93, 0x49,
                             0x6d, 0xd9, 0x89, 0xe1, 0x41, 0x04, 0x86, 0x4a,
                             0x8f, 0xbd, 0x86, 0x12, 0xb9, 0x58, 0x9b, 0xfb,
                             0x4f, 0xbb, 0x1b, 0xa9, 0xd3, 0x85, 0x37, 0xef};

// File name of the Widevine CDM component manifest on different platforms.
const char kWidevineCdmManifestName[] = "WidevineCdm";

// File name of the Widevine CDM adapter version file. The CDM adapter shares
// the same version number with Chromium version.
const char kCdmAdapterVersionName[] = "CdmAdapterVersion";

// Name of the Widevine CDM OS in the component manifest.
const char kWidevineCdmPlatform[] =
#if defined(OS_MACOSX)
    "mac";
#elif defined(OS_WIN)
    "win";
#else  // OS_LINUX, etc. TODO(viettrungluu): Separate out Chrome OS and Android?
    "linux";
#endif

// Name of the Widevine CDM architecture in the component manifest.
const char kWidevineCdmArch[] =
#if defined(ARCH_CPU_X86)
    "x86";
#elif defined(ARCH_CPU_X86_64)
    "x64";
#else  // TODO(viettrungluu): Support an ARM check?
    "???";
#endif

// The CDM manifest includes several custom values, all beginning with "x-cdm-".
// All values are strings.
// All values that are lists are delimited by commas. No trailing commas.
// For example, "1,2,4".
const char kCdmValueDelimiter = ',';
static_assert(kCdmValueDelimiter == kCdmSupportedCodecsValueDelimiter,
              "cdm delimiters must match");
// The following entries are required.
//  Interface versions are lists of integers (e.g. "1" or "1,2,4").
//  These are checked in this file before registering the CDM.
//  All match the interface versions from content_decryption_module.h that the
//  CDM supports.
//    Matches CDM_MODULE_VERSION.
const char kCdmModuleVersionsName[] = "x-cdm-module-versions";
//    Matches supported ContentDecryptionModule_* version(s).
const char kCdmInterfaceVersionsName[] = "x-cdm-interface-versions";
//    Matches supported Host_* version(s).
const char kCdmHostVersionsName[] = "x-cdm-host-versions";
//  The codecs list is a list of simple codec names (e.g. "vp8,vorbis").
//  The list is passed to other parts of Chrome.
const char kCdmCodecsListName[] = "x-cdm-codecs";

// Widevine CDM is packaged as a multi-CRX. Widevine CDM binaries are located in
// _platform_specific/<platform_arch> folder in the package. This function
// returns the platform-specific subdirectory that is part of that multi-CRX.
base::FilePath GetPlatformDirectory(const base::FilePath& base_path) {
  std::string platform_arch = kWidevineCdmPlatform;
  platform_arch += '_';
  platform_arch += kWidevineCdmArch;
  return base_path.AppendASCII("_platform_specific").AppendASCII(platform_arch);
}

bool MakeWidevineCdmPluginInfo(
    const base::Version& version,
    const base::FilePath& path,
    const std::vector<base::string16>& additional_param_names,
    const std::vector<base::string16>& additional_param_values,
    content::PepperPluginInfo* plugin_info) {
  if (!version.IsValid() ||
      version.components().size() !=
          static_cast<size_t>(kWidevineCdmVersionNumComponents)) {
    return false;
  }

  plugin_info->is_internal = false;
  // Widevine CDM must run out of process.
  plugin_info->is_out_of_process = true;
  plugin_info->path = path;
  plugin_info->name = kWidevineCdmDisplayName;
  plugin_info->description = kWidevineCdmDescription +
                             std::string(" (version: ") + version.GetString() +
                             ")";
  plugin_info->version = version.GetString();
  content::WebPluginMimeType widevine_cdm_mime_type(
      kWidevineCdmPluginMimeType,
      kWidevineCdmPluginExtension,
      kWidevineCdmPluginMimeTypeDescription);
  widevine_cdm_mime_type.additional_param_names = additional_param_names;
  widevine_cdm_mime_type.additional_param_values = additional_param_values;
  plugin_info->mime_types.push_back(widevine_cdm_mime_type);
  plugin_info->permissions = kWidevineCdmPluginPermissions;

  return true;
}

typedef bool (*VersionCheckFunc)(int version);

bool CheckForCompatibleVersion(const base::DictionaryValue& manifest,
                               const std::string version_name,
                               VersionCheckFunc version_check_func) {
  std::string versions_string;
  if (!manifest.GetString(version_name, &versions_string)) {
    DLOG(WARNING) << "Widevine CDM component manifest missing " << version_name;
    return false;
  }
  DLOG_IF(WARNING, versions_string.empty())
      << "Widevine CDM component manifest has empty " << version_name;

  std::vector<std::string> versions;
  base::SplitString(versions_string, kCdmValueDelimiter, &versions);

  for (size_t i = 0; i < versions.size(); ++i) {
    int version = 0;
    if (base::StringToInt(versions[i], &version))
      if (version_check_func(version))
        return true;
  }

  DLOG(WARNING) << "Widevine CDM component manifest has no supported "
                << version_name << " in '" << versions_string << "'";
  return false;
}

// Returns whether the CDM's API versions, as specified in the manifest, are
// compatible with this Chrome binary.
// Checks the module API, CDM interface API, and Host API.
// This should never fail except in rare cases where the component has not been
// updated recently or the user downgrades Chrome.
bool IsCompatibleWithChrome(const base::DictionaryValue& manifest) {
  return CheckForCompatibleVersion(manifest,
                                   kCdmModuleVersionsName,
                                   media::IsSupportedCdmModuleVersion) &&
         CheckForCompatibleVersion(manifest,
                                   kCdmInterfaceVersionsName,
                                   media::IsSupportedCdmInterfaceVersion) &&
         CheckForCompatibleVersion(manifest,
                                   kCdmHostVersionsName,
                                   media::IsSupportedCdmHostVersion);
}

void GetAdditionalParams(const base::DictionaryValue& manifest,
                         std::vector<base::string16>* additional_param_names,
                         std::vector<base::string16>* additional_param_values) {
  base::string16 codecs;
  if (manifest.GetString(kCdmCodecsListName, &codecs)) {
    DLOG_IF(WARNING, codecs.empty())
        << "Widevine CDM component manifest has empty codecs list";
    additional_param_names->push_back(
        base::ASCIIToUTF16(kCdmSupportedCodecsParamName));
    additional_param_values->push_back(codecs);
  } else {
    DLOG(WARNING) << "Widevine CDM component manifest is missing codecs";
  }
}

void RegisterWidevineCdmWithChrome(const base::Version& cdm_version,
                                   const base::FilePath& adapter_install_path,
                                   scoped_ptr<base::DictionaryValue> manifest) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::vector<base::string16> additional_param_names;
  std::vector<base::string16> additional_param_values;
  GetAdditionalParams(
      *manifest, &additional_param_names, &additional_param_values);
  content::PepperPluginInfo plugin_info;
  if (!MakeWidevineCdmPluginInfo(cdm_version,
                                 adapter_install_path,
                                 additional_param_names,
                                 additional_param_values,
                                 &plugin_info)) {
    return;
  }

  // true = Add to beginning of list to override any existing registrations.
  PluginService::GetInstance()->RegisterInternalPlugin(
      plugin_info.ToWebPluginInfo(), true);
  // Tell the browser to refresh the plugin list. Then tell all renderers to
  // update their plugin list caches.
  PluginService::GetInstance()->RefreshPlugins();
  PluginService::GetInstance()->PurgePluginListCache(NULL, false);
}

}  // namespace

class WidevineCdmComponentInstallerTraits : public ComponentInstallerTraits {
 public:
  WidevineCdmComponentInstallerTraits();
  ~WidevineCdmComponentInstallerTraits() override {}

 private:
  // The following methods override ComponentInstallerTraits.
  bool CanAutoUpdate() const override;
  bool OnCustomInstall(const base::DictionaryValue& manifest,
                               const base::FilePath& install_dir) override;
  bool VerifyInstallation(
      const base::DictionaryValue& manifest,
      const base::FilePath& install_dir) const override;
  void ComponentReady(
      const base::Version& version,
      const base::FilePath& path,
      scoped_ptr<base::DictionaryValue> manifest) override;
  base::FilePath GetBaseDirectory() const override;
  void GetHash(std::vector<uint8_t>* hash) const override;
  std::string GetName() const override;

  // Checks and updates CDM adapter if necessary to make sure the latest CDM
  // adapter is always used.
  // Note: The component is ready when CDM is present, but the CDM won't be
  // registered until the adapter is copied by this function (see
  // VerifyInstallation).
  void UpdateCdmAdapter(const base::Version& cdm_version,
                        const base::FilePath& cdm_install_dir,
                        scoped_ptr<base::DictionaryValue> manifest);

  DISALLOW_COPY_AND_ASSIGN(WidevineCdmComponentInstallerTraits);
};

WidevineCdmComponentInstallerTraits::WidevineCdmComponentInstallerTraits() {
}

bool WidevineCdmComponentInstallerTraits::CanAutoUpdate() const {
  return true;
}

bool WidevineCdmComponentInstallerTraits::OnCustomInstall(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) {
  return true;
}

// Once the CDM is ready, check the CDM adapter.
void WidevineCdmComponentInstallerTraits::ComponentReady(
    const base::Version& version,
    const base::FilePath& path,
    scoped_ptr<base::DictionaryValue> manifest) {
  if (!IsCompatibleWithChrome(*manifest)) {
    DLOG(WARNING) << "Installed Widevine CDM component is incompatible.";
    return;
  }

  BrowserThread::PostBlockingPoolTask(
      FROM_HERE,
      base::Bind(&WidevineCdmComponentInstallerTraits::UpdateCdmAdapter,
                 base::Unretained(this),
                 version,
                 path,
                 base::Passed(&manifest)));
}

bool WidevineCdmComponentInstallerTraits::VerifyInstallation(
    const base::DictionaryValue& manifest,
    const base::FilePath& install_dir) const {
  return IsCompatibleWithChrome(manifest) &&
         base::PathExists(GetPlatformDirectory(install_dir)
                              .AppendASCII(kWidevineCdmFileName));
}

// The base directory on Windows looks like:
// <profile>\AppData\Local\Google\Chrome\User Data\WidevineCdm\.
base::FilePath WidevineCdmComponentInstallerTraits::GetBaseDirectory() const {
  base::FilePath result;
  PathService::Get(chrome::DIR_COMPONENT_WIDEVINE_CDM, &result);
  return result;
}

void WidevineCdmComponentInstallerTraits::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(kSha2Hash, kSha2Hash + arraysize(kSha2Hash));
}

std::string WidevineCdmComponentInstallerTraits::GetName() const {
  return kWidevineCdmManifestName;
}

void WidevineCdmComponentInstallerTraits::UpdateCdmAdapter(
    const base::Version& cdm_version,
    const base::FilePath& cdm_install_dir,
    scoped_ptr<base::DictionaryValue> manifest) {
  const base::FilePath adapter_version_path =
      GetPlatformDirectory(cdm_install_dir).AppendASCII(kCdmAdapterVersionName);
  const base::FilePath adapter_install_path =
      GetPlatformDirectory(cdm_install_dir)
          .AppendASCII(kWidevineCdmAdapterFileName);

  const std::string chrome_version = base::StringPrintf("%d.%d.%d.%d",
            CHROME_VERSION_MAJOR,
            CHROME_VERSION_MINOR,
            CHROME_VERSION_BUILD,
            CHROME_VERSION_PATCH);
  DCHECK(!chrome_version.empty());
  std::string adapter_version;
  if (!base::ReadFileToString(adapter_version_path, &adapter_version) ||
      adapter_version != chrome_version ||
      !base::PathExists(adapter_install_path)) {
    int bytes_written = base::WriteFile(
        adapter_version_path, chrome_version.data(), chrome_version.size());
    if (bytes_written < 0 ||
        static_cast<size_t>(bytes_written) != chrome_version.size()) {
      DLOG(WARNING) << "Failed to write Widevine CDM adapter version file.";
      // Ignore version file writing failure and try to copy the CDM adapter.
    }

    base::FilePath adapter_source_path;
    PathService::Get(chrome::FILE_WIDEVINE_CDM_ADAPTER, &adapter_source_path);
    if (!base::CopyFile(adapter_source_path, adapter_install_path)) {
      DLOG(WARNING) << "Failed to copy Widevine CDM adapter.";
      return;
    }
  }

  BrowserThread::PostTask(content::BrowserThread::UI,
                          FROM_HERE,
                          base::Bind(&RegisterWidevineCdmWithChrome,
                                     cdm_version,
                                     adapter_install_path,
                                     base::Passed(&manifest)));
}

#endif  // defined(WIDEVINE_CDM_AVAILABLE) && defined(WIDEVINE_CDM_IS_COMPONENT)

void RegisterWidevineCdmComponent(ComponentUpdateService* cus) {
#if defined(WIDEVINE_CDM_AVAILABLE) && defined(WIDEVINE_CDM_IS_COMPONENT)
  base::FilePath adapter_source_path;
  PathService::Get(chrome::FILE_WIDEVINE_CDM_ADAPTER, &adapter_source_path);
  if (!base::PathExists(adapter_source_path))
    return;
  scoped_ptr<ComponentInstallerTraits> traits(
      new WidevineCdmComponentInstallerTraits);
  // |cus| will take ownership of |installer| during installer->Register(cus).
  DefaultComponentInstaller* installer =
      new DefaultComponentInstaller(traits.Pass());
  installer->Register(cus, base::Closure());
#endif  // defined(WIDEVINE_CDM_AVAILABLE) && defined(WIDEVINE_CDM_IS_COMPONENT)
}

}  // namespace component_updater
