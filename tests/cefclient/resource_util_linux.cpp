// Copyright (c) 2011 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "resource_util.h"
#include "util.h"
#include <stdio.h>

#include "base/file_util.h"

bool GetResourceDir(std::string& dir) {
  char buff[1024];
  ssize_t len = ::readlink("/proc/self/exe", buff, sizeof(buff)-1);

  if (len != -1) {
    buff[len] = '\0';
    dir = std::string(buff);
    return true;
  } else {
    /* handle error condition */
    return false;
  }
}

bool LoadBinaryResource(const char* resource_name, std::string& resource_data) {
  return false;
}

CefRefPtr<CefStreamReader> GetBinaryResourceReader(const char* resource_name) {
  std::string path;
  if (!GetResourceDir(path))
    return NULL;

  path.append("/");
  path.append(resource_name);

  return CefStreamReader::CreateForFile(path);
}
