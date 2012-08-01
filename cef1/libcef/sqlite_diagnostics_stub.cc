// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "chrome/browser/diagnostics/sqlite_diagnostics.h"
#include "content/public/common/url_constants.h"

// Used by SQLitePersistentCookieStore
sql::ErrorDelegate* GetErrorHandlerForCookieDb() {
  return NULL;
}

namespace chrome {

// Used by ClearOnExitPolicy
const char kHttpScheme[] = "http";
const char kHttpsScheme[] = "https";

}  // namespace chrome

namespace content {

// Used by ClearOnExitPolicy
const char kStandardSchemeSeparator[] = "://";

}  // namespace content
