// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cefclient/resource_util.h"
#include "include/base/cef_logging.h"
#include "include/cef_stream.h"
#include "include/wrapper/cef_byte_read_handler.h"
#include "cefclient/resource.h"

namespace client {

namespace {

bool LoadBinaryResource(int binaryId, DWORD &dwSize, LPBYTE &pBytes) {
  HINSTANCE hInst = GetModuleHandle(NULL);
  HRSRC hRes = FindResource(hInst, MAKEINTRESOURCE(binaryId),
                            MAKEINTRESOURCE(256));
  if (hRes) {
    HGLOBAL hGlob = LoadResource(hInst, hRes);
    if (hGlob) {
      dwSize = SizeofResource(hInst, hRes);
      pBytes = (LPBYTE)LockResource(hGlob);
      if (dwSize > 0 && pBytes)
        return true;
    }
  }

  return false;
}

int GetResourceId(const char* resource_name) {
  // Map of resource labels to BINARY id values.
  static struct _resource_map {
    char* name;
    int id;
  } resource_map[] = {
    {"binding.html", IDS_BINDING},
    {"dialogs.html", IDS_DIALOGS},
    {"localstorage.html", IDS_LOCALSTORAGE},
    {"logo.png", IDS_LOGO},
    {"osr_test.html", IDS_OSRTEST},
    {"other_tests.html", IDS_OTHER_TESTS},
    {"performance.html", IDS_PERFORMANCE},
    {"performance2.html", IDS_PERFORMANCE2},
    {"transparency.html", IDS_TRANSPARENCY},
    {"window.html", IDS_WINDOW},
    {"xmlhttprequest.html", IDS_XMLHTTPREQUEST},
  };

  for (int i = 0; i < sizeof(resource_map)/sizeof(_resource_map); ++i) {
    if (!strcmp(resource_map[i].name, resource_name))
      return resource_map[i].id;
  }
  
  return 0;
}

}  // namespace

bool LoadBinaryResource(const char* resource_name, std::string& resource_data) {
  int resource_id = GetResourceId(resource_name);
  if (resource_id == 0)
    return false;

  DWORD dwSize;
  LPBYTE pBytes;

  if (LoadBinaryResource(resource_id, dwSize, pBytes)) {
    resource_data = std::string(reinterpret_cast<char*>(pBytes), dwSize);
    return true;
  }

  NOTREACHED();  // The resource should be found.
  return false;
}

CefRefPtr<CefStreamReader> GetBinaryResourceReader(const char* resource_name) {
  int resource_id = GetResourceId(resource_name);
  if (resource_id == 0)
    return NULL;

  DWORD dwSize;
  LPBYTE pBytes;

  if (LoadBinaryResource(resource_id, dwSize, pBytes)) {
    return CefStreamReader::CreateForHandler(
        new CefByteReadHandler(pBytes, dwSize, NULL));
  }

  NOTREACHED();  // The resource should be found.
  return NULL;
}

}  // namespace client
