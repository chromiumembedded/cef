// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/common/crash_reporter_client.h"
#include "libcef/common/cef_switches.h"
#include "include/cef_version.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/files/file_path.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"

CefCrashReporterClient::CefCrashReporterClient() {}
CefCrashReporterClient::~CefCrashReporterClient() {}

#if defined(OS_WIN)
void CefCrashReporterClient::GetProductNameAndVersion(
    const base::FilePath& exe_path,
    base::string16* product_name,
    base::string16* version,
    base::string16* special_build,
    base::string16* channel_name) {
  *product_name = base::ASCIIToUTF16("cef");
  *version = base::UTF8ToUTF16(base::StringPrintf(
        "%d.%d.%d", CEF_VERSION_MAJOR, CHROME_VERSION_BUILD, CEF_REVISION));
  *special_build = base::string16();
  *channel_name = base::string16();
}
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_IOS)
void CefCrashReporterClient::GetProductNameAndVersion(std::string* product_name,
                                                 std::string* version) {
  *product_name = "cef";
  *version = base::StringPrintf(
        "%d.%d.%d", CEF_VERSION_MAJOR, CHROME_VERSION_BUILD, CEF_REVISION);
}

base::FilePath CefCrashReporterClient::GetReporterLogFilename() {
  return base::FilePath(FILE_PATH_LITERAL("uploads.log"));
}
#endif

bool CefCrashReporterClient::GetCrashDumpLocation(base::FilePath* crash_dir) {
#if !defined(OS_WIN)
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kCrashDumpsDir))
    return false;
  *crash_dir = CommandLine::ForCurrentProcess()->GetSwitchValuePath(
      switches::kCrashDumpsDir);
  return true;
#else
  NOTREACHED();
  return false;
#endif
}
