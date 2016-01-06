// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_CRASH_REPORTER_CLIENT_H_
#define CEF_LIBCEF_COMMON_CRASH_REPORTER_CLIENT_H_

#include "base/macros.h"
#include "build/build_config.h"
#include "components/crash/content/app/crash_reporter_client.h"

class CefCrashReporterClient : public crash_reporter::CrashReporterClient {
 public:
  CefCrashReporterClient();
  ~CefCrashReporterClient() override;

#if defined(OS_WIN)
  // Returns a textual description of the product type and version to include
  // in the crash report.
  void GetProductNameAndVersion(const base::FilePath& exe_path,
                                base::string16* product_name,
                                base::string16* version,
                                base::string16* special_build,
                                base::string16* channel_name) override;
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_IOS)
  // Returns a textual description of the product type and version to include
  // in the crash report.
  void GetProductNameAndVersion(const char** product_name,
                                const char** version) override;

  base::FilePath GetReporterLogFilename() override;
#endif

  // The location where minidump files should be written. Returns true if
  // |crash_dir| was set.
  bool GetCrashDumpLocation(base::FilePath* crash_dir) override;

private:
  DISALLOW_COPY_AND_ASSIGN(CefCrashReporterClient);
};

#endif  // CEF_LIBCEF_COMMON_CRASH_REPORTER_CLIENT_H_
