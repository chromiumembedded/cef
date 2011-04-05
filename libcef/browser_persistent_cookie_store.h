// Copyright (c) 2011 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A sqlite implementation of a cookie monster persistent store.

// This class is based on src/chrome/browser/net/sqlite_persistent_cookie_store.h
// with the following modifications for use in the cef:
// - BrowserThread has been replaced with CefThread
// - Performance diagnostic code has been removed (UMA_HISTOGRAM_ENUMERATION)

#ifndef _BROWSER_PERSISTENT_COOKIE_STORE_H
#define _BROWSER_PERSISTENT_COOKIE_STORE_H
#pragma once

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "net/base/cookie_monster.h"

class FilePath;

// Implements the PersistentCookieStore interface in terms of a SQLite database.
// For documentation about the actual member functions consult the documentation
// of the parent class |net::CookieMonster::PersistentCookieStore|.
class BrowserPersistentCookieStore
    : public net::CookieMonster::PersistentCookieStore {
 public:
  explicit BrowserPersistentCookieStore(const FilePath& path);
  virtual ~BrowserPersistentCookieStore();

  virtual bool Load(std::vector<net::CookieMonster::CanonicalCookie*>* cookies);

  virtual void AddCookie(const net::CookieMonster::CanonicalCookie& cc);
  virtual void UpdateCookieAccessTime(
      const net::CookieMonster::CanonicalCookie& cc);
  virtual void DeleteCookie(const net::CookieMonster::CanonicalCookie& cc);

  virtual void SetClearLocalStateOnExit(bool clear_local_state);

  virtual void Flush(Task* completion_task);

 private:
  class Backend;

  scoped_refptr<Backend> backend_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPersistentCookieStore);
};

#endif  // _BROWSER_PERSISTENT_COOKIE_STORE_H
