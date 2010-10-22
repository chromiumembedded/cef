// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _BROWSER_DATABASE_SYSTEM_H
#define _BROWSER_DATABASE_SYSTEM_H

#include "base/file_path.h"
#include "base/hash_tables.h"
#include "base/lock.h"
#include "base/platform_file.h"
#include "base/ref_counted.h"
#include "base/scoped_temp_dir.h"
#include "base/string16.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDatabaseObserver.h"
#include "webkit/database/database_connections.h"
#include "webkit/database/database_tracker.h"

class BrowserDatabaseSystem : public webkit_database::DatabaseTracker::Observer,
                             public WebKit::WebDatabaseObserver {
 public:
  static BrowserDatabaseSystem* GetInstance();
  BrowserDatabaseSystem();
  ~BrowserDatabaseSystem();

  // VFS functions
  base::PlatformFile OpenFile(const string16& vfs_file_name, int desired_flags);
  int DeleteFile(const string16& vfs_file_name, bool sync_dir);
  long GetFileAttributes(const string16& vfs_file_name);
  long long GetFileSize(const string16& vfs_file_name);

  // database tracker functions
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
                                     int64 database_size,
                                     int64 space_available);
  virtual void OnDatabaseScheduledForDeletion(const string16& origin_identifier,
                                              const string16& database_name);

  // WebDatabaseObserver implementation
  virtual void databaseOpened(const WebKit::WebDatabase& database);
  virtual void databaseModified(const WebKit::WebDatabase& database);
  virtual void databaseClosed(const WebKit::WebDatabase& database);

  void ClearAllDatabases();
  void SetDatabaseQuota(int64 quota);

 private:
  // The calls that come from the database tracker run on the main thread.
  // Therefore, we can only call DatabaseUtil::GetFullFilePathForVfsFile()
  // on the main thread. However, the VFS calls run on the DB thread and
  // they need to crack VFS file paths. To resolve this problem, we store
  // a map of vfs_file_names to file_paths. The map is updated on the main
  // thread on each DatabaseOpened() call that comes from the database
  // tracker, and is read on the DB thread by each VFS call.
  void SetFullFilePathsForVfsFile(const string16& origin_identifier,
                                  const string16& database_name);
  FilePath GetFullFilePathForVfsFile(const string16& vfs_file_name);

  static BrowserDatabaseSystem* instance_;

  bool waiting_for_dbs_to_close_;

  ScopedTempDir temp_dir_;

  scoped_refptr<webkit_database::DatabaseTracker> db_tracker_;

  Lock file_names_lock_;
  base::hash_map<string16, FilePath> file_names_;

  webkit_database::DatabaseConnections database_connections_;
};

#endif  // _BROWSER_DATABASE_SYSTEM_H
