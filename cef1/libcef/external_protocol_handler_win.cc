// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/external_protocol_handler.h"

#include <windows.h>
#include <shellapi.h>

#include "base/utf_string_conversions.h"
#include "base/win/registry.h"
#include "googleurl/src/gurl.h"

namespace ExternalProtocolHandler {

// According to Mozilla in uriloader/exthandler/win/nsOSHelperAppService.cpp:
// "Some versions of windows (Win2k before SP3, Win XP before SP1) crash in
// ShellExecute on long URLs (bug 161357 on bugzilla.mozilla.org). IE 5 and 6
// support URLS of 2083 chars in length, 2K is safe."
const int kMaxAddressLengthChars = 2048;

bool HasExternalHandler(const std::string& scheme) {
  base::win::RegKey key;
  const std::wstring registry_path =
      ASCIIToWide(scheme + "\\shell\\open\\command");
  key.Open(HKEY_CLASSES_ROOT, registry_path.c_str(), KEY_READ);
  if (key.Valid()) {
    DWORD size = 0;
    key.ReadValue(NULL, NULL, &size, NULL);
    if (size > 2) {
       // ShellExecute crashes the process when the command is empty.
       // We check for "2" because it always returns the trailing NULL.
       return true;
    }
  }

  return false;
}

bool HandleExternalProtocol(const GURL& gurl) {
  if (!HasExternalHandler(gurl.scheme()))
    return false;

  const std::string& address = gurl.spec();
  if (address.length() > kMaxAddressLengthChars)
    return false;

  ShellExecuteA(NULL, "open", address.c_str(), NULL, NULL, SW_SHOWNORMAL);

  return true;
}

}  // namespace ExternalProtocolHandler
