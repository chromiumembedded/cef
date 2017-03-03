// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/extensions/pdf_web_contents_helper_client.h"

namespace extensions {

CefPDFWebContentsHelperClient::CefPDFWebContentsHelperClient() {
}

CefPDFWebContentsHelperClient::~CefPDFWebContentsHelperClient() {
}

void CefPDFWebContentsHelperClient::UpdateContentRestrictions(
    content::WebContents* contents,
    int content_restrictions) {
}

void CefPDFWebContentsHelperClient::OnPDFHasUnsupportedFeature(
    content::WebContents* contents) {
}

void CefPDFWebContentsHelperClient::OnSaveURL(
    content::WebContents* contents) {
}

}  // namespace extensions
