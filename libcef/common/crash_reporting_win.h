// Copyright 2016 The Chromium Embedded Framework Authors. Portions copyright
// 2016 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

namespace crash_reporting_win {

// Called from libcef to initialize crash key globals. Retrieves the necessary
// state from chrome_elf via exported functions.
bool InitializeCrashReportingForModule();

// Called from libcef early in main process startup to ensure that the crash
// handler process is started before other sub-processes are created.
void BlockUntilHandlerStarted();

}  // namespace crash_reporting_win
