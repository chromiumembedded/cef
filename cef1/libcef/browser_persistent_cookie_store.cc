// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser_persistent_cookie_store.h"

#include <algorithm>
#include <list>
#include <map>
#include <set>
#include <utility>

#include "libcef/cef_thread.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "googleurl/src/gurl.h"
#include "net/base/registry_controlled_domain.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"

using base::Time;

// This class is designed to be shared between any calling threads and the
// database thread. It batches operations and commits them on a timer.
//
// BrowserPersistentCookieStore::Load is called to load all cookies.  It
// delegates to Backend::Load, which posts a Backend::LoadAndNotifyOnDBThread
// task to the DB thread.  This task calls Backend::ChainLoadCookies(), which
// repeatedly posts itself to the DB thread to load each eTLD+1's cookies in
// separate tasks.  When this is complete, Backend::NotifyOnIOThread is posted
// to the IO thread, which notifies the caller of BrowserPersistentCookieStore::
// Load that the load is complete.
//
// If a priority load request is invoked via BrowserPersistentCookieStore::
// LoadCookiesForKey, it is delegated to Backend::LoadCookiesForKey, which posts
// Backend::LoadKeyAndNotifyOnDBThread to the DB thread. That routine loads just
// that single domain key (eTLD+1)'s cookies, and posts a Backend::
// NotifyOnIOThread to the IO thread to notify the caller of
// BrowserPersistentCookieStore::LoadCookiesForKey that that load is complete.
//
// Subsequent to loading, mutations may be queued by any thread using
// AddCookie, UpdateCookieAccessTime, and DeleteCookie. These are flushed to
// disk on the DB thread every 30 seconds, 512 operations, or call to Flush(),
// whichever occurs first.
class BrowserPersistentCookieStore::Backend
    : public base::RefCountedThreadSafe<BrowserPersistentCookieStore::Backend> {
 public:
  Backend(const FilePath& path, bool restore_old_session_cookies)
      : path_(path),
        db_(NULL),
        num_pending_(0),
        clear_local_state_on_exit_(false),
        initialized_(false),
        restore_old_session_cookies_(restore_old_session_cookies) {
  }

  // Creates or loads the SQLite database.
  void Load(const LoadedCallback& loaded_callback);

  // Loads cookies for the domain key (eTLD+1).
  void LoadCookiesForKey(const std::string& domain,
      const LoadedCallback& loaded_callback);

  // Batch a cookie addition.
  void AddCookie(const net::CookieMonster::CanonicalCookie& cc);

  // Batch a cookie access time update.
  void UpdateCookieAccessTime(const net::CookieMonster::CanonicalCookie& cc);

  // Batch a cookie deletion.
  void DeleteCookie(const net::CookieMonster::CanonicalCookie& cc);

  // Commit pending operations as soon as possible.
  void Flush(const base::Closure& callback);

  // Commit any pending operations and close the database.  This must be called
  // before the object is destructed.
  void Close();

  void SetClearLocalStateOnExit(bool clear_local_state);

 private:
  friend class base::RefCountedThreadSafe<
      BrowserPersistentCookieStore::Backend>;

  // You should call Close() before destructing this object.
  ~Backend() {
    DCHECK(!db_.get()) << "Close should have already been called.";
    DCHECK(num_pending_ == 0 && pending_.empty());
  }

  // Database upgrade statements.
  bool EnsureDatabaseVersion();

  class PendingOperation {
   public:
    typedef enum {
      COOKIE_ADD,
      COOKIE_UPDATEACCESS,
      COOKIE_DELETE,
    } OperationType;

    PendingOperation(OperationType op,
                     const net::CookieMonster::CanonicalCookie& cc)
        : op_(op), cc_(cc) { }

    OperationType op() const { return op_; }
    const net::CookieMonster::CanonicalCookie& cc() const { return cc_; }

   private:
    OperationType op_;
    net::CookieMonster::CanonicalCookie cc_;
  };

 private:
  // Creates or loads the SQLite database on DB thread.
  void LoadAndNotifyOnDBThread(const LoadedCallback& loaded_callback);

  // Loads cookies for the domain key (eTLD+1) on DB thread.
  void LoadKeyAndNotifyOnDBThread(const std::string& domains,
      const LoadedCallback& loaded_callback);

  // Notifies the CookieMonster when loading completes for a specific domain key
  // or for all domain keys. Triggers the callback and passes it all cookies
  // that have been loaded from DB since last IO notification.
  void NotifyOnIOThread(
      const LoadedCallback& loaded_callback,
      bool load_success);

  // Initialize the data base.
  bool InitializeDatabase();

  // Loads cookies for the next domain key from the DB, then either reschedules
  // itself or schedules the provided callback to run on the IO thread (if all
  // domains are loaded).
  void ChainLoadCookies(const LoadedCallback& loaded_callback);

  // Load all cookies for a set of domains/hosts
  bool LoadCookiesForDomains(const std::set<std::string>& key);

  // Batch a cookie operation (add or delete)
  void BatchOperation(PendingOperation::OperationType op,
                      const net::CookieMonster::CanonicalCookie& cc);
  // Commit our pending operations to the database.
  void Commit();
  // Close() executed on the background thread.
  void InternalBackgroundClose();

  void DeleteSessionCookies();

  FilePath path_;
  scoped_ptr<sql::Connection> db_;
  sql::MetaTable meta_table_;

  typedef std::list<PendingOperation*> PendingOperationsList;
  PendingOperationsList pending_;
  PendingOperationsList::size_type num_pending_;
  // True if the persistent store should be deleted upon destruction.
  bool clear_local_state_on_exit_;
  // Guard |cookies_|, |pending_|, |num_pending_|, |clear_local_state_on_exit_|
  base::Lock lock_;

  // Temporary buffer for cookies loaded from DB. Accumulates cookies to reduce
  // the number of messages sent to the IO thread. Sent back in response to
  // individual load requests for domain keys or when all loading completes.
  std::vector<net::CookieMonster::CanonicalCookie*> cookies_;

  // Map of domain keys(eTLD+1) to domains/hosts that are to be loaded from DB.
  std::map<std::string, std::set<std::string> > keys_to_load_;

  // Indicates if DB has been initialized.
  bool initialized_;

  // If false, we should filter out session cookies when reading the DB.
  bool restore_old_session_cookies_;
  DISALLOW_COPY_AND_ASSIGN(Backend);
};

