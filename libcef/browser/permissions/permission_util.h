// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_PERMISSIONS_PERMISSION_UTIL_H_
#define CEF_LIBCEF_BROWSER_PERMISSIONS_PERMISSION_UTIL_H_

#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/permission_type.h"

namespace permission_util {

// Helper method to convert PermissionType to ContentSettingType.
ContentSettingsType PermissionTypeToContentSetting(
    content::PermissionType permission);

}  // namespace permission_util

#endif // CEF_LIBCEF_BROWSER_PERMISSIONS_PERMISSION_UTIL_H_
