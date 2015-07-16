// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_EXTENSIONS_PDF_EXTENSION_UTIL_H_
#define CEF_LIBCEF_BROWSER_EXTENSIONS_PDF_EXTENSION_UTIL_H_

#include <string>

namespace extensions {
namespace pdf_extension_util {

// The ResourceIdentifier for the PDF Viewer plugin.
extern const char kPdfResourceIdentifier[];

// The name of the PDF Viewer plugin.
extern const char kPdfPluginName[];

// Return the extensions manifest for PDF. The manifest is loaded from
// browser_resources.grd and certain fields are replaced based on what chrome
// flags are enabled.
std::string GetManifest();

}  // namespace pdf_extension_util
}  // namespace extensions

#endif  // CEF_LIBCEF_BROWSER_EXTENSIONS_PDF_EXTENSION_UTIL_H_
