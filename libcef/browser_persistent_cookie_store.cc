// Copyright (c) 2011 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "browser_persistent_cookie_store.h"

#include <list>

#include "cef_thread.h"
#include "app/sql/meta_table.h"
#include "app/sql/statement.h"
#include "app/sql/transaction.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "googleurl/src/gurl.h"

using base::Time;

// This class is designed to be shared between any calling threads and the
// database thread.  It batches operations and commits them on a timer.
class BrowserPersistentCookieStore::Backend
    : public base::RefCountedThreadSafe<BrowserPersistentCookieStore::Backend> {
 public:
  explicit Backend(const FilePath& path)
      : path_(path),
        db_(NULL),
        num_pending_(0),
        clear_local_state_on_exit_(false) {
  }

  // Creates or load the SQLite database.
  bool Load(std::vector<net::CookieMonster::CanonicalCookie*>* cookies);

  // Batch a cookie addition.
  void AddCookie(const net::CookieMonster::CanonicalCookie& cc);

  // Batch a cookie access time update.
  void UpdateCookieAccessTime(const net::CookieMonster::CanonicalCookie& cc);

  // Batch a cookie deletion.
  void DeleteCookie(const net::CookieMonster::CanonicalCookie& cc);

  // Commit pending operations as soon as possible.
  void Flush(Task* completion_task);

  // Commit any pending operations and close the database.  This must be called
  // before the object is destructed.
  void Close();

  void SetClearLocalStateOnExit(bool clear_local_state);

 private:
  friend class base::RefCountedThreadSafe<BrowserPersistentCookieStore::Backend>;

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
  // Batch a cookie operation (add or delete)
  void BatchOperation(PendingOperation::OperationType op,
                      const net::CookieMonster::CanonicalCookie& cc);
  // Commit our pending operations to the database.
  void Commit();
  // Close() executed on the background thread.
  void InternalBackgroundClose();

  FilePath path_;
  scoped_ptr<sql::Connection> db_;
  sql::MetaTable meta_table_;

  typedef std::list<PendingOperation*> PendingOperationsList;
  PendingOperationsList pending_;
  PendingOperationsList::size_type num_pending_;
  // True if the persistent store should be deleted upon destruction.
  bool clear_local_state_on_exit_;
  // Guard |pending_|, |num_pending_| and |clear_local_state_on_exit_|.
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(Backend);
};

// Version number of the database. In version 4, we migrated the time epoch.
// If you open the DB with an older version on Mac or Linux, the times will
// look wonky, but the file will likely be usable. On Windows version 3 and 4
// are the same.
//
// Version 3 updated the database to include the last access time, so we can
// expire them in decreasing order of use when we've reached the maximum
// number of cookies.
static const int kCurrentVersionNumber = 4;
static const int kCompatibleVersionNumber = 3;

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
                     // We only store persistent, so we know it expires
                     "expires_utc INTEGER NOT NULL,"
                     "secure INTEGER NOT NULL,"
                     "httponly INTEGER NOT NULL,"
                     "last_access_utc INTEGER NOT NULL)"))
      return false;
  }

  // Try to create the index every time. Older versions did not have this index,
  // so we want those people to get it. Ignore errors, since it may exist.
  db->Execute("CREATE INDEX cookie_times ON cookies (creation_utc)");
  return true;
}

}  // namespace

