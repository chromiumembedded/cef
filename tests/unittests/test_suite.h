// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef _CEF_TEST_SUITE_H
#define _CEF_TEST_SUITE_H

#include "build/build_config.h"
#include "base/test/test_suite.h"
#include "include/cef.h"

class CefTestSuite : public TestSuite {
 public:
  CefTestSuite(int argc, char** argv) : TestSuite(argc, argv) {
  }

 protected:

  virtual void Initialize() {
    TestSuite::Initialize();
    CefInitialize(true, std::wstring());
  }

  virtual void Shutdown() {
    CefShutdown();
    TestSuite::Shutdown();
  }
};

#endif  // _CEF_TEST_SUITE_H
