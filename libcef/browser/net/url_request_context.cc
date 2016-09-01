// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/net/url_request_context.h"

#if DCHECK_IS_ON()
base::AtomicRefCount CefURLRequestContext::DebugObjCt = 0;
#endif

CefURLRequestContext::CefURLRequestContext() {
#if DCHECK_IS_ON()
  base::AtomicRefCountInc(&DebugObjCt);
#endif
}

CefURLRequestContext::~CefURLRequestContext() {
#if DCHECK_IS_ON()
  base::AtomicRefCountDec(&DebugObjCt);
#endif
}
