// Copyright (c) 2011 Google Inc. All rights reserved.
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

// Do not include this header file directly. Use base/cef_callback.h instead.

#ifndef CEF_INCLUDE_BASE_INTERNAL_CEF_RAW_SCOPED_REFPTR_MISMATCH_CHECKER_H_
#define CEF_INCLUDE_BASE_INTERNAL_CEF_RAW_SCOPED_REFPTR_MISMATCH_CHECKER_H_

#include <type_traits>

// It is dangerous to post a task with a T* argument where T is a subtype of
// RefCounted(Base|ThreadSafeBase), since by the time the parameter is used, the
// object may already have been deleted since it was not held with a
// scoped_refptr. Example: http://crbug.com/27191
// The following set of traits are designed to generate a compile error
// whenever this antipattern is attempted.

namespace base {

// This is a base internal implementation file used by task.h and callback.h.
// Not for public consumption, so we wrap it in namespace internal.
namespace cef_internal {

template <typename T, typename = void>
struct IsRefCountedType : std::false_type {};

template <typename T>
struct IsRefCountedType<T,
                        std::void_t<decltype(std::declval<T*>()->AddRef()),
                                    decltype(std::declval<T*>()->Release())>>
    : std::true_type {};

// Human readable translation: you needed to be a scoped_refptr if you are a raw
// pointer type and are convertible to a RefCounted(Base|ThreadSafeBase) type.
template <typename T>
struct NeedsScopedRefptrButGetsRawPtr
    : std::conjunction<std::is_pointer<T>,
                       IsRefCountedType<std::remove_pointer_t<T>>> {
  static_assert(!std::is_reference<T>::value,
                "NeedsScopedRefptrButGetsRawPtr requires non-reference type.");
};

}  // namespace cef_internal

}  // namespace base

#endif  // CEF_INCLUDE_BASE_INTERNAL_CEF_RAW_SCOPED_REFPTR_MISMATCH_CHECKER_H_
