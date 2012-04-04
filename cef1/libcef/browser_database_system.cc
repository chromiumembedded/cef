// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser_database_system.h"

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/utf_string_conversions.h"
#include "third_party/sqlite/sqlite3.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDatabase.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "webkit/database/database_util.h"
#include "webkit/database/vfs_backend.h"

using webkit_database::DatabaseTracker;
using webkit_database::DatabaseUtil;
using webkit_database::OriginInfo;
using webkit_database::VfsBackend;

BrowserDatabaseSystem* BrowserDatabaseSystem::instance_ = NULL;

BrowserDatabaseSystem* BrowserDatabaseSystem::GetInstance() {
  DCHECK(instance_);
  return instance_;
}

BrowserDatabaseSystem::BrowserDatabaseSystem()
    : db_thread_("BrowserDBThread"),
      quota_per_origin_(5 * 1024 * 1024),
      open_connections_(new webkit_database::DatabaseConnectionsWrapper) {
  DCHECK(!instance_);
  instance_ = this;
  CHECK(temp_dir_.CreateUniqueTempDir());
  db_tracker_ =
      new DatabaseTracker(temp_dir_.path(), false, NULL, NULL, NULL);
  db_tracker_->AddObserver(this);
  db_thread_.Start();
  db_thread_proxy_ = db_thread_.message_loop_proxy();
}

BrowserDatabaseSystem::~BrowserDatabaseSystem() {
  base::WaitableEvent done_event(false, false);
  db_thread_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&BrowserDatabaseSystem::ThreadCleanup,
                 base::Unretained(this), &done_event));
  done_event.Wait();
  instance_ = NULL;
}

void BrowserDatabaseSystem::databaseOpened(
    const WebKit::WebDatabase& database) {
  string16 origin_identifier = database.securityOrigin().databaseIdentifier();
  string16 database_name = database.name();
  open_connections_->AddOpenConnection(origin_identifier, database_name);
  db_thread_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&BrowserDatabaseSystem::DatabaseOpened,
                 base::Unretained(this),
                 origin_identifier,
                 database_name, database.displayName(),
                 database.estimatedSize()));
}

void BrowserDatabaseSystem::databaseModified(
    const WebKit::WebDatabase& database) {
  db_thread_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&BrowserDatabaseSystem::DatabaseModified,
                 base::Unretained(this),
                 database.securityOrigin().databaseIdentifier(),
                 database.name()));
}

void BrowserDatabaseSystem::databaseClosed(
    const WebKit::WebDatabase& database) {
  string16 origin_identifier = database.securityOrigin().databaseIdentifier();
  string16 database_name = database.name();
  db_thread_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&BrowserDatabaseSystem::DatabaseClosed,
                 base::Unretained(this), origin_identifier, database_name));
}

base::PlatformFile BrowserDatabaseSystem::OpenFile(
    const string16& vfs_file_name, int desired_flags) {
  base::PlatformFile result = base::kInvalidPlatformFileValue;
  base::WaitableEvent done_event(false, false);
  db_thread_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&BrowserDatabaseSystem::VfsOpenFile,
                 base::Unretained(this),
                 vfs_file_name, desired_flags,
                 &result, &done_event));
  done_event.Wait();
  return result;
}

int BrowserDatabaseSystem::DeleteFile(
    const string16& vfs_file_name, bool sync_dir) {
  int result = SQLITE_OK;
  base::WaitableEvent done_event(false, false);
  db_thread_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&BrowserDatabaseSystem::VfsDeleteFile,
                 base::Unretained(this),
                 vfs_file_name, sync_dir,
                 &result, &done_event));
  done_event.Wait();
  return result;
}

uint32 BrowserDatabaseSystem::GetFileAttributes(const string16& vfs_file_name) {
  uint32 result = 0;
  base::WaitableEvent done_event(false, false);
  db_thread_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&BrowserDatabaseSystem::VfsGetFileAttributes,
                 base::Unretained(this), vfs_file_name, &result, &done_event));
  done_event.Wait();
  return result;
}

int64 BrowserDatabaseSystem::GetFileSize(const string16& vfs_file_name) {
  int64 result = 0;
  base::WaitableEvent done_event(false, false);
  db_thread_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&BrowserDatabaseSystem::VfsGetFileSize,
                 base::Unretained(this), vfs_file_name, &result, &done_event));
  done_event.Wait();
  return result;
}

int64 BrowserDatabaseSystem::GetSpaceAvailable(
    const string16& origin_identifier) {
  int64 result = 0;
  base::WaitableEvent done_event(false, false);
  db_thread_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&BrowserDatabaseSystem::VfsGetSpaceAvailable,
                 base::Unretained(this), origin_identifier,
                 &result, &done_event));
  done_event.Wait();
  return result;
}

void BrowserDatabaseSystem::ClearAllDatabases() {
  open_connections_->WaitForAllDatabasesToClose();
  db_thread_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&BrowserDatabaseSystem::ResetTracker, base::Unretained(this)));
}

void BrowserDatabaseSystem::SetDatabaseQuota(int64 quota) {
  if (!db_thread_proxy_->BelongsToCurrentThread()) {
    db_thread_proxy_->PostTask(
        FROM_HERE,
        base::Bind(&BrowserDatabaseSystem::SetDatabaseQuota,
                   base::Unretained(this), quota));
    return;
  }
  quota_per_origin_ = quota;
}

void BrowserDatabaseSystem::DatabaseOpened(const string16& origin_identifier,
                                          const string16& database_name,
                                          const string16& description,
                                          int64 estimated_size) {
  DCHECK(db_thread_proxy_->BelongsToCurrentThread());
  int64 database_size = 0;
  db_tracker_->DatabaseOpened(
      origin_identifier, database_name, description,
      estimated_size, &database_size);
  OnDatabaseSizeChanged(origin_identifier, database_name,
                        database_size);
}