// Version number of the database.
//
// Version 5 adds the columns has_expires and is_persistent, so that the
// database can store session cookies as well as persistent cookies. Databases
// of version 5 are incompatible with older versions of code. If a database of
// version 5 is read by older code, session cookies will be treated as normal
// cookies.
//
// In version 4, we migrated the time epoch.  If you open the DB with an older
// version on Mac or Linux, the times will look wonky, but the file will likely
// be usable. On Windows version 3 and 4 are the same.
//
// Version 3 updated the database to include the last access time, so we can
// expire them in decreasing order of use when we've reached the maximum
// number of cookies.
static const int kCurrentVersionNumber = 5;
static const int kCompatibleVersionNumber = 5;

namespace {

// Initializes the cookies table, returning true on success.
bool InitTable(sql::Connection* db) {
  if (!db->DoesTableExist("cookies")) {
    if (!db->Execute("CREATE TABLE cookies ("
                     "creation_utc INTEGER NOT NULL UNIQUE PRIMARY KEY,"
                     "host_key TEXT NOT NULL,"
                     "name TEXT NOT NULL,"
                     "value TEXT NOT NULL,"
                     "path TEXT NOT NULL,"
                     "expires_utc INTEGER NOT NULL,"
                     "secure INTEGER NOT NULL,"
                     "httponly INTEGER NOT NULL,"
                     "last_access_utc INTEGER NOT NULL, "
                     "has_expires INTEGER NOT NULL DEFAULT 1, "
                     "persistent INTEGER NOT NULL DEFAULT 1)"))
      return false;
  }

  // Older code created an index on creation_utc, which is already
  // primary key for the table.
  if (!db->Execute("DROP INDEX IF EXISTS cookie_times"))
    return false;

  if (!db->Execute("CREATE INDEX IF NOT EXISTS domain ON cookies(host_key)"))
    return false;

  return true;
}

}  // namespace

