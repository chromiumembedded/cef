// Copyright 2020 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/common/task_runner_manager.h"

#include "base/logging.h"

namespace {

CefTaskRunnerManager* g_manager = nullptr;

}  // namespace

// static
CefTaskRunnerManager* CefTaskRunnerManager::Get() {
  return g_manager;
}

CefTaskRunnerManager::CefTaskRunnerManager() {
  // Only a single instance should exist.
  DCHECK(!g_manager);
  g_manager = this;
}

CefTaskRunnerManager::~CefTaskRunnerManager() {
  g_manager = nullptr;
}
