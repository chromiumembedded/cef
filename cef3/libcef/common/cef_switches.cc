// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/common/cef_switches.h"

namespace switches {

// Log file path.
const char kLogFile[]                     = "log-file";

// Severity of messages to log.
const char kLogSeverity[]                 = "log-severity";
const char kLogSeverity_Verbose[]         = "verbose";
const char kLogSeverity_Info[]            = "info";
const char kLogSeverity_Warning[]         = "warning";
const char kLogSeverity_Error[]           = "error";
const char kLogSeverity_ErrorReport[]     = "error-report";
const char kLogSeverity_Disable[]         = "disable";

// Enable DCHECK for release mode.
const char kEnableReleaseDcheck[]         = "enable-release-dcheck";

// Path to resources directory.
const char kResourcesDirPath[]            = "resources-dir-path";

// Path to locales directory.
const char kLocalesDirPath[]              = "locales-dir-path";

// Path to locales directory.
const char kDisablePackLoading[]          = "disable-pack-loading";

// Stack size for uncaught exceptions.
const char kUncaughtExceptionStackSize[]  = "uncaught-exception-stack-size";

// Context safety implementation type.
const char kContextSafetyImplementation[] = "context-safety-implementation";

// Default encoding.
const char kDefaultEncoding[]             = "default-encoding";

// User style sheet location.
const char kUserStyleSheetLocation[]      = "user-style-sheet-location";

// Disable opening of windows via JavaScript.
const char kDisableJavascriptOpenWindows[] =
    "disable-javascript-open-windows";

// Disable closing of windows via JavaScript.
const char kDisableJavascriptCloseWindows[] =
    "disable-javascript-close-windows";

// Disable clipboard access via JavaScript.
const char kDisableJavascriptAccessClipboard[] =
    "disable-javascript-access-clipboard";

// Disable DOM paste via JavaScript execCommand("paste").
const char kDisableJavascriptDomPaste[]   = "disable-javascript-dom-paste";

// Enable caret browsing.
const char kEnableCaretBrowsing[]         = "enable-caret-browsing";

// Allow universal access from file URLs.
const char kAllowUniversalAccessFromFileUrls[] =
    "allow-universal-access-from-files";

// Disable loading of images from the network. A cached image will still be
// rendered if requested.
const char kDisableImageLoading[]         = "disable-image-loading";

// Shrink stand-alone images to fit.
const char kImageShrinkStandaloneToFit[]  = "image-shrink-standalone-to-fit";

// Disable resizing of text areas.
const char kDisableTextAreaResize[]       = "disable-text-area-resize";

// Disable using the tab key to advance focus to links.
const char kDisableTabToLinks[]           = "disable-tab-to-links";

// Disable user style sheets.
const char kDisableAuthorAndUserStyles[]  = "disable-author-and-user-styles";

// Disable developer tools (WebKit Inspector).
const char kDisableDeveloperTools[]       = "disable-developer-tools";

// Persist session cookies.
const char kPersistSessionCookies[]       = "persist-session-cookies";

}  // namespace switches
