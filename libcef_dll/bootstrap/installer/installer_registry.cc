// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_registry.h"

#include "base/win/windows_version.h"

namespace cef_installer {

REGSAM GetSharedMachineRegistryAccess(REGSAM access) {
  if (base::win::OSInfo::GetArchitecture() !=
      base::win::OSInfo::X86_ARCHITECTURE) {
    access |= KEY_WOW64_64KEY;
  }
  return access;
}

LONG OpenSharedMachineRegistryKey(const wchar_t* subkey,
                                  REGSAM access,
                                  base::win::RegKey* key) {
  if (!key) {
    return ERROR_INVALID_PARAMETER;
  }
  return key->Open(HKEY_LOCAL_MACHINE, subkey,
                   GetSharedMachineRegistryAccess(access));
}

}  // namespace cef_installer
