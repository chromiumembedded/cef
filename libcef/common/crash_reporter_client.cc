// Copyright 2016 The Chromium Embedded Framework Authors. Portions copyright
// 2016 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#include "libcef/common/crash_reporter_client.h"

#include <utility>

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

#include "base/environment.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/crash/core/common/crash_key.h"
#include "content/public/common/content_switches.h"
#include "third_party/crashpad/crashpad/client/annotation.h"

#if BUILDFLAG(IS_MAC)
#include "libcef/common/util_mac.h"
#endif

#if BUILDFLAG(IS_POSIX)
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

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
#include "content/public/common/content_switches.h"
#include "libcef/common/cef_crash_report_utils.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/debug/leak_annotations.h"
#include "chrome/install_static/install_util.h"
#include "components/crash/core/app/crashpad.h"
#endif

namespace {

#if BUILDFLAG(IS_WIN)
using PathString = std::wstring;
const char kPathSep = '\\';
#else
using PathString = std::string;
#endif

PathString GetCrashConfigPath() {
#if BUILDFLAG(IS_WIN)
  // Start with the path to the running executable.
  wchar_t module_path[MAX_PATH];
  if (GetModuleFileName(nullptr, module_path, MAX_PATH) == 0) {
    return PathString();
  }

  PathString config_path = module_path;

  // Remove the executable file name.
  PathString::size_type last_backslash =
      config_path.rfind(kPathSep, config_path.size());
  if (last_backslash != PathString::npos) {
    config_path.erase(last_backslash + 1);
  }

  config_path += L"crash_reporter.cfg";
  return config_path;
#elif BUILDFLAG(IS_POSIX)
  base::FilePath config_path;

#if BUILDFLAG(IS_MAC)
  // Start with the path to the main app Resources directory. May be empty if
  // not running in an app bundle.
  config_path = util_mac::GetMainResourcesDirectory();
#endif

  if (config_path.empty()) {
    // Start with the path to the running executable.
    if (!base::PathService::Get(base::DIR_EXE, &config_path)) {
      return PathString();
    }
  }

  return config_path.Append(FILE_PATH_LITERAL("crash_reporter.cfg")).value();
#endif  // BUILDFLAG(IS_POSIX)
}

#if BUILDFLAG(IS_WIN)

// On Windows, FAT32 and NTFS both limit filenames to a maximum of 255
// characters. On POSIX systems, the typical filename length limit is 255
// character units. HFS+'s limit is actually 255 Unicode characters using
// Apple's modification of Normalization Form D, but the differences aren't
// really worth dealing with here.
const unsigned maxFilenameLength = 255;

const char kInvalidFileChars[] = "<>:\"/\\|?*";

bool isInvalidFileCharacter(unsigned char c) {
  if (c < ' ' || c == 0x7F) {
    return true;
  }
  for (char kInvalidFileChar : kInvalidFileChars) {
    if (c == kInvalidFileChar) {
      return true;
    }
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
  if (!isAbsolutePath(s)) {
    return std::string();
  }

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
  if (s.empty()) {
    return std::string();
  }

  std::string result;
  result.reserve(s.length());
  for (size_t i = 0; i < s.length(); ++i) {
    if (!isInvalidFileCharacter(s[i])) {
      result.push_back(s[i]);
    }
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
  if (ext.length() >= maxFilenameLength) {
    ext = std::string();
  }

  // Truncate an overly-long filename, reserving one character for a dot.
  std::string::size_type max_name_len = maxFilenameLength - ext.length() - 1;
  if (name.length() > max_name_len) {
    name = name.substr(0, max_name_len);
  }

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
  for (auto part : parts) {
    if (part != "." && part != "..") {
      part = sanitizePathComponent(part);
    }
    if (!result.empty() && result[result.length() - 1] != kPathSep) {
      result += kPathSep;
    }
    result += part;
  }

  return result;
}

std::string joinPath(const std::string& s1, const std::string& s2) {
  if (s1.empty() && s2.empty()) {
    return std::string();
  }
  if (s1.empty()) {
    return s2;
  }
  if (s2.empty()) {
    return s1;
  }

  // Don't try to join absolute paths on Windows.
  // Skip this check on POSIX where it's more difficult to differentiate.
  if (isAbsolutePath(s2)) {
    return s2;
  }

  std::string result = s1;
  if (result[result.size() - 1] != kPathSep) {
    result += kPathSep;
  }
  if (s2[0] == kPathSep) {
    result += s2.substr(1);
  } else {
    result += s2;
  }
  return result;
}

// This will only be non-nullptr in the chrome_elf address space.
CefCrashReporterClient* g_crash_reporter_client = nullptr;

#endif  // BUILDFLAG(IS_WIN)

const char kKeyMapDelim = ',';

std::string NormalizeCrashKey(const base::StringPiece& key) {
  std::string str(key);
  std::replace(str.begin(), str.end(), kKeyMapDelim, '-');
  if (str.length() > crashpad::Annotation::kNameMaxLength) {
    return str.substr(0, crashpad::Annotation::kNameMaxLength);
  }
  return str;
}

void ParseURL(const std::string& value, std::string* url) {
  if (value.find("http://") == 0 || value.find("https://") == 0) {
    *url = value;
    if (url->rfind('/') <= 8) {
      // Make sure the URL includes a path component. Otherwise, crash
      // upload will fail on older Windows versions due to
      // https://crbug.com/826564.
      *url += "/";
    }
  }
}

bool ParseBool(const std::string& value) {
  return base::EqualsCaseInsensitiveASCII(value, "true") || value == "1";
}

int ParseZeroBasedInt(const std::string& value) {
  int int_val;
  if (base::StringToInt(value, &int_val) && int_val > 0) {
    return int_val;
  }
  return 0;
}

}  // namespace

#if BUILDFLAG(IS_WIN)

extern "C" {

// Export functions from chrome_elf that are required by crash_reporting.cc

int __declspec(dllexport) __cdecl SetCrashKeyValueImpl(const char* key,
                                                       size_t key_size,
                                                       const char* value,
                                                       size_t value_size) {
  if (g_crash_reporter_client) {
    return g_crash_reporter_client->SetCrashKeyValue(
        base::StringPiece(key, key_size), base::StringPiece(value, value_size));
  }
  return 0;
}

int __declspec(dllexport) __cdecl IsCrashReportingEnabledImpl() {
  return g_crash_reporter_client &&
         g_crash_reporter_client->HasCrashConfigFile();
}

}  // extern "C"

// The below functions were deleted from chrome/install_static/install_util.cc
// in https://crbug.com/565446#c17.

constexpr wchar_t kUserDataDirname[] = L"User Data";

// Populates |result| with the default User Data directory for the current
// user.This may be overidden by a command line option. Returns false if all
// attempts at locating a User Data directory fail.
bool GetDefaultUserDataDirectory(std::wstring* result,
                                 const std::wstring& install_sub_directory) {
  // This environment variable should be set on Windows Vista and later
  // (https://msdn.microsoft.com/library/windows/desktop/dd378457.aspx).
  std::wstring user_data_dir =
      install_static::GetEnvironmentString(L"LOCALAPPDATA");

  if (user_data_dir.empty()) {
    // LOCALAPPDATA was not set; fallback to the temporary files path.
    DWORD size = ::GetTempPath(0, nullptr);
    if (!size) {
      return false;
    }
    user_data_dir.resize(size + 1);
    size = ::GetTempPath(size + 1, &user_data_dir[0]);
    if (!size || size >= user_data_dir.size()) {
      return false;
    }
    user_data_dir.resize(size);
  }

  result->swap(user_data_dir);
  if ((*result)[result->length() - 1] != L'\\') {
    result->push_back(L'\\');
  }
  result->append(install_sub_directory);
  result->push_back(L'\\');
  result->append(kUserDataDirname);
  return true;
}

// Populates |crash_dir| with the default crash dump location regardless of
// whether DIR_USER_DATA or DIR_CRASH_DUMPS has been overridden.
bool GetDefaultCrashDumpLocation(std::wstring* crash_dir,
                                 const std::wstring& install_sub_directory) {
  // In order to be able to start crash handling very early, we do not rely on
  // chrome's PathService entries (for DIR_CRASH_DUMPS) being available on
  // Windows. See https://crbug.com/564398.
  if (!GetDefaultUserDataDirectory(crash_dir, install_sub_directory)) {
    return false;
  }

  // We have to make sure the user data dir exists on first run. See
  // http://crbug.com/591504.
  if (!install_static::RecursiveDirectoryCreate(*crash_dir)) {
    return false;
  }
  crash_dir->append(L"\\Crashpad");
  return true;
}

#endif  // OS_WIN

CefCrashReporterClient::CefCrashReporterClient() = default;
CefCrashReporterClient::~CefCrashReporterClient() = default;

// Be aware that logging is not initialized at the time this method is called.
bool CefCrashReporterClient::ReadCrashConfigFile() {
  if (has_crash_config_file_) {
    return true;
  }

  PathString config_path = GetCrashConfigPath();
  if (config_path.empty()) {
    return false;
  }

#if BUILDFLAG(IS_WIN)
  FILE* fp = _wfopen(config_path.c_str(), L"r");
#else
  FILE* fp = fopen(config_path.c_str(), "r");
#endif
  if (!fp) {
    return false;
  }

  char line[1000];

  size_t small_index = 0;
  size_t medium_index = 0;
  size_t large_index = 0;
  std::string map_keys;

  enum section {
    kNoSection,
    kConfigSection,
    kCrashKeysSection,
  } current_section = kNoSection;

  while (fgets(line, sizeof(line) - 1, fp) != nullptr) {
    std::string str = line;
    base::TrimString(str, base::kWhitespaceASCII, &str);
    if (str.empty() || str[0] == '#') {
      continue;
    }

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

    if (current_section == kNoSection) {
      continue;
    }

    size_t div = str.find('=');
    if (div == std::string::npos) {
      continue;
    }

    std::string name_str = str.substr(0, div);
    base::TrimString(name_str, base::kWhitespaceASCII, &name_str);
    std::string val_str = str.substr(div + 1);
    base::TrimString(val_str, base::kWhitespaceASCII, &val_str);
    if (name_str.empty()) {
      continue;
    }

    if (current_section == kConfigSection) {
      if (name_str == "ServerURL") {
        ParseURL(val_str, &server_url_);
      } else if (name_str == "ProductName") {
        product_name_ = val_str;
      } else if (name_str == "ProductVersion") {
        product_version_ = val_str;
      } else if (name_str == "RateLimitEnabled") {
        rate_limit_ = ParseBool(val_str);
      } else if (name_str == "MaxUploadsPerDay") {
        max_uploads_ = ParseZeroBasedInt(val_str);
      } else if (name_str == "MaxDatabaseSizeInMb") {
        max_db_size_ = ParseZeroBasedInt(val_str);
      } else if (name_str == "MaxDatabaseAgeInDays") {
        max_db_age_ = ParseZeroBasedInt(val_str);
      }
#if BUILDFLAG(IS_WIN)
      else if (name_str == "ExternalHandler") {
        if (!val_str.empty()) {
          external_handler_ = sanitizePath(val_str);
        }
      } else if (name_str == "AppName") {
        if (!val_str.empty()) {
          val_str = sanitizePathComponent(val_str);
          if (!val_str.empty()) {
            app_name_ = val_str;
          }
        }
      }
#elif BUILDFLAG(IS_MAC)
      else if (name_str == "BrowserCrashForwardingEnabled") {
        enable_browser_crash_forwarding_ = ParseBool(val_str);
      }
#endif
    } else if (current_section == kCrashKeysSection) {
      // Skip duplicate definitions.
      if (!crash_keys_.empty() &&
          crash_keys_.find(name_str) != crash_keys_.end()) {
        continue;
      }

      KeySize size;
      size_t index;
      char group;
      if (val_str == "small") {
        size = SMALL_SIZE;
        index = small_index++;
        group = 'S';
      } else if (val_str == "medium") {
        size = MEDIUM_SIZE;
        index = medium_index++;
        group = 'M';
      } else if (val_str == "large") {
        size = LARGE_SIZE;
        index = large_index++;
        group = 'L';
      } else {
        continue;
      }

      name_str = NormalizeCrashKey(name_str);
      crash_keys_.insert(std::make_pair(name_str, std::make_pair(size, index)));

      const std::string& key =
          std::string(1, group) + "-" + std::string(1, 'A' + index);
      if (!map_keys.empty()) {
        map_keys.append(std::string(1, kKeyMapDelim));
      }
      map_keys.append(key + "=" + name_str);
    }
  }

  fclose(fp);

  if (!map_keys.empty()) {
    // Split |map_keys| across multiple Annotations if necessary.
    // Must match the logic in crash_report_utils::FilterParameters.
    using IDKey =
        crash_reporter::CrashKeyString<crashpad::Annotation::kValueMaxSize>;
    static IDKey ids[] = {
        {"K-A", IDKey::Tag::kArray},
        {"K-B", IDKey::Tag::kArray},
        {"K-C", IDKey::Tag::kArray},
    };

    // Make sure we can fit all possible name/value pairs.
    static_assert(std::size(ids) * crashpad::Annotation::kValueMaxSize >=
                      3 * 26 /* sizes (small, medium, large) * slots (A to Z) */
                          * (3 + 2 /* key size ("S-A") + delim size ("=,") */
                             + crashpad::Annotation::kNameMaxLength),
                  "Not enough storage for key map");

    size_t offset = 0;
    for (auto& id : ids) {
      size_t length = std::min(map_keys.size() - offset,
                               crashpad::Annotation::kValueMaxSize);
      id.Set(base::StringPiece(map_keys.data() + offset, length));
      offset += length;
      if (offset >= map_keys.size()) {
        break;
      }
    }
  }

  // Allow override of some values via environment variables.
  {
    std::unique_ptr<base::Environment> env(base::Environment::Create());
    std::string val_str;

    if (env->GetVar("CEF_CRASH_REPORTER_SERVER_URL", &val_str)) {
      ParseURL(val_str, &server_url_);
    }
    if (env->GetVar("CEF_CRASH_REPORTER_RATE_LIMIT_ENABLED", &val_str)) {
      rate_limit_ = ParseBool(val_str);
    }
  }

  has_crash_config_file_ = true;
  return true;
}

bool CefCrashReporterClient::HasCrashConfigFile() const {
  return has_crash_config_file_;
}

#if BUILDFLAG(IS_WIN)

// static
void CefCrashReporterClient::InitializeCrashReportingForProcess() {
  if (g_crash_reporter_client) {
    return;
  }

  g_crash_reporter_client = new CefCrashReporterClient();
  ANNOTATE_LEAKING_OBJECT_PTR(g_crash_reporter_client);

  if (!g_crash_reporter_client->ReadCrashConfigFile()) {
    return;
  }

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
          process_type.empty(), install_static::WideToUTF8(process_type),
          std::string(), base::FilePath());
    } else {
      crash_reporter::InitializeCrashpad(
          process_type.empty(), install_static::WideToUTF8(process_type));
    }
  }
}

