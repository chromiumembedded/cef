// Copyright (c) 2010 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _CEFCLIENT_UTIL_H
#define _CEFCLIENT_UTIL_H

#ifdef _WIN32

#include <windows.h>

#ifdef _DEBUG
#define ASSERT(condition) if(!(condition)) { DebugBreak(); }
#else
#define ASSERT(condition) ((void)0)
#endif

// MSVC_PUSH_DISABLE_WARNING pushes |n| onto a stack of warnings to be disabled.
// The warning remains disabled until popped by MSVC_POP_WARNING.
#define MSVC_PUSH_DISABLE_WARNING(n) __pragma(warning(push)) \
                                     __pragma(warning(disable:n))

// MSVC_PUSH_WARNING_LEVEL pushes |n| as the global warning level.  The level
// remains in effect until popped by MSVC_POP_WARNING().  Use 0 to disable all
// warnings.
#define MSVC_PUSH_WARNING_LEVEL(n) __pragma(warning(push, n))

// Pop effects of innermost MSVC_PUSH_* macro.
#define MSVC_POP_WARNING() __pragma(warning(pop))

// Allows |this| to be passed as an argument in constructor initializer lists.
// This uses push/pop instead of the seemingly simpler suppress feature to avoid
// having the warning be disabled for more than just |code|.
//
// Example usage:
// Foo::Foo() : x(NULL), ALLOW_THIS_IN_INITIALIZER_LIST(y(this)), z(3) {}
//
// Compiler warning C4355: 'this': used in base member initializer list:
// http://msdn.microsoft.com/en-us/library/3c594ae3(VS.80).aspx
#define ALLOW_THIS_IN_INITIALIZER_LIST(code) MSVC_PUSH_DISABLE_WARNING(4355) \
                                             code \
                                             MSVC_POP_WARNING()

#else // !_WIN32

#include <assert.h>

#ifdef _DEBUG
#define ASSERT(condition) if(!(condition)) { assert(false); }
#else
#define ASSERT(condition) ((void)0)
#endif

#define ALLOW_THIS_IN_INITIALIZER_LIST(code) code

#endif // !_WIN32

#endif // _CEFCLIENT_UTIL_H
