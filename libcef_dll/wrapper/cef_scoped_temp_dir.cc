// Copyright 2016 The Chromium Embedded Framework Authors. Portions copyright
// 2011 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#include "include/wrapper/cef_scoped_temp_dir.h"

#include "include/base/cef_logging.h"
#include "include/cef_file_util.h"

CefScopedTempDir::CefScopedTempDir() {}

CefScopedTempDir::~CefScopedTempDir() {
  if (!path_.empty() && !Delete())
    DLOG(WARNING) << "Could not delete temp dir in dtor.";
}

bool CefScopedTempDir::CreateUniqueTempDir() {
  if (!path_.empty())
    return false;

  // This "scoped_dir" prefix is only used on Windows and serves as a template
  // for the unique name.
  if (!CefCreateNewTempDirectory("scoped_dir", path_))
    return false;

  return true;
}

bool CefScopedTempDir::CreateUniqueTempDirUnderPath(
    const CefString& base_path) {
  if (!path_.empty())
    return false;

  // If |base_path| does not exist, create it.
  if (!CefCreateDirectory(base_path))
    return false;

  // Create a new, uniquely named directory under |base_path|.
  if (!CefCreateTempDirectoryInDirectory(base_path, "scoped_dir_", path_))
    return false;

  return true;
}

bool CefScopedTempDir::Set(const CefString& path) {
  if (!path_.empty())
    return false;

  if (!CefDirectoryExists(path) && !CefCreateDirectory(path))
    return false;

  path_ = path;
  return true;
}

bool CefScopedTempDir::Delete() {
  if (path_.empty())
    return false;

  bool ret = CefDeleteFile(path_, true);
  if (ret) {
    // We only clear the path if deleted the directory.
    path_.clear();
  }

  return ret;
}

CefString CefScopedTempDir::Take() {
  CefString ret = path_;
  path_.clear();
  return ret;
}

const CefString& CefScopedTempDir::GetPath() const {
  DCHECK(!path_.empty()) << "Did you call CreateUniqueTempDir* before?";
  return path_;
}

bool CefScopedTempDir::IsEmpty() const {
  return path_.empty();
}

bool CefScopedTempDir::IsValid() const {
  return !path_.empty() && CefDirectoryExists(path_);
}