bool BrowserPersistentCookieStore::Backend::Load(
    std::vector<net::CookieMonster::CanonicalCookie*>* cookies) {
  // This function should be called only once per instance.
  DCHECK(!db_.get());

  db_.reset(new sql::Connection);
  if (!db_->Open(path_)) {
    NOTREACHED() << "Unable to open cookie DB.";
    db_.reset();
    return false;
  }

  //db_->set_error_delegate(GetErrorHandlerForCookieDb());

  if (!EnsureDatabaseVersion() || !InitTable(db_.get())) {
    NOTREACHED() << "Unable to open cookie DB.";
    db_.reset();
    return false;
  }

  db_->Preload();

  // Slurp all the cookies into the out-vector.
  sql::Statement smt(db_->GetUniqueStatement(
      "SELECT creation_utc, host_key, name, value, path, expires_utc, secure, "
      "httponly, last_access_utc FROM cookies"));
  if (!smt) {
    NOTREACHED() << "select statement prep failed";
    db_.reset();
    return false;
  }

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
            true));                                         // has_
    DLOG_IF(WARNING,
            cc->CreationDate() > Time::Now()) << L"CreationDate too recent";
    cookies->push_back(cc.release());
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
    db_->Execute(
        "UPDATE cookies "
        "SET creation_utc = creation_utc + 11644473600000000 "
        "WHERE rowid IN "
        "(SELECT rowid FROM cookies WHERE "
          "creation_utc > 0 AND creation_utc < 11644473600000000)");
    db_->Execute(
        "UPDATE cookies "
        "SET expires_utc = expires_utc + 11644473600000000 "
        "WHERE rowid IN "
        "(SELECT rowid FROM cookies WHERE "
          "expires_utc > 0 AND expires_utc < 11644473600000000)");
    db_->Execute(
        "UPDATE cookies "
        "SET last_access_utc = last_access_utc + 11644473600000000 "
        "WHERE rowid IN "
        "(SELECT rowid FROM cookies WHERE "
          "last_access_utc > 0 AND last_access_utc < 11644473600000000)");
#endif
    ++cur_version;
    meta_table_.SetVersionNumber(cur_version);
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
      NewRunnableMethod(this, &Backend::Commit), kCommitIntervalMs);
  } else if (num_pending == kCommitAfterBatchSize) {
    // We've reached a big enough batch, fire off a commit now.
    CefThread::PostTask(
      CefThread::FILE, FROM_HERE,
      NewRunnableMethod(this, &Backend::Commit));
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
      "expires_utc, secure, httponly, last_access_utc) "
      "VALUES (?,?,?,?,?,?,?,?,?)"));
  if (!add_smt) {
    NOTREACHED();
    return;
  }

  sql::Statement update_access_smt(db_->GetCachedStatement(SQL_FROM_HERE,
      "UPDATE cookies SET last_access_utc=? WHERE creation_utc=?"));
  if (!update_access_smt) {
    NOTREACHED();
    return;
  }

  sql::Statement del_smt(db_->GetCachedStatement(SQL_FROM_HERE,
                         "DELETE FROM cookies WHERE creation_utc=?"));
  if (!del_smt) {
    NOTREACHED();
    return;
  }

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin()) {
    NOTREACHED();
    return;
  }
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

void BrowserPersistentCookieStore::Backend::Flush(Task* completion_task) {
  DCHECK(!CefThread::CurrentlyOn(CefThread::FILE));
  CefThread::PostTask(
      CefThread::FILE, FROM_HERE, NewRunnableMethod(this, &Backend::Commit));
  if (completion_task) {
    // We want the completion task to run immediately after Commit() returns.
    // Posting it from here means there is less chance of another task getting
    // onto the message queue first, than if we posted it from Commit() itself.
    CefThread::PostTask(CefThread::FILE, FROM_HERE, completion_task);
  }
}

// Fire off a close message to the background thread.  We could still have a
// pending commit timer that will be holding a reference on us, but if/when
// this fires we will already have been cleaned up and it will be ignored.
void BrowserPersistentCookieStore::Backend::Close() {
  DCHECK(!CefThread::CurrentlyOn(CefThread::FILE));
  // Must close the backend on the background thread.
  CefThread::PostTask(
      CefThread::FILE, FROM_HERE,
      NewRunnableMethod(this, &Backend::InternalBackgroundClose));
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
BrowserPersistentCookieStore::BrowserPersistentCookieStore(const FilePath& path)
    : backend_(new Backend(path)) {
}

BrowserPersistentCookieStore::~BrowserPersistentCookieStore() {
  if (backend_.get()) {
    backend_->Close();
    // Release our reference, it will probably still have a reference if the
    // background thread has not run Close() yet.
    backend_ = NULL;
  }
}

bool BrowserPersistentCookieStore::Load(
    std::vector<net::CookieMonster::CanonicalCookie*>* cookies) {
  return backend_->Load(cookies);
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

void BrowserPersistentCookieStore::Flush(Task* completion_task) {
  if (backend_.get())
    backend_->Flush(completion_task);
  else if (completion_task)
    MessageLoop::current()->PostTask(FROM_HERE, completion_task);
}