bool CefCrashReporterClient::GetAlternativeCrashDumpLocation(
    std::wstring* crash_dir) {
  // By setting the BREAKPAD_DUMP_LOCATION environment variable, an alternate
  // location to write breakpad crash dumps can be set.
  *crash_dir = install_static::GetEnvironmentString(L"BREAKPAD_DUMP_LOCATION");
  return !crash_dir->empty();
}

void CefCrashReporterClient::GetProductNameAndVersion(
    const std::wstring& exe_path,
    std::wstring* product_name,
    std::wstring* version,
    std::wstring* special_build,
    std::wstring* channel_name) {
  *product_name = base::ASCIIToWide(product_name_);
  *version = base::ASCIIToWide(product_version_);
  *special_build = std::wstring();
  *channel_name = std::wstring();
}

bool CefCrashReporterClient::GetCrashDumpLocation(std::wstring* crash_dir) {
  // By setting the BREAKPAD_DUMP_LOCATION environment variable, an alternate
  // location to write breakpad crash dumps can be set.
  if (GetAlternativeCrashDumpLocation(crash_dir)) {
    return true;
  }

  return GetDefaultCrashDumpLocation(crash_dir, base::UTF8ToWide(app_name_));
}

bool CefCrashReporterClient::GetCrashMetricsLocation(
    std::wstring* metrics_dir) {
  return GetDefaultUserDataDirectory(metrics_dir, base::UTF8ToWide(app_name_));
}