void BrowserPersistentCookieStore::Backend::Load(
    const LoadedCallback& loaded_callback) {
  // This function should be called only once per instance.
  DCHECK(!db_.get());
  CefThread::PostTask(
      CefThread::FILE, FROM_HERE,
      base::Bind(&Backend::LoadAndNotifyOnDBThread, this, loaded_callback));
}

void BrowserPersistentCookieStore::Backend::LoadCookiesForKey(
    const std::string& key,
    const LoadedCallback& loaded_callback) {
  CefThread::PostTask(
    CefThread::FILE, FROM_HERE,
    base::Bind(&Backend::LoadKeyAndNotifyOnDBThread, this,
    key,
    loaded_callback));
}

void BrowserPersistentCookieStore::Backend::LoadAndNotifyOnDBThread(
    const LoadedCallback& loaded_callback) {
  DCHECK(CefThread::CurrentlyOn(CefThread::FILE));

  if (!InitializeDatabase()) {
    CefThread::PostTask(
      CefThread::IO, FROM_HERE,
      base::Bind(&BrowserPersistentCookieStore::Backend::NotifyOnIOThread,
                 this, loaded_callback, false));
  } else {
    ChainLoadCookies(loaded_callback);
  }
}

void BrowserPersistentCookieStore::Backend::LoadKeyAndNotifyOnDBThread(
    const std::string& key,
    const LoadedCallback& loaded_callback) {
  DCHECK(CefThread::CurrentlyOn(CefThread::FILE));

  bool success = false;
  if (InitializeDatabase()) {
    std::map<std::string, std::set<std::string> >::iterator
      it = keys_to_load_.find(key);
    if (it != keys_to_load_.end()) {
      success = LoadCookiesForDomains(it->second);
      keys_to_load_.erase(it);
    } else {
      success = true;
    }
  }

  CefThread::PostTask(
    CefThread::IO, FROM_HERE,
    base::Bind(&BrowserPersistentCookieStore::Backend::NotifyOnIOThread,
    this, loaded_callback, success));
}

void BrowserPersistentCookieStore::Backend::NotifyOnIOThread(
    const LoadedCallback& loaded_callback,
    bool load_success) {
  DCHECK(CefThread::CurrentlyOn(CefThread::IO));

  std::vector<net::CookieMonster::CanonicalCookie*> cookies;
  {
    base::AutoLock locked(lock_);
    cookies.swap(cookies_);
  }

  loaded_callback.Run(cookies);
}

bool BrowserPersistentCookieStore::Backend::InitializeDatabase() {
  DCHECK(CefThread::CurrentlyOn(CefThread::FILE));

  if (initialized_) {
    // Return false if we were previously initialized but the DB has since been
    // closed.
    return db_.get() ? true : false;
  }

  const FilePath dir = path_.DirName();
  if (!file_util::PathExists(dir) && !file_util::CreateDirectory(dir)) {
    return false;
  }

  db_.reset(new sql::Connection);
  if (!db_->Open(path_)) {
    NOTREACHED() << "Unable to open cookie DB.";
    db_.reset();
    return false;
  }

  // db_->set_error_delegate(GetErrorHandlerForCookieDb());

  if (!EnsureDatabaseVersion() || !InitTable(db_.get())) {
    NOTREACHED() << "Unable to open cookie DB.";
    db_.reset();
    return false;
  }

  // Retrieve all the domains
  sql::Statement smt(db_->GetUniqueStatement(
    "SELECT DISTINCT host_key FROM cookies"));

  if (!smt.is_valid()) {
    smt.Clear();  // Disconnect smt_ref from db_.
    db_.reset();
    return false;
  }

  // Build a map of domain keys (always eTLD+1) to domains.
  while (smt.Step()) {
    std::string domain = smt.ColumnString(0);
    std::string key =
      net::RegistryControlledDomainService::GetDomainAndRegistry(domain);

    std::map<std::string, std::set<std::string> >::iterator it =
      keys_to_load_.find(key);
    if (it == keys_to_load_.end())
      it = keys_to_load_.insert(std::make_pair
                                (key, std::set<std::string>())).first;
    it->second.insert(domain);
  }

  initialized_ = true;
  return true;
}

