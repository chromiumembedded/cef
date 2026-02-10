// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.
//

#include <cstddef>
#include <map>
#include <string_view>

#include "base/logging.h"
#include "base/no_destructor.h"
#include "cef/include/base/cef_build.h"
#include "cef/include/cef_api_hash.h"
#include "cef/include/cef_id_mappers.h"
#include "cef/include/cef_version_info.h"

#if defined(OS_WIN)
#include "cef/include/internal/cef_win.h"
#endif

namespace {
int g_version = -1;
}

CEF_EXPORT int cef_version_info(int entry) {
  switch (entry) {
    case 0:
      return CEF_VERSION_MAJOR;
    case 1:
      return CEF_VERSION_MINOR;
    case 2:
      return CEF_VERSION_PATCH;
    case 3:
      return CEF_COMMIT_NUMBER;
    case 4:
      return CHROME_VERSION_MAJOR;
    case 5:
      return CHROME_VERSION_MINOR;
    case 6:
      return CHROME_VERSION_BUILD;
    case 7:
      return CHROME_VERSION_PATCH;
    default:
      return 0;
  }
}

CEF_EXPORT void cef_version_info_all(cef_version_info_t* info) {
  if (!info || info->size == 0) {
    return;
  }

  // Always populate base version fields (present in all struct versions).
  info->cef_version_major = CEF_VERSION_MAJOR;
  info->cef_version_minor = CEF_VERSION_MINOR;
  info->cef_version_patch = CEF_VERSION_PATCH;
  info->cef_commit_number = CEF_COMMIT_NUMBER;
  info->chrome_version_major = CHROME_VERSION_MAJOR;
  info->chrome_version_minor = CHROME_VERSION_MINOR;
  info->chrome_version_build = CHROME_VERSION_BUILD;
  info->chrome_version_patch = CHROME_VERSION_PATCH;

  // Populate sandbox_compat_hash if the struct is large enough to contain it.
  // This allows older clients with smaller structs to still work.
  if (info->size >= CEF_VERSION_INFO_SIZE_WITH_SANDBOX_HASH) {
    strncpy(info->sandbox_compat_hash, CEF_SANDBOX_COMPAT_HASH,
            sizeof(info->sandbox_compat_hash) - 1);
    info->sandbox_compat_hash[sizeof(info->sandbox_compat_hash) - 1] = '\0';
  }
}

#include "cef/libcef_dll/cef_api_versions.inc"

CEF_EXPORT const char* cef_api_hash(int version, int entry) {
  static const ApiVersionHash* current_version_hash = nullptr;

  // Initialize on the first successful lookup.
  if (!current_version_hash) {
    for (const auto& version_hash : kApiVersionHashes) {
      if (version == version_hash.version) {
        current_version_hash = &version_hash;
        break;
      }
    }

    if (current_version_hash) {
      g_version = version;
    }
  }

  if (!current_version_hash) {
    LOG(ERROR) << "Request for unsupported CEF API version " << version;
    return nullptr;
  }

  if (version != g_version) {
    LOG(ERROR) << "CEF API version cannot be configured multiple times";
    return nullptr;
  }

  switch (entry) {
    case 0:
    case 1:
      return current_version_hash->hash;
    case 2:
      return CEF_COMMIT_HASH;
    case 3:
      return CEF_SANDBOX_COMPAT_HASH;
    default:
      return nullptr;
  }
}

CEF_EXPORT int cef_api_version() {
  return g_version;
}

#include "cef/libcef_dll/cef_pack_resources.inc"

CEF_EXPORT int cef_id_for_pack_resource_name(const char* name) {
  static base::NoDestructor<std::map<std::string_view, int>> string_to_id_map;

  // Initialize on the first call.
  if (string_to_id_map->empty()) {
    for (const auto& pack_resource : kIdNamesPackResources) {
      (*string_to_id_map)[pack_resource.name] = pack_resource.id;
    }
  }

  const auto& it = string_to_id_map->find(name);
  if (it != string_to_id_map->end()) {
    return it->second;
  }

  LOG(WARNING) << __func__ << " called with unsupported value " << name;
  return -1;
}

#include "cef/libcef_dll/cef_pack_strings.inc"

CEF_EXPORT int cef_id_for_pack_string_name(const char* name) {
  static base::NoDestructor<std::map<std::string_view, int>> string_to_id_map;

  // Initialize on the first call.
  if (string_to_id_map->empty()) {
    for (const auto& pack_string : kIdNamesPackStrings) {
      (*string_to_id_map)[pack_string.name] = pack_string.id;
    }
  }

  const auto& it = string_to_id_map->find(name);
  if (it != string_to_id_map->end()) {
    return it->second;
  }

  LOG(WARNING) << __func__ << " called with unsupported value " << name;
  return -1;
}

#include "cef/libcef_dll/cef_command_ids.inc"

CEF_EXPORT int cef_id_for_command_id_name(const char* name) {
  static base::NoDestructor<std::map<std::string_view, int>> string_to_id_map;

  // Initialize on the first call.
  if (string_to_id_map->empty()) {
    for (const auto& command : kIdNamesCommandIds) {
      (*string_to_id_map)[command.name] = command.id;
    }
  }

  const auto& it = string_to_id_map->find(name);
  if (it != string_to_id_map->end()) {
    return it->second;
  }

  LOG(WARNING) << __func__ << " called with unsupported value " << name;
  return -1;
}

#if defined(OS_WIN)

#if defined(ARCH_CPU_32_BITS)
CEF_EXPORT int cef_run_winmain_with_preferred_stack_size(wWinMainPtr wWinMain,
                                                         HINSTANCE hInstance,
                                                         LPWSTR lpCmdLine,
                                                         int nCmdShow) {
  return CefRunWinMainWithPreferredStackSize(wWinMain, hInstance, lpCmdLine,
                                             nCmdShow);
}

CEF_EXPORT int cef_run_main_with_preferred_stack_size(mainPtr main,
                                                      int argc,
                                                      char* argv[]) {
  return CefRunMainWithPreferredStackSize(main, argc, argv);
}
#endif  // defined(ARCH_CPU_32_BITS)

CEF_EXPORT void cef_set_osmodal_loop(int osModalLoop) {
  CefSetOSModalLoop(osModalLoop ? true : false);
}

#endif  // defined(OS_WIN)