#elif BUILDFLAG(IS_POSIX)

void CefCrashReporterClient::GetProductNameAndVersion(const char** product_name,
                                                      const char** version) {
  *product_name = product_name_.c_str();
  *version = product_version_.c_str();
}

void CefCrashReporterClient::GetProductNameAndVersion(std::string* product_name,
                                                      std::string* version,
                                                      std::string* channel) {
  *product_name = product_name_;
  *version = product_version_;
}

bool CefCrashReporterClient::GetCrashDumpLocation(base::FilePath* crash_dir) {
  // By setting the BREAKPAD_DUMP_LOCATION environment variable, an alternate
  // location to write breakpad crash dumps can be set.
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  std::string alternate_crash_dump_location;
  if (env->GetVar("BREAKPAD_DUMP_LOCATION", &alternate_crash_dump_location)) {
    base::FilePath crash_dumps_dir_path =
        base::FilePath::FromUTF8Unsafe(alternate_crash_dump_location);
    base::PathService::Override(chrome::DIR_CRASH_DUMPS, crash_dumps_dir_path);
  }
  return base::PathService::Get(chrome::DIR_CRASH_DUMPS, crash_dir);
}

#endif  // !BUILDFLAG(IS_POSIX)

bool CefCrashReporterClient::GetCollectStatsConsent() {
  return true;
}

