// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_DATABASE_SYSTEM_H_
#define CEF_LIBCEF_BROWSER_DATABASE_SYSTEM_H_
#pragma once

#include "base/file_path.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/platform_file.h"
#include "base/scoped_temp_dir.h"
#include "base/string16.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDatabaseObserver.h"
#include "webkit/database/database_connections.h"
#include "webkit/database/database_tracker.h"

namespace base {
class MessageLoopProxy;
class WaitableEvent;
}

class BrowserDatabaseSystem : public webkit_database::DatabaseTracker::Observer,
                             public WebKit::WebDatabaseObserver {
 public:
  static BrowserDatabaseSystem* GetInstance();

  BrowserDatabaseSystem();
  virtual ~BrowserDatabaseSystem();

  // WebDatabaseObserver implementation, these are called on the script
  // execution context thread on which the database is opened. This may be
  // the main thread or background WebWorker threads.
  virtual void databaseOpened(const WebKit::WebDatabase& database);
  virtual void databaseModified(const WebKit::WebDatabase& database);
  virtual void databaseClosed(const WebKit::WebDatabase& database);

  // SQLite VFS related methods, these are called on webcore's
  // background database threads via the WebKitPlatformSupport impl.
  base::PlatformFile OpenFile(const string16& vfs_file_name, int desired_flags);
  int DeleteFile(const string16& vfs_file_name, bool sync_dir);
  uint32 GetFileAttributes(const string16& vfs_file_name);
  int64 GetFileSize(const string16& vfs_file_name);
  int64 GetSpaceAvailable(const string16& origin_identifier);

  // For use by LayoutTestController, called on the main thread.
  void ClearAllDatabases();
  void SetDatabaseQuota(int64 quota);

 private:
  // Used by our WebDatabaseObserver impl, only called on the db_thread
  void DatabaseOpened(const string16& origin_identifier,
                      const string16& database_name,
                      const string16& description,
                      int64 estimated_size);
  void DatabaseModified(const string16& origin_identifier,
                        const string16& database_name);
  void DatabaseClosed(const string16& origin_identifier,
                      const string16& database_name);

  // DatabaseTracker::Observer implementation
  virtual void OnDatabaseSizeChanged(const string16& origin_identifier,
                                     const string16& database_name,
                                     int64 database_size) OVERRIDE;
  virtual void OnDatabaseScheduledForDeletion(
      const string16& origin_identifier,
      const string16& database_name) OVERRIDE;

  // Used by our public SQLite VFS methods, only called on the db_thread.
  void VfsOpenFile(const string16& vfs_file_name, int desired_flags,
                   base::PlatformFile* result, base::WaitableEvent* done_event);
  void VfsDeleteFile(const string16& vfs_file_name, bool sync_dir,
                     int* result, base::WaitableEvent* done_event);
  void VfsGetFileAttributes(const string16& vfs_file_name,
                            uint32* result, base::WaitableEvent* done_event);
  void VfsGetFileSize(const string16& vfs_file_name,
                      int64* result, base::WaitableEvent* done_event);
  void VfsGetSpaceAvailable(const string16& origin_identifier,
                            int64* result, base::WaitableEvent* done_event);

  FilePath GetFullFilePathForVfsFile(const string16& vfs_file_name);

  void ResetTracker();
  void ThreadCleanup(base::WaitableEvent* done_event);

  // Where the tracker database file and per origin database files reside.
  ScopedTempDir temp_dir_;

  // All access to the db_tracker (except for its construction) and
  // vfs operations are serialized on a background thread.
  base::Thread db_thread_;
  scoped_refptr<base::MessageLoopProxy> db_thread_proxy_;
  scoped_refptr<webkit_database::DatabaseTracker> db_tracker_;
  int64 quota_per_origin_;

  // Data members to support waiting for all connections to be closed.
  scoped_refptr<webkit_database::DatabaseConnectionsWrapper> open_connections_;

  static BrowserDatabaseSystem* instance_;
};

#endif  // CEF_LIBCEF_BROWSER_DATABASE_SYSTEM_H_
