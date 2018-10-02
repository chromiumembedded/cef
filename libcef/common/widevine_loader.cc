// Copyright 2016 The Chromium Embedded Framework Authors. Portions copyright
// 2013 the Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#include "libcef/common/widevine_loader.h"

#if BUILDFLAG(ENABLE_WIDEVINE) && BUILDFLAG(ENABLE_LIBRARY_CDMS)

#include "libcef/browser/context.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/cef_switches.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/ptr_util.h"
#include "base/native_library.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/plugin_service_impl.h"
#include "content/public/browser/cdm_registry.h"
#include "content/public/common/cdm_info.h"
#include "content/public/common/content_switches.h"
#include "media/cdm/cdm_host_file.h"
#include "media/cdm/supported_cdm_versions.h"
#include "services/service_manager/embedder/switches.h"
#include "services/service_manager/sandbox/switches.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"  // nogncheck

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
const char kCdmValueDelimiter[] = ",";
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
//  Whether persistent license is supported by the CDM: "true" or "false".
const char kCdmPersistentLicenseSupportName[] =
    "x-cdm-persistent-license-support";
const char kCdmSupportedEncryptionSchemesName[] =
    "x-cdm-supported-encryption-schemes";

// The following strings are used to specify supported codecs in the
// parameter |kCdmCodecsListName|.
const char kCdmSupportedCodecVp8[] = "vp8";
const char kCdmSupportedCodecVp9[] = "vp9.0";
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
const char kCdmSupportedCodecAvc1[] = "avc1";
#endif

// The following strings are used to specify supported encryption schemes in
// the parameter |kCdmSupportedEncryptionSchemesName|.
const char kCdmSupportedEncryptionSchemeCenc[] = "cenc";
const char kCdmSupportedEncryptionSchemeCbcs[] = "cbcs";

// Arguments passed to MakeCdmInfo.
struct CdmInfoArgs {
  base::FilePath path;
  std::string version;
  content::CdmCapability capability;
};

