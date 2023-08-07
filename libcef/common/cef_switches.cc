// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/common/cef_switches.h"

namespace switches {

// Severity of messages to log.
const char kLogSeverity[] = "log-severity";
const char kLogSeverity_Verbose[] = "verbose";
const char kLogSeverity_Info[] = "info";
const char kLogSeverity_Warning[] = "warning";
const char kLogSeverity_Error[] = "error";
const char kLogSeverity_Fatal[] = "fatal";
const char kLogSeverity_Disable[] = "disable";

// Customization of items automatically prepended to log lines.
const char kLogItems[] = "log-items";
const char kLogItems_None[] = "none";
const char kLogItems_PId[] = "pid";
const char kLogItems_TId[] = "tid";
const char kLogItems_TimeStamp[] = "timestamp";
const char kLogItems_TickCount[] = "tickcount";

// Path to resources directory.
const char kResourcesDirPath[] = "resources-dir-path";

// Path to locales directory.
const char kLocalesDirPath[] = "locales-dir-path";

// Path to locales directory.
const char kDisablePackLoading[] = "disable-pack-loading";

// Stack size for uncaught exceptions.
const char kUncaughtExceptionStackSize[] = "uncaught-exception-stack-size";

// Default encoding.
const char kDefaultEncoding[] = "default-encoding";

// Disable JavaScript.
const char kDisableJavascript[] = "disable-javascript";

// Disable closing of windows via JavaScript.
const char kDisableJavascriptCloseWindows[] =
    "disable-javascript-close-windows";

// Disable clipboard access via JavaScript.
const char kDisableJavascriptAccessClipboard[] =
    "disable-javascript-access-clipboard";

// Disable DOM paste via JavaScript execCommand("paste").
const char kDisableJavascriptDomPaste[] = "disable-javascript-dom-paste";

// Allow universal access from file URLs.
const char kAllowUniversalAccessFromFileUrls[] =
    "allow-universal-access-from-files";

// Disable loading of images from the network. A cached image will still be
// rendered if requested.
const char kDisableImageLoading[] = "disable-image-loading";

// Shrink stand-alone images to fit.
const char kImageShrinkStandaloneToFit[] = "image-shrink-standalone-to-fit";

// Disable resizing of text areas.
const char kDisableTextAreaResize[] = "disable-text-area-resize";

// Disable using the tab key to advance focus to links.
const char kDisableTabToLinks[] = "disable-tab-to-links";

// Persist session cookies.
const char kPersistSessionCookies[] = "persist-session-cookies";

// Persist user preferences.
const char kPersistUserPreferences[] = "persist-user-preferences";

// Enable media (WebRTC audio/video) streaming.
const char kEnableMediaStream[] = "enable-media-stream";

// Enable speech input (x-webkit-speech).
const char kEnableSpeechInput[] = "enable-speech-input";

// Enable the speech input profanity filter.
const char kEnableProfanityFilter[] = "enable-profanity-filter";

// Disable spell checking.
const char kDisableSpellChecking[] = "disable-spell-checking";

// Enable the remote spelling service.
const char kEnableSpellingService[] = "enable-spelling-service";

// Override the default spellchecking language which comes from locales.pak.
const char kOverrideSpellCheckLang[] = "override-spell-check-lang";

// Disable scroll bounce (rubber-banding) on OS X Lion and newer.
const char kDisableScrollBounce[] = "disable-scroll-bounce";

// Disable the PDF extension.
const char kDisablePdfExtension[] = "disable-pdf-extension";

// Enable print preview.
const char kEnablePrintPreview[] = "enable-print-preview";

// Disable the timeout for delivering new browser info to the renderer process.
const char kDisableNewBrowserInfoTimeout[] = "disable-new-browser-info-timeout";

// File used for logging DevTools protocol messages.
const char kDevToolsProtocolLogFile[] = "devtools-protocol-log-file";

// Enable use of the Chrome runtime in CEF. See issue #2969 for details.
const char kEnableChromeRuntime[] = "enable-chrome-runtime";

// Delegate all login requests to the client GetAuthCredentials callback when
// using the Chrome runtime.
const char kDisableChromeLoginPrompt[] = "disable-chrome-login-prompt";

// Override the product component of the default User-Agent string.
const char kUserAgentProductAndVersion[] = "user-agent-product";

// Disable request handling in CEF to faciliate debugging of network-related
// issues.
const char kDisableRequestHandlingForTesting[] =
    "disable-request-handling-for-testing";

#if BUILDFLAG(IS_MAC)
// Path to the framework directory.
const char kFrameworkDirPath[] = "framework-dir-path";
const char kMainBundlePath[] = "main-bundle-path";
#endif

}  // namespace switches
