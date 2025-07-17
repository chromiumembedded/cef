// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_PREFS_PREF_NAMES_H_
#define CEF_LIBCEF_BROWSER_PREFS_PREF_NAMES_H_

namespace cef::prefs {

// If false, return nullptr from ProfileImpl::GetStorageNotificationService.
// QuotaManager and the rest of the storage layer will have no connection to the
// Chrome layer for UI purposes.
inline constexpr char kEnableStorageNotificationService[] =
    "cef.storage_notification_service_enabled";

}  // namespace cef::prefs

#endif  // CEF_LIBCEF_BROWSER_PREFS_PREF_NAMES_H_
