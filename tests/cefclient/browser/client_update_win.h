// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_CLIENT_UPDATE_WIN_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_CLIENT_UPDATE_WIN_H_
#pragma once

#if defined(CEF_USE_BOOTSTRAP)

namespace client {

// Start a background update check after a delay.
// Should be called from OnContextInitialized.
// This demonstrates how client applications can use the RunInstaller export
// from bootstrap.exe for background updates.
void StartBackgroundUpdateCheck();

// Confirm launch health after a delay.
// Should be called from OnContextInitialized.
// This demonstrates how client applications can use the RunInstaller export
// from bootstrap.exe to confirm that the CEF version is working, reducing
// false-positive rollbacks from app-level crashes that occur after CEF has
// already loaded and rendered successfully.
void StartLaunchHealthConfirmation();

}  // namespace client

#endif  // defined(CEF_USE_BOOTSTRAP)

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_CLIENT_UPDATE_WIN_H_
