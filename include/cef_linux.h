// Copyright (c) 2010 Marshall A. Greenblatt. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the name Chromium Embedded
// Framework nor the names of its contributors may be used to endorse
// or promote products derived from this software without specific prior
// written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


#ifndef _CEF_LINUX_H
#define _CEF_LINUX_H

#if defined(__linux__)
#include <pthread.h>
#include "cef_types_linux.h"

// Atomic increment and decrement.
inline long CefAtomicIncrement(long volatile *pDest)
{
  return __sync_add_and_fetch(pDest, 1);
}
inline long CefAtomicDecrement(long volatile *pDest)
{
  return __sync_sub_and_fetch(pDest, 1);
}

// Critical section wrapper.
class CefCriticalSection
{
public:
  CefCriticalSection()
  {
    pthread_mutexattr_init(&attr_);
    pthread_mutexattr_settype(&attr_, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&lock_, &attr_);
  }
  virtual ~CefCriticalSection()
  {
    pthread_mutex_destroy(&lock_);
    pthread_mutexattr_destroy(&attr_);
  }
  void Lock()
  {
    pthread_mutex_lock(&lock_);
  }
  void Unlock()
  {
    pthread_mutex_unlock(&lock_);
  }

  pthread_mutex_t lock_;
  pthread_mutexattr_t attr_;
};

// Handle types.
#define CefWindowHandle cef_window_handle_t
#define CefCursorHandle cef_cursor_handle_t

// Class representing window information.
class CefWindowInfo : public cef_window_info_t
{
public:
  CefWindowInfo()
  {
    Init();
  }
  virtual ~CefWindowInfo()
  {
    Reset();
  }

  CefWindowInfo(const CefWindowInfo& r)
  {
    Init();
    *this = r;
  }
  CefWindowInfo(const cef_window_info_t& r)
  {
    Init();
    *this = r;
  }

  void Reset()
  {
    Init();
  }

  void Attach(const cef_window_info_t& r)
  {
    Reset();
    *static_cast<cef_window_info_t*>(this) = r;
  }

  void Detach()
  {
    Init();
  }

  void SetAsChild(CefWindowHandle ParentWidget)
  {
    m_ParentWidget = ParentWidget;
  }

  CefWindowInfo& operator=(const CefWindowInfo& r)
  {
    return operator=(static_cast<const cef_window_info_t&>(r));
  }
  CefWindowInfo& operator=(const cef_window_info_t& r)
  {
    m_Widget = r.m_Widget;
    m_ParentWidget = r.m_ParentWidget;
    return *this;
  }

protected:
  void Init()
  {
    memset(static_cast<cef_window_info_t*>(this), 0, sizeof(cef_window_info_t));
  }
};

// Class representing print context information.
class CefPrintInfo : public cef_print_info_t
{
public:
  CefPrintInfo()
  {
    Init();
  }
  virtual ~CefPrintInfo()
  {
  }

  CefPrintInfo(const CefPrintInfo& r)
  {
    Init();
    *this = r;
  }
  CefPrintInfo(const cef_print_info_t& r)
  {
    Init();
    *this = r;
  }

  CefPrintInfo& operator=(const CefPrintInfo& r)
  {
    return operator=(static_cast<const cef_print_info_t&>(r));
  }
  CefPrintInfo& operator=(const cef_print_info_t& r)
  {
    m_Scale = r.m_Scale;
    return *this;
  }

protected:
  void Init()
  {
    m_Scale = 0;
  }
};

#endif // defined(__linux__)

#endif // _CEF_LINUX_H
