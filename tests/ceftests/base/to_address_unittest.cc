// Copyright (c) 2025 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ---------------------------------------------------------------------------
//
// This file was ported from Chromium source file:
// base/types/to_address_unittest.cc
// Chromium commit: 70909488d8cc2
//
// ---------------------------------------------------------------------------

#include <array>
#include <memory>
#include <type_traits>
#include <utility>

#include "include/base/cef_to_address.h"

namespace base {
namespace {

// A constant whose definition will fail to compile if `to_address()` is not
// SFINAE-compatible.
template <typename T>
inline constexpr bool kIsPtr = false;

template <typename T>
  requires requires(const T& t) { to_address(t); }
inline constexpr bool kIsPtr<T> =
    std::is_pointer_v<decltype(to_address(std::declval<T>()))>;

struct FancyPointer {
  void* operator->() const { return nullptr; }
};

struct NotPointer {};

enum class EnumClass { kZero, kOne };

// Like `std::to_address()`, `to_address()` should correctly handle things
// that can act like pointers:
// * Raw pointers
static_assert(kIsPtr<int*>);
// * Iterators
static_assert(kIsPtr<decltype(std::declval<std::array<int, 3>>().begin())>);
// * STL-provided smart pointers
static_assert(kIsPtr<std::unique_ptr<int>>);
// * User-defined smart pointers, as long as they have `operator->()`
static_assert(kIsPtr<FancyPointer>);

// Unlike `std::to_address()`, `to_address()` is guaranteed to be
// SFINAE-compatible with things that don't act like pointers:
// * Basic types
static_assert(!kIsPtr<int>);
// * Enum classes
static_assert(!kIsPtr<EnumClass>);
// * Structs without an `operator->()`
static_assert(!kIsPtr<NotPointer>);

}  // namespace
}  // namespace base