void BrowserPersistentCookieStore::Backend::ChainLoadCookies(
    const LoadedCallback& loaded_callback) {
  DCHECK(CefThread::CurrentlyOn(CefThread::FILE));

  bool load_success = true;

  if (!db_.get()) {
    // Close() has been called on this store.
    load_success = false;
  } else if (keys_to_load_.size() > 0) {
    // Load cookies for the first domain key.
    std::map<std::string, std::set<std::string> >::iterator
      it = keys_to_load_.begin();
    load_success = LoadCookiesForDomains(it->second);
    keys_to_load_.erase(it);
  }

  // If load is successful and there are more domain keys to be loaded,
  // then post a DB task to continue chain-load;
  // Otherwise notify on IO thread.
  if (load_success && keys_to_load_.size() > 0) {
    CefThread::PostTask(
      CefThread::FILE, FROM_HERE,
      base::Bind(&Backend::ChainLoadCookies, this, loaded_callback));
  } else {
    CefThread::PostTask(
      CefThread::IO, FROM_HERE,
      base::Bind(&BrowserPersistentCookieStore::Backend::NotifyOnIOThread,
                 this, loaded_callback, load_success));
    if (load_success && !restore_old_session_cookies_)
      DeleteSessionCookies();
  }
}

bool BrowserPersistentCookieStore::Backend::LoadCookiesForDomains(
  const std::set<std::string>& domains) {
  DCHECK(CefThread::CurrentlyOn(CefThread::FILE));

  sql::Statement smt;
  if (restore_old_session_cookies_) {
    smt.Assign(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT creation_utc, host_key, name, value, path, expires_utc, "
      "secure, httponly, last_access_utc, has_expires, persistent "
      "FROM cookies WHERE host_key = ?"));
  } else {
    smt.Assign(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT creation_utc, host_key, name, value, path, expires_utc, "
      "secure, httponly, last_access_utc, has_expires, persistent "
      "FROM cookies WHERE host_key = ? AND persistent = 1"));
  }
  if (!smt.is_valid()) {
    NOTREACHED() << "select statement prep failed";
    smt.Clear();  // Disconnect smt_ref from db_.
    db_.reset();
    return false;
  }

  std::vector<net::CookieMonster::CanonicalCookie*> cookies;
  std::set<std::string>::const_iterator it = domains.begin();
  for (; it != domains.end(); ++it) {
    smt.BindString(0, *it);
    while (smt.Step()) {
      scoped_ptr<net::CookieMonster::CanonicalCookie> cc(
          new net::CookieMonster::CanonicalCookie(
              // The "source" URL is not used with persisted cookies.
              GURL(),                                         // Source
              smt.ColumnString(2),                            // name
              smt.ColumnString(3),                            // value
              smt.ColumnString(1),                            // domain
              smt.ColumnString(4),                            // path
              std::string(),  // TODO(abarth): Persist mac_key
              std::string(),  // TODO(abarth): Persist mac_algorithm
              Time::FromInternalValue(smt.ColumnInt64(0)),    // creation_utc
              Time::FromInternalValue(smt.ColumnInt64(5)),    // expires_utc
              Time::FromInternalValue(smt.ColumnInt64(8)),    // last_access_utc
              smt.ColumnInt(6) != 0,                          // secure
              smt.ColumnInt(7) != 0,                          // httponly
              smt.ColumnInt(9) != 0,                          // has_expires
              smt.ColumnInt(10) != 0));                       // is_persistent
      DLOG_IF(WARNING,
              cc->CreationDate() > Time::Now()) << L"CreationDate too recent";
      cookies.push_back(cc.release());
    }
    smt.Reset();
  }
  {
    base::AutoLock locked(lock_);
    cookies_.insert(cookies_.end(), cookies.begin(), cookies.end());
  }
  return true;
}

