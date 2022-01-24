// Copyright (c) 2019 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if BUILDFLAG(IS_MAC)

#include "chrome/app/chrome_crash_reporter_client.h"

// Required due to https://crrev.com/1c9f89a06f
void ChromeCrashReporterClient::Create() {}

#endif  // BUILDFLAG(IS_MAC)
