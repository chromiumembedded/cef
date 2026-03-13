// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_CONFIG_H_
#define CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_CONFIG_H_

#include <windows.h>

#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace cef_installer {

namespace config_fields {

inline constexpr char kAppIdField[] = "appid";
inline constexpr char kVminField[] = "vmin";
inline constexpr char kVmaxField[] = "vmax";
inline constexpr char kAbiHashField[] = "abi_hash";
inline constexpr char kChannelField[] = "channel";
inline constexpr char kCdnUrlsField[] = "cdn_urls";
inline constexpr char kLaunchHealthField[] = "launch_health";
inline constexpr char kEnableExplicitModesField[] = "enable_explicit_modes";
inline constexpr char kUncheckedCefPathField[] = "unchecked_cef_path";
inline constexpr char kBundledCefPathField[] = "bundled_cef_path";
inline constexpr char kInstallPathField[] = "install_path";
inline constexpr char kCertificateThumbprintField[] = "certificate_thumbprint";
inline constexpr char kForceCheckField[] = "force_check";
inline constexpr char kShowProgressUiField[] = "show_progress_ui";
inline constexpr char kParentWindowField[] = "parent_window";
inline constexpr char kLocalDownloadPathField[] = "local_download_path";
inline constexpr char kLogLevelField[] = "log_level";
inline constexpr char kDownloadTimeoutMsField[] = "download_timeout_ms";
inline constexpr char kMaxAgeDaysField[] = "max_age_days";

inline constexpr std::array<std::string_view, 11> kApplicationConfigFields = {
    kAppIdField,
    kVminField,
    kVmaxField,
    kAbiHashField,
    kChannelField,
    kCdnUrlsField,
    kLaunchHealthField,
    kEnableExplicitModesField,
    kUncheckedCefPathField,
    kBundledCefPathField,
    kInstallPathField,
};

inline constexpr std::array<std::string_view, 11> kExtendedConfigFields = {
    kCdnUrlsField,        kInstallPathField,
    kBundledCefPathField, kCertificateThumbprintField,
    kForceCheckField,     kShowProgressUiField,
    kParentWindowField,   kLocalDownloadPathField,
    kLogLevelField,       kDownloadTimeoutMsField,
    kMaxAgeDaysField,
};

constexpr bool IsRecognizedConfigField(std::string_view key) {
  for (const std::string_view field : kApplicationConfigFields) {
    if (field == key) {
      return true;
    }
  }
  for (const std::string_view field : kExtendedConfigFields) {
    if (field == key) {
      return true;
    }
  }
  return false;
}

}  // namespace config_fields

// Controls local launch-health tracking for this application.
enum class LaunchHealthMode {
  kOff,
  kExplicit,
  kExitCode,
};

const char* LaunchHealthModeToString(LaunchHealthMode mode);

// Controls which optional fields are parsed from config JSON. Each flag
// corresponds to a field that is only meaningful in certain contexts (e.g.,
// client DLL resource vs. bootstrap resource or operation API). When a flag is
// false, the corresponding Config field retains its default value regardless
// of what the JSON contains.
//
// SECURITY: install_path is accepted only from the trusted client DLL resource
// as an application-owned namespace selector. Bootstrap resources cannot
// select it. CDN URL lists are accepted from every selected application config
// source. Command-line overrides for sensitive fields are compiled out of
// official release builds entirely.
struct ConfigParseOptions {
  bool allow_enable_explicit_modes = false;
  bool allow_unchecked_cef_path = false;
  bool allow_bundled_cef_path = false;
  bool allow_install_path = false;
};

// Application configuration loaded from an embedded resource.
struct Config {
  std::string appid;     // UUID - unique identifier that never changes
  std::string vmin;      // Minimum version (required, e.g., "137.1")
  std::string vmax;      // Maximum version (optional, e.g., "137.99")
  std::string abi_hash;  // Sandbox compatibility hash (16-char hex)
  std::string channel;   // Release channel: "" (stable, default) or "beta"

  // Ordered application CDN base URLs. Empty means the effective-source
  // resolver supplies the hardcoded default after considering policy and
  // operation-specific sources.
  std::vector<std::string> cdn_urls;

