// Copyright (c) 2020 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <vector>

#include "include/wrapper/cef_helpers.h"
#include "tests/ceftests/test_server.h"
#include "tests/ceftests/test_server_manager.h"
#include "tests/gtest/include/gtest/gtest.h"

namespace test_server {

ObserverHelper::ObserverHelper() : weak_ptr_factory_(this) {
  CEF_REQUIRE_UI_THREAD();
}

ObserverHelper::~ObserverHelper() {
  EXPECT_TRUE(state_ == State::NONE);
}

void ObserverHelper::Initialize(bool https_server) {
  CEF_REQUIRE_UI_THREAD();
  EXPECT_TRUE(state_ == State::NONE);
  state_ = State::INITIALIZING;
  registration_ = Manager::AddObserverAndStart(
      this,
      base::BindOnce(&ObserverHelper::OnStartDone,
                     weak_ptr_factory_.GetWeakPtr()),
      https_server);
}

void ObserverHelper::Shutdown() {
  CEF_REQUIRE_UI_THREAD();
  EXPECT_TRUE(state_ == State::INITIALIZED);
  state_ = State::SHUTTINGDOWN;
  registration_ = nullptr;
}

void ObserverHelper::OnStartDone(const std::string& server_origin) {
  EXPECT_TRUE(state_ == State::INITIALIZING);
  state_ = State::INITIALIZED;
  OnInitialized(server_origin);
}

void ObserverHelper::OnRegistered() {
  EXPECT_TRUE(state_ == State::INITIALIZING);
}

void ObserverHelper::OnUnregistered() {
  EXPECT_TRUE(state_ == State::SHUTTINGDOWN);
  state_ = State::NONE;
  OnShutdown();
}

}  // namespace test_server