bool BrowserPersistentCookieStore::Backend::EnsureDatabaseVersion() {
  // Version check.
  if (!meta_table_.Init(
      db_.get(), kCurrentVersionNumber, kCompatibleVersionNumber)) {
    return false;
  }

  if (meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    LOG(WARNING) << "Cookie database is too new.";
    return false;
  }

  int cur_version = meta_table_.GetVersionNumber();
  if (cur_version == 2) {
    sql::Transaction transaction(db_.get());
    if (!transaction.Begin())
      return false;
    if (!db_->Execute("ALTER TABLE cookies ADD COLUMN last_access_utc "
                     "INTEGER DEFAULT 0") ||
        !db_->Execute("UPDATE cookies SET last_access_utc = creation_utc")) {
      LOG(WARNING) << "Unable to update cookie database to version 3.";
      return false;
    }
    ++cur_version;
    meta_table_.SetVersionNumber(cur_version);
    meta_table_.SetCompatibleVersionNumber(
        std::min(cur_version, kCompatibleVersionNumber));
    transaction.Commit();
  }

  if (cur_version == 3) {
    // The time epoch changed for Mac & Linux in this version to match Windows.
    // This patch came after the main epoch change happened, so some
    // developers have "good" times for cookies added by the more recent
    // versions. So we have to be careful to only update times that are under
    // the old system (which will appear to be from before 1970 in the new
    // system). The magic number used below is 1970 in our time units.
    sql::Transaction transaction(db_.get());
    transaction.Begin();
#if !defined(OS_WIN)
    ignore_result(db_->Execute(
        "UPDATE cookies "
        "SET creation_utc = creation_utc + 11644473600000000 "
        "WHERE rowid IN "
        "(SELECT rowid FROM cookies WHERE "
          "creation_utc > 0 AND creation_utc < 11644473600000000)"));
    ignore_result(db_->Execute(
        "UPDATE cookies "
        "SET expires_utc = expires_utc + 11644473600000000 "
        "WHERE rowid IN "
        "(SELECT rowid FROM cookies WHERE "
          "expires_utc > 0 AND expires_utc < 11644473600000000)"));
    ignore_result(db_->Execute(
        "UPDATE cookies "
        "SET last_access_utc = last_access_utc + 11644473600000000 "
        "WHERE rowid IN "
        "(SELECT rowid FROM cookies WHERE "
          "last_access_utc > 0 AND last_access_utc < 11644473600000000)"));
#endif
    ++cur_version;
    meta_table_.SetVersionNumber(cur_version);
    transaction.Commit();
  }

  if (cur_version == 4) {
    sql::Transaction transaction(db_.get());
    if (!transaction.Begin())
      return false;
    if (!db_->Execute("ALTER TABLE cookies "
                      "ADD COLUMN has_expires INTEGER DEFAULT 1") ||
        !db_->Execute("ALTER TABLE cookies "
                      "ADD COLUMN persistent INTEGER DEFAULT 1")) {
      LOG(WARNING) << "Unable to update cookie database to version 5.";
      return false;
    }
    ++cur_version;
    meta_table_.SetVersionNumber(cur_version);
    meta_table_.SetCompatibleVersionNumber(
        std::min(cur_version, kCompatibleVersionNumber));
    transaction.Commit();
  }

  // Put future migration cases here.

  // When the version is too old, we just try to continue anyway, there should
  // not be a released product that makes a database too old for us to handle.
  LOG_IF(WARNING, cur_version < kCurrentVersionNumber) <<
      "Cookie database version " << cur_version << " is too old to handle.";

  return true;
}

void BrowserPersistentCookieStore::Backend::AddCookie(
    const net::CookieMonster::CanonicalCookie& cc) {
  BatchOperation(PendingOperation::COOKIE_ADD, cc);
}

void BrowserPersistentCookieStore::Backend::UpdateCookieAccessTime(
    const net::CookieMonster::CanonicalCookie& cc) {
  BatchOperation(PendingOperation::COOKIE_UPDATEACCESS, cc);
}

void BrowserPersistentCookieStore::Backend::DeleteCookie(
    const net::CookieMonster::CanonicalCookie& cc) {
  BatchOperation(PendingOperation::COOKIE_DELETE, cc);
}

