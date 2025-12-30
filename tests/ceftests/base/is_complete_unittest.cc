// Copyright (c) 2025 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ---------------------------------------------------------------------------
//
// This file was ported from Chromium source file:
// base/types/is_complete_unittest.cc
// Chromium commit: 70909488d8cc2
//
// ---------------------------------------------------------------------------

#include "include/base/cef_is_complete.h"

namespace base {

struct CompleteStruct {};
struct IncompleteStruct;
using Function = void();
using FunctionPtr = void (*)();

template <typename T>
struct SpecializedForInt;

template <>
struct SpecializedForInt<int> {};

static_assert(IsComplete<int>);
static_assert(IsComplete<CompleteStruct>);
static_assert(!IsComplete<IncompleteStruct>);
static_assert(IsComplete<Function>);
static_assert(IsComplete<FunctionPtr>);
static_assert(IsComplete<SpecializedForInt<int>>);
static_assert(!IsComplete<SpecializedForInt<float>>);

}  // namespace base
