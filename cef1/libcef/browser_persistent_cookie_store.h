// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A sqlite implementation of a cookie monster persistent store.

// This class is based on src/chrome/browser/net/
//   sqlite_persistent_cookie_store.h
// with the following modifications for use in the cef:
// - BrowserThread has been replaced with CefThread
// - Performance diagnostic code has been removed (UMA_HISTOGRAM_ENUMERATION)

#ifndef CEF_LIBCEF_BROWSER_PERSISTENT_COOKIE_STORE_H_
#define CEF_LIBCEF_BROWSER_PERSISTENT_COOKIE_STORE_H_
#pragma once

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "net/cookies/cookie_monster.h"

class FilePath;
class Task;

// Implements the PersistentCookieStore interface in terms of a SQLite database.
// For documentation about the actual member functions consult the documentation
// of the parent class |net::CookieMonster::PersistentCookieStore|.
class BrowserPersistentCookieStore
    : public net::CookieMonster::PersistentCookieStore {
 public:
  BrowserPersistentCookieStore(const FilePath& path,
                              bool restore_old_session_cookies);
  virtual ~BrowserPersistentCookieStore();

  virtual void Load(const LoadedCallback& loaded_callback) OVERRIDE;

  virtual void LoadCookiesForKey(const std::string& key,
      const LoadedCallback& callback) OVERRIDE;

  virtual void AddCookie(
      const net::CookieMonster::CanonicalCookie& cc) OVERRIDE;

  virtual void UpdateCookieAccessTime(
      const net::CookieMonster::CanonicalCookie& cc) OVERRIDE;

  virtual void DeleteCookie(
      const net::CookieMonster::CanonicalCookie& cc) OVERRIDE;

  virtual void SetClearLocalStateOnExit(bool clear_local_state) OVERRIDE;

  virtual void Flush(const base::Closure& callback) OVERRIDE;

 private:
  class Backend;

  scoped_refptr<Backend> backend_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPersistentCookieStore);
};

#endif  // CEF_LIBCEF_BROWSER_PERSISTENT_COOKIE_STORE_H_
