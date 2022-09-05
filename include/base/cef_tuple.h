// Copyright (c) 2014 Marshall A. Greenblatt. Portions copyright (c) 2011
// Google Inc. All rights reserved.
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

///
/// \file
/// Use std::tuple as tuple type. This file contains helper functions for
/// working with std::tuples.
/// The functions DispatchToMethod and DispatchToFunction take a function
/// pointer or instance and method pointer, and unpack a tuple into arguments to
/// the call.
///
/// Example usage:
/// <pre>
///   // These two methods of creating a Tuple are identical.
///   std::tuple<int, const char*> tuple_a(1, "wee");
///   std::tuple<int, const char*> tuple_b = std::make_tuple(1, "wee");
///
///   void SomeFunc(int a, const char* b) { }
///   DispatchToFunction(&SomeFunc, tuple_a);  // SomeFunc(1, "wee")
///   DispatchToFunction(
///       &SomeFunc, std::make_tuple(10, "foo"));    // SomeFunc(10, "foo")
///
///   struct { void SomeMeth(int a, int b, int c) { } } foo;
///   DispatchToMethod(&foo, &Foo::SomeMeth, std::make_tuple(1, 2, 3));
///   // foo->SomeMeth(1, 2, 3);
/// </pre>
///

#ifndef CEF_INCLUDE_BASE_CEF_TUPLE_H_
#define CEF_INCLUDE_BASE_CEF_TUPLE_H_
#pragma once

#if defined(USING_CHROMIUM_INCLUDES)
// When building CEF include the Chromium header directly.
#include "base/tuple.h"
#else  // !USING_CHROMIUM_INCLUDES
// The following is substantially similar to the Chromium implementation.
// If the Chromium implementation diverges the below implementation should be
// updated to match.

#include <stddef.h>
#include <tuple>
#include <utility>

#include "include/base/cef_build.h"

namespace base {

// Dispatchers ----------------------------------------------------------------
//
// Helper functions that call the given method on an object, with the unpacked
// tuple arguments.  Notice that they all have the same number of arguments,
// so you need only write:
//   DispatchToMethod(object, &Object::method, args);
// This is very useful for templated dispatchers, since they don't need to know
// what type |args| is.

// Non-Static Dispatchers with no out params.

template <typename ObjT, typename Method, typename Tuple, size_t... Ns>
inline void DispatchToMethodImpl(const ObjT& obj,
                                 Method method,
                                 Tuple&& args,
                                 std::index_sequence<Ns...>) {
  (obj->*method)(std::get<Ns>(std::forward<Tuple>(args))...);
}

template <typename ObjT, typename Method, typename Tuple>
inline void DispatchToMethod(const ObjT& obj, Method method, Tuple&& args) {
  constexpr size_t size = std::tuple_size<std::decay_t<Tuple>>::value;
  DispatchToMethodImpl(obj, method, std::forward<Tuple>(args),
                       std::make_index_sequence<size>());
}

// Static Dispatchers with no out params.

template <typename Function, typename Tuple, size_t... Ns>
inline void DispatchToFunctionImpl(Function function,
                                   Tuple&& args,
                                   std::index_sequence<Ns...>) {
  (*function)(std::get<Ns>(std::forward<Tuple>(args))...);
}

template <typename Function, typename Tuple>
inline void DispatchToFunction(Function function, Tuple&& args) {
  constexpr size_t size = std::tuple_size<std::decay_t<Tuple>>::value;
  DispatchToFunctionImpl(function, std::forward<Tuple>(args),
                         std::make_index_sequence<size>());
}

// Dispatchers with out parameters.

template <typename ObjT,
          typename Method,
          typename InTuple,
          typename OutTuple,
          size_t... InNs,
          size_t... OutNs>
inline void DispatchToMethodImpl(const ObjT& obj,
                                 Method method,
                                 InTuple&& in,
                                 OutTuple* out,
                                 std::index_sequence<InNs...>,
                                 std::index_sequence<OutNs...>) {
  (obj->*method)(std::get<InNs>(std::forward<InTuple>(in))...,
                 &std::get<OutNs>(*out)...);
}

template <typename ObjT, typename Method, typename InTuple, typename OutTuple>
inline void DispatchToMethod(const ObjT& obj,
                             Method method,
                             InTuple&& in,
                             OutTuple* out) {
  constexpr size_t in_size = std::tuple_size<std::decay_t<InTuple>>::value;
  constexpr size_t out_size = std::tuple_size<OutTuple>::value;
  DispatchToMethodImpl(obj, method, std::forward<InTuple>(in), out,
                       std::make_index_sequence<in_size>(),
                       std::make_index_sequence<out_size>());
}

}  // namespace base

#endif  // !USING_CHROMIUM_INCLUDES

#endif  // CEF_INCLUDE_BASE_CEF_TUPLE_H_