std::unique_ptr<base::DictionaryValue> ParseManifestFile(
    const base::FilePath& manifest_path) {
  CEF_REQUIRE_BLOCKING();

  // Manifest file should be < 1kb. Read at most 2kb.
  std::string manifest_contents;
  if (!base::ReadFileToStringWithMaxSize(manifest_path, &manifest_contents,
                                         2048)) {
    return nullptr;
  }

  JSONStringValueDeserializer deserializer(manifest_contents);
  std::unique_ptr<base::Value> manifest(deserializer.Deserialize(NULL, NULL));

  if (!manifest.get() || !manifest->is_dict())
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

  for (const base::StringPiece& ver_str :
       base::SplitStringPiece(versions_string, kCdmValueDelimiter,
                              base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    int version = 0;
    if (base::StringToInt(ver_str, &version))
      if (version_check_func(version))
        return true;
  }

  std::stringstream ss;
  ss << "Manifest has no supported " << version_name << " in '"
     << versions_string << "'";
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
         CheckForCompatibleVersion(manifest, kCdmModuleVersionsName,
                                   media::IsSupportedCdmModuleVersion,
                                   error_message) &&
         CheckForCompatibleVersion(manifest, kCdmInterfaceVersionsName,
                                   media::IsSupportedCdmInterfaceVersion,
                                   error_message) &&
         CheckForCompatibleVersion(manifest, kCdmHostVersionsName,
                                   media::IsSupportedCdmHostVersion,
                                   error_message);
}

// Returns true and updates |video_codecs| if the appropriate manifest entry is
// valid. Returns false and does not modify |video_codecs| if the manifest entry
// is incorrectly formatted.
bool GetCodecs(const base::DictionaryValue& manifest,
               std::vector<media::VideoCodec>* video_codecs,
               std::string* error_message) {
  DCHECK(video_codecs);

  const base::Value* value = manifest.FindKey(kCdmCodecsListName);
  if (!value) {
    std::stringstream ss;
    ss << "Widevine CDM component manifest is missing codecs.";
    *error_message = ss.str();
    return true;
  }

  if (!value->is_string()) {
    std::stringstream ss;
    ss << "Manifest entry " << kCdmCodecsListName << " is not a string.";
    *error_message = ss.str();
    return false;
  }

  const std::string& codecs = value->GetString();
  if (codecs.empty()) {
    std::stringstream ss;
    ss << "Widevine CDM component manifest has empty codecs list.";
    *error_message = ss.str();
    return true;
  }

  std::vector<media::VideoCodec> result;
  const std::vector<base::StringPiece> supported_codecs =
      base::SplitStringPiece(codecs, kCdmValueDelimiter, base::TRIM_WHITESPACE,
                             base::SPLIT_WANT_NONEMPTY);

  for (const auto& codec : supported_codecs) {
    if (codec == kCdmSupportedCodecVp8)
      result.push_back(media::VideoCodec::kCodecVP8);
    else if (codec == kCdmSupportedCodecVp9)
      result.push_back(media::VideoCodec::kCodecVP9);
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
    else if (codec == kCdmSupportedCodecAvc1)
      result.push_back(media::VideoCodec::kCodecH264);
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
  }

  video_codecs->swap(result);
  return true;
}

// Returns true and updates |encryption_schemes| if the appropriate manifest
// entry is valid. Returns false and does not modify |encryption_schemes| if the
// manifest entry is incorrectly formatted. It is assumed that all CDMs support
// 'cenc', so if the manifest entry is missing, the result will indicate support
// for 'cenc' only. Incorrect types in the manifest entry will log the error and
// fail. Unrecognized values will be reported but otherwise ignored.
bool GetEncryptionSchemes(
    const base::DictionaryValue& manifest,
    base::flat_set<media::EncryptionMode>* encryption_schemes,
    std::string* error_message) {
  DCHECK(encryption_schemes);

  const base::Value* value =
      manifest.FindKey(kCdmSupportedEncryptionSchemesName);
  if (!value) {
    // No manifest entry found, so assume only 'cenc' supported for backwards
    // compatibility.
    encryption_schemes->insert(media::EncryptionMode::kCenc);
    return true;
  }

  if (!value->is_list()) {
    std::stringstream ss;
    ss << "Manifest entry " << kCdmSupportedEncryptionSchemesName
       << " is not a list.";
    *error_message = ss.str();
    return false;
  }

  const base::Value::ListStorage& list = value->GetList();
  base::flat_set<media::EncryptionMode> result;
  for (const auto& item : list) {
    if (!item.is_string()) {
      std::stringstream ss;
      ss << "Unrecognized item type in manifest entry "
         << kCdmSupportedEncryptionSchemesName;
      *error_message = ss.str();
      return false;
    }

    const std::string& scheme = item.GetString();
    if (scheme == kCdmSupportedEncryptionSchemeCenc) {
      result.insert(media::EncryptionMode::kCenc);
    } else if (scheme == kCdmSupportedEncryptionSchemeCbcs) {
      result.insert(media::EncryptionMode::kCbcs);
    } else {
      std::stringstream ss;
      ss << "Unrecognized encryption scheme " << scheme << " in manifest entry "
         << kCdmSupportedEncryptionSchemesName;
      *error_message = ss.str();
    }
  }

  // As the manifest entry exists, it must specify at least one valid value.
  if (result.empty())
    return false;

  encryption_schemes->swap(result);
  return true;
}

// Returns true and updates |session_types| if the appropriate manifest entry is
// valid. Returns false if the manifest entry is incorrectly formatted.
bool GetSessionTypes(const base::DictionaryValue& manifest,
                     base::flat_set<media::CdmSessionType>* session_types,
                     std::string* error_message) {
  DCHECK(session_types);

  bool is_persistent_license_supported = false;
  const base::Value* value = manifest.FindKey(kCdmPersistentLicenseSupportName);
  if (value) {
    if (!value->is_bool())
      return false;
    is_persistent_license_supported = value->GetBool();
  }

  // Temporary session is always supported.
  session_types->insert(media::CdmSessionType::kTemporary);
  if (is_persistent_license_supported)
    session_types->insert(media::CdmSessionType::kPersistentLicense);

  return true;
}

// Verify and load the contents of |base_path|.
cef_cdm_registration_error_t LoadWidevineCdmInfo(
    const base::FilePath& base_path,
    CdmInfoArgs* args,
    std::string* error_message) {
  std::stringstream ss;

  args->path = base_path.AppendASCII(
      base::GetNativeLibraryName(kWidevineCdmLibraryName));
  if (!base::PathExists(args->path)) {
    ss << "Missing file " << args->path.value();
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

  args->version = GetManifestValue(*manifest, kCdmVersionName, error_message);
  if (args->version.empty())
    return CEF_CDM_REGISTRATION_ERROR_INCORRECT_CONTENTS;

  if (!GetCodecs(*manifest, &args->capability.video_codecs, error_message) ||
      !GetEncryptionSchemes(*manifest, &args->capability.encryption_schemes,
                            error_message) ||
      !GetSessionTypes(*manifest, &args->capability.session_types,
                       error_message)) {
    return CEF_CDM_REGISTRATION_ERROR_INCORRECT_CONTENTS;
  }

  return CEF_CDM_REGISTRATION_ERROR_NONE;
}

void DeliverWidevineCdmCallback(cef_cdm_registration_error_t result,
                                const std::string& error_message,
                                CefRefPtr<CefRegisterCdmCallback> callback) {
  CEF_REQUIRE_UIT();

  if (result != CEF_CDM_REGISTRATION_ERROR_NONE)
    LOG(ERROR) << "Widevine CDM registration failed; " << error_message;
  else if (!error_message.empty())
    LOG(WARNING) << "Widevine CDM registration warning; " << error_message;

  if (callback)
    callback->OnCdmRegistrationComplete(result, error_message);
}

content::CdmInfo MakeCdmInfo(const CdmInfoArgs& args) {
  return content::CdmInfo(kWidevineCdmDisplayName, kWidevineCdmGuid,
                          base::Version(args.version), args.path,
                          kWidevineCdmFileSystemId, args.capability,
                          kWidevineKeySystem, false);
}

void RegisterWidevineCdmOnUIThread(std::unique_ptr<CdmInfoArgs> args,
                                   CefRefPtr<CefRegisterCdmCallback> callback) {
  CEF_REQUIRE_UIT();

  // Register Widevine with the CdmRegistry.
  content::CdmRegistry::GetInstance()->RegisterCdm(MakeCdmInfo(*args));

  DeliverWidevineCdmCallback(CEF_CDM_REGISTRATION_ERROR_NONE, std::string(),
                             callback);
}

void LoadWidevineCdmInfoOnBlockingThread(
    const base::FilePath& base_path,
    CefRefPtr<CefRegisterCdmCallback> callback) {
  CEF_REQUIRE_BLOCKING();

  std::unique_ptr<CdmInfoArgs> args = std::make_unique<CdmInfoArgs>();
  std::string error_message;
  cef_cdm_registration_error_t result =
      LoadWidevineCdmInfo(base_path, args.get(), &error_message);
  if (result != CEF_CDM_REGISTRATION_ERROR_NONE) {
    CEF_POST_TASK(CEF_UIT, base::Bind(DeliverWidevineCdmCallback, result,
                                      error_message, callback));
    return;
  }

  // Continue execution on the UI thread.
  CEF_POST_TASK(CEF_UIT, base::Bind(RegisterWidevineCdmOnUIThread,
                                    base::Passed(std::move(args)), callback));
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

  CEF_POST_USER_VISIBLE_TASK(
      base::Bind(LoadWidevineCdmInfoOnBlockingThread, path, callback));
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
void CefWidevineLoader::AddContentDecryptionModules(
    std::vector<content::CdmInfo>* cdms,
    std::vector<media::CdmHostFilePath>* cdm_host_file_paths) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  // Perform early plugin registration in the zygote process when the sandbox is
  // enabled to avoid "cannot open shared object file: Operation not permitted"
  // errors during plugin loading. This is because the Zygote process must pre-
  // load all plugins before initializing the sandbox.
  if (command_line.GetSwitchValueASCII(switches::kProcessType) !=
          service_manager::switches::kZygoteProcess ||
      command_line.HasSwitch(service_manager::switches::kNoSandbox)) {
    return;
  }

  // The Widevine CDM path is passed to the zygote process via
  // CefContentBrowserClient::AppendExtraCommandLineSwitches.
  const base::FilePath& base_path =
      command_line.GetSwitchValuePath(switches::kWidevineCdmPath);
  if (base_path.empty())
    return;

  // Load contents of the plugin directory synchronously. This only occurs once
  // on zygote process startup so should not have a huge performance penalty.
  CdmInfoArgs args;
  std::string error_message;
  cef_cdm_registration_error_t result =
      LoadWidevineCdmInfo(base_path, &args, &error_message);
  if (result != CEF_CDM_REGISTRATION_ERROR_NONE) {
    LOG(ERROR) << "Widevine CDM registration failed; " << error_message;
    return;
  }

  cdms->push_back(MakeCdmInfo(args));
}

#endif  // defined(OS_LINUX)

CefWidevineLoader::CefWidevineLoader() {}

CefWidevineLoader::~CefWidevineLoader() {}

#endif  // BUILDFLAG(ENABLE_WIDEVINE) && BUILDFLAG(ENABLE_LIBRARY_CDMS)