bool CefCrashReporterClient::GetCollectStatsInSample() {
  return true;
}

bool CefCrashReporterClient::ReportingIsEnforcedByPolicy(
    bool* crashpad_enabled) {
  *crashpad_enabled = true;
  return true;
}

std::string CefCrashReporterClient::GetUploadUrl() {
  return server_url_;
}

// See HandlerMain() in third_party/crashpad/crashpad/handler/handler_main.cc
// for supported arguments.
void CefCrashReporterClient::GetCrashOptionalArguments(
    std::vector<std::string>* arguments) {
  if (!rate_limit_) {
    arguments->emplace_back("--no-rate-limit");
  }

  if (max_uploads_ > 0) {
    arguments->push_back(std::string("--max-uploads=") +
                         base::NumberToString(max_uploads_));
  }

  if (max_db_size_ > 0) {
    arguments->push_back(std::string("--max-db-size=") +
                         base::NumberToString(max_db_size_));
  }

  if (max_db_age_ > 0) {
    arguments->push_back(std::string("--max-db-age=") +
                         base::NumberToString(max_db_age_));
  }
}

#if BUILDFLAG(IS_WIN)

std::wstring CefCrashReporterClient::GetCrashExternalHandler(
    const std::wstring& exe_dir) {
  if (external_handler_.empty()) {
    return CrashReporterClient::GetCrashExternalHandler(exe_dir);
  }
  if (isAbsolutePath(external_handler_)) {
    return base::UTF8ToWide(external_handler_);
  }
  return base::UTF8ToWide(
      joinPath(base::WideToUTF8(exe_dir), external_handler_));
}

