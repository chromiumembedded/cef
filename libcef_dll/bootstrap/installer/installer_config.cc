// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_config.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/uuid.h"
#include "cef/include/cef_api_hash.h"
#include "cef/libcef_dll/bootstrap/installer/installer_constants.h"
#include "cef/libcef_dll/bootstrap/installer/installer_paths.h"
#include "cef/libcef_dll/bootstrap/installer/installer_policy.h"
#include "cef/libcef_dll/bootstrap/installer/installer_validation.h"
#include "cef/libcef_dll/bootstrap/installer/installer_version.h"
#include "include/wrapper/cef_util_win.h"

namespace cef_installer {

namespace {

using config_fields::kAbiHashField;
using config_fields::kAppIdField;
using config_fields::kBundledCefPathField;
using config_fields::kCdnUrlsField;
using config_fields::kChannelField;
using config_fields::kEnableExplicitModesField;
using config_fields::kInstallPathField;
using config_fields::kLaunchHealthField;
using config_fields::kUncheckedCefPathField;
using config_fields::kVmaxField;
using config_fields::kVminField;

void WarnUnrecognizedConfigKeys(const base::DictValue& dict) {
  for (const auto item : dict) {
    if (config_fields::IsRecognizedConfigField(item.first)) {
      continue;
    }

    std::string escaped_key;
    base::EscapeJSONString(item.first, /*put_in_quotes=*/true, &escaped_key);
    LOG(WARNING) << "Unrecognized installer config key: " << escaped_key;
  }
}

}  // namespace

const char* LaunchHealthModeToString(LaunchHealthMode mode) {
  switch (mode) {
    case LaunchHealthMode::kOff:
      return "off";
    case LaunchHealthMode::kExplicit:
      return "explicit";
    case LaunchHealthMode::kExitCode:
      return "exit_code";
  }
  NOTREACHED();
}

std::string ConfigToJson(const Config& config) {
  base::DictValue dict;
  dict.Set(kAppIdField, config.appid);
  dict.Set(kVminField, config.vmin);
  if (!config.vmax.empty()) {
    dict.Set(kVmaxField, config.vmax);
  }
  if (!config.abi_hash.empty()) {
    dict.Set(kAbiHashField, config.abi_hash);
  }
  if (!config.channel.empty()) {
    dict.Set(kChannelField, config.channel);
  }
  if (!config.cdn_urls.empty()) {
    base::ListValue urls;
    for (const auto& url : config.cdn_urls) {
      urls.Append(url);
    }
    dict.Set(kCdnUrlsField, std::move(urls));
  }
  dict.Set(kLaunchHealthField,
           std::string(LaunchHealthModeToString(config.launch_health)));
  if (config.enable_explicit_modes) {
    dict.Set(kEnableExplicitModesField, true);
  }
  if (!config.unchecked_cef_path.empty()) {
    dict.Set(kUncheckedCefPathField, config.unchecked_cef_path);
  }
  if (!config.bundled_cef_path.empty()) {
    dict.Set(kBundledCefPathField, config.bundled_cef_path);
  }
  std::string json;
  base::JSONWriter::Write(dict, &json);
  return json;
}

ConfigError ParseConfigFromJson(const std::string& json,
                                Config* config,
                                const ConfigParseOptions& options,
                                std::string* diagnostic) {
  if (diagnostic) {
    diagnostic->clear();
  }
  if (!config) {
    if (diagnostic) {
      *diagnostic = "Config output is null";
    }
    return ConfigError::kMissingRequiredField;
  }

  std::optional<base::DictValue> parsed =
      base::JSONReader::ReadDict(json, base::JSON_PARSE_RFC);
  if (!parsed) {
    if (diagnostic) {
      *diagnostic = "Malformed installer config JSON";
    }
    return ConfigError::kJsonParseError;
  }

  const base::DictValue& dict = *parsed;
  WarnUnrecognizedConfigKeys(dict);

  // Read appid (required)
  const std::string* appid = dict.FindString(kAppIdField);
  if (!appid || appid->empty()) {
    return ConfigError::kMissingRequiredField;
  }
  base::Uuid uuid = base::Uuid::ParseCaseInsensitive(*appid);
  if (!uuid.is_valid()) {
    return ConfigError::kInvalidFieldValue;
  }
  config->appid = *appid;

  // Read vmin (required)
  const std::string* vmin = dict.FindString(kVminField);
  if (!vmin || vmin->empty()) {
    return ConfigError::kMissingRequiredField;
  }
  Version vmin_parsed = Version::Parse(*vmin);
  if (!vmin_parsed.IsValid()) {
    return ConfigError::kInvalidFieldValue;
  }
  const std::string configured_vmin = *vmin;
  config->vmin = configured_vmin;

  // Clamp vmin to the bootstrap's API version - bootstrap can't use older CEF
  // than it was compiled against. Cap at CEF_API_VERSION_LAST to handle
  // CEF_API_VERSION being EXPERIMENTAL or NEXT (larger than any real version).
  constexpr int bootstrap_api_version = (CEF_API_VERSION < CEF_API_VERSION_LAST)
                                            ? CEF_API_VERSION
                                            : CEF_API_VERSION_LAST;
  Version api_min = Version::FromApiVersion(bootstrap_api_version);
  if (api_min.IsValid() && api_min > vmin_parsed) {
    LOG(WARNING) << "Config vmin " << *vmin << " clamped to "
                 << api_min.ToString() << " (bootstrap API version)";
    config->vmin = api_min.ToString();
  }

  // Read vmax (optional)
  const std::string* vmax = dict.FindString(kVmaxField);
  if (vmax && !vmax->empty()) {
    Version vmax_parsed = Version::Parse(*vmax);
    if (!vmax_parsed.IsValid()) {
      return ConfigError::kInvalidFieldValue;
    }
    config->vmax = *vmax;
    Version effective_vmin = Version::Parse(config->vmin);
    if (effective_vmin > vmax_parsed) {
      if (diagnostic) {
        if (vmin_parsed > vmax_parsed) {
          *diagnostic = base::StringPrintf(
              "Configured vmin %s exceeds configured vmax %s",
              configured_vmin.c_str(), vmax->c_str());
        } else {
          *diagnostic = base::StringPrintf(
              "Configured vmin %s has effective vmin %s after clamping to "
              "bootstrap API version %d, which exceeds configured vmax %s",
              configured_vmin.c_str(), config->vmin.c_str(),
              bootstrap_api_version, vmax->c_str());
        }
      }
      return ConfigError::kInvalidVersionRange;
    }
  } else {
    config->vmax.clear();
  }

  // Read abi_hash (optional, must be hex if present)
  const std::string* abi_hash = dict.FindString(kAbiHashField);
  if (abi_hash && !abi_hash->empty()) {
    if (!IsValidAbiHash(*abi_hash)) {
      return ConfigError::kInvalidFieldValue;
    }
    config->abi_hash = *abi_hash;
  } else {
    config->abi_hash.clear();
  }

  // Read channel (optional, empty = stable)
  const std::string* channel = dict.FindString(kChannelField);
  if (channel && !channel->empty()) {
    // Only "beta" is a valid non-default channel
    if (*channel != kChannelBeta) {
      return ConfigError::kInvalidFieldValue;
    }
    config->channel = *channel;
  } else {
    config->channel.clear();  // Empty = stable (default)
  }

  const base::Value* cdn_urls = dict.Find(kCdnUrlsField);
  if (!cdn_urls) {
    config->cdn_urls.clear();
  } else if (!cdn_urls->is_list()) {
    config->cdn_urls.clear();
    if (diagnostic) {
      *diagnostic = "cdn_urls must be an array";
    }
    return ConfigError::kInvalidFieldValue;
  } else {
    std::vector<std::string> values;
    values.reserve(cdn_urls->GetList().size());
    for (const auto& value : cdn_urls->GetList()) {
      if (!value.is_string()) {
        config->cdn_urls.clear();
        if (diagnostic) {
          *diagnostic = "cdn_urls entries must be strings";
        }
        return ConfigError::kInvalidFieldValue;
      }
      values.push_back(value.GetString());
    }
    if (!ValidateAndNormalizeCdnUrls(values, &config->cdn_urls, diagnostic)) {
      config->cdn_urls.clear();
      return ConfigError::kInvalidFieldValue;
    }
  }

  // Launch health is a trusted core field in every Config source. Missing
  // legacy configs default to off; present values must use canonical strings.
  const base::Value* launch_health = dict.Find(kLaunchHealthField);
  if (!launch_health) {
    config->launch_health = LaunchHealthMode::kOff;
  } else if (!launch_health->is_string()) {
    if (diagnostic) {
      *diagnostic = "launch_health must be a string";
    }
    return ConfigError::kInvalidFieldValue;
  } else if (launch_health->GetString() == "off") {
    config->launch_health = LaunchHealthMode::kOff;
  } else if (launch_health->GetString() == "explicit") {
    config->launch_health = LaunchHealthMode::kExplicit;
  } else if (launch_health->GetString() == "exit_code") {
    config->launch_health = LaunchHealthMode::kExitCode;
  } else {
    if (diagnostic) {
      *diagnostic = "launch_health must be one of: off, explicit, exit_code";
    }
    return ConfigError::kInvalidFieldValue;
  }

  // Only authoritative in the bootstrap's own embedded resource.
  // Ignored in client DLL resources and RunInstaller JSON.
  if (options.allow_enable_explicit_modes) {
    if (std::optional<bool> val = dict.FindBool(kEnableExplicitModesField)) {
      config->enable_explicit_modes = *val;
    } else {
      config->enable_explicit_modes = false;
    }
  } else {
    config->enable_explicit_modes = false;
  }

  // Only from client DLL resource or RunInstaller JSON. Ignored in the
  // bootstrap resource, which has no client DLL directory to resolve relative
  // paths against.
  if (options.allow_unchecked_cef_path) {
    const std::string* unchecked_path = dict.FindString(kUncheckedCefPathField);
    if (unchecked_path && !unchecked_path->empty()) {
      config->unchecked_cef_path = *unchecked_path;
    } else {
      config->unchecked_cef_path.clear();
    }
  } else {
    config->unchecked_cef_path.clear();
  }

  // Only from client DLL resource or RunInstaller JSON. Same gating rationale
  // as unchecked_cef_path: relative paths need a client DLL directory.
  if (options.allow_bundled_cef_path) {
    const std::string* bundled_path = dict.FindString(kBundledCefPathField);
    if (bundled_path && !bundled_path->empty()) {
      config->bundled_cef_path = *bundled_path;
    } else {
      config->bundled_cef_path.clear();
    }
  } else {
    config->bundled_cef_path.clear();
  }

  // Only the trusted client DLL resource may select the automatic installer
  // namespace. Bootstrap resources cannot select a write location.
  // RunInstaller JSON is parsed independently into ExtendedConfig.
  if (options.allow_install_path) {
    const base::Value* install_path = dict.Find(kInstallPathField);
    if (!install_path) {
      config->install_path.clear();
    } else if (!install_path->is_string()) {
      config->install_path.clear();
      if (diagnostic) {
        *diagnostic = "install_path must be a string";
      }
      return ConfigError::kInvalidFieldValue;
    } else {
      config->install_path = install_path->GetString();
    }
  } else {
    config->install_path.clear();
  }

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
  // Read certificate_thumbprint (optional, test-only).
  const std::string* thumbprint =
      dict.FindString(config_fields::kCertificateThumbprintField);
  if (thumbprint && !thumbprint->empty()) {
    config->certificate_thumbprint = *thumbprint;
  } else {
    config->certificate_thumbprint.clear();
  }
#endif

  return ConfigError::kSuccess;
}

ConfigError ReadConfigFromResource(HMODULE module,
                                   Config* config,
                                   const ConfigParseOptions& options,
                                   std::string* diagnostic) {
  if (!config) {
    return ConfigError::kMissingRequiredField;
  }

  std::string json;
  if (!cef_util::ReadResourceData(module, kConfigResourceName, &json)) {
    if (diagnostic) {
      *diagnostic = "Embedded installer config resource not found";
    }
    return ConfigError::kResourceNotFound;
  }

  return ParseConfigFromJson(json, config, options, diagnostic);
}

const char* ConfigErrorToString(ConfigError error) {
  switch (error) {
    case ConfigError::kSuccess:
      return "Success";
    case ConfigError::kJsonParseError:
      return "Malformed JSON";
    case ConfigError::kMissingRequiredField:
      return "Required field missing";
    case ConfigError::kInvalidFieldValue:
      return "Invalid field value";
    case ConfigError::kInvalidVersionRange:
      return "Invalid version range";
    case ConfigError::kResourceNotFound:
      return "Embedded resource not found";
  }
  return "Unknown error";
}

}  // namespace cef_installer