void BrowserPersistentCookieStore::Backend::BatchOperation(
    PendingOperation::OperationType op,
    const net::CookieMonster::CanonicalCookie& cc) {
  // Commit every 30 seconds.
  static const int kCommitIntervalMs = 30 * 1000;
  // Commit right away if we have more than 512 outstanding operations.
  static const size_t kCommitAfterBatchSize = 512;
  DCHECK(!CefThread::CurrentlyOn(CefThread::FILE));

  // We do a full copy of the cookie here, and hopefully just here.
  scoped_ptr<PendingOperation> po(new PendingOperation(op, cc));

  PendingOperationsList::size_type num_pending;
  {
    base::AutoLock locked(lock_);
    pending_.push_back(po.release());
    num_pending = ++num_pending_;
  }

  if (num_pending == 1) {
    // We've gotten our first entry for this batch, fire off the timer.
    CefThread::PostDelayedTask(
        CefThread::FILE, FROM_HERE,
        base::Bind(&Backend::Commit, this),
        base::TimeDelta::FromMilliseconds(kCommitIntervalMs));
  } else if (num_pending == kCommitAfterBatchSize) {
    // We've reached a big enough batch, fire off a commit now.
    CefThread::PostTask(
        CefThread::FILE, FROM_HERE,
        base::Bind(&Backend::Commit, this));
  }
}

void BrowserPersistentCookieStore::Backend::Commit() {
  DCHECK(CefThread::CurrentlyOn(CefThread::FILE));

  PendingOperationsList ops;
  {
    base::AutoLock locked(lock_);
    pending_.swap(ops);
    num_pending_ = 0;
  }

  // Maybe an old timer fired or we are already Close()'ed.
  if (!db_.get() || ops.empty())
    return;

  sql::Statement add_smt(db_->GetCachedStatement(SQL_FROM_HERE,
      "INSERT INTO cookies (creation_utc, host_key, name, value, path, "
      "expires_utc, secure, httponly, last_access_utc, has_expires, "
      "persistent) "
      "VALUES (?,?,?,?,?,?,?,?,?,?,?)"));
  if (!add_smt.is_valid())
    return;

  sql::Statement update_access_smt(db_->GetCachedStatement(SQL_FROM_HERE,
      "UPDATE cookies SET last_access_utc=? WHERE creation_utc=?"));
  if (!update_access_smt.is_valid())
    return;

  sql::Statement del_smt(db_->GetCachedStatement(SQL_FROM_HERE,
                         "DELETE FROM cookies WHERE creation_utc=?"));
  if (!del_smt.is_valid())
    return;

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return;

  for (PendingOperationsList::iterator it = ops.begin();
       it != ops.end(); ++it) {
    // Free the cookies as we commit them to the database.
    scoped_ptr<PendingOperation> po(*it);
    switch (po->op()) {
      case PendingOperation::COOKIE_ADD:
        add_smt.Reset();
        add_smt.BindInt64(0, po->cc().CreationDate().ToInternalValue());
        add_smt.BindString(1, po->cc().Domain());
        add_smt.BindString(2, po->cc().Name());
        add_smt.BindString(3, po->cc().Value());
        add_smt.BindString(4, po->cc().Path());
        add_smt.BindInt64(5, po->cc().ExpiryDate().ToInternalValue());
        add_smt.BindInt(6, po->cc().IsSecure());
        add_smt.BindInt(7, po->cc().IsHttpOnly());
        add_smt.BindInt64(8, po->cc().LastAccessDate().ToInternalValue());
        add_smt.BindInt(9, po->cc().DoesExpire());
        add_smt.BindInt(10, po->cc().IsPersistent());
        if (!add_smt.Run())
          NOTREACHED() << "Could not add a cookie to the DB.";
        break;

      case PendingOperation::COOKIE_UPDATEACCESS:
        update_access_smt.Reset();
        update_access_smt.BindInt64(0,
            po->cc().LastAccessDate().ToInternalValue());
        update_access_smt.BindInt64(1,
            po->cc().CreationDate().ToInternalValue());
        if (!update_access_smt.Run())
          NOTREACHED() << "Could not update cookie last access time in the DB.";
        break;

      case PendingOperation::COOKIE_DELETE:
        del_smt.Reset();
        del_smt.BindInt64(0, po->cc().CreationDate().ToInternalValue());
        if (!del_smt.Run())
          NOTREACHED() << "Could not delete a cookie from the DB.";
        break;

      default:
        NOTREACHED();
        break;
    }
  }
  transaction.Commit();
}

