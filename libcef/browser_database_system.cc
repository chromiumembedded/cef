// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "browser_database_system.h"

#include "base/auto_reset.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/platform_thread.h"
#include "base/process_util.h"
#include "third_party/sqlite/preprocessed/sqlite3.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDatabase.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"
#include "webkit/database/database_util.h"
#include "webkit/database/vfs_backend.h"

using webkit_database::DatabaseTracker;
using webkit_database::DatabaseUtil;
using webkit_database::VfsBackend;

BrowserDatabaseSystem* BrowserDatabaseSystem::instance_ = NULL;

BrowserDatabaseSystem* BrowserDatabaseSystem::GetInstance() {
  DCHECK(instance_);
  return instance_;
}

BrowserDatabaseSystem::BrowserDatabaseSystem()
    : waiting_for_dbs_to_close_(false) {
  temp_dir_.CreateUniqueTempDir();
  db_tracker_ = new DatabaseTracker(temp_dir_.path(), false);
  db_tracker_->AddObserver(this);
  DCHECK(!instance_);
  instance_ = this;
}

BrowserDatabaseSystem::~BrowserDatabaseSystem() {
  db_tracker_->RemoveObserver(this);
  instance_ = NULL;
}

base::PlatformFile BrowserDatabaseSystem::OpenFile(
      const string16& vfs_file_name, int desired_flags) {
  base::PlatformFile file_handle = base::kInvalidPlatformFileValue;
  FilePath file_name = GetFullFilePathForVfsFile(vfs_file_name);
  if (file_name.empty()) {
    VfsBackend::OpenTempFileInDirectory(
        db_tracker_->DatabaseDirectory(), desired_flags, &file_handle);
  } else {
    VfsBackend::OpenFile(file_name, desired_flags, &file_handle);
  }

  return file_handle;
}

int BrowserDatabaseSystem::DeleteFile(
    const string16& vfs_file_name, bool sync_dir) {
  // We try to delete the file multiple times, because that's what the default
  // VFS does (apparently deleting a file can sometimes fail on Windows).
  // We sleep for 10ms between retries for the same reason.
  const int kNumDeleteRetries = 3;
  int num_retries = 0;
  int error_code = SQLITE_OK;
  FilePath file_name = GetFullFilePathForVfsFile(vfs_file_name);
  do {
    error_code = VfsBackend::DeleteFile(file_name, sync_dir);
  } while ((++num_retries < kNumDeleteRetries) &&
           (error_code == SQLITE_IOERR_DELETE) &&
           (PlatformThread::Sleep(10), 1));

  return error_code;
}

long BrowserDatabaseSystem::GetFileAttributes(const string16& vfs_file_name) {
  return VfsBackend::GetFileAttributes(
      GetFullFilePathForVfsFile(vfs_file_name));
}

long long BrowserDatabaseSystem::GetFileSize(const string16& vfs_file_name) {
  return VfsBackend::GetFileSize(GetFullFilePathForVfsFile(vfs_file_name));
}

void BrowserDatabaseSystem::DatabaseOpened(const string16& origin_identifier,
                                          const string16& database_name,
                                          const string16& description,
                                          int64 estimated_size) {
  int64 database_size = 0;
  int64 space_available = 0;
  database_connections_.AddConnection(origin_identifier, database_name);
  db_tracker_->DatabaseOpened(origin_identifier, database_name, description,
                              estimated_size, &database_size, &space_available);
  SetFullFilePathsForVfsFile(origin_identifier, database_name);

  OnDatabaseSizeChanged(origin_identifier, database_name,
                        database_size, space_available);
}

void BrowserDatabaseSystem::DatabaseModified(const string16& origin_identifier,
                                            const string16& database_name) {
  DCHECK(database_connections_.IsDatabaseOpened(
      origin_identifier, database_name));
  db_tracker_->DatabaseModified(origin_identifier, database_name);
}

void BrowserDatabaseSystem::DatabaseClosed(const string16& origin_identifier,
                                          const string16& database_name) {
  DCHECK(database_connections_.IsDatabaseOpened(
      origin_identifier, database_name));
  db_tracker_->DatabaseClosed(origin_identifier, database_name);
  database_connections_.RemoveConnection(origin_identifier, database_name);

  if (waiting_for_dbs_to_close_ && database_connections_.IsEmpty())
    MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
}

void BrowserDatabaseSystem::OnDatabaseSizeChanged(
    const string16& origin_identifier,
    const string16& database_name,
    int64 database_size,
    int64 space_available) {
  if (database_connections_.IsOriginUsed(origin_identifier)) {
    WebKit::WebDatabase::updateDatabaseSize(
        origin_identifier, database_name, database_size, space_available);
  }
}

void BrowserDatabaseSystem::OnDatabaseScheduledForDeletion(
    const string16& origin_identifier,
    const string16& database_name) {
  WebKit::WebDatabase::closeDatabaseImmediately(
      origin_identifier, database_name);
}

void BrowserDatabaseSystem::databaseOpened(const WebKit::WebDatabase& database) {
  DatabaseOpened(database.securityOrigin().databaseIdentifier(),
                 database.name(), database.displayName(),
                 database.estimatedSize());
}

void BrowserDatabaseSystem::databaseModified(
    const WebKit::WebDatabase& database) {
  DatabaseModified(database.securityOrigin().databaseIdentifier(),
                   database.name());
}

void BrowserDatabaseSystem::databaseClosed(const WebKit::WebDatabase& database) {
  DatabaseClosed(database.securityOrigin().databaseIdentifier(),
                 database.name());
}

void BrowserDatabaseSystem::ClearAllDatabases() {
  // Wait for all databases to be closed.
  if (!database_connections_.IsEmpty()) {
    AutoReset<bool> waiting_for_dbs_auto_reset(&waiting_for_dbs_to_close_, true);
    MessageLoop::ScopedNestableTaskAllower nestable(MessageLoop::current());
    MessageLoop::current()->Run();
  }

  db_tracker_->CloseTrackerDatabaseAndClearCaches();
  file_util::Delete(db_tracker_->DatabaseDirectory(), true);
  file_names_.clear();
}

void BrowserDatabaseSystem::SetDatabaseQuota(int64 quota) {
  db_tracker_->SetDefaultQuota(quota);
}

void BrowserDatabaseSystem::SetFullFilePathsForVfsFile(
    const string16& origin_identifier,
    const string16& database_name) {
  string16 vfs_file_name = origin_identifier + ASCIIToUTF16("/") +
      database_name + ASCIIToUTF16("#");
  FilePath file_name =
      DatabaseUtil::GetFullFilePathForVfsFile(db_tracker_, vfs_file_name);

  AutoLock file_names_auto_lock(file_names_lock_);
  file_names_[vfs_file_name] = file_name;
  file_names_[vfs_file_name + ASCIIToUTF16("-journal")] =
      FilePath::FromWStringHack(file_name.ToWStringHack() +
                                ASCIIToWide("-journal"));
}

FilePath BrowserDatabaseSystem::GetFullFilePathForVfsFile(
    const string16& vfs_file_name) {
  AutoLock file_names_auto_lock(file_names_lock_);
  DCHECK(file_names_.find(vfs_file_name) != file_names_.end());
  return file_names_[vfs_file_name];
}