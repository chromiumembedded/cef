// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/internal/cef_time_wrappers.h"
#include "include/internal/cef_types_wrappers.h"
#include "libcef/common/time_util.h"

#include <limits>
#include <tuple>

#if BUILDFLAG(IS_WIN)
namespace {

// From MSDN, FILETIME "Contains a 64-bit value representing the number of
// 100-nanosecond intervals since January 1, 1601 (UTC)." This value must
// be less than 0x8000000000000000. Otherwise, the function
// FileTimeToSystemTime fails.
// From base/time/time_win.cc:
bool CanConvertToFileTime(int64_t us) {
  return us >= 0 && us <= (std::numeric_limits<int64_t>::max() / 10);
}

}  // namespace
#endif  // BUILDFLAG(IS_WIN)

void cef_time_to_basetime(const cef_time_t& cef_time, base::Time& time) {
  base::Time::Exploded exploded;
  exploded.year = cef_time.year;
  exploded.month = cef_time.month;
  exploded.day_of_week = cef_time.day_of_week;
  exploded.day_of_month = cef_time.day_of_month;
  exploded.hour = cef_time.hour;
  exploded.minute = cef_time.minute;
  exploded.second = cef_time.second;
  exploded.millisecond = cef_time.millisecond;
  std::ignore = base::Time::FromUTCExploded(exploded, &time);
}

void cef_time_from_basetime(const base::Time& time, cef_time_t& cef_time) {
#if BUILDFLAG(IS_WIN)
  int64_t t = time.ToDeltaSinceWindowsEpoch().InMicroseconds();
  if (!CanConvertToFileTime(t)) {
    return;
  }
#endif

  base::Time::Exploded exploded;
  time.UTCExplode(&exploded);
  cef_time.year = exploded.year;
  cef_time.month = exploded.month;
  cef_time.day_of_week = exploded.day_of_week;
  cef_time.day_of_month = exploded.day_of_month;
  cef_time.hour = exploded.hour;
  cef_time.minute = exploded.minute;
  cef_time.second = exploded.second;
  cef_time.millisecond = exploded.millisecond;
}

CEF_EXPORT int cef_time_to_timet(const cef_time_t* cef_time, time_t* time) {
  if (!cef_time || !time) {
    return 0;
  }

  base::Time base_time;
  cef_time_to_basetime(*cef_time, base_time);
  *time = base_time.ToTimeT();
  return 1;
}

CEF_EXPORT int cef_time_from_timet(time_t time, cef_time_t* cef_time) {
  if (!cef_time) {
    return 0;
  }

  base::Time base_time = base::Time::FromTimeT(time);
  cef_time_from_basetime(base_time, *cef_time);
  return 1;
}

CEF_EXPORT int cef_time_to_doublet(const cef_time_t* cef_time, double* time) {
  if (!cef_time || !time) {
    return 0;
  }

  base::Time base_time;
  cef_time_to_basetime(*cef_time, base_time);
  *time = base_time.InSecondsFSinceUnixEpoch();
  return 1;
}

CEF_EXPORT int cef_time_from_doublet(double time, cef_time_t* cef_time) {
  if (!cef_time) {
    return 0;
  }

  base::Time base_time = base::Time::FromSecondsSinceUnixEpoch(time);
  cef_time_from_basetime(base_time, *cef_time);
  return 1;
}

CEF_EXPORT int cef_time_now(cef_time_t* cef_time) {
  if (!cef_time) {
    return 0;
  }

  base::Time base_time = base::Time::Now();
  cef_time_from_basetime(base_time, *cef_time);
  return 1;
}

CEF_EXPORT cef_basetime_t cef_basetime_now() {
  return CefBaseTime(base::Time::Now());
}

CEF_EXPORT int cef_time_delta(const cef_time_t* cef_time1,
                              const cef_time_t* cef_time2,
                              long long* delta) {
  if (!cef_time1 || !cef_time2 || !delta) {
    return 0;
  }

  base::Time base_time1, base_time2;
  cef_time_to_basetime(*cef_time1, base_time1);
  cef_time_to_basetime(*cef_time2, base_time2);

  base::TimeDelta time_delta = base_time2 - base_time1;
  *delta = time_delta.InMilliseconds();
  return 1;
}

CEF_EXPORT int cef_time_to_basetime(const cef_time_t* from,
                                    cef_basetime_t* to) {
  if (!from || !to) {
    return 0;
  }

  base::Time::Exploded exploded;
  exploded.year = from->year;
  exploded.month = from->month;
  exploded.day_of_week = from->day_of_week;
  exploded.day_of_month = from->day_of_month;
  exploded.hour = from->hour;
  exploded.minute = from->minute;
  exploded.second = from->second;
  exploded.millisecond = from->millisecond;
  base::Time time;
  bool result = base::Time::FromUTCExploded(exploded, &time);
  *to = CefBaseTime(time);
  return result ? 1 : 0;
}

CEF_EXPORT int cef_time_from_basetime(const cef_basetime_t from,
                                      cef_time_t* to) {
  if (!to) {
    return 0;
  }

  base::Time time = CefBaseTime(from);

  base::Time::Exploded exploded;
  time.UTCExplode(&exploded);
  to->year = exploded.year;
  to->month = exploded.month;
  to->day_of_week = exploded.day_of_week;
  to->day_of_month = exploded.day_of_month;
  to->hour = exploded.hour;
  to->minute = exploded.minute;
  to->second = exploded.second;
  to->millisecond = exploded.millisecond;
  return exploded.HasValidValues() ? 1 : 0;
}
