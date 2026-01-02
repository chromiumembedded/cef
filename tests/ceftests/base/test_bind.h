// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.
//
// ---------------------------------------------------------------------------
//
// This file was ported from Chromium source file:
// base/test/bind.h
//
// ---------------------------------------------------------------------------

#ifndef CEF_TESTS_CEFTESTS_BASE_TEST_BIND_H_
#define CEF_TESTS_CEFTESTS_BASE_TEST_BIND_H_

#include <type_traits>
#include <utility>

#include "include/base/cef_bind.h"

namespace base {

namespace cef_internal {

template <typename Signature, typename R, typename F, typename... Args>
static constexpr bool kHasConstCallOperator = false;
template <typename R, typename F, typename... Args>
static constexpr bool
    kHasConstCallOperator<R (F::*)(Args...) const, R, F, Args...> = true;

// Implementation of `BindLambdaForTesting()`, which checks preconditions before
// handing off to `Bind{Once,Repeating}()`.
template <typename Lambda,
          typename = ExtractCallableRunType<std::decay_t<Lambda>>>
struct BindLambdaForTestingHelper;

template <typename Lambda, typename R, typename... Args>
struct BindLambdaForTestingHelper<Lambda, R(Args...)> {
 private:
  using F = std::decay_t<Lambda>;

  static constexpr bool kIsConstCallOperator =
      kHasConstCallOperator<decltype(&F::operator()), R, F, Args...>;
  static constexpr bool kIsNonConstRvalueRef =
      std::is_rvalue_reference_v<Lambda&&> &&
      !std::is_const_v<std::remove_reference_t<Lambda>>;

  static R Run(const F& f, Args... args) {
    return f(std::forward<Args>(args)...);
  }

  static R RunOnce(F&& f, Args... args) {
    return f(std::forward<Args>(args)...);
  }

 public:
  static auto BindLambdaForTesting(Lambda&& lambda) {
    if constexpr (kIsConstCallOperator) {
      // If blink::BindRepeating is available, and a callback argument is in
      // the blink namespace, then this call is ambiguous without the full
      // namespace path.
      return ::base::BindRepeating(&Run, std::forward<Lambda>(lambda));
    } else {
      // Since a mutable lambda potentially can invalidate its state after being
      // run once, this method returns a `OnceCallback` instead of a
      // `RepeatingCallback`.
      static_assert(
          kIsNonConstRvalueRef,
          "BindLambdaForTesting() requires non-const rvalue for mutable lambda "
          "binding, i.e. base::BindLambdaForTesting(std::move(lambda)).");
      return BindOnce(&RunOnce, std::move(lambda));
    }
  }
};

}  // namespace cef_internal

// A variant of `Bind{Once,Repeating}()` that can bind capturing lambdas for
// testing. This doesn't support extra arguments binding as the lambda itself
// can do.
template <typename Lambda>
auto BindLambdaForTesting(Lambda&& lambda) {
  return cef_internal::BindLambdaForTestingHelper<Lambda>::BindLambdaForTesting(
      std::forward<Lambda>(lambda));
}

}  // namespace base

#endif  // CEF_TESTS_CEFTESTS_BASE_TEST_BIND_H_
