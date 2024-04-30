// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "cef/libcef/common/extensions/extensions_util.h"

#include "base/command_line.h"
#include "cef/libcef/common/cef_switches.h"
#include "cef/libcef/features/runtime.h"
#include "chrome/common/chrome_switches.h"

namespace extensions {

bool ExtensionsEnabled() {
  static bool enabled = cef::IsAlloyRuntimeEnabled() &&
                        !base::CommandLine::ForCurrentProcess()->HasSwitch(
                            switches::kDisableExtensions);
  return enabled;
}

bool PdfExtensionEnabled() {
  static bool enabled =
      ExtensionsEnabled() && !base::CommandLine::ForCurrentProcess()->HasSwitch(
                                 switches::kDisablePdfExtension);
  return enabled;
}

}  // namespace extensions
