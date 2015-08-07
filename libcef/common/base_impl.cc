// Copyright 2014 The Chromium Embedded Framework Authors. Portions copyright
// 2011 the Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#include "include/base/cef_build.h"

#if defined(OS_WIN)
#include <windows.h>
#include <shellscalingapi.h>
#endif

#include "include/internal/cef_trace_event_internal.h"
#include "include/internal/cef_logging_internal.h"
#include "include/internal/cef_thread_internal.h"

#include "base/logging.h"
#include "base/threading/platform_thread.h"
#include "base/trace_event/trace_event.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"

namespace {

// Implementation from chrome/app/chrome_exe_main_win.cc.

// Win8.1 supports monitor-specific DPI scaling.
bool SetProcessDpiAwarenessWrapper(PROCESS_DPI_AWARENESS value) {
  typedef HRESULT(WINAPI *SetProcessDpiAwarenessPtr)(PROCESS_DPI_AWARENESS);
  SetProcessDpiAwarenessPtr set_process_dpi_awareness_func =
      reinterpret_cast<SetProcessDpiAwarenessPtr>(
          GetProcAddress(GetModuleHandleA("user32.dll"),
                         "SetProcessDpiAwarenessInternal"));
  if (set_process_dpi_awareness_func) {
    HRESULT hr = set_process_dpi_awareness_func(value);
    if (SUCCEEDED(hr)) {
      VLOG(1) << "SetProcessDpiAwareness succeeded.";
      return true;
    } else if (hr == E_ACCESSDENIED) {
      LOG(ERROR) << "Access denied error from SetProcessDpiAwareness. "
          "Function called twice, or manifest was used.";
    }
  }
  return false;
}

// This function works for Windows Vista through Win8. Win8.1 must use
// SetProcessDpiAwareness[Wrapper].
BOOL SetProcessDPIAwareWrapper() {
  typedef BOOL(WINAPI *SetProcessDPIAwarePtr)(VOID);
  SetProcessDPIAwarePtr set_process_dpi_aware_func =
      reinterpret_cast<SetProcessDPIAwarePtr>(
      GetProcAddress(GetModuleHandleA("user32.dll"),
                      "SetProcessDPIAware"));
  return set_process_dpi_aware_func &&
    set_process_dpi_aware_func();
}

void EnableHighDPISupport() {
  if (!SetProcessDpiAwarenessWrapper(PROCESS_SYSTEM_DPI_AWARE)) {
    SetProcessDPIAwareWrapper();
  }
}

}  // namespace

#endif  // defined(OS_WIN)

// The contents of this file are a compilation unit that is not called by other
// functions in the the library. Consiquently MSVS will exclude it during the
// linker stage if we don't call a stub function.
#if defined(COMPILER_MSVC)
#pragma optimize("", off)
#endif

void base_impl_stub() {}

#if defined(COMPILER_MSVC)
#pragma optimize("", on)
#endif


CEF_EXPORT void cef_trace_event_instant(const char* category,
                                        const char* name,
                                        const char* arg1_name,
                                        uint64 arg1_val,
                                        const char* arg2_name,
                                        uint64 arg2_val,
                                        int copy) {
  DCHECK(category);
  DCHECK(name);
  if (!category || !name)
    return;

  if (copy) {
    if (arg1_name == NULL && arg2_name == NULL) {
      TRACE_EVENT_COPY_INSTANT0(category, name, TRACE_EVENT_SCOPE_THREAD);
    } else if (arg2_name == NULL) {
      TRACE_EVENT_COPY_INSTANT1(category, name, TRACE_EVENT_SCOPE_THREAD,
                                arg1_name, arg1_val);
    } else {
      TRACE_EVENT_COPY_INSTANT2(category, name, TRACE_EVENT_SCOPE_THREAD,
                                arg1_name, arg1_val, arg2_name, arg2_val);
    }
  } else {
    if (arg1_name == NULL && arg2_name == NULL) {
      TRACE_EVENT_INSTANT0(category, name, TRACE_EVENT_SCOPE_THREAD);
    } else if (arg2_name == NULL) {
      TRACE_EVENT_INSTANT1(category, name, TRACE_EVENT_SCOPE_THREAD,
                           arg1_name, arg1_val);
    } else {
      TRACE_EVENT_INSTANT2(category, name, TRACE_EVENT_SCOPE_THREAD,
                           arg1_name, arg1_val, arg2_name, arg2_val);
    }
  }
}