bool CefCrashReporterClient::HasCrashExternalHandler() const {
  return !external_handler_.empty();
}

#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
bool CefCrashReporterClient::EnableBrowserCrashForwarding() {
  return enable_browser_crash_forwarding_;
}
#endif

// The new Crashpad Annotation API requires that annotations be declared using
// static storage. We work around this limitation by defining a fixed amount of
// storage for each key size and later substituting the actual key name during
// crash dump processing.

#define IDKEY(name) \
  { name, IDKey::Tag::kArray }

#define IDKEY_ENTRIES(n)                                                     \
  IDKEY(n "-A"), IDKEY(n "-B"), IDKEY(n "-C"), IDKEY(n "-D"), IDKEY(n "-E"), \
      IDKEY(n "-F"), IDKEY(n "-G"), IDKEY(n "-H"), IDKEY(n "-I"),            \
      IDKEY(n "-J"), IDKEY(n "-K"), IDKEY(n "-L"), IDKEY(n "-M"),            \
      IDKEY(n "-N"), IDKEY(n "-O"), IDKEY(n "-P"), IDKEY(n "-Q"),            \
      IDKEY(n "-R"), IDKEY(n "-S"), IDKEY(n "-T"), IDKEY(n "-U"),            \
      IDKEY(n "-V"), IDKEY(n "-W"), IDKEY(n "-X"), IDKEY(n "-Y"),            \
      IDKEY(n "-Z")

#define IDKEY_FUNCTION(name, size_)                                          \
  static_assert(size_ <= crashpad::Annotation::kValueMaxSize,                \
                "Annotation size is too large.");                            \
  bool Set##name##Annotation(size_t index, const base::StringPiece& value) { \
    using IDKey = crash_reporter::CrashKeyString<size_>;                     \
    static IDKey ids[] = {IDKEY_ENTRIES(#name)};                             \
    if (index < std::size(ids)) {                                            \
      if (value.empty()) {                                                   \
        ids[index].Clear();                                                  \
      } else {                                                               \
        ids[index].Set(value);                                               \
      }                                                                      \
      return true;                                                           \
    }                                                                        \
    return false;                                                            \
  }

// The first argument must be kept synchronized with the logic in
// CefCrashReporterClient::ReadCrashConfigFile and
// crash_report_utils::FilterParameters.
IDKEY_FUNCTION(S, 64)
IDKEY_FUNCTION(M, 256)
IDKEY_FUNCTION(L, 1024)

bool CefCrashReporterClient::SetCrashKeyValue(const base::StringPiece& key,
                                              const base::StringPiece& value) {
  if (key.empty() || crash_keys_.empty()) {
    return false;
  }

  KeyMap::const_iterator it = crash_keys_.find(NormalizeCrashKey(key));
  if (it == crash_keys_.end()) {
    return false;
  }

  const KeySize size = it->second.first;
  const size_t index = it->second.second;

  base::AutoLock lock_scope(crash_key_lock_);

  switch (size) {
    case SMALL_SIZE:
      return SetSAnnotation(index, value);
    case MEDIUM_SIZE:
      return SetMAnnotation(index, value);
    case LARGE_SIZE:
      return SetLAnnotation(index, value);
  }

  return false;
}
