// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/common/crash_reporter_client.h"

#include <utility>

#include "libcef/common/cef_switches.h"
#include "include/cef_version.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/files/file_path.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/common/content_switches.h"

CefCrashReporterClient::CefCrashReporterClient() {}
CefCrashReporterClient::~CefCrashReporterClient() {}

#if defined(OS_WIN)
void CefCrashReporterClient::GetProductNameAndVersion(
    const base::string16& exe_path,
    base::string16* product_name,
    base::string16* version,
    base::string16* special_build,
    base::string16* channel_name) {
  *product_name = base::ASCIIToUTF16("cef");
  *version = base::ASCIIToUTF16(CEF_VERSION);
  *special_build = base::string16();
  *channel_name = base::string16();
}
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_IOS)
void CefCrashReporterClient::GetProductNameAndVersion(
    const char** product_name,
    const char** version) {
  *product_name = "cef";
  *version = CEF_VERSION;
}

base::FilePath CefCrashReporterClient::GetReporterLogFilename() {
  return base::FilePath(FILE_PATH_LITERAL("uploads.log"));
}
#endif

#if defined(OS_WIN)
bool CefCrashReporterClient::GetCrashDumpLocation(base::string16* crash_dir) {
#else
bool CefCrashReporterClient::GetCrashDumpLocation(base::FilePath* crash_dir) {
#endif
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kCrashDumpsDir))
    return false;

  base::FilePath crash_directory =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          switches::kCrashDumpsDir);
#if defined(OS_WIN)
  *crash_dir = crash_directory.value();
#else
  *crash_dir = std::move(crash_directory);
#endif
  return true;
}

bool CefCrashReporterClient::EnableBreakpadForProcess(
    const std::string& process_type) {
  return process_type == switches::kRendererProcess ||
         process_type == switches::kPpapiPluginProcess ||
         process_type == switches::kZygoteProcess ||
         process_type == switches::kGpuProcess;
}
