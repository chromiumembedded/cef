// Copyright 2016 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "include/cef_file_util.h"

#include "include/cef_task.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "third_party/zlib/google/zip.h"

namespace {

bool AllowFileIO() {
  if (CefCurrentlyOn(TID_UI) || CefCurrentlyOn(TID_IO)) {
    NOTREACHED() << "file IO is not allowed on the current thread";
    return false;
  }
  return true;
}

}  // namespace

bool CefCreateDirectory(const CefString& full_path) {
  if (!AllowFileIO())
    return false;
  return base::CreateDirectory(full_path);
}

bool CefGetTempDirectory(CefString& temp_dir) {
  if (!AllowFileIO())
    return false;
  base::FilePath result;
  if (base::GetTempDir(&result)) {
    temp_dir = result.value();
    return true;
  }
  return false;
}

bool CefCreateNewTempDirectory(const CefString& prefix,
                               CefString& new_temp_path) {
  if (!AllowFileIO())
    return false;
  base::FilePath result;
  if (base::CreateNewTempDirectory(prefix, &result)) {
    new_temp_path = result.value();
    return true;
  }
  return false;
}

bool CefCreateTempDirectoryInDirectory(const CefString& base_dir,
                                       const CefString& prefix,
                                       CefString& new_dir) {
  if (!AllowFileIO())
    return false;
  base::FilePath result;
  if (base::CreateTemporaryDirInDir(base_dir, prefix, &result)) {
    new_dir = result.value();
    return true;
  }
  return false;
}

bool CefDirectoryExists(const CefString& path) {
  if (!AllowFileIO())
    return false;
  return base::DirectoryExists(path);
}

bool CefDeleteFile(const CefString& path, bool recursive) {
  if (!AllowFileIO())
    return false;
  return base::DeleteFile(path, recursive);
}

bool CefZipDirectory(const CefString& src_dir,
                     const CefString& dest_file,
                     bool include_hidden_files) {
  if (!AllowFileIO())
    return false;
  return zip::Zip(src_dir, dest_file, include_hidden_files);
}
