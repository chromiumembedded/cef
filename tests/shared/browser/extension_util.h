// Copyright (c) 2017 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_EXTENSION_UTIL_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_EXTENSION_UTIL_H_
#pragma once

#include <string>

#include "include/cef_extension.h"
#include "include/cef_extension_handler.h"
#include "include/wrapper/cef_resource_manager.h"

namespace client {
namespace extension_util {

// Returns true if |extension_path| can be handled internally via
// LoadBinaryResource. This checks a hard-coded list of allowed extension path
// components.
bool IsInternalExtension(const std::string& extension_path);

// Returns the path relative to the resource directory after removing the
// PK_DIR_RESOURCES prefix. This will be the relative path expected by
// LoadBinaryResource (uses '/' as path separator on all platforms). Only call
// this method for internal extensions, either when IsInternalExtension returns
// true or when the extension is handled internally through some means other
// than LoadBinaryResource. Use GetExtensionResourcePath instead if you are
// unsure whether the extension is internal or external.
std::string GetInternalExtensionResourcePath(const std::string& extension_path);

// Returns the resource path for |extension_path|. For external extensions this
// will be the full file path on disk. For internal extensions this will be the
// relative path expected by LoadBinaryResource (uses '/' as path separator on
// all platforms). Internal extensions must be on the hard-coded list enforced
// by IsInternalExtension. If |internal| is non-nullptr it will be set to true
// if the extension is handled internally.
std::string GetExtensionResourcePath(const std::string& extension_path,
                                     bool* internal);

// Read the contents of |extension_path| into |contents|. For external
// extensions this will read the file from disk. For internal extensions this
// will call LoadBinaryResource. Internal extensions must be on the hard-coded
// list enforced by IsInternalExtension. Returns true on success. Must be
// called on the FILE thread.
bool GetExtensionResourceContents(const std::string& extension_path,
                                  std::string& contents);

// Load |extension_path| in |request_context|. May be an internal or external
// extension. Internal extensions must be on the hard-coded list enforced by
// IsInternalExtension.
void LoadExtension(CefRefPtr<CefRequestContext> request_context,
                   const std::string& extension_path,
                   CefRefPtr<CefExtensionHandler> handler);

// Register an internal handler for extension resources. Internal extensions
// must be on the hard-coded list enforced by IsInternalExtension.
void AddInternalExtensionToResourceManager(
    CefRefPtr<CefExtension> extension,
    CefRefPtr<CefResourceManager> resource_manager);

// Returns the URL origin for |extension_id|.
std::string GetExtensionOrigin(const std::string& extension_id);

// Parse browser_action manifest values as defined at
// https://developer.chrome.com/extensions/browserAction

// Look for a browser_action.default_popup manifest value.
std::string GetExtensionURL(CefRefPtr<CefExtension> extension);

// Look for a browser_action.default_icon manifest value and return the resource
// path. If |internal| is non-nullptr it will be set to true if the extension is
// handled internally.
std::string GetExtensionIconPath(CefRefPtr<CefExtension> extension,
                                 bool* internal);

}  // namespace extension_util
}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_EXTENSION_UTIL_H_