CEF_EXPORT void cef_trace_event_begin(const char* category,
                                      const char* name,
                                      const char* arg1_name,
                                      uint64 arg1_val,
                                      const char* arg2_name,
                                      uint64 arg2_val,
                                      int copy) {
  DCHECK(category);
  DCHECK(name);
  if (!category || !name)
    return;

  if (copy) {
    if (arg1_name == NULL && arg2_name == NULL) {
      TRACE_EVENT_COPY_BEGIN0(category, name);
    } else if (arg2_name == NULL) {
      TRACE_EVENT_COPY_BEGIN1(category, name, arg1_name, arg1_val);
    } else {
      TRACE_EVENT_COPY_BEGIN2(category, name, arg1_name, arg1_val,
                              arg2_name, arg2_val);
    }
  } else {
    if (arg1_name == NULL && arg2_name == NULL) {
      TRACE_EVENT_BEGIN0(category, name);
    } else if (arg2_name == NULL) {
      TRACE_EVENT_BEGIN1(category, name, arg1_name, arg1_val);
    } else {
      TRACE_EVENT_BEGIN2(category, name, arg1_name, arg1_val,
                         arg2_name, arg2_val);
    }
  }
}

CEF_EXPORT void cef_trace_event_end(const char* category,
                                    const char* name,
                                    const char* arg1_name,
                                    uint64 arg1_val,
                                    const char* arg2_name,
                                    uint64 arg2_val,
                                    int copy) {
  DCHECK(category);
  DCHECK(name);
  if (!category || !name)
    return;

  if (copy) {
    if (arg1_name == NULL && arg2_name == NULL) {
      TRACE_EVENT_COPY_END0(category, name);
    } else if (arg2_name == NULL) {
      TRACE_EVENT_COPY_END1(category, name, arg1_name, arg1_val);
    } else {
      TRACE_EVENT_COPY_END2(category, name, arg1_name, arg1_val,
                            arg2_name, arg2_val);
    }
  } else {
    if (arg1_name == NULL && arg2_name == NULL) {
      TRACE_EVENT_END0(category, name);
    } else if (arg2_name == NULL) {
      TRACE_EVENT_END1(category, name, arg1_name, arg1_val);
    } else {
      TRACE_EVENT_END2(category, name, arg1_name, arg1_val,
                       arg2_name, arg2_val);
    }
  }
}

CEF_EXPORT void cef_trace_counter(const char* category,
                                  const char* name,
                                  const char* value1_name,
                                  uint64 value1_val,
                                  const char* value2_name,
                                  uint64 value2_val,
                                  int copy) {
  DCHECK(category);
  DCHECK(name);
  if (!category || !name)
    return;

  if (copy) {
    if (value1_name == NULL && value2_name == NULL) {
      TRACE_COPY_COUNTER1(category, name, value1_val);
    } else {
      TRACE_COPY_COUNTER2(category, name, value1_name, value1_val,
                          value2_name, value2_val);
    }
  } else {
    if (value1_name == NULL && value2_name == NULL) {
      TRACE_COUNTER1(category, name, value1_val);
    } else {
      TRACE_COUNTER2(category, name, value1_name, value1_val,
                     value2_name, value2_val);
    }
  }
}

CEF_EXPORT void cef_trace_counter_id(const char* category,
                                     const char* name,
                                     uint64 id,
                                     const char* value1_name,
                                     uint64 value1_val,
                                     const char* value2_name,
                                     uint64 value2_val,
                                     int copy) {
  DCHECK(category);
  DCHECK(name);
  if (!category || !name)
    return;

  if (copy) {
    if (value1_name == NULL && value2_name == NULL) {
      TRACE_COPY_COUNTER_ID1(category, name, id, value1_val);
    } else {
      TRACE_COPY_COUNTER_ID2(category, name, id, value1_name,
                             value1_val, value2_name, value2_val);
    }
  } else {
    if (value1_name == NULL && value2_name == NULL) {
      TRACE_COUNTER_ID1(category, name, id, value1_val);
    } else {
      TRACE_COUNTER_ID2(category, name, id, value1_name, value1_val,
                        value2_name, value2_val);
    }
  }
}

CEF_EXPORT void cef_trace_event_async_begin(const char* category,
                                            const char* name,
                                            uint64 id,
                                            const char* arg1_name,
                                            uint64 arg1_val,
                                            const char* arg2_name,
                                            uint64 arg2_val,
                                            int copy) {
  DCHECK(category);
  DCHECK(name);
  if (!category || !name)
    return;

  if (copy) {
    if (arg1_name == NULL && arg2_name == NULL) {
      TRACE_EVENT_COPY_ASYNC_BEGIN0(category, name, id);
    } else if (arg2_name == NULL) {
      TRACE_EVENT_COPY_ASYNC_BEGIN1(category, name, id, arg1_name, arg1_val);
    } else {
      TRACE_EVENT_COPY_ASYNC_BEGIN2(category, name, id, arg1_name, arg1_val,
                                    arg2_name, arg2_val);
    }
  } else {
    if (arg1_name == NULL && arg2_name == NULL) {
      TRACE_EVENT_ASYNC_BEGIN0(category, name, id);
    } else if (arg2_name == NULL) {
      TRACE_EVENT_ASYNC_BEGIN1(category, name, id, arg1_name, arg1_val);
    } else {
      TRACE_EVENT_ASYNC_BEGIN2(category, name, id, arg1_name, arg1_val,
                               arg2_name, arg2_val);
    }
  }
}

