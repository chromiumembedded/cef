// Copyright 2016 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef  CEF_LIBCEF_COMMON_THREAD_IMPL_H_
#define  CEF_LIBCEF_COMMON_THREAD_IMPL_H_
#pragma once

#include "include/cef_thread.h"
#include "base/threading/thread.h"

class CefThreadImpl : public CefThread {
 public:
  CefThreadImpl();
  ~CefThreadImpl();

  bool Create(const CefString& display_name,
              cef_thread_priority_t priority,
              cef_message_loop_type_t message_loop_type,
              bool stoppable,
              cef_com_init_mode_t com_init_mode);

  // CefThread methods:
  CefRefPtr<CefTaskRunner> GetTaskRunner() override;
  cef_platform_thread_id_t GetPlatformThreadId() override;
  void Stop() override;
  bool IsRunning() override;

 private:
  std::unique_ptr<base::Thread> thread_;
  cef_platform_thread_id_t thread_id_;
  CefRefPtr<CefTaskRunner> thread_task_runner_;

  // TaskRunner for the owner thread.
  scoped_refptr<base::SequencedTaskRunner> owner_task_runner_;

  IMPLEMENT_REFCOUNTING(CefThreadImpl);
  DISALLOW_COPY_AND_ASSIGN(CefThreadImpl);
};

#endif  // CEF_LIBCEF_COMMON_THREAD_IMPL_H_
