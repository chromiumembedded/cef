// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifdef BUILDING_CEF_SHARED
#include "base/logging.h"
#else
#include <assert.h>
#define DCHECK(condition) assert(condition)
#endif
