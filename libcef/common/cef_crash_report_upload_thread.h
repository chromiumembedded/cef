// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_CEF_CRASH_REPORT_UPLOAD_THREAD_H_
#define CEF_LIBCEF_COMMON_CEF_CRASH_REPORT_UPLOAD_THREAD_H_

#include "third_party/crashpad/crashpad/handler/crash_report_upload_thread.h"

class CefCrashReportUploadThread : public crashpad::CrashReportUploadThread {
 public:
  CefCrashReportUploadThread(crashpad::CrashReportDatabase* database,
                             const std::string& url,
                             bool rate_limit,
                             bool upload_gzip,
                             int max_uploads);
  ~CefCrashReportUploadThread();

 private:
  void ProcessPendingReports() override;
  void ProcessPendingReport(
    const crashpad::CrashReportDatabase::Report& report) override;

  bool UploadsEnabled() const;

  bool MaxUploadsEnabled() const;
  bool MaxUploadsExceeded() const;

  bool BackoffPending() const;
  void IncreaseBackoff();
  void ResetBackoff();

  int max_uploads_;

  // Track the number of uploads that have completed within the last 24 hours.
  // Only used when RateLimitEnabled() is true. Value is reset each time
  // ProcessPendingReports() is called.
  int recent_upload_ct_ = 0;

  DISALLOW_COPY_AND_ASSIGN(CefCrashReportUploadThread);
};

#endif  // CEF_LIBCEF_COMMON_CEF_CRASH_REPORT_UPLOAD_THREAD_H_
