// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef _CEF_TEST_SUITE_H
#define _CEF_TEST_SUITE_H

#include "base/test/test_suite.h"

class CefTestSuite : public TestSuite {
public:
  CefTestSuite(int argc, char** argv);

protected:
  virtual void Initialize();
  virtual void Shutdown();
};

#endif  // _CEF_TEST_SUITE_H
