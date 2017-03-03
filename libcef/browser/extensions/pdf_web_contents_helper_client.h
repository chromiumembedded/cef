// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_EXTENSIONS_PDF_WEB_CONTENTS_HELPER_CLIENT_H_
#define CEF_LIBCEF_BROWSER_EXTENSIONS_PDF_WEB_CONTENTS_HELPER_CLIENT_H_

#include "base/macros.h"
#include "components/pdf/browser/pdf_web_contents_helper_client.h"

namespace extensions {

class CefPDFWebContentsHelperClient
    : public pdf::PDFWebContentsHelperClient {
 public:
  CefPDFWebContentsHelperClient();
  ~CefPDFWebContentsHelperClient() override;

 private:
  // pdf::PDFWebContentsHelperClient:
  void UpdateContentRestrictions(content::WebContents* contents,
                                 int content_restrictions) override;
  void OnPDFHasUnsupportedFeature(content::WebContents* contents) override;
  void OnSaveURL(content::WebContents* contents) override;

  DISALLOW_COPY_AND_ASSIGN(CefPDFWebContentsHelperClient);
};

}  // namespace extensions

#endif  // CEF_LIBCEF_BROWSER_EXTENSIONS_PDF_WEB_CONTENTS_HELPER_CLIENT_H_
