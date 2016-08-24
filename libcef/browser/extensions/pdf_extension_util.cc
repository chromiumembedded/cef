// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/extensions/pdf_extension_util.h"

#include "base/strings/string_util.h"
#include "chrome/grit/browser_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace extensions {
namespace pdf_extension_util {

namespace {

// Tags in the manifest to be replaced.
const char kNameTag[] = "<NAME>";

}  // namespace

// These should match the keys for the Chrome and Chromium PDF Viewer entries in
// chrome/browser/resources/plugin_metadata/plugins_*.json.
#if defined(GOOGLE_CHROME_BUILD)
const char kPdfResourceIdentifier[] = "google-chrome-pdf";
#else
const char kPdfResourceIdentifier[] = "chromium-pdf";
#endif

// Match the GOOGLE_CHROME_BUILD value from ChromeContentClient::kPDFPluginName
// to avoid breaking Websites that specifically look for this string in the
// plugin list.
const char kPdfPluginName[] = "Chrome PDF Viewer";

std::string GetManifest() {
  std::string manifest_contents =
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_PDF_MANIFEST).as_string();
  DCHECK(manifest_contents.find(kNameTag) != std::string::npos);
  base::ReplaceFirstSubstringAfterOffset(
      &manifest_contents, 0, kNameTag, kPdfPluginName);

  return manifest_contents;
}

}  // namespace pdf_extension_util
}  // namespace extensions
