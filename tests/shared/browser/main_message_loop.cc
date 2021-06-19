// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/shared/browser/main_message_loop.h"

#include "include/cef_task.h"
#include "include/wrapper/cef_closure_task.h"

namespace client {

namespace {

MainMessageLoop* g_main_message_loop = nullptr;

}  // namespace

MainMessageLoop::MainMessageLoop() {
  DCHECK(!g_main_message_loop);
  g_main_message_loop = this;
}

MainMessageLoop::~MainMessageLoop() {
  g_main_message_loop = nullptr;
}

// static
MainMessageLoop* MainMessageLoop::Get() {
  DCHECK(g_main_message_loop);
  return g_main_message_loop;
}

void MainMessageLoop::PostClosure(base::OnceClosure closure) {
  PostTask(CefCreateClosureTask(std::move(closure)));
}

void MainMessageLoop::PostClosure(const base::RepeatingClosure& closure) {
  PostTask(CefCreateClosureTask(closure));
}

}  // namespace client
