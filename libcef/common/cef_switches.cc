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
const char kLogSeverity_Disable[]         = "disable";

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

// Persist session cookies.
const char kPersistSessionCookies[]       = "persist-session-cookies";

// Persist user preferences.
const char kPersistUserPreferences[]      = "persist-user-preferences";

// Enable media (WebRTC audio/video) streaming.
const char kEnableMediaStream[]           = "enable-media-stream";

// Enable speech input (x-webkit-speech).
const char kEnableSpeechInput[]           = "enable-speech-input";

// Enable the speech input profanity filter.
const char kEnableProfanityFilter[]       = "enable-profanity-filter";

// Disable spell checking.
const char kDisableSpellChecking[]        = "disable-spell-checking";

// Enable the remote spelling service.
const char kEnableSpellingService[]       = "enable-spelling-service";

// Override the default spellchecking language which comes from locales.pak.
const char kOverrideSpellCheckLang[]      = "override-spell-check-lang";

// Enable detection and use of a system-wide Pepper Flash install.
const char kEnableSystemFlash[]           = "enable-system-flash";

// Disable scroll bounce (rubber-banding) on OS X Lion and newer.
const char kDisableScrollBounce[]         = "disable-scroll-bounce";

// Disable the PDF extension.
const char kDisablePdfExtension[]         = "disable-pdf-extension";

// Path to Widevine CDM binaries.
const char kWidevineCdmPath[]             = "widevine-cdm-path";

// Default plugin policy action.
const char kPluginPolicy[]                = "plugin-policy";
// Allow the content. This is the default value.
const char kPluginPolicy_Allow[]          = "allow";
// Allow important content and block unimportant content based on heuristics.
// The user can manually load blocked content.
const char kPluginPolicy_Detect[]         = "detect";
// Block the content. The user can manually load blocked content.
const char kPluginPolicy_Block[]          = "block";

// Expose preferences used only by unit tests.
const char kEnablePreferenceTesting[]     = "enable-preference-testing";

// Enable date-based expiration of built in network security information.
const char kEnableNetSecurityExpiration[] = "enable-net-security-expiration";

#if defined(OS_MACOSX)
// Path to the framework directory.
const char kFrameworkDirPath[]            = "framework-dir-path";
#endif

}  // namespace switches
