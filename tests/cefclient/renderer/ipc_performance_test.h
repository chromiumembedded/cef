// Copyright (c) 2022 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_RENDERER_IPC_PERFORMANCE_TEST_H_
#define CEF_TESTS_CEFCLIENT_RENDERER_IPC_PERFORMANCE_TEST_H_
#pragma once

#include "tests/shared/renderer/client_app_renderer.h"

namespace client::ipc_performance_test {

void CreateDelegates(ClientAppRenderer::DelegateSet& delegates);

}  // namespace client::ipc_performance_test

#endif  // CEF_TESTS_CEFCLIENT_RENDERER_IPC_PERFORMANCE_TEST_H_
