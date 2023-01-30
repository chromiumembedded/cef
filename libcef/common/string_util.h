// Copyright 2021 The Chromium Embedded Framework Authors. Portions copyright
// 2011 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_STRING_UTIL_H_
#define CEF_LIBCEF_COMMON_STRING_UTIL_H_
#pragma once

#include "include/cef_base.h"

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"

namespace base {
class ReadOnlySharedMemoryRegion;
class RefCountedMemory;
}  // namespace base

namespace blink {
class WebString;
}

namespace string_util {

// Convert |source| to |cef_string|, avoiding UTF conversions if possible.
void GetCefString(const blink::WebString& source, CefString& cef_string);
void GetCefString(scoped_refptr<base::RefCountedMemory> source,
                  CefString& cef_string);

// Read |source| into shared memory, avoiding UTF conversions if possible.
// Use ExecuteWithScopedCefString() to retrieve the value on the receiving end
// with zero UTF conversions and zero copies if possible.
base::ReadOnlySharedMemoryRegion CreateSharedMemoryRegion(
    const blink::WebString& source);

using ScopedCefStringCallback = base::OnceCallback<void(const CefString&)>;

// Helper for executing |callback| with |region| as a scoped CefString.
void ExecuteWithScopedCefString(base::ReadOnlySharedMemoryRegion region,
                                ScopedCefStringCallback callback);

}  // namespace string_util

#endif  // CEF_LIBCEF_COMMON_STRING_UTIL_H_
