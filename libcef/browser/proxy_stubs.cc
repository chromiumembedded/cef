// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_service_syncable.h"

// Required by PrefProxyConfigTrackerImpl::RegisterUserPrefs.
void PrefServiceSyncable::RegisterDictionaryPref(const char* path,
                                                 DictionaryValue* default_value,
                                                 PrefSyncStatus sync_status) {
  NOTREACHED();
}
