// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_EXTENSIONS_EXTENSIONS_UTIL_H_
#define CEF_LIBCEF_COMMON_EXTENSIONS_EXTENSIONS_UTIL_H_

namespace extensions {

// Returns true if extensions have not been disabled via the command-line.
bool ExtensionsEnabled();

// Returns true if the PDF extension has not been disabled via the command-line.
bool PdfExtensionEnabled();

}  // namespace extensions

#endif  // CEF_LIBCEF_COMMON_EXTENSIONS_EXTENSIONS_UTIL_H_
