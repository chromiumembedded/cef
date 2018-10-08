// Copyright (c) 2018 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/net/cookie_store_source.h"

#include "libcef/browser/content_browser_client.h"
#include "libcef/browser/cookie_manager_impl.h"
#include "libcef/browser/net/url_request_context_impl.h"
#include "libcef/browser/thread_util.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "net/extras/sqlite/sqlite_persistent_cookie_store.h"

CefCookieStoreHandlerSource::CefCookieStoreHandlerSource(
    CefURLRequestContextImpl* parent,
    CefRefPtr<CefRequestContextHandler> handler)
    : parent_(parent), handler_(handler) {
  DCHECK(parent_);
  DCHECK(handler_);
}

net::CookieStore* CefCookieStoreHandlerSource::GetCookieStore() {
  CEF_REQUIRE_IOT();

  CefRefPtr<CefCookieManager> manager = handler_->GetCookieManager();
  if (manager) {
    // Use the cookie store provided by the manager. May be nullptr if the
    // cookie manager is blocking.
    return reinterpret_cast<CefCookieManagerImpl*>(manager.get())
        ->GetExistingCookieStore();
  }

  DCHECK(parent_);
  if (parent_) {
    // Use the cookie store from the parent.
    net::CookieStore* cookie_store = parent_->cookie_store();
    DCHECK(cookie_store);
    if (!cookie_store)
      LOG(ERROR) << "Cookie store does not exist";
    return cookie_store;
  }

  return nullptr;
}

CefCookieStoreOwnerSource::CefCookieStoreOwnerSource() {}

void CefCookieStoreOwnerSource::SetCookieStoragePath(
    const base::FilePath& path,
    bool persist_session_cookies,
    net::NetLog* net_log) {
  CEF_REQUIRE_IOT();

  if (cookie_store_ && ((path_.empty() && path.empty()) || path_ == path)) {
    // The path has not changed so don't do anything.
    return;
  }

  scoped_refptr<net::SQLitePersistentCookieStore> persistent_store;
  if (!path.empty()) {
    // TODO(cef): Move directory creation to the blocking pool instead of
    // allowing file IO on this thread.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    if (base::DirectoryExists(path) || base::CreateDirectory(path)) {
      const base::FilePath& cookie_path = path.AppendASCII("Cookies");
      persistent_store = new net::SQLitePersistentCookieStore(
          cookie_path,
          base::CreateSingleThreadTaskRunnerWithTraits(
              {content::BrowserThread::IO}),
          // Intentionally using the background task runner exposed by CEF to
          // facilitate unit test expectations. This task runner MUST be
          // configured with BLOCK_SHUTDOWN.
          CefContentBrowserClient::Get()->background_task_runner(),
          persist_session_cookies, NULL);
    } else {
      NOTREACHED() << "The cookie storage directory could not be created";
    }
  }

  // Set the new cookie store that will be used for all new requests. The old
  // cookie store, if any, will be automatically flushed and closed when no
  // longer referenced.
  std::unique_ptr<net::CookieMonster> cookie_monster(
      new net::CookieMonster(persistent_store.get(), nullptr, net_log));
  if (persistent_store.get() && persist_session_cookies)
    cookie_monster->SetPersistSessionCookies(true);
  path_ = path;

  // Restore the previously supported schemes.
  CefCookieManagerImpl::SetCookieMonsterSchemes(cookie_monster.get(),
                                                supported_schemes_);

  cookie_store_ = std::move(cookie_monster);
}

void CefCookieStoreOwnerSource::SetCookieSupportedSchemes(
    const std::vector<std::string>& schemes) {
  CEF_REQUIRE_IOT();

  supported_schemes_ = schemes;
  CefCookieManagerImpl::SetCookieMonsterSchemes(
      static_cast<net::CookieMonster*>(cookie_store_.get()),
      supported_schemes_);
}

net::CookieStore* CefCookieStoreOwnerSource::GetCookieStore() {
  CEF_REQUIRE_IOT();
  return cookie_store_.get();
}
