// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/common/extensions/extensions_util.h"

#include "libcef/common/cef_switches.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"

namespace extensions {

bool ExtensionsEnabled() {
  static bool enabled = !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableExtensions);
  return enabled;
}

bool PdfExtensionEnabled() {
  static bool enabled =
      ExtensionsEnabled() && !base::CommandLine::ForCurrentProcess()->HasSwitch(
                                 switches::kDisablePdfExtension);
  return enabled;
}

bool PrintPreviewEnabled() {
#if BUILDFLAG(IS_MAC)
  // Not currently supported on macOS.
  return false;
#else
  if (!PdfExtensionEnabled()) {
    return false;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisablePrintPreview)) {
    return false;
  }

  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnablePrintPreview);
#endif
}

}  // namespace extensions
