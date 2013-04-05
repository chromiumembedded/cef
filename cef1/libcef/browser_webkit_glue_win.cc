// Copyright (c) 2008 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser_webkit_glue.h"
#include "webkit/glue/webkit_glue.h"

namespace webkit_glue {

bool EnsureFontLoaded(HFONT font) {
  return true;
}

}  // namespace webkit_glue
