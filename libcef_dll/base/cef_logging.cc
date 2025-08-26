// Copyright (c) 2014 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/base/cef_logging.h"

#if defined(OS_WIN)
#include <windows.h>

#include <algorithm>
#elif defined(OS_POSIX)
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#endif

#if defined(OS_APPLE)
#include <mach/mach_time.h>
#endif

#include <iomanip>
#include <iostream>
#include <sstream>

#include "include/base/cef_immediate_crash.h"

namespace cef {
namespace logging {

namespace {

#if defined(OS_POSIX)
// From base/posix/safe_strerror.cc

#if defined(__GLIBC__) || defined(OS_NACL)
#define USE_HISTORICAL_STRERRO_R 1
#else
#define USE_HISTORICAL_STRERRO_R 0
#endif

#if USE_HISTORICAL_STRERRO_R && defined(__GNUC__)
// GCC will complain about the unused second wrap function unless we tell it
// that we meant for them to be potentially unused, which is exactly what this
// attribute is for.
#define POSSIBLY_UNUSED __attribute__((unused))
#else
#define POSSIBLY_UNUSED
#endif

#if USE_HISTORICAL_STRERRO_R
// glibc has two strerror_r functions: a historical GNU-specific one that
// returns type char *, and a POSIX.1-2001 compliant one available since 2.3.4
// that returns int. This wraps the GNU-specific one.
static void POSSIBLY_UNUSED
wrap_posix_strerror_r(char* (*strerror_r_ptr)(int, char*, size_t),
                      int err,
                      char* buf,
                      size_t len) {
  // GNU version.
  char* rc = (*strerror_r_ptr)(err, buf, len);
  if (rc != buf) {
    // glibc did not use buf and returned a static string instead. Copy it
    // into buf.
    buf[0] = '\0';
    strncat(buf, rc, len - 1);
  }
  // The GNU version never fails. Unknown errors get an "unknown error" message.
  // The result is always null terminated.
}
#endif  // USE_HISTORICAL_STRERRO_R

// Wrapper for strerror_r functions that implement the POSIX interface. POSIX
// does not define the behaviour for some of the edge cases, so we wrap it to
// guarantee that they are handled. This is compiled on all POSIX platforms, but
// it will only be used on Linux if the POSIX strerror_r implementation is
// being used (see below).
static void POSSIBLY_UNUSED wrap_posix_strerror_r(int (*strerror_r_ptr)(int,
                                                                        char*,
                                                                        size_t),
                                                  int err,
                                                  char* buf,
                                                  size_t len) {
  int old_errno = errno;
  // Have to cast since otherwise we get an error if this is the GNU version
  // (but in such a scenario this function is never called). Sadly we can't use
  // C++-style casts because the appropriate one is reinterpret_cast but it's
  // considered illegal to reinterpret_cast a type to itself, so we get an
  // error in the opposite case.
  int result = (*strerror_r_ptr)(err, buf, len);
  if (result == 0) {
    // POSIX is vague about whether the string will be terminated, although
    // it indirectly implies that typically ERANGE will be returned, instead
    // of truncating the string. We play it safe by always terminating the
    // string explicitly.
    buf[len - 1] = '\0';
  } else {
    // Error. POSIX is vague about whether the return value is itself a system
    // error code or something else. On Linux currently it is -1 and errno is
    // set. On BSD-derived systems it is a system error and errno is unchanged.
    // We try and detect which case it is so as to put as much useful info as
    // we can into our message.
    int strerror_error;  // The error encountered in strerror
    int new_errno = errno;
    if (new_errno != old_errno) {
      // errno was changed, so probably the return value is just -1 or something
      // else that doesn't provide any info, and errno is the error.
      strerror_error = new_errno;
    } else {
      // Either the error from strerror_r was the same as the previous value, or
      // errno wasn't used. Assume the latter.
      strerror_error = result;
    }
    // snprintf truncates and always null-terminates.
    snprintf(buf, len, "Error %d while retrieving error %d", strerror_error,
             err);
  }
  errno = old_errno;
}

void safe_strerror_r(int err, char* buf, size_t len) {
  if (buf == NULL || len <= 0) {
    return;
  }
  // If using glibc (i.e., Linux), the compiler will automatically select the
  // appropriate overloaded function based on the function type of strerror_r.
  // The other one will be elided from the translation unit since both are
  // static.
  wrap_posix_strerror_r(&strerror_r, err, buf, len);
}

std::string safe_strerror(int err) {
  const int buffer_size = 256;
  char buf[buffer_size];
  safe_strerror_r(err, buf, sizeof(buf));
  return std::string(buf);
}
#endif  // defined(OS_POSIX)

const internal::Implementation* g_impl_override = nullptr;

#if defined(OS_WIN)
using SetLogFatalCrashKeyFunc = void (*)(const char* /*file*/,
                                         int /*line*/,
                                         const char* /*message*/);

SetLogFatalCrashKeyFunc SetLogFatalCrashKeyFuncGetter() {
  static SetLogFatalCrashKeyFunc log_fatal_crash_key_func = []() {
    // Function exported by bootstrap.exe.
    return reinterpret_cast<SetLogFatalCrashKeyFunc>(
        GetProcAddress(GetModuleHandle(NULL), "SetLogFatalCrashKey"));
  }();
  return log_fatal_crash_key_func;
}

void SetLogFatalCrashKey(const char* file, int line, const char* message) {
  if (auto func = SetLogFatalCrashKeyFuncGetter()) {
    func(file, line, message);
  }
}
#endif  // defined(OS_WIN)

const char* const log_severity_names[] = {"INFO", "WARNING", "ERROR", "FATAL"};
static_assert(LOG_NUM_SEVERITIES == std::size(log_severity_names),
              "Incorrect number of log_severity_names");

const char* log_severity_name(int severity) {
  if (severity >= 0 && severity < LOG_NUM_SEVERITIES) {
    return log_severity_names[severity];
  }
  return "UNKNOWN";
}

#if defined(OS_WIN)
std::string WideToUTF8(const std::wstring& wstr) {
  if (wstr.empty()) {
    return {};
  }
  int size = WideCharToMultiByte(CP_UTF8, 0, wstr.data(),
                                 static_cast<int>(wstr.size()), nullptr, 0,
                                 nullptr, nullptr);
  if (size <= 0) {
    return {};
  }
  std::string utf8(size, '\0');
  if (WideCharToMultiByte(CP_UTF8, 0, wstr.data(),
                          static_cast<int>(wstr.size()), &utf8[0], size,
                          nullptr, nullptr) != size) {
    return {};
  }
  return utf8;
}

#if !defined(NDEBUG)
bool IsUser32AndGdi32Available() {
  static const bool is_user32_and_gdi32_available = [] {
    // If win32k syscalls aren't disabled, then user32 and gdi32 are available.
    PROCESS_MITIGATION_SYSTEM_CALL_DISABLE_POLICY policy = {};
    if (::GetProcessMitigationPolicy(GetCurrentProcess(),
                                     ProcessSystemCallDisablePolicy, &policy,
                                     sizeof(policy))) {
      return policy.DisallowWin32kSystemCalls == 0;
    }

    return true;
  }();
  return is_user32_and_gdi32_available;
}

std::wstring UTF8ToWide(const std::string& utf8) {
  if (utf8.empty()) {
    return {};
  }
  int size = MultiByteToWideChar(CP_UTF8, 0, utf8.data(),
                                 static_cast<int>(utf8.size()), nullptr, 0);
  if (size <= 0) {
    return {};
  }
  std::wstring utf16(size, L'\0');
  if (MultiByteToWideChar(CP_UTF8, 0, utf8.data(),
                          static_cast<int>(utf8.size()), &utf16[0],
                          size) != size) {
    return {};
  }
  return utf16;
}

// Displays a message box to the user with the error message in it. Used for
// fatal messages, where we close the app simultaneously. This is for developers
// only; we don't use this in circumstances (like release builds) where users
// could see it, since users don't understand these messages anyway.
void DisplayDebugMessageInDialog(const std::string& message) {
  if (IsUser32AndGdi32Available()) {
    MessageBoxW(nullptr, UTF8ToWide(message).c_str(), L"Fatal error",
                MB_OK | MB_ICONHAND | MB_TOPMOST);
  } else {
    OutputDebugStringW(UTF8ToWide(message).c_str());
  }
}
#endif  // !defined(NDEBUG)
#endif  // defined(OS_WIN)

[[noreturn]] void HandleFatal(const std::string& message) {
  // Don't display assertions to the user in release mode. The enduser can't do
  // anything with this information, and displaying message boxes when the
  // application is hosed can cause additional problems. We intentionally don't
  // implement a dialog on other platforms. You can just look at stderr.
#if !defined(NDEBUG) && defined(OS_WIN)
  if (!::IsDebuggerPresent()) {
    // Displaying a dialog is unnecessary when debugging and can complicate
    // debugging.
    DisplayDebugMessageInDialog(message);
  }
#endif

  // Crash the process to generate a dump.
  base::ImmediateCrash();
}

uint64_t TickCount() {
#if defined(OS_WIN)
  return ::GetTickCount();
#elif defined(OS_APPLE)
  return mach_absolute_time();
#elif defined(OS_POSIX)
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);

