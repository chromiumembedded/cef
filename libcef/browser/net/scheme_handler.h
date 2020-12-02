// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NET_SCHEME_HANDLER_H_
#define CEF_LIBCEF_BROWSER_NET_SCHEME_HANDLER_H_
#pragma once

#include "include/cef_frame.h"

#include "content/public/browser/browser_context.h"
#include "url/gurl.h"

class CefIOThreadState;

namespace scheme {

// Register the internal scheme handlers that can be overridden.
void RegisterInternalHandlers(CefIOThreadState* iothread_state);

}  // namespace scheme

#endif  // CEF_LIBCEF_BROWSER_NET_SCHEME_HANDLER_H_
