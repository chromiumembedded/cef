// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/unittests/test_app.h"

// static
void TestApp::CreateTests(TestSet& tests) {
  // Bring in the process message tests.
  extern void CreateProcessMessageRendererTests(TestApp::TestSet& tests);
  CreateProcessMessageRendererTests(tests);

  // Bring in the V8 tests.
  extern void CreateV8RendererTests(TestApp::TestSet& tests);
  CreateV8RendererTests(tests);
}
