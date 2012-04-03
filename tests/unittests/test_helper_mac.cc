// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "include/cef_app.h"
#include "tests/unittests/test_app.h"

int main(int argc, char* argv[]) {
  CefMainArgs main_args(argc, argv);
  
  CefRefPtr<CefApp> app(new TestApp);

  // Execute the secondary process.
  return CefExecuteProcess(main_args, app);
}
