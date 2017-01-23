// Copyright 2016 The Chromium Embedded Framework Authors. Portions copyright
// 2013 the Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#include "libcef/common/widevine_loader.h"

#if defined(WIDEVINE_CDM_AVAILABLE) && BUILDFLAG(ENABLE_PEPPER_CDMS)

#include "libcef/browser/context.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/cef_switches.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/ptr_util.h"
#include "base/native_library.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/widevine_cdm_constants.h"
#include "content/browser/plugin_service_impl.h"
#include "content/public/browser/cdm_registry.h"
#include "content/public/common/cdm_info.h"
#include "content/public/common/content_switches.h"
#include "media/cdm/supported_cdm_versions.h"

namespace {

base::LazyInstance<CefWidevineLoader>::Leaky g_widevine_loader =
    LAZY_INSTANCE_INITIALIZER;


// Based on chrome/browser/component_updater/widevine_cdm_component_installer.cc

// Name of the Widevine CDM OS in the component manifest.
const char kWidevineCdmOs[] =
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
    "ia32";  // This differs from the component updater which uses "x86".
#elif defined(ARCH_CPU_X86_64)
    "x64";
#else  // TODO(viettrungluu): Support an ARM check?
    "???";
#endif

// The CDM OS and architecture.
const char kCdmOsName[] = "os";
const char kCdmArchName[] = "arch";

//  The CDM version (e.g. "1.4.8.903").
const char kCdmVersionName[] = "version";

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

std::unique_ptr<base::DictionaryValue> ParseManifestFile(
    const base::FilePath& manifest_path) {
  CEF_REQUIRE_FILET();

  // Manifest file should be < 1kb. Read at most 2kb.
  std::string manifest_contents;
  if (!base::ReadFileToStringWithMaxSize(manifest_path, &manifest_contents,
                                         2048)) {
    return nullptr;
  }

  JSONStringValueDeserializer deserializer(manifest_contents);
  std::unique_ptr<base::Value> manifest(deserializer.Deserialize(NULL, NULL));

  if (!manifest.get() || !manifest->IsType(base::Value::Type::DICTIONARY))
    return nullptr;

  // Transfer ownership to the caller.
  return base::WrapUnique(
      static_cast<base::DictionaryValue*>(manifest.release()));
}

std::string GetManifestValue(const base::DictionaryValue& manifest,
                             const std::string& key,
                             std::string* error_message) {
  std::stringstream ss;
  std::string value;
  if (!manifest.GetString(key, &value)) {
    ss << "Manifest missing " << key;
    *error_message = ss.str();
  } else if (value.empty()) {
    ss << "Manifest has empty " << key;
    *error_message = ss.str();
  }
  return value;
}

typedef bool (*VersionCheckFunc)(int version);

bool CheckForCompatibleVersion(const base::DictionaryValue& manifest,
                               const std::string version_name,
                               VersionCheckFunc version_check_func,
                               std::string* error_message) {
  std::string versions_string =
      GetManifestValue(manifest, version_name, error_message);
  if (versions_string.empty())
    return false;

  for (const base::StringPiece& ver_str : base::SplitStringPiece(
           versions_string, std::string(1, kCdmValueDelimiter),
           base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    int version = 0;
    if (base::StringToInt(ver_str, &version))
      if (version_check_func(version))
        return true;
  }

  std::stringstream ss;
  ss << "Manifest has no supported " << version_name << " in '" <<
        versions_string << "'";
  *error_message = ss.str();
  return false;
}

// Returns whether the CDM's OS/platform and module/interface/host API versions,
// as specified in the manifest, are compatible with this Chromium binary.
bool IsCompatibleWithChrome(const base::DictionaryValue& manifest,
                            std::string* error_message) {
  return GetManifestValue(manifest, kCdmOsName, error_message) ==
             kWidevineCdmOs &&
         GetManifestValue(manifest, kCdmArchName, error_message) ==
             kWidevineCdmArch &&
         CheckForCompatibleVersion(manifest,
                                   kCdmModuleVersionsName,
                                   media::IsSupportedCdmModuleVersion,
                                   error_message) &&
         CheckForCompatibleVersion(manifest,
                                   kCdmInterfaceVersionsName,
                                   media::IsSupportedCdmInterfaceVersion,
                                   error_message) &&
         CheckForCompatibleVersion(manifest,
                                   kCdmHostVersionsName,
                                   media::IsSupportedCdmHostVersion,
                                   error_message);
}

// Populate the PepperPluginInfo structure.
void GetPluginInfo(const base::FilePath& cdm_adapter_path,
                   const base::FilePath& cdm_path,
                   const std::string& cdm_version,
                   const std::string& cdm_codecs,
                   content::PepperPluginInfo* widevine_cdm) {
  widevine_cdm->is_out_of_process = true;
  widevine_cdm->path = cdm_adapter_path;
  widevine_cdm->name = kWidevineCdmDisplayName;
  widevine_cdm->description = kWidevineCdmDescription +
                              std::string(" (version: ") +
                              cdm_version + ")";
  widevine_cdm->version = cdm_version;
  content::WebPluginMimeType widevine_cdm_mime_type(
      kWidevineCdmPluginMimeType,
      kWidevineCdmPluginExtension,
      kWidevineCdmPluginMimeTypeDescription);

  widevine_cdm_mime_type.additional_param_names.push_back(
      base::ASCIIToUTF16(kCdmSupportedCodecsParamName));
  widevine_cdm_mime_type.additional_param_values.push_back(
      base::ASCIIToUTF16(cdm_codecs));

  widevine_cdm->mime_types.push_back(widevine_cdm_mime_type);
  widevine_cdm->permissions = kWidevineCdmPluginPermissions;
}

// Verify and load the contents of |base_path|.
cef_cdm_registration_error_t LoadWidevineCdmInfo(
    const base::FilePath& base_path,
    base::FilePath* cdm_adapter_path,
    base::FilePath* cdm_path,
    std::string* cdm_version,
    std::string* cdm_codecs,
    std::string* error_message) {
  std::stringstream ss;

  *cdm_adapter_path = base_path.AppendASCII(kWidevineCdmAdapterFileName);
  if (!base::PathExists(*cdm_adapter_path)) {
    ss << "Missing adapter file " << cdm_adapter_path->value();
    *error_message = ss.str();
    return CEF_CDM_REGISTRATION_ERROR_INCORRECT_CONTENTS;
  }

  *cdm_path = base_path.AppendASCII(
      base::GetNativeLibraryName(kWidevineCdmLibraryName));
  if (!base::PathExists(*cdm_path)) {
    ss << "Missing file " << cdm_path->value();
    *error_message = ss.str();
    return CEF_CDM_REGISTRATION_ERROR_INCORRECT_CONTENTS;
  }

  base::FilePath manifest_path = base_path.AppendASCII("manifest.json");
  if (!base::PathExists(manifest_path)) {
    ss << "Missing manifest file " << manifest_path.value();
    *error_message = ss.str();
    return CEF_CDM_REGISTRATION_ERROR_INCORRECT_CONTENTS;
  }

  std::unique_ptr<base::DictionaryValue> manifest =
      ParseManifestFile(manifest_path);
  if (!manifest) {
    ss << "Failed to parse manifest file " << manifest_path.value();
    *error_message = ss.str();
    return CEF_CDM_REGISTRATION_ERROR_INCORRECT_CONTENTS;
  }

  if (!IsCompatibleWithChrome(*manifest, error_message))
    return CEF_CDM_REGISTRATION_ERROR_INCOMPATIBLE;

  *cdm_version = GetManifestValue(*manifest, kCdmVersionName, error_message);
  if (cdm_version->empty())
    return CEF_CDM_REGISTRATION_ERROR_INCORRECT_CONTENTS;

  *cdm_codecs = GetManifestValue(*manifest, kCdmCodecsListName, error_message);
  if (cdm_codecs->empty())
    return CEF_CDM_REGISTRATION_ERROR_INCORRECT_CONTENTS;

  return CEF_CDM_REGISTRATION_ERROR_NONE;
}

void DeliverWidevineCdmCallback(cef_cdm_registration_error_t result,
                                const std::string& error_message,
                                CefRefPtr<CefRegisterCdmCallback> callback) {
  CEF_REQUIRE_UIT();

  if (result != CEF_CDM_REGISTRATION_ERROR_NONE)
    LOG(ERROR) << "Widevine CDM registration failed; " << error_message;

  if (callback)
    callback->OnCdmRegistrationComplete(result, error_message);
}

void RegisterWidevineCdmOnUIThread(
    const base::FilePath& cdm_adapter_path,
    const base::FilePath& cdm_path,
    const std::string& cdm_version,
    const std::string& cdm_codecs,
    CefRefPtr<CefRegisterCdmCallback> callback) {
  CEF_REQUIRE_UIT();

  content::PepperPluginInfo widevine_cdm;
  GetPluginInfo(cdm_adapter_path, cdm_path, cdm_version, cdm_codecs,
                &widevine_cdm);

  // true = Add to beginning of list to override any existing registrations.
  content::PluginService::GetInstance()->RegisterInternalPlugin(
      widevine_cdm.ToWebPluginInfo(), true);
  // Tell the browser to refresh the plugin list. Then tell all renderers to
  // update their plugin list caches.
  content::PluginService::GetInstance()->RefreshPlugins();
  content::PluginService::GetInstance()->PurgePluginListCache(NULL, false);

  // Also register Widevine with the CdmRegistry.
  const std::vector<std::string> codecs = base::SplitString(
      cdm_codecs, std::string(1, kCdmSupportedCodecsValueDelimiter),
      base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  content::CdmRegistry::GetInstance()->RegisterCdm(content::CdmInfo(
      kWidevineCdmType, base::Version(cdm_version), cdm_path, codecs));

  DeliverWidevineCdmCallback(CEF_CDM_REGISTRATION_ERROR_NONE, std::string(),
                             callback);
}

void LoadWidevineCdmInfoOnFileThread(
    const base::FilePath& base_path,
    CefRefPtr<CefRegisterCdmCallback> callback) {
  CEF_REQUIRE_FILET();

  base::FilePath cdm_adapter_path;
  base::FilePath cdm_path;
  std::string cdm_version;
  std::string cdm_codecs;
  std::string error_message;
  cef_cdm_registration_error_t result =
      LoadWidevineCdmInfo(base_path, &cdm_adapter_path, &cdm_path, &cdm_version,
                          &cdm_codecs, &error_message);
  if (result != CEF_CDM_REGISTRATION_ERROR_NONE) {
    CEF_POST_TASK(CEF_UIT, base::Bind(DeliverWidevineCdmCallback, result,
                                      error_message, callback));
    return;
  }

  // Continue execution on the UI thread.
  CEF_POST_TASK(CEF_UIT,
      base::Bind(RegisterWidevineCdmOnUIThread, cdm_adapter_path, cdm_path,
                 cdm_version, cdm_codecs, callback));
}

}  // namespace

// static
CefWidevineLoader* CefWidevineLoader::GetInstance() {
  return &g_widevine_loader.Get();
}

void CefWidevineLoader::LoadWidevineCdm(
    const base::FilePath& path,
    CefRefPtr<CefRegisterCdmCallback> callback) {
  if (!CONTEXT_STATE_VALID()) {
    // Loading will proceed from OnContextInitialized().
    load_pending_ = true;
    path_ = path;
    callback_ = callback;
    return;
  }

  // Continue execution on the FILE thread.
  CEF_POST_TASK(CEF_FILET,
      base::Bind(LoadWidevineCdmInfoOnFileThread, path, callback));
}

void CefWidevineLoader::OnContextInitialized() {
  CEF_REQUIRE_UIT();
  if (load_pending_) {
    load_pending_ = false;
    LoadWidevineCdm(path_, callback_);
    callback_ = nullptr;
  }
}

#if defined(OS_LINUX)

// static
void CefWidevineLoader::AddPepperPlugins(
    std::vector<content::PepperPluginInfo>* plugins) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  // Perform early plugin registration in the zygote process when the sandbox is
  // enabled to avoid "cannot open shared object file: Operation not permitted"
  // errors during plugin loading. This is because the Zygote process must pre-
  // load all plugins before initializing the sandbox.
  if (command_line.GetSwitchValueASCII(switches::kProcessType) !=
          switches::kZygoteProcess ||
      command_line.HasSwitch(switches::kNoSandbox)) {
    return;
  }

  // The Widevine CDM path is passed to the zygote process via
  // CefContentBrowserClient::AppendExtraCommandLineSwitches.
  const base::FilePath& base_path = command_line.GetSwitchValuePath(
      switches::kWidevineCdmPath);
  if (base_path.empty())
    return;

  // Load contents of the plugin directory synchronously. This only occurs once
  // on zygote process startup so should not have a huge performance penalty.
  base::FilePath cdm_adapter_path;
  base::FilePath cdm_path;
  std::string cdm_version;
  std::string cdm_codecs;
  std::string error_message;
  cef_cdm_registration_error_t result =
      LoadWidevineCdmInfo(base_path, &cdm_adapter_path, &cdm_path, &cdm_version,
                          &cdm_codecs, &error_message);
  if (result != CEF_CDM_REGISTRATION_ERROR_NONE) {
    LOG(ERROR) << "Widevine CDM registration failed; " << error_message;
    return;
  }

  content::PepperPluginInfo widevine_cdm;
  GetPluginInfo(cdm_adapter_path, cdm_path, cdm_version, cdm_codecs,
                &widevine_cdm);
  plugins->push_back(widevine_cdm);
}

#endif  // defined(OS_LINUX)

CefWidevineLoader::CefWidevineLoader() {
}

CefWidevineLoader::~CefWidevineLoader() {
}

#endif  // defined(WIDEVINE_CDM_AVAILABLE) && BUILDFLAG(ENABLE_PEPPER_CDMS)
