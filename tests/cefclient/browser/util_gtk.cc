// Copyright (c) 2018 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/util_gtk.h"

#include <gdk/gdk.h>

namespace client {

base::PlatformThreadId ScopedGdkThreadsEnter::locked_thread_ =
    kInvalidPlatformThreadId;

ScopedGdkThreadsEnter::ScopedGdkThreadsEnter() {
  // The GDK lock is not reentrant, so don't try to lock again if the current
  // thread already holds it.
  base::PlatformThreadId current_thread = base::PlatformThread::CurrentId();
  take_lock_ = current_thread != locked_thread_;

  if (take_lock_) {
    gdk_threads_enter();
    locked_thread_ = current_thread;
  }
}

ScopedGdkThreadsEnter::~ScopedGdkThreadsEnter() {
  if (take_lock_) {
    locked_thread_ = kInvalidPlatformThreadId;
    gdk_threads_leave();
  }
}

}  // namespace client
