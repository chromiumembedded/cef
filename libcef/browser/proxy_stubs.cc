// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "components/user_prefs/pref_registry_syncable.h"

namespace user_prefs {

// Required by PrefProxyConfigTrackerImpl::RegisterUserPrefs.
void PrefRegistrySyncable::RegisterDictionaryPref(
    const char* path,
    base::DictionaryValue* default_value,
    PrefSyncStatus sync_status) {
  NOTREACHED();
}

}  // namespace user_prefs
