// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "include/cef_trace.h"
#include "include/cef_trace_event.h"
#include "libcef/browser/trace_subscriber.h"
#include "libcef/browser/context.h"
#include "libcef/browser/thread_util.h"

#include "base/debug/trace_event.h"
#include "base/time/time.h"

bool CefBeginTracing(CefRefPtr<CefTraceClient> client,
                     const CefString& categories) {
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return false;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    NOTREACHED() << "called on invalid thread";
    return false;
  }

  CefTraceSubscriber* subscriber = CefContext::Get()->GetTraceSubscriber();
  if (!subscriber)
    return false;

  return subscriber->BeginTracing(client, categories);
}

bool CefGetTraceBufferPercentFullAsync() {
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return false;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    NOTREACHED() << "called on invalid thread";
    return false;
  }

  CefTraceSubscriber* subscriber = CefContext::Get()->GetTraceSubscriber();
  if (!subscriber)
    return false;

  return subscriber->GetTraceBufferPercentFullAsync();
}

bool CefEndTracingAsync() {
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return false;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    NOTREACHED() << "called on invalid thread";
    return false;
  }

  CefTraceSubscriber* subscriber = CefContext::Get()->GetTraceSubscriber();
  if (!subscriber)
    return false;

  return subscriber->EndTracingAsync();
}

int64 CefNowFromSystemTraceTime() {
  return base::TimeTicks::NowFromSystemTraceTime().ToInternalValue();
}

// The below functions can be called from any process.

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