  // Local launch-health tracking is opt-in. kExplicit is recommended for
  // applications that can reliably call RunInstaller("launch_success");
  // kExitCode preserves the legacy exit-code classification behavior.
  LaunchHealthMode launch_health = LaunchHealthMode::kOff;

  // Enable standalone installer features. When true in the bootstrap's embedded
  // config:
  // 1. Allows explicit /cef-update and /cef-uninstall commands in standalone
  // mode
  //    (no client DLL) and when a client DLL is present.
  // 2. Allows auto-install when no client DLL is present.
  //
  // When false (default), explicit commands and standalone auto-install are
  // blocked in official builds. The bootstrap can only function as a launcher
  // when a client DLL is present.
  //
  // This prevents signed bootstrap binaries from being repurposed as
  // arbitrary CEF installers. In non-official builds all standalone operations
  // are allowed regardless of this flag (for development/testing).
  bool enable_explicit_modes = false;

  // Path to a directory containing libcef.dll, used as-is without any version,
  // ABI, platform, or signature checks. When set and libcef.dll exists at the
  // path, the installer returns immediately without contacting the CDN or
  // scanning the shared install directory.
  //
  // Relative paths are resolved against the directory containing the module
  // that provided this config (the client DLL in bootstrap mode). For example,
  // "." means "libcef.dll is next to the client DLL."
  //
  // This is intended for applications that bundle CEF alongside their own
  // binaries and want to skip the installer entirely. The application is
  // responsible for the integrity of the DLL at this path.
  //
  // If libcef.dll is not found at the path, the installer falls through to the
  // normal resolution flow (shared directory, CDN download).
  std::string unchecked_cef_path;

  // Path to a directory containing a full CEF distribution (cef_version.json,
  // catalog.cat, Release/libcef.dll). Participates in version selection
  // (newer wins, installed wins ties, revocation demotion). Relative paths are
  // resolved against the directory containing the module that provided this
  // config (the client DLL in bootstrap mode).
  //
  // Only read from the client DLL's embedded resource or RunInstaller JSON.
  // Ignored in the bootstrap resource.
  std::string bundled_cef_path;

  // Exclusive installer state namespace selected by the client DLL resource.
  // Relative paths are resolved against the client DLL directory before being
  // propagated to ExtendedConfig. Ignored in bootstrap resources;
  // RunInstaller JSON has an independent ExtendedConfig field.
  std::string install_path;

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
  // Test-only: override the certificate thumbprint used for archive
  // verification. ApplyConfigThumbprintOverride() copies this into
  // ExtendedConfig::certificate_thumbprint and auto-enables signature testing
  // mode so self-signed test certs are accepted. Compiled out of official
  // builds.
  std::string certificate_thumbprint;
#endif
};

// Error codes for configuration parsing
enum class ConfigError {
  kSuccess,
  kJsonParseError,        // Malformed JSON
  kMissingRequiredField,  // appid or vmin missing
  kInvalidFieldValue,     // Field present but invalid format
  kInvalidVersionRange,   // Effective vmin is greater than vmax
  kResourceNotFound,      // Embedded resource not found
};

// Serialize Config to JSON for prepared-uninstall configuration binding.
std::string ConfigToJson(const Config& config);

// Parse JSON config string directly. |options| controls which context-specific
// fields are parsed; see ConfigParseOptions.
ConfigError ParseConfigFromJson(const std::string& json,
                                Config* config,
                                const ConfigParseOptions& options = {},
                                std::string* diagnostic = nullptr);

// Read from embedded Windows resource (RT_RCDATA with string name).
// Resource must be embedded in the client executable's .rc file:
//   CEF_INSTALLER_CONFIG RCDATA "installer_config.json"
// |options| is forwarded to ParseConfigFromJson.
ConfigError ReadConfigFromResource(HMODULE module,
                                   Config* config,
                                   const ConfigParseOptions& options = {},
                                   std::string* diagnostic = nullptr);

// Convert error code to human-readable string for logging
const char* ConfigErrorToString(ConfigError error);

}  // namespace cef_installer

#endif  // CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_CONFIG_H_
