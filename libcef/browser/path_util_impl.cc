// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef_path_util.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"

bool CefGetPath(PathKey key, CefString& path) {
  int pref_key = base::PATH_START;
  switch(key) {
    case PK_DIR_CURRENT:
      pref_key = base::DIR_CURRENT;
      break;
    case PK_DIR_EXE:
      pref_key = base::DIR_EXE;
      break;
    case PK_DIR_MODULE:
      pref_key = base::DIR_MODULE;
      break;
    case PK_DIR_TEMP:
      pref_key = base::DIR_TEMP;
      break;
    case PK_FILE_EXE:
      pref_key = base::FILE_EXE;
      break;
    case PK_FILE_MODULE:
      pref_key = base::FILE_MODULE;
      break;
#if defined(OS_WIN)
    case PK_LOCAL_APP_DATA:
      pref_key = base::DIR_LOCAL_APP_DATA;
      break;
#endif
    case PK_USER_DATA:
      pref_key = chrome::DIR_USER_DATA;
      break;
    default:
      NOTREACHED() << "invalid argument";
      return false;
  }

  base::FilePath file_path;
  if (PathService::Get(pref_key, &file_path)) {
    path = file_path.value();
    return true;
  }

  return false;
}
