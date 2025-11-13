// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFSIMPLE_CAPI_SIMPLE_UTILS_H_
#define CEF_TESTS_CEFSIMPLE_CAPI_SIMPLE_UTILS_H_

#include <stdio.h>
#include <stdlib.h>

//
// Assertion Helpers
//
// For invariants that should never fail, crash immediately with a clear
// message.
//

// Check that a condition is true. If false, crash with error message.
// Example: CHECK(handler != NULL);
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      fprintf(stderr, "[cefsimple_capi FATAL] %s:%d: CHECK failed: %s\n", \
              __FILE__, __LINE__, #cond);                                 \
      abort();                                                            \
    }                                                                     \
  } while (0)

#endif  // CEF_TESTS_CEFSIMPLE_CAPI_SIMPLE_UTILS_H_