CEF_EXPORT void cef_trace_event_async_step_into(const char* category,
                                                const char* name,
                                                uint64 id,
                                                uint64 step,
                                                const char* arg1_name,
                                                uint64 arg1_val,
                                                int copy) {
  DCHECK(category);
  DCHECK(name);
  if (!category || !name)
    return;

  if (copy) {
    if (arg1_name == NULL) {
      INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_ASYNC_STEP_INTO,
          category, name, id, TRACE_EVENT_FLAG_COPY, "step", step);
    } else {
      INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_ASYNC_STEP_INTO,
          category, name, id, TRACE_EVENT_FLAG_COPY, "step", step,
          arg1_name, arg1_val);
    }
  } else {
    if (arg1_name == NULL) {
      TRACE_EVENT_ASYNC_STEP_INTO0(category, name, id, step);
    } else {
      TRACE_EVENT_ASYNC_STEP_INTO1(category, name, id, step,
                                   arg1_name, arg1_val);
    }
  }
}

CEF_EXPORT void cef_trace_event_async_step_past(const char* category,
                                                const char* name,
                                                uint64 id,
                                                uint64 step,
                                                const char* arg1_name,
                                                uint64 arg1_val,
                                                int copy) {
  DCHECK(category);
  DCHECK(name);
  if (!category || !name)
    return;

  if (copy) {
    if (arg1_name == NULL) {
      INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_ASYNC_STEP_PAST,
          category, name, id, TRACE_EVENT_FLAG_COPY, "step", step);
    } else {
      INTERNAL_TRACE_EVENT_ADD_WITH_ID(TRACE_EVENT_PHASE_ASYNC_STEP_PAST,
          category, name, id, TRACE_EVENT_FLAG_COPY, "step", step,
          arg1_name, arg1_val);
    }
  } else {
    if (arg1_name == NULL) {
      TRACE_EVENT_ASYNC_STEP_PAST0(category, name, id, step);
    } else {
      TRACE_EVENT_ASYNC_STEP_PAST1(category, name, id, step,
                                   arg1_name, arg1_val);
    }
  }
}

CEF_EXPORT void cef_trace_event_async_end(const char* category,
                                          const char* name,
                                          uint64 id,
                                          const char* arg1_name,
                                          uint64 arg1_val,
                                          const char* arg2_name,
                                          uint64 arg2_val,
                                          int copy) {
  DCHECK(category);
  DCHECK(name);
  if (!category || !name)
    return;

  if (copy) {
    if (arg1_name == NULL && arg2_name == NULL) {
      TRACE_EVENT_COPY_ASYNC_END0(category, name, id);
    } else if (arg2_name == NULL) {
      TRACE_EVENT_COPY_ASYNC_END1(category, name, id, arg1_name,
                                  arg1_val);
    } else {
      TRACE_EVENT_COPY_ASYNC_END2(category, name, id, arg1_name,
                                  arg1_val, arg2_name, arg2_val);
    }
  } else {
    if (arg1_name == NULL && arg2_name == NULL) {
      TRACE_EVENT_ASYNC_END0(category, name, id);
    } else if (arg2_name == NULL) {
      TRACE_EVENT_ASYNC_END1(category, name, id, arg1_name,
                             arg1_val);
    } else {
      TRACE_EVENT_ASYNC_END2(category, name, id, arg1_name,
                             arg1_val, arg2_name, arg2_val);
    }
  }
}

CEF_EXPORT int cef_get_min_log_level() {
  return logging::GetMinLogLevel();
}

CEF_EXPORT int cef_get_vlog_level(const char* file_start, size_t N) {
  return logging::GetVlogLevelHelper(file_start, N);
}

CEF_EXPORT void cef_log(const char* file,
                        int line,
                        int severity,
                        const char* message) {
  logging::LogMessage(file, line, severity).stream() << message;
}

CEF_EXPORT cef_platform_thread_id_t cef_get_current_platform_thread_id() {
  return base::PlatformThread::CurrentId();
}

CEF_EXPORT cef_platform_thread_handle_t
    cef_get_current_platform_thread_handle() {
#if defined(OS_WIN)
  return base::PlatformThread::CurrentId();
#else
  return base::PlatformThread::CurrentHandle().platform_handle();
#endif
}

void CefEnableHighDPISupport() {
#if defined(OS_WIN)
  // We don't want to set DPI awareness on pre-Win7 because we don't support
  // DirectWrite there. GDI fonts are kerned very badly, so better to leave
  // DPI-unaware and at effective 1.0. See also ShouldUseDirectWrite().
  if (base::win::GetVersion() >= base::win::VERSION_WIN7)
    EnableHighDPISupport();
#endif
}
