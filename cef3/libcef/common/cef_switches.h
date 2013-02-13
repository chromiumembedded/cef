// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

// Defines all the "cef" command-line switches.

#ifndef CEF_LIBCEF_COMMON_CEF_SWITCHES_H_
#define CEF_LIBCEF_COMMON_CEF_SWITCHES_H_
#pragma once

namespace switches {

extern const char kLogFile[];
extern const char kLogSeverity[];
extern const char kLogSeverity_Verbose[];
extern const char kLogSeverity_Info[];
extern const char kLogSeverity_Warning[];
extern const char kLogSeverity_Error[];
extern const char kLogSeverity_ErrorReport[];
extern const char kLogSeverity_Disable[];
extern const char kEnableReleaseDcheck[];
extern const char kResourcesDirPath[];
extern const char kLocalesDirPath[];
extern const char kDisablePackLoading[];
extern const char kUncaughtExceptionStackSize[];
extern const char kContextSafetyImplementation[];
extern const char kDefaultEncoding[];
extern const char kUserStyleSheetLocation[];
extern const char kDisableJavascriptOpenWindows[];
extern const char kDisableJavascriptCloseWindows[];
extern const char kDisableJavascriptAccessClipboard[];
extern const char kDisableJavascriptDomPaste[];
extern const char kEnableCaretBrowsing[];
extern const char kAllowUniversalAccessFromFileUrls[];
extern const char kDisableImageLoading[];
extern const char kImageShrinkStandaloneToFit[];
extern const char kDisableTextAreaResize[];
extern const char kDisableTabToLinks[];
extern const char kDisableAuthorAndUserStyles[];
extern const char kDisableDeveloperTools[];
extern const char kPersistSessionCookies[];

}  // namespace switches

#endif  // CEF_LIBCEF_COMMON_CEF_SWITCHES_H_
