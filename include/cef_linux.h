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
  ~CefCriticalSection()
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

// Class representing window information.
class CefWindowInfo : public cef_window_info_t
{
public:
  CefWindowInfo()
  {
    Init();
  }
  ~CefWindowInfo()
  {
    if(m_windowName)
      cef_string_free(m_windowName);
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

  void Init()
  {
    m_windowName = NULL;
    m_x = 0;
    m_y = 0;
    m_nWidth = 0;
    m_nHeight = 0;
  }

  CefWindowInfo& operator=(const CefWindowInfo& r)
  {
    return operator=(static_cast<const cef_window_info_t&>(r));
  }
  CefWindowInfo& operator=(const cef_window_info_t& r)
  {
    if(m_windowName)
      cef_string_free(m_windowName);
    if(r.m_windowName)
      m_windowName = cef_string_alloc(r.m_windowName);
    else
      m_windowName = NULL;
    m_x = r.m_x;
    m_y = r.m_y;
    m_nWidth = r.m_nWidth;
    m_nHeight = r.m_nHeight;
    return *this;
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
  ~CefPrintInfo()
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

  void Init()
  {
    m_Scale = 0;
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
};

// Window handle.
#define CefWindowHandle cef_window_handle_t
#endif // defined(__linux__)

#endif // _CEF_LINUX_H
