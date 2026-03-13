// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_REGISTRY_H_
#define CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_REGISTRY_H_

#include <windows.h>

#include "base/win/registry.h"

namespace cef_installer {

// Returns the access mask used for machine-wide CEF configuration. All CEF
// processes use the 64-bit registry view on a 64-bit OS and the native view on
// a 32-bit OS.
REGSAM GetSharedMachineRegistryAccess(REGSAM access);

// Opens |subkey| below HKLM using the shared machine configuration view.
LONG OpenSharedMachineRegistryKey(const wchar_t* subkey,
                                  REGSAM access,
                                  base::win::RegKey* key);

}  // namespace cef_installer

#endif  // CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_REGISTRY_H_
