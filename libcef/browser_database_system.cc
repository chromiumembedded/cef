// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "browser_database_system.h"

#if defined(USE_SYSTEM_SQLITE)
#include <sqlite3.h>
#else
#include "third_party/sqlite/preprocessed/sqlite3.h"
#endif

#include "base/file_util.h"
#include "base/logging.h"
#include "base/platform_thread.h"
#include "base/process_util.h"
#include "webkit/database/vfs_backend.h"
#include "webkit/glue/webkit_glue.h"

using webkit_database::VfsBackend;

BrowserDatabaseSystem* BrowserDatabaseSystem::instance_ = NULL;

BrowserDatabaseSystem* BrowserDatabaseSystem::GetInstance() {
  DCHECK(instance_);
  return instance_;
}

BrowserDatabaseSystem::BrowserDatabaseSystem()
    : hack_main_db_handle_(base::kInvalidPlatformFileValue) {
  temp_dir_.CreateUniqueTempDir();
  DCHECK(!instance_);
  instance_ = this;
}

BrowserDatabaseSystem::~BrowserDatabaseSystem() {
  base::ClosePlatformFile(hack_main_db_handle_);
  instance_ = NULL;
}

base::PlatformFile BrowserDatabaseSystem::OpenFile(
      const FilePath& file_name, int desired_flags,
      base::PlatformFile* dir_handle) {
  base::PlatformFile file_handle = base::kInvalidPlatformFileValue;
  VfsBackend::OpenFile(GetDBFileFullPath(file_name), GetDBDir(), desired_flags,
                       base::GetCurrentProcessHandle(), &file_handle,
                       dir_handle);

  // HACK: Currently, the DB object that keeps track of the main database
  // (DatabaseTracker) is a singleton that is declared as a static variable
  // in a function, so it gets destroyed at the very end of the program.
  // Because of that, we have a handle opened to the main DB file until the
  // very end of the program, which prevents temp_dir_'s destructor from
  // deleting the database directory.
  //
  // We will properly solve this problem when we reimplement DatabaseTracker.
  // For now, however, we are going to take advantage of the fact that in order
  // to do anything related to DBs, we have to call openDatabase() first, which
  // opens a handle to the main DB before opening handles to any other DB files.
  // We are going to cache the first file handle we get, and we are going to
  // manually close it in the destructor.
  if (hack_main_db_handle_ == base::kInvalidPlatformFileValue) {
    hack_main_db_handle_ = file_handle;
  }

  return file_handle;
}

int BrowserDatabaseSystem::DeleteFile(
    const FilePath& file_name, bool sync_dir) {
  // We try to delete the file multiple times, because that's what the default
  // VFS does (apparently deleting a file can sometimes fail on Windows).
  // We sleep for 10ms between retries for the same reason.
  const int kNumDeleteRetries = 3;
  int num_retries = 0;
  int error_code = SQLITE_OK;
  do {
    error_code = VfsBackend::DeleteFile(
        GetDBFileFullPath(file_name), GetDBDir(), sync_dir);
  } while ((++num_retries < kNumDeleteRetries) &&
           (error_code == SQLITE_IOERR_DELETE) &&
           (PlatformThread::Sleep(10), 1));

  return error_code;
}

long BrowserDatabaseSystem::GetFileAttributes(
    const FilePath& file_name) {
  return VfsBackend::GetFileAttributes(GetDBFileFullPath(file_name));
}

long long BrowserDatabaseSystem::GetFileSize(
    const FilePath& file_name) {
  return VfsBackend::GetFileSize(GetDBFileFullPath(file_name));
}

void BrowserDatabaseSystem::ClearAllDatabases() {
  // TODO(dumi): implement this once we refactor DatabaseTracker
  //file_util::Delete(GetDBDir(), true);
}

FilePath BrowserDatabaseSystem::GetDBDir() {
  return temp_dir_.path().Append(FILE_PATH_LITERAL("databases"));
}

FilePath BrowserDatabaseSystem::GetDBFileFullPath(const FilePath& file_name) {
  return GetDBDir().Append(file_name);
}
