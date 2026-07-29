// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_CLIENT_UPDATE_WIN_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_CLIENT_UPDATE_WIN_H_
#pragma once

#if defined(CEF_USE_BOOTSTRAP)

namespace client {

// Confirm launch health after a delay and then start a background update check
// only if confirmation succeeds. Call from OnContextInitialized.
//
// Demonstrates how client applications can use the RunInstaller export from
// bootstrap.exe to confirm that CEF is working before updating. Confirmation
// preserves the current version as a fallback under the installer's existing
// launch-health pruning policy.
void StartLaunchHealthConfirmation();

}  // namespace client

#endif  // defined(CEF_USE_BOOTSTRAP)

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_CLIENT_UPDATE_WIN_H_
