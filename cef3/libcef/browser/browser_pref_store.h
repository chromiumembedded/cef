// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_BROWSER_PREF_STORE_H_
#define CEF_LIBCEF_BROWSER_BROWSER_PREF_STORE_H_

#include "base/prefs/testing_pref_store.h"

class PrefService;

class BrowserPrefStore : public TestingPrefStore {
 public:
  BrowserPrefStore();

  PrefService* CreateService();

 protected:
  virtual ~BrowserPrefStore();

  DISALLOW_COPY_AND_ASSIGN(BrowserPrefStore);
};

#endif  // CEF_LIBCEF_BROWSER_BROWSER_PREF_STORE_H_
