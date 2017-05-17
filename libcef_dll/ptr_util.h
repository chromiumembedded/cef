// Copyright (c) 2017 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_PTR_UTIL_H_
#define CEF_LIBCEF_DLL_PTR_UTIL_H_
#pragma once

// Helpers for CefOwnPtr<>.
#if defined(USING_CHROMIUM_INCLUDES)
#define OWN_PASS(p) std::move(p)
#define OWN_RETURN_AS(p, t) (p)
#else
#define OWN_PASS(p) (p).Pass()
#define OWN_RETURN_AS(p, t) (p).PassAs<t>()
#endif

#endif  // CEF_LIBCEF_DLL_PTR_UTIL_H_
