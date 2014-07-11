// Copyright (c) 2011 Marshall A. Greenblatt. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the name Chromium Embedded
// Framework nor the names of its contributors may be used to endorse
// or promote products derived from this software without specific prior
// written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


#ifndef CEF_INCLUDE_BASE_CEF_BUILD_H_
#define CEF_INCLUDE_BASE_CEF_BUILD_H_
#pragma once

#if defined(BUILDING_CEF_SHARED)
// When building CEF include the Chromium header directly.
#include "base/compiler_specific.h"
#else  // !BUILDING_CEF_SHARED
// The following is substantially similar to the Chromium implementation.
// If the Chromium implementation diverges the below implementation should be
// updated to match.

#if defined(_WIN32)
#ifndef OS_WIN
#define OS_WIN 1
#endif
#elif defined(__APPLE__)
#ifndef OS_MACOSX
#define OS_MACOSX 1
#endif
#elif defined(__linux__)
#ifndef OS_LINUX
#define OS_LINUX 1
#endif
#else
#error Please add support for your platform in cef_build.h
#endif

// For access to standard POSIXish features, use OS_POSIX instead of a
// more specific macro.
#if defined(OS_MACOSX) || defined(OS_LINUX)
#ifndef OS_POSIX
#define OS_POSIX 1
#endif
#endif

// Compiler detection.
#if defined(__GNUC__)
#ifndef COMPILER_GCC
#define COMPILER_GCC 1
#endif
#elif defined(_MSC_VER)
#ifndef COMPILER_MSVC
#define COMPILER_MSVC 1
#endif
#else
#error Please add support for your compiler in cef_build.h
#endif

// Annotate a virtual method indicating it must be overriding a virtual
// method in the parent class.
// Use like:
//   virtual void foo() OVERRIDE;
#if defined(__clang__) || defined(COMPILER_MSVC)
#define OVERRIDE override
#elif defined(COMPILER_GCC) && __cplusplus >= 201103 && \
      (__GNUC__ * 10000 + __GNUC_MINOR__ * 100) >= 40700
// GCC 4.7 supports explicit virtual overrides when C++11 support is enabled.
#define OVERRIDE override
#else
#define OVERRIDE
#endif

#endif  // !BUILDING_CEF_SHARED

#endif  // CEF_INCLUDE_BASE_CEF_BUILD_H_
