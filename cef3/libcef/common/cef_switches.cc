// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/common/cef_switches.h"

namespace switches {

// Product version string.
const char kProductVersion[]          = "product-version";

// Log file path.
const char kLogFile[]                 = "log-file";

// Severity of messages to log.
const char kLogSeverity[]             = "log-severity";
const char kLogSeverity_Verbose[]     = "verbose";
const char kLogSeverity_Info[]        = "info";
const char kLogSeverity_Warning[]     = "warning";
const char kLogSeverity_Error[]       = "error";
const char kLogSeverity_ErrorReport[] = "error-report";
const char kLogSeverity_Disable[]     = "disable";

// Enable DCHECK for release mode.
const char kReleaseDcheckEnabled[]    = "release-dcheck-enabled";

// Path to resources directory.
const char kResourcesDirPath[]        = "resources-dir-path";

// Path to locales directory.
const char kLocalesDirPath[]          = "locales-dir-path";

// Path to locales directory.
const char kPackLoadingDisabled[]     = "pack-loading-disabled";

// Stack size for uncaught exceptions.
const char kUncaughtExceptionStackSize[]  = "uncaught-exception-stack-size";

// Context safety implementation type.
const char kContextSafetyImplementation[] = "context-safety-implementation";

}  // namespace switches
