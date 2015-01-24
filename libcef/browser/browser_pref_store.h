// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_BROWSER_PREF_STORE_H_
#define CEF_LIBCEF_BROWSER_BROWSER_PREF_STORE_H_

#include "base/memory/scoped_ptr.h"
#include "base/prefs/testing_pref_store.h"

class PrefService;

class CefBrowserPrefStore : public TestingPrefStore {
 public:
  CefBrowserPrefStore();

  scoped_ptr<PrefService> CreateService();

 protected:
  ~CefBrowserPrefStore() override;

  DISALLOW_COPY_AND_ASSIGN(CefBrowserPrefStore);
};

#endif  // CEF_LIBCEF_BROWSER_BROWSER_PREF_STORE_H_
