// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_UTIL_H_
#define CEF_TESTS_CEFCLIENT_UTIL_H_
#pragma once

#include <cstring>
#include <string>
#include <vector>

#include "include/cef_task.h"

#if defined(OS_WIN)

#include <windows.h>  // NOLINT(build/include_order)

#ifndef NDEBUG
#define ASSERT(condition) if (!(condition)) { DebugBreak(); }
#else
#define ASSERT(condition) ((void)0)
#endif

#else  // !OS_WIN

#include <assert.h>  // NOLINT(build/include_order)

#ifndef NDEBUG
#define ASSERT(condition) if (!(condition)) { assert(false); }
#else
#define ASSERT(condition) ((void)0)
#endif

#endif  // !OS_WIN

#define REQUIRE_UI_THREAD()   ASSERT(CefCurrentlyOn(TID_UI));
#define REQUIRE_IO_THREAD()   ASSERT(CefCurrentlyOn(TID_IO));
#define REQUIRE_FILE_THREAD() ASSERT(CefCurrentlyOn(TID_FILE));

// Helper class to manage a scoped copy of |argv|.
class ScopedArgArray {
 public:
  ScopedArgArray(int argc, char* argv[]) {
    array_ = new char*[argc];
    for (int i = 0; i < argc; ++i) {
      values_.push_back(argv[i]);
      array_[i] = const_cast<char*>(values_[i].c_str());
    }
  }
  ~ScopedArgArray() {
    delete [] array_;
  }

  char** array() const { return array_; }

 private:
  char** array_;

  // Keep values in a vector separate from |array_| because various users may
  // modify |array_| and we still want to clean up memory properly.
  std::vector<std::string> values_;
};


// The arraysize(arr) macro returns the # of elements in an array arr.
// The expression is a compile-time constant, and therefore can be
// used in defining new arrays, for example.  If you use arraysize on
// a pointer by mistake, you will get a compile-time error.
//
// One caveat is that arraysize() doesn't accept any array of an
// anonymous type or a type defined inside a function.  In these rare
// cases, you have to use the unsafe ARRAYSIZE_UNSAFE() macro below.  This is
// due to a limitation in C++'s template system.  The limitation might
// eventually be removed, but it hasn't happened yet.

// This template function declaration is used in defining arraysize.
// Note that the function doesn't need an implementation, as we only
// use its type.
template <typename T, size_t N>
char (&ArraySizeHelper(T (&array)[N]))[N];

// That gcc wants both of these prototypes seems mysterious. VC, for
// its part, can't decide which to use (another mystery). Matching of
// template overloads: the final frontier.
#ifndef _MSC_VER
template <typename T, size_t N>
char (&ArraySizeHelper(const T (&array)[N]))[N];
#endif

#define arraysize(array) (sizeof(ArraySizeHelper(array)))


// A macro to disallow the copy constructor and operator= functions
// This should be used in the private: declarations for a class
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&);               \
  void operator=(const TypeName&)

#endif  // CEF_TESTS_CEFCLIENT_UTIL_H_
