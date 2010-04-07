// Copyright (c) 2010 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#pragma once
#include <windows.h>

class Thread
{
public:
  // Interface for thread execution.
  class Handler
  {
  public:
    virtual DWORD Run() =0;
    virtual void Destroy() =0;
  };

  // Create and execute a new thread for the specified handler.
  static HANDLE Execute(Handler* pHandler)
  {
    if (!pHandler)
      return 0;
    Thread* pThread = new Thread(pHandler);
    return pThread->Execute();
  }

private:
  Thread(Handler* pHandler) : m_hThread(NULL), m_pHandler(pHandler) {}
  ~Thread()
  {
    if (m_hThread)
      CloseHandle(m_hThread);
    m_pHandler->Destroy();
  }

  HANDLE Execute()
  {
    m_hThread = CreateThread(NULL, 4096, ThreadHandler,
        reinterpret_cast<LPVOID>(this), 0, NULL);
    if (m_hThread == NULL) {
      delete this;
      return NULL;
    }
    return m_hThread;
  }

  static DWORD WINAPI ThreadHandler(LPVOID lpThreadParameter)
  {
    Thread* pThread = reinterpret_cast<Thread*>(lpThreadParameter);
    DWORD ret = pThread->m_pHandler->Run();
    delete pThread;
    return ret;
  }

  Handler* m_pHandler;
  HANDLE m_hThread;
};
