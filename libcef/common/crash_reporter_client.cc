// Copyright 2016 The Chromium Embedded Framework Authors. Portions copyright
// 2016 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#include "libcef/common/crash_reporter_client.h"

#include <utility>

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/crash_keys.h"

#if defined(OS_MACOSX)
#include "libcef/common/util_mac.h"
#endif

#if defined(OS_POSIX)
// Don't use CommandLine, FilePath or PathService on Windows. FilePath has
// dependencies outside of kernel32, which is disallowed by chrome_elf.
// CommandLine and PathService depend on global state that will not be
// initialized at the time the CefCrashReporterClient object is created.
#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX)
#include "content/public/common/content_switches.h"
#endif

#if defined(OS_WIN)
#include "base/debug/leak_annotations.h"
#include "chrome/install_static/install_util.h"
#include "components/crash/content/app/crashpad.h"
#endif

namespace {

#if defined(OS_WIN)
typedef base::string16 PathString;
const char kPathSep = '\\';
#else
typedef std::string PathString;
#endif

PathString GetCrashConfigPath() {
#if defined(OS_WIN)
  // Start with the path to the running executable.
  wchar_t module_path[MAX_PATH];
  if (GetModuleFileName(nullptr, module_path, MAX_PATH) == 0)
    return PathString();

  PathString config_path = module_path;

  // Remove the executable file name.
  PathString::size_type last_backslash =
      config_path.rfind(kPathSep, config_path.size());
  if (last_backslash != PathString::npos)
    config_path.erase(last_backslash + 1);

  config_path += L"crash_reporter.cfg";
  return config_path;
#elif defined(OS_POSIX)
  base::FilePath config_path;

#if defined(OS_MACOSX)
  // Start with the path to the main app Resources directory. May be empty if
  // not running in an app bundle.
  config_path = util_mac::GetMainResourcesDirectory();
#endif

  if (config_path.empty()) {
    // Start with the path to the running executable.
    if (!PathService::Get(base::DIR_EXE, &config_path))
      return PathString();
  }

  return config_path.Append(FILE_PATH_LITERAL("crash_reporter.cfg")).value();
#endif  // defined(OS_POSIX)
}

#if defined(OS_WIN)

// On Windows, FAT32 and NTFS both limit filenames to a maximum of 255
// characters. On POSIX systems, the typical filename length limit is 255
// character units. HFS+'s limit is actually 255 Unicode characters using
// Apple's modification of Normalization Form D, but the differences aren't
// really worth dealing with here.
const unsigned maxFilenameLength = 255;

const char kInvalidFileChars[] = "<>:\"/\\|?*";

bool isInvalidFileCharacter(unsigned char c) {
  if (c < ' ' || c == 0x7F)
    return true;
  for(size_t i = 0; i < sizeof(kInvalidFileChars); ++i) {
    if (c == kInvalidFileChars[i])
      return true;
  }
  return false;
}

bool isAbsolutePath(const std::string& s) {
  // Check for local paths (beginning with "c:\") and network paths
  // (beginning with "\\").
  return s.length() > 2 &&
         ((isalpha(s[0]) && s[1] == ':' && s[2] == kPathSep) ||
          (s[0] == kPathSep && s[1] == kPathSep));
}

std::string extractAbsolutePathStart(std::string& s) {
  if (!isAbsolutePath(s))
    return std::string();

  std::string start;
  if (s[0] == kPathSep) {
    // Network path.
    start = s.substr(0, 2);
    s = s.substr(2);
  } else {
    // Local path.
    start = s.substr(0, 3);
    s = s.substr(3);
  }
  return start;
}

std::string sanitizePathComponentPart(const std::string& s) {
  if (s.empty())
    return std::string();

  std::string result;
  result.reserve(s.length());
  for (size_t i = 0; i < s.length(); ++i) {
    if (!isInvalidFileCharacter(s[i]))
      result.push_back(s[i]);
  }
  return result;
}

std::string sanitizePathComponent(const std::string& s) {
  std::string name, ext;

  // Separate name and extension, if any.
  std::string::size_type pos = s.rfind('.');
  if (pos != std::string::npos) {
    name = s.substr(0, pos);
    ext = s.substr(pos + 1);
  } else {
    name = s;
  }

  // Remove invalid characters.
  name = sanitizePathComponentPart(name);
  ext = sanitizePathComponentPart(ext);

  // Remove a ridiculously-long extension.
  if (ext.length() >= maxFilenameLength)
    ext = std::string();

  // Truncate an overly-long filename, reserving one character for a dot.
  std::string::size_type max_name_len = maxFilenameLength - ext.length() - 1;
  if (name.length() > max_name_len)
    name = name.substr(0, max_name_len);

  return ext.empty() ? name : name + "." + ext;
}

std::string sanitizePath(const std::string& s) {
  std::string path = s;

  // Extract the absolute path start component, if any (e.g. "c:\" on Windows).
  std::string result = extractAbsolutePathStart(path);
  result.reserve(s.length());

  std::vector<std::string> parts =
      base::SplitString(path, std::string() + kPathSep, base::KEEP_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  for (size_t i = 0; i < parts.size(); ++i) {
    std::string part = parts[i];
    if (part != "." && part != "..")
      part = sanitizePathComponent(part);
    if (!result.empty() && result[result.length()-1] != kPathSep)
      result += kPathSep;
    result += part;
  }

  return result;
}

std::string joinPath(const std::string& s1, const std::string& s2) {
  if (s1.empty() && s2.empty())
    return std::string();
  if (s1.empty())
    return s2;
  if (s2.empty())
    return s1;

  // Don't try to join absolute paths on Windows.
  // Skip this check on POSIX where it's more difficult to differentiate.
  if (isAbsolutePath(s2))
    return s2;

  std::string result = s1;
  if (result[result.size() - 1] != kPathSep)
    result += kPathSep;
  if (s2[0] == kPathSep)
    result += s2.substr(1);
  else
    result += s2;
  return result;
}


// This will only be non-nullptr in the chrome_elf address space.
CefCrashReporterClient* g_crash_reporter_client = nullptr;

#endif  // defined(OS_WIN)

}  // namespace

#if defined(OS_WIN)

extern "C" {

// Export functions from chrome_elf that are required by
// crash_reporting_win::InitializeCrashReportingForModule().

size_t __declspec(dllexport) __cdecl GetCrashKeyCountImpl() {
  if (!g_crash_reporter_client)
    return 0;
  return g_crash_reporter_client->GetCrashKeyCount();
}

bool __declspec(dllexport) __cdecl GetCrashKeyImpl(size_t index,
                                                   const char** key_name,
                                                   size_t* max_length) {
  if (!g_crash_reporter_client)
    return false;
  return g_crash_reporter_client->GetCrashKey(index, key_name, max_length);
}

}  // extern "C"


// The below functions were deleted from chrome/install_static/install_util.cc
// in https://crbug.com/565446#c17.

constexpr wchar_t kUserDataDirname[] = L"User Data";

// Populates |result| with the default User Data directory for the current
// user.This may be overidden by a command line option. Returns false if all
// attempts at locating a User Data directory fail.
bool GetDefaultUserDataDirectory(
    std::wstring* result,
    const std::wstring& install_sub_directory) {
  // This environment variable should be set on Windows Vista and later
  // (https://msdn.microsoft.com/library/windows/desktop/dd378457.aspx).
  std::wstring user_data_dir =
      install_static::GetEnvironmentString16(L"LOCALAPPDATA");

  if (user_data_dir.empty()) {
    // LOCALAPPDATA was not set; fallback to the temporary files path.
    DWORD size = ::GetTempPath(0, nullptr);
    if (!size)
      return false;
    user_data_dir.resize(size + 1);
    size = ::GetTempPath(size + 1, &user_data_dir[0]);
    if (!size || size >= user_data_dir.size())
      return false;
    user_data_dir.resize(size);
  }

  result->swap(user_data_dir);
  if ((*result)[result->length() - 1] != L'\\')
    result->push_back(L'\\');
  result->append(install_sub_directory);
  result->push_back(L'\\');
  result->append(kUserDataDirname);
  return true;
}

// Populates |crash_dir| with the default crash dump location regardless of
// whether DIR_USER_DATA or DIR_CRASH_DUMPS has been overridden.
bool GetDefaultCrashDumpLocation(
    std::wstring* crash_dir,
    const std::wstring& install_sub_directory) {
  // In order to be able to start crash handling very early, we do not rely on
  // chrome's PathService entries (for DIR_CRASH_DUMPS) being available on
  // Windows. See https://crbug.com/564398.
  if (!GetDefaultUserDataDirectory(crash_dir, install_sub_directory))
    return false;

  // We have to make sure the user data dir exists on first run. See
  // http://crbug.com/591504.
  if (!install_static::RecursiveDirectoryCreate(*crash_dir))
    return false;
  crash_dir->append(L"\\Crashpad");
  return true;
}

#endif  // OS_WIN

CefCrashReporterClient::CefCrashReporterClient() {}
CefCrashReporterClient::~CefCrashReporterClient() {}

// Be aware that logging is not initialized at the time this method is called.
bool CefCrashReporterClient::ReadCrashConfigFile() {
  if (has_crash_config_file_)
    return true;

  PathString config_path = GetCrashConfigPath();
  if (config_path.empty())
    return false;

#if defined(OS_WIN)
  FILE* fp = _wfopen(config_path.c_str(), L"r");
#else
  FILE* fp = fopen(config_path.c_str(), "r");
#endif
  if (!fp)
    return false;

  char line[1000];

  enum section {
    kNoSection,
    kConfigSection,
    kCrashKeysSection,
  } current_section = kNoSection;

  while (fgets(line, sizeof(line) - 1, fp) != NULL) {
    std::string str = line;
    base::TrimString(str, base::kWhitespaceASCII, &str);
    if (str.empty() || str[0] == '#')
      continue;

    if (str == "[Config]") {
      current_section = kConfigSection;
      continue;
    } else if (str == "[CrashKeys]") {
      current_section = kCrashKeysSection;
      continue;
    } else if (str[0] == '[') {
      current_section = kNoSection;
      continue;
    }

    if (current_section == kNoSection)
      continue;

    size_t div = str.find('=');
    if (div == std::string::npos)
      continue;

    std::string name_str = str.substr(0, div);
    base::TrimString(name_str, base::kWhitespaceASCII, &name_str);
    std::string val_str = str.substr(div + 1);
    base::TrimString(val_str, base::kWhitespaceASCII, &val_str);
    if (name_str.empty())
      continue;

    if (current_section == kConfigSection) {
      if (name_str == "ServerURL") {
        if (val_str.find("http://") == 0 || val_str.find("https://") == 0)
          server_url_ = val_str;
      } else if (name_str == "ProductName") {
        product_name_ = val_str;
      } else if (name_str == "ProductVersion") {
        product_version_ = val_str;
      } else if (name_str == "RateLimitEnabled") {
        rate_limit_ = (base::EqualsCaseInsensitiveASCII(val_str, "true") ||
                       val_str == "1");
      } else if (name_str == "MaxUploadsPerDay") {
        if (base::StringToInt(val_str, &max_uploads_)) {
          if (max_uploads_ < 0)
            max_uploads_ = 0;
        }
      } else if (name_str == "MaxDatabaseSizeInMb") {
        if (base::StringToInt(val_str, &max_db_size_)) {
          if (max_db_size_ < 0)
            max_db_size_ = 0;
        }
      } else if (name_str == "MaxDatabaseAgeInDays") {
        if (base::StringToInt(val_str, &max_db_age_)) {
          if (max_db_age_ < 0)
            max_db_age_ = 0;
        }
      }
#if defined(OS_WIN)
      else if (name_str == "ExternalHandler") {
        if (!val_str.empty())
          external_handler_ = sanitizePath(val_str);
      } else if (name_str == "AppName") {
        if (!val_str.empty()) {
          val_str = sanitizePathComponent(val_str);
          if (!val_str.empty())
            app_name_ = val_str;
        }
      }
#elif defined(OS_MACOSX)
      else if (name_str == "BrowserCrashForwardingEnabled") {
        enable_browser_crash_forwarding_ =
            (base::EqualsCaseInsensitiveASCII(val_str, "true") ||
             val_str == "1");
      }
#endif
    } else if (current_section == kCrashKeysSection) {
      size_t max_size = 0;
      if (val_str == "small")
        max_size = crash_keys::kSmallSize;
      else if (val_str == "medium")
        max_size = crash_keys::kMediumSize;
      else if (val_str == "large")
        max_size = crash_keys::kLargeSize;

      if (max_size == 0)
        continue;

      crash_keys_.push_back({name_str, max_size});
    }
  }

  fclose(fp);

  // Add the list of potential crash keys from chrome, content and other layers.
  // Do it here so that they're also exported to the libcef module for Windows.
  {
    std::vector<base::debug::CrashKey> keys;
    crash_keys::GetChromeCrashKeys(keys);

    if (!keys.empty()) {
      crash_keys_.reserve(crash_keys_.size() + keys.size());
      for (const auto& key : keys) {
        crash_keys_.push_back({key.key_name, key.max_length});
      }
    }
  }

  has_crash_config_file_ = true;
  return true;
}

bool CefCrashReporterClient::HasCrashConfigFile() const {
  return has_crash_config_file_;
}

#if defined(OS_WIN)

// static
void CefCrashReporterClient::InitializeCrashReportingForProcess() {
  if (g_crash_reporter_client)
    return;

  g_crash_reporter_client = new CefCrashReporterClient();
  ANNOTATE_LEAKING_OBJECT_PTR(g_crash_reporter_client);

  if (!g_crash_reporter_client->ReadCrashConfigFile())
    return;

  std::wstring process_type = install_static::GetSwitchValueFromCommandLine(
      ::GetCommandLineW(), install_static::kProcessType);
  if (process_type != install_static::kCrashpadHandler) {
    crash_reporter::SetCrashReporterClient(g_crash_reporter_client);

    // If |embedded_handler| is true then we launch another instance of the main
    // executable as the crashpad-handler process.
    const bool embedded_handler =
        !g_crash_reporter_client->HasCrashExternalHandler();
    if (embedded_handler) {
      crash_reporter::InitializeCrashpadWithEmbeddedHandler(
          process_type.empty(), install_static::UTF16ToUTF8(process_type));
    } else {
      crash_reporter::InitializeCrashpad(
          process_type.empty(), install_static::UTF16ToUTF8(process_type));
    }
  }
}

bool CefCrashReporterClient::GetAlternativeCrashDumpLocation(
    base::string16* crash_dir) {
  // By setting the BREAKPAD_DUMP_LOCATION environment variable, an alternate
  // location to write breakpad crash dumps can be set.
  *crash_dir =
      install_static::GetEnvironmentString16(L"BREAKPAD_DUMP_LOCATION");
  return !crash_dir->empty();
}

void CefCrashReporterClient::GetProductNameAndVersion(
    const base::string16& exe_path,
    base::string16* product_name,
    base::string16* version,
    base::string16* special_build,
    base::string16* channel_name) {
  *product_name = base::ASCIIToUTF16(product_name_);
  *version = base::ASCIIToUTF16(product_version_);
  *special_build = base::string16();
  *channel_name = base::string16();
}

bool CefCrashReporterClient::GetCrashDumpLocation(base::string16* crash_dir) {
  // By setting the BREAKPAD_DUMP_LOCATION environment variable, an alternate
  // location to write breakpad crash dumps can be set.
  if (GetAlternativeCrashDumpLocation(crash_dir))
    return true;

  return GetDefaultCrashDumpLocation(crash_dir, base::UTF8ToUTF16(app_name_));
}

bool CefCrashReporterClient::GetCrashMetricsLocation(
    base::string16* metrics_dir) {
  return GetDefaultUserDataDirectory(metrics_dir, base::UTF8ToUTF16(app_name_));
}

#elif defined(OS_POSIX)

void CefCrashReporterClient::GetProductNameAndVersion(
    const char** product_name,
    const char** version) {
  *product_name = product_name_.c_str();
  *version = product_version_.c_str();
}

#if !defined(OS_MACOSX)

base::FilePath CefCrashReporterClient::GetReporterLogFilename() {
  return base::FilePath(FILE_PATH_LITERAL("uploads.log"));
}

bool CefCrashReporterClient::EnableBreakpadForProcess(
    const std::string& process_type) {
  return process_type == switches::kRendererProcess ||
         process_type == switches::kPpapiPluginProcess ||
         process_type == switches::kZygoteProcess ||
         process_type == switches::kGpuProcess;
}

#endif  // !defined(OS_MACOSX)

bool CefCrashReporterClient::GetCrashDumpLocation(base::FilePath* crash_dir) {
  // By setting the BREAKPAD_DUMP_LOCATION environment variable, an alternate
  // location to write breakpad crash dumps can be set.
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  std::string alternate_crash_dump_location;
  if (env->GetVar("BREAKPAD_DUMP_LOCATION", &alternate_crash_dump_location)) {
    base::FilePath crash_dumps_dir_path =
        base::FilePath::FromUTF8Unsafe(alternate_crash_dump_location);
    PathService::Override(chrome::DIR_CRASH_DUMPS, crash_dumps_dir_path);
  }
  return PathService::Get(chrome::DIR_CRASH_DUMPS, crash_dir);
}

#endif  // !defined(OS_POSIX)

bool CefCrashReporterClient::GetCollectStatsConsent() {
  return true;
}

bool CefCrashReporterClient::GetCollectStatsInSample() {
  return true;
}

#if defined(OS_WIN) || defined(OS_MACOSX)
bool CefCrashReporterClient::ReportingIsEnforcedByPolicy(
    bool* crashpad_enabled) {
  *crashpad_enabled = true;
  return true;
}
#endif

size_t CefCrashReporterClient::RegisterCrashKeys() {
  std::vector<base::debug::CrashKey> keys;

  if (!crash_keys_.empty()) {
    keys.reserve(crash_keys_.size());
    for (const auto& key : crash_keys_) {
      keys.push_back({key.key_name_.c_str(), key.max_length_});
    }
  }

  return base::debug::InitCrashKeys(&keys[0], keys.size(),
                                    crash_keys::kChunkMaxLength);
}

#if defined(OS_POSIX) && !defined(OS_MACOSX)
bool CefCrashReporterClient::IsRunningUnattended() {
  // Crash upload will only be enabled with Breakpad on Linux if this method
  // returns false.
  return false;
}
#endif

#if defined(OS_WIN)
size_t CefCrashReporterClient::GetCrashKeyCount() const {
  return crash_keys_.size();
}

bool CefCrashReporterClient::GetCrashKey(size_t index,
                                         const char** key_name,
                                         size_t* max_length) const {
  if (index >= crash_keys_.size())
    return false;

  const auto& key = crash_keys_[index];
  *key_name = key.key_name_.c_str();
  *max_length = key.max_length_;
  return true;
}
#endif  // defined(OS_WIN)

std::string CefCrashReporterClient::GetCrashServerURL() {
  return server_url_;
}

// See HandlerMain() in third_party/crashpad/crashpad/handler/handler_main.cc
// for supported arguments.
void CefCrashReporterClient::GetCrashOptionalArguments(
    std::vector<std::string>* arguments) {
  if (!rate_limit_)
    arguments->push_back(std::string("--no-rate-limit"));

  if (max_uploads_ > 0) {
    arguments->push_back(
        std::string("--max-uploads=") + base::IntToString(max_uploads_));
  }

  if (max_db_size_ > 0) {
    arguments->push_back(
        std::string("--max-db-size=") + base::IntToString(max_db_size_));
  }

  if (max_db_age_ > 0) {
    arguments->push_back(
        std::string("--max-db-age=") + base::IntToString(max_db_age_));
  }
}

#if defined(OS_WIN)

base::string16 CefCrashReporterClient::GetCrashExternalHandler(
    const base::string16& exe_dir) {
  if (external_handler_.empty())
    return CrashReporterClient::GetCrashExternalHandler(exe_dir);
  if (isAbsolutePath(external_handler_))
    return base::UTF8ToUTF16(external_handler_);
  return base::UTF8ToWide(
      joinPath(base::UTF16ToUTF8(exe_dir), external_handler_));
}

bool CefCrashReporterClient::HasCrashExternalHandler() const {
  return !external_handler_.empty();
}

#endif  // defined(OS_WIN)

#if defined(OS_MACOSX)
bool CefCrashReporterClient::EnableBrowserCrashForwarding() {
  return enable_browser_crash_forwarding_;
}
#endif