  uint64_t absolute_micro = static_cast<uint64_t>(ts.tv_sec) * 1000000 +
                            static_cast<uint64_t>(ts.tv_nsec) / 1000;

  return absolute_micro;
#else
#error Unsupported platform
#endif
}

}  // namespace

namespace internal {

const Implementation* GetImplementation() {
  if (g_impl_override) {
    return g_impl_override;
  }
  static constexpr Implementation default_impl = {
      &cef_get_min_log_level, &cef_get_vlog_level, &cef_log};
  return &default_impl;
}

ScopedImplementation::~ScopedImplementation() {
  g_impl_override = previous_;
}

ScopedImplementation::ScopedImplementation() {
#if defined(OS_WIN)
  // Preload the function pointer so that we do minimal work while crashing.
  SetLogFatalCrashKeyFuncGetter();
#endif
}

void ScopedImplementation::Init(const Implementation* impl) {
  previous_ = g_impl_override;
  g_impl_override = impl;
}

}  // namespace internal

ScopedEarlySupport::ScopedEarlySupport(const Config& config)
    : impl_{{&ScopedEarlySupport::get_min_log_level,
             &ScopedEarlySupport::get_vlog_level, &ScopedEarlySupport::log},
            config} {
  Init(&impl_.ptrs);
}

// static
const ScopedEarlySupport::Config& ScopedEarlySupport::GetConfig() {
  return reinterpret_cast<const ScopedEarlySupport::Impl*>(g_impl_override)
      ->config;
}

// static
int ScopedEarlySupport::get_min_log_level() {
  return GetConfig().min_log_level;
}

// static
int ScopedEarlySupport::get_vlog_level(const char* file_start, size_t N) {
  return GetConfig().vlog_level;
}

// static
void ScopedEarlySupport::log(const char* file,
                             int line,
                             int severity,
                             const char* message) {
  const Config& config = GetConfig();

  // Most logging initializes `file` from __FILE__. Unfortunately, because we
  // build from out/Foo we get a `../../` (or \) prefix for all of our
  // __FILE__s. This isn't true for base::Location::Current() which already does
  // the stripping (and is used for some logging, especially CHECKs).
  //
  // Here we strip the first 6 (../../ or ..\..\) characters if `file` starts
  // with `.` but defensively clamp to strlen(file) just in case.
  const std::string_view filename =
      file[0] == '.' ? std::string_view(file).substr(
                           std::min(std::size_t{6}, strlen(file)))
                     : file;

  std::stringstream stream;

  stream << '[';
  if (config.log_prefix) {
    stream << config.log_prefix << ':';
  }
  if (config.log_process_id) {
#if defined(OS_WIN)
    stream << ::GetCurrentProcessId() << ':';
#elif defined(OS_POSIX)
    stream << getpid() << ':';
#else
#error Unsupported platform
#endif
  }
  if (config.log_thread_id) {
#if defined(OS_WIN)
    stream << ::GetCurrentThreadId() << ':';
#elif defined(OS_APPLE)
    uint64_t tid;
    if (pthread_threadid_np(nullptr, &tid) == 0) {
      stream << tid << ':';
    }
#elif defined(OS_POSIX)
    stream << pthread_self() << ':';
#else
#error Unsupported platform
#endif
  }
  if (config.log_timestamp) {
#if defined(OS_WIN)
    SYSTEMTIME local_time;
    GetLocalTime(&local_time);
    stream << std::setfill('0') << std::setw(2) << local_time.wMonth
           << std::setw(2) << local_time.wDay << '/' << std::setw(2)
           << local_time.wHour << std::setw(2) << local_time.wMinute
           << std::setw(2) << local_time.wSecond << '.' << std::setw(3)
           << local_time.wMilliseconds << ':';
#elif defined(OS_POSIX)
    timeval tv;
    gettimeofday(&tv, nullptr);
    time_t t = tv.tv_sec;
    struct tm local_time;
    localtime_r(&t, &local_time);
    struct tm* tm_time = &local_time;
    stream << std::setfill('0') << std::setw(2) << 1 + tm_time->tm_mon
           << std::setw(2) << tm_time->tm_mday << '/' << std::setw(2)
           << tm_time->tm_hour << std::setw(2) << tm_time->tm_min
           << std::setw(2) << tm_time->tm_sec << '.' << std::setw(6)
           << tv.tv_usec << ':';
#else
#error Unsupported platform
#endif
  }
  if (config.log_tickcount) {
    stream << TickCount() << ':';
  }
  if (severity >= 0) {
    stream << log_severity_name(severity);
  } else {
    stream << "VERBOSE" << -severity;
  }
  stream << ":" << filename << ":" << line << "] " << message;

  const std::string& log_line = stream.str();

  if (!config.formatted_log_handler ||
      !config.formatted_log_handler(log_line.c_str())) {
    // Log to stderr.
    std::cerr << log_line << std::endl;

#if !defined(NDEBUG) && defined(OS_WIN)
    if (severity < LOG_FATAL) {
      // Log to the debugger console in debug builds.
      OutputDebugStringW(UTF8ToWide(log_line).c_str());
    }
#endif
  }

  if (severity == LOG_FATAL) {
#if defined(OS_WIN)
    SetLogFatalCrashKey(file, line, message);
#endif

    HandleFatal(log_line);
  }
}

// MSVC doesn't like complex extern templates and DLLs.
#if !defined(COMPILER_MSVC)
// Explicit instantiations for commonly used comparisons.
template std::string* MakeCheckOpString<int, int>(const int&,
                                                  const int&,
                                                  const char* names);
template std::string* MakeCheckOpString<unsigned long, unsigned long>(
    const unsigned long&,
    const unsigned long&,
    const char* names);
template std::string* MakeCheckOpString<unsigned long, unsigned int>(
    const unsigned long&,
    const unsigned int&,
    const char* names);
template std::string* MakeCheckOpString<unsigned int, unsigned long>(
    const unsigned int&,
    const unsigned long&,
    const char* names);
template std::string* MakeCheckOpString<std::string, std::string>(
    const std::string&,
    const std::string&,
    const char* name);
#endif

#if defined(OS_WIN)
LogMessage::SaveLastError::SaveLastError() : last_error_(::GetLastError()) {}

LogMessage::SaveLastError::~SaveLastError() {
  ::SetLastError(last_error_);
}
#endif  // defined(OS_WIN)

LogMessage::LogMessage(const char* file, int line, LogSeverity severity)
    : severity_(severity), file_(file), line_(line) {}

LogMessage::LogMessage(const char* file, int line, std::string* result)
    : severity_(LOG_FATAL), file_(file), line_(line) {
  stream_ << "Check failed: " << *result;
  delete result;
}

LogMessage::LogMessage(const char* file,
                       int line,
                       LogSeverity severity,
                       std::string* result)
    : severity_(severity), file_(file), line_(line) {
  stream_ << "Check failed: " << *result;
  delete result;
}

LogMessage::~LogMessage() {
  std::string str_newline(stream_.str());
  internal::GetImplementation()->log(file_, line_, severity_,
                                     str_newline.c_str());
}

#if defined(OS_WIN)
// This has already been defined in the header, but defining it again as DWORD
// ensures that the type used in the header is equivalent to DWORD. If not,
// the redefinition is a compile error.
using SystemErrorCode = DWORD;
#endif

SystemErrorCode GetLastSystemErrorCode() {
#if defined(OS_WIN)
  return ::GetLastError();
#elif defined(OS_POSIX)
  return errno;
#else
#error Not implemented
#endif
}

#if defined(OS_WIN)
std::string SystemErrorCodeToString(SystemErrorCode error_code) {
  const int error_message_buffer_size = 256;
  char msgbuf[error_message_buffer_size];
  DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
  DWORD len = FormatMessageA(flags, NULL, error_code, 0, msgbuf,
                             static_cast<DWORD>(std::size(msgbuf)), NULL);
  std::stringstream ss;
  if (len) {
    std::string s(msgbuf);
    // Messages returned by system end with line breaks.
    s.erase(std::remove_if(s.begin(), s.end(), ::isspace), s.end());
    ss << s << " (0x" << std::hex << error_code << ")";
  } else {
    ss << "Error (0x" << std::hex << GetLastError()
       << ") while retrieving error. (0x" << error_code << ")";
  }
  return ss.str();
}
#elif defined(OS_POSIX)
std::string SystemErrorCodeToString(SystemErrorCode error_code) {
  return safe_strerror(error_code);
}
#else
#error Not implemented
#endif

#if defined(OS_WIN)
Win32ErrorLogMessage::Win32ErrorLogMessage(const char* file,
                                           int line,
                                           LogSeverity severity,
                                           SystemErrorCode err)
    : err_(err), log_message_(file, line, severity) {}

Win32ErrorLogMessage::~Win32ErrorLogMessage() {
  stream() << ": " << SystemErrorCodeToString(err_);
}
#elif defined(OS_POSIX)
ErrnoLogMessage::ErrnoLogMessage(const char* file,
                                 int line,
                                 LogSeverity severity,
                                 SystemErrorCode err)
    : err_(err), log_message_(file, line, severity) {}

ErrnoLogMessage::~ErrnoLogMessage() {
  stream() << ": " << SystemErrorCodeToString(err_);
}
#endif  // OS_WIN

}  // namespace logging
}  // namespace cef

#if defined(OS_WIN)
std::ostream& operator<<(std::ostream& out, const std::wstring& wstr) {
  out << cef::logging::WideToUTF8(wstr);
  return out;
}
#endif  // defined(OS_WIN)
