// Copyright (c) 2012 the Chromium Embedded Framework authors.
// Portions copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser_webcookiejar_impl.h"

#include <string>

#include "libcef/browser_resource_loader_bridge.h"
#include "libcef/browser_impl.h"
#include "libcef/cookie_manager_impl.h"
#include "libcef/cef_context.h"
#include "libcef/cef_thread.h"

#include "base/synchronization/waitable_event.h"
#include "net/base/cookie_store.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURL.h"

using WebKit::WebString;
using WebKit::WebURL;

namespace {

net::CookieStore* GetCookieStore(CefRefPtr<CefBrowserImpl> browser) {
  scoped_refptr<net::CookieStore> cookie_store;
  if (browser) {
     CefRefPtr<CefClient> client = browser->GetClient();
     if (client.get()) {
       CefRefPtr<CefRequestHandler> handler = client->GetRequestHandler();
       if (handler.get()) {
         // Get the manager from the handler.
         CefRefPtr<CefCookieManager> manager =
            handler->GetCookieManager(browser.get(),
                                      browser->pending_url().spec());
         if (manager.get()) {
           cookie_store =
              reinterpret_cast<CefCookieManagerImpl*>(
                  manager.get())->cookie_monster();
         }
       }
     }
  }

  if (!cookie_store) {
    // Use the global cookie store.
    cookie_store = _Context->request_context()->cookie_store();
  }

  DCHECK(cookie_store);
  return cookie_store;
}

class CookieSetter : public base::RefCountedThreadSafe<CookieSetter> {
 public:
  void Set(CefRefPtr<CefBrowserImpl> browser,
           const GURL& url,
           const std::string& cookie) {
    REQUIRE_IOT();

    scoped_refptr<net::CookieStore> cookie_store = GetCookieStore(browser);
    cookie_store->SetCookieWithOptionsAsync(
        url, cookie, net::CookieOptions(),
        net::CookieStore::SetCookiesCallback());
  }

 private:
  friend class base::RefCountedThreadSafe<CookieSetter>;

  ~CookieSetter() {}
};

class CookieGetter : public base::RefCountedThreadSafe<CookieGetter> {
 public:
  CookieGetter() : event_(false, false) {
  }

  void Get(CefRefPtr<CefBrowserImpl> browser, const GURL& url) {
    REQUIRE_IOT();

    scoped_refptr<net::CookieStore> cookie_store = GetCookieStore(browser);
    cookie_store->GetCookiesWithOptionsAsync(
        url, net::CookieOptions(),
        base::Bind(&CookieGetter::OnGetCookies, this));
  }

  std::string GetResult() {
    event_.Wait();
    return result_;
  }

 private:
  void OnGetCookies(const std::string& cookie_line) {
    result_ = cookie_line;
    event_.Signal();
  }
  friend class base::RefCountedThreadSafe<CookieGetter>;

  ~CookieGetter() {}

  base::WaitableEvent event_;
  std::string result_;
};

}  // namespace

BrowserWebCookieJarImpl::BrowserWebCookieJarImpl()
  : browser_(NULL) {
}

BrowserWebCookieJarImpl::BrowserWebCookieJarImpl(CefBrowserImpl* browser)
  : browser_(browser) {
}

void BrowserWebCookieJarImpl::setCookie(const WebURL& url,
                                        const WebURL& first_party_for_cookies,
                                        const WebString& value) {
  GURL gurl = url;
  std::string cookie = value.utf8();

  // Proxy to IO thread to synchronize w/ network loading.
  scoped_refptr<CookieSetter> cookie_setter = new CookieSetter();
  CefThread::PostTask(CefThread::IO, FROM_HERE, base::Bind(
      &CookieSetter::Set, cookie_setter.get(), browser_, gurl, cookie));
}

WebString BrowserWebCookieJarImpl::cookies(
    const WebURL& url,
    const WebURL& first_party_for_cookies) {
  GURL gurl = url;

  // Proxy to IO thread to synchronize w/ network loading.
  scoped_refptr<CookieGetter> cookie_getter = new CookieGetter();
  CefThread::PostTask(CefThread::IO, FROM_HERE, base::Bind(
      &CookieGetter::Get, cookie_getter.get(), browser_, gurl));

  // Blocks until the result is available.
  return WebString::fromUTF8(cookie_getter->GetResult());
}
