// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

// Defines all of the command line switches used by cefclient.

#ifndef CEF_TESTS_SHARED_SHARED_COMMON_SWITCHES_H_
#define CEF_TESTS_SHARED_SHARED_COMMON_SWITCHES_H_
#pragma once

namespace client::switches {

extern const char kMultiThreadedMessageLoop[];
extern const char kExternalMessagePump[];
extern const char kCachePath[];
extern const char kUrl[];
extern const char kOffScreenRenderingEnabled[];
extern const char kOffScreenFrameRate[];
extern const char kTransparentPaintingEnabled[];
extern const char kShowUpdateRect[];
extern const char kSharedTextureEnabled[];
extern const char kExternalBeginFrameEnabled[];
extern const char kMouseCursorChangeDisabled[];
extern const char kOffline[];
extern const char kFilterChromeCommands[];
extern const char kRequestContextPerBrowser[];
extern const char kRequestContextSharedCache[];
extern const char kBackgroundColor[];
extern const char kEnableGPU[];
extern const char kFilterURL[];
extern const char kUseViews[];
extern const char kUseNative[];
extern const char kHideFrame[];
extern const char kHideControls[];
extern const char kHideOverlays[];
extern const char kAlwaysOnTop[];
extern const char kHideTopMenu[];
extern const char kSslClientCertificate[];
extern const char kCRLSetsPath[];
extern const char kNoActivate[];
extern const char kShowChromeToolbar[];
extern const char kInitialShowState[];
extern const char kUseDefaultPopup[];
extern const char kUseClientDialogs[];
extern const char kUseTestHttpServer[];
extern const char kShowWindowButtons[];
extern const char kUseWindowModalDialog[];
extern const char kUseBottomControls[];
extern const char kHidePipFrame[];
extern const char kHideChromeBubbles[];
extern const char kHideWindowOnClose[];
extern const char kAcceptsFirstMouse[];
extern const char kUseAlloyStyle[];
extern const char kUseChromeStyleWindow[];
extern const char kShowOverlayBrowser[];

}  // namespace client::switches

#endif  // CEF_TESTS_SHARED_SHARED_COMMON_SWITCHES_H_
