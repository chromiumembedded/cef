// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_SCHEME_IMPL_H_
#define CEF_LIBCEF_BROWSER_SCHEME_IMPL_H_
#pragma once

#include <string>
#include <vector>
#include "include/cef_frame.h"
#include "googleurl/src/gurl.h"

namespace scheme {

// Add all standard schemes.
void AddStandardSchemes(std::vector<std::string>* standard_schemes);

// Register all internal scheme handlers.
void RegisterInternalHandlers();

// Used to fire any asynchronous content updates.
void DidFinishLoad(CefRefPtr<CefFrame> frame, const GURL& validated_url);

}  // namespace scheme

#endif  // CEF_LIBCEF_BROWSER_SCHEME_IMPL_H_
