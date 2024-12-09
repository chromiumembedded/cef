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

#include "cef/libcef_dll/cef_api_versions.inc"

CEF_EXPORT const char* cef_api_hash(int version, int entry) {
  static const ApiVersionHash* hash = nullptr;

  // Initialize on the first successful lookup.
  if (!hash) {
    for (size_t i = 0; i < kApiVersionHashesSize; ++i) {
      if (version == kApiVersionHashes[i].version) {
        hash = &kApiVersionHashes[i];
        break;
      }
    }

    if (hash) {
      g_version = version;
    }
  }

  if (!hash) {
    LOG(ERROR) << "Request for unsupported CEF API version " << version;
    return nullptr;
  }

  if (version != g_version) {
    LOG(ERROR) << "CEF API version cannot be configured multiple times";
    return nullptr;
  }

  switch (entry) {
    case 0:
      return hash->platform;
    case 1:
      return hash->universal;
    case 2:
      return CEF_COMMIT_HASH;
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
    for (size_t i = 0; i < kIdNamesPackResourcesSize; ++i) {
      (*string_to_id_map)[kIdNamesPackResources[i].name] =
          kIdNamesPackResources[i].id;
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
    for (size_t i = 0; i < kIdNamesPackStringsSize; ++i) {
      (*string_to_id_map)[kIdNamesPackStrings[i].name] =
          kIdNamesPackStrings[i].id;
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
    for (size_t i = 0; i < kIdNamesCommandIdsSize; ++i) {
      (*string_to_id_map)[kIdNamesCommandIds[i].name] =
          kIdNamesCommandIds[i].id;
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
