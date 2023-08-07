// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

// Defines all the "cef" command-line switches.

#ifndef CEF_LIBCEF_COMMON_CEF_SWITCHES_H_
#define CEF_LIBCEF_COMMON_CEF_SWITCHES_H_
#pragma once

#include "build/build_config.h"

namespace switches {

extern const char kLogSeverity[];
extern const char kLogSeverity_Verbose[];
extern const char kLogSeverity_Info[];
extern const char kLogSeverity_Warning[];
extern const char kLogSeverity_Error[];
extern const char kLogSeverity_Fatal[];
extern const char kLogSeverity_Disable[];
extern const char kLogItems[];
extern const char kLogItems_None[];
extern const char kLogItems_PId[];
extern const char kLogItems_TId[];
extern const char kLogItems_TimeStamp[];
extern const char kLogItems_TickCount[];
extern const char kResourcesDirPath[];
extern const char kLocalesDirPath[];
extern const char kDisablePackLoading[];
extern const char kUncaughtExceptionStackSize[];
extern const char kDefaultEncoding[];
extern const char kDisableJavascript[];
extern const char kDisableJavascriptCloseWindows[];
extern const char kDisableJavascriptAccessClipboard[];
extern const char kDisableJavascriptDomPaste[];
extern const char kAllowUniversalAccessFromFileUrls[];
extern const char kDisableImageLoading[];
extern const char kImageShrinkStandaloneToFit[];
extern const char kDisableTextAreaResize[];
extern const char kDisableTabToLinks[];
extern const char kPersistSessionCookies[];
extern const char kPersistUserPreferences[];
extern const char kEnableMediaStream[];
extern const char kEnableSpeechInput[];
extern const char kEnableProfanityFilter[];
extern const char kDisableSpellChecking[];
extern const char kEnableSpellingService[];
extern const char kOverrideSpellCheckLang[];
extern const char kDisableScrollBounce[];
extern const char kDisablePdfExtension[];
extern const char kEnablePrintPreview[];
extern const char kDisableNewBrowserInfoTimeout[];
extern const char kDevToolsProtocolLogFile[];
extern const char kEnableChromeRuntime[];
extern const char kDisableChromeLoginPrompt[];
extern const char kUserAgentProductAndVersion[];
extern const char kDisableRequestHandlingForTesting[];

#if BUILDFLAG(IS_MAC)
extern const char kFrameworkDirPath[];
extern const char kMainBundlePath[];
#endif

}  // namespace switches

#endif  // CEF_LIBCEF_COMMON_CEF_SWITCHES_H_