void BrowserPersistentCookieStore::Backend::Flush(
    const base::Closure& callback) {
  DCHECK(!CefThread::CurrentlyOn(CefThread::FILE));
  CefThread::PostTask(
      CefThread::FILE, FROM_HERE, base::Bind(&Backend::Commit, this));
  if (!callback.is_null()) {
    // We want the completion task to run immediately after Commit() returns.
    // Posting it from here means there is less chance of another task getting
    // onto the message queue first, than if we posted it from Commit() itself.
    CefThread::PostTask(CefThread::FILE, FROM_HERE, callback);
  }
}

// Fire off a close message to the background thread.  We could still have a
// pending commit timer or Load operations holding references on us, but if/when
// this fires we will already have been cleaned up and it will be ignored.
void BrowserPersistentCookieStore::Backend::Close() {
  if (CefThread::CurrentlyOn(CefThread::FILE)) {
    InternalBackgroundClose();
  } else {
    // Must close the backend on the background thread.
    CefThread::PostTask(
        CefThread::FILE, FROM_HERE,
        base::Bind(&Backend::InternalBackgroundClose, this));
  }
}

void BrowserPersistentCookieStore::Backend::InternalBackgroundClose() {
  DCHECK(CefThread::CurrentlyOn(CefThread::FILE));
  // Commit any pending operations
  Commit();

  db_.reset();

  if (clear_local_state_on_exit_)
    file_util::Delete(path_, false);
}

void BrowserPersistentCookieStore::Backend::SetClearLocalStateOnExit(
    bool clear_local_state) {
  base::AutoLock locked(lock_);
  clear_local_state_on_exit_ = clear_local_state;
}

void BrowserPersistentCookieStore::Backend::DeleteSessionCookies() {
  DCHECK(CefThread::CurrentlyOn(CefThread::FILE));
  if (!db_->Execute("DELETE FROM cookies WHERE persistent == 0"))
    LOG(WARNING) << "Unable to delete session cookies.";
}

BrowserPersistentCookieStore::BrowserPersistentCookieStore(
    const FilePath& path,
    bool restore_old_session_cookies)
    : backend_(new Backend(path, restore_old_session_cookies)) {
}

BrowserPersistentCookieStore::~BrowserPersistentCookieStore() {
  if (backend_.get()) {
    backend_->Close();
    // Release our reference, it will probably still have a reference if the
    // background thread has not run Close() yet.
    backend_ = NULL;
  }
}

void BrowserPersistentCookieStore::Load(const LoadedCallback& loaded_callback) {
  backend_->Load(loaded_callback);
}

void BrowserPersistentCookieStore::LoadCookiesForKey(
    const std::string& key,
    const LoadedCallback& loaded_callback) {
  backend_->LoadCookiesForKey(key, loaded_callback);
}

void BrowserPersistentCookieStore::AddCookie(
    const net::CookieMonster::CanonicalCookie& cc) {
  if (backend_.get())
    backend_->AddCookie(cc);
}

void BrowserPersistentCookieStore::UpdateCookieAccessTime(
    const net::CookieMonster::CanonicalCookie& cc) {
  if (backend_.get())
    backend_->UpdateCookieAccessTime(cc);
}

void BrowserPersistentCookieStore::DeleteCookie(
    const net::CookieMonster::CanonicalCookie& cc) {
  if (backend_.get())
    backend_->DeleteCookie(cc);
}

void BrowserPersistentCookieStore::SetClearLocalStateOnExit(
    bool clear_local_state) {
  if (backend_.get())
    backend_->SetClearLocalStateOnExit(clear_local_state);
}

void BrowserPersistentCookieStore::Flush(const base::Closure& callback) {
  if (backend_.get())
    backend_->Flush(callback);
  else if (!callback.is_null())
    MessageLoop::current()->PostTask(FROM_HERE, callback);
}