void BrowserDatabaseSystem::DatabaseModified(const string16& origin_identifier,
                                            const string16& database_name) {
  DCHECK(db_thread_proxy_->BelongsToCurrentThread());
  db_tracker_->DatabaseModified(origin_identifier, database_name);
}

void BrowserDatabaseSystem::DatabaseClosed(const string16& origin_identifier,
                                          const string16& database_name) {
  DCHECK(db_thread_proxy_->BelongsToCurrentThread());
  db_tracker_->DatabaseClosed(origin_identifier, database_name);
  open_connections_->RemoveOpenConnection(origin_identifier, database_name);
}

void BrowserDatabaseSystem::OnDatabaseSizeChanged(
    const string16& origin_identifier,
    const string16& database_name,
    int64 database_size) {
  DCHECK(db_thread_proxy_->BelongsToCurrentThread());
  // We intentionally call into webkit on our background db_thread_
  // to better emulate what happens in chrome where this method is
  // invoked on the background ipc thread.
  WebKit::WebDatabase::updateDatabaseSize(
      origin_identifier, database_name, database_size);
}

void BrowserDatabaseSystem::OnDatabaseScheduledForDeletion(
    const string16& origin_identifier,
    const string16& database_name) {
  DCHECK(db_thread_proxy_->BelongsToCurrentThread());
  // We intentionally call into webkit on our background db_thread_
  // to better emulate what happens in chrome where this method is
  // invoked on the background ipc thread.
  WebKit::WebDatabase::closeDatabaseImmediately(
      origin_identifier, database_name);
}

void BrowserDatabaseSystem::VfsOpenFile(
    const string16& vfs_file_name, int desired_flags,
    base::PlatformFile* file_handle, base::WaitableEvent* done_event ) {
  DCHECK(db_thread_proxy_->BelongsToCurrentThread());
  FilePath file_name = GetFullFilePathForVfsFile(vfs_file_name);
  if (file_name.empty()) {
    VfsBackend::OpenTempFileInDirectory(
        db_tracker_->DatabaseDirectory(), desired_flags, file_handle);
  } else {
    VfsBackend::OpenFile(file_name, desired_flags, file_handle);
  }
  done_event->Signal();
}

void BrowserDatabaseSystem::VfsDeleteFile(
    const string16& vfs_file_name, bool sync_dir,
    int* result, base::WaitableEvent* done_event) {
  DCHECK(db_thread_proxy_->BelongsToCurrentThread());
  // We try to delete the file multiple times, because that's what the default
  // VFS does (apparently deleting a file can sometimes fail on Windows).
  // We sleep for 10ms between retries for the same reason.
  const int kNumDeleteRetries = 3;
  int num_retries = 0;
  *result = SQLITE_OK;
  FilePath file_name = GetFullFilePathForVfsFile(vfs_file_name);
  do {
    *result = VfsBackend::DeleteFile(file_name, sync_dir);
  } while ((++num_retries < kNumDeleteRetries) &&
           (*result == SQLITE_IOERR_DELETE) &&
           (base::PlatformThread::Sleep(
               base::TimeDelta::FromMilliseconds(10)),
            1));

  done_event->Signal();
}

void BrowserDatabaseSystem::VfsGetFileAttributes(
    const string16& vfs_file_name,
    uint32* result, base::WaitableEvent* done_event) {
  DCHECK(db_thread_proxy_->BelongsToCurrentThread());
  *result = VfsBackend::GetFileAttributes(
      GetFullFilePathForVfsFile(vfs_file_name));
  done_event->Signal();
}

void BrowserDatabaseSystem::VfsGetFileSize(
    const string16& vfs_file_name,
    int64* result, base::WaitableEvent* done_event) {
  DCHECK(db_thread_proxy_->BelongsToCurrentThread());
  *result = VfsBackend::GetFileSize(GetFullFilePathForVfsFile(vfs_file_name));
  done_event->Signal();
}

void BrowserDatabaseSystem::VfsGetSpaceAvailable(
    const string16& origin_identifier,
    int64* result, base::WaitableEvent* done_event) {
  DCHECK(db_thread_proxy_->BelongsToCurrentThread());
  // This method isn't actually part of the "vfs" interface, but it is
  // used from within webcore and handled here in the same fashion.
  OriginInfo info;
  if (db_tracker_->GetOriginInfo(origin_identifier, &info)) {
    int64 space_available = quota_per_origin_ - info.TotalSize();
    *result = space_available < 0 ? 0 : space_available;
  } else {
    NOTREACHED();
    *result = 0;
  }
  done_event->Signal();
}

FilePath BrowserDatabaseSystem::GetFullFilePathForVfsFile(
    const string16& vfs_file_name) {
  DCHECK(db_thread_proxy_->BelongsToCurrentThread());
  if (vfs_file_name.empty())  // temp file, used for vacuuming
    return FilePath();
  return DatabaseUtil::GetFullFilePathForVfsFile(
      db_tracker_.get(), vfs_file_name);
}

void BrowserDatabaseSystem::ResetTracker() {
  DCHECK(db_thread_proxy_->BelongsToCurrentThread());
  db_tracker_->CloseTrackerDatabaseAndClearCaches();
  file_util::Delete(db_tracker_->DatabaseDirectory(), true);
}

void BrowserDatabaseSystem::ThreadCleanup(base::WaitableEvent* done_event) {
  ResetTracker();
  db_tracker_->RemoveObserver(this);
  db_tracker_ = NULL;
  done_event->Signal();
}

