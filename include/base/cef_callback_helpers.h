// Copyright (c) 2014 Marshall A. Greenblatt. Portions copyright (c) 2012
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

// This defines helpful methods for dealing with Callbacks.  Because Callbacks
// are implemented using templates, with a class per callback signature, adding
// methods to Callback<> itself is unattractive (lots of extra code gets
// generated).  Instead, consider adding methods here.

#ifndef CEF_INCLUDE_BASE_CEF_CALLBACK_HELPERS_H_
#define CEF_INCLUDE_BASE_CEF_CALLBACK_HELPERS_H_
#pragma once

#if defined(USING_CHROMIUM_INCLUDES)
// When building CEF include the Chromium header directly.
#include "base/callback_helpers.h"
#else  // !USING_CHROMIUM_INCLUDES
// The following is substantially similar to the Chromium implementation.
// If the Chromium implementation diverges the below implementation should be
// updated to match.

#include <atomic>
#include <memory>
#include <type_traits>
#include <utility>

#include "include/base/cef_bind.h"
#include "include/base/cef_callback.h"
#include "include/base/cef_logging.h"

namespace base {

namespace internal {

template <typename T>
struct IsBaseCallbackImpl : std::false_type {};

template <typename R, typename... Args>
struct IsBaseCallbackImpl<OnceCallback<R(Args...)>> : std::true_type {};

template <typename R, typename... Args>
struct IsBaseCallbackImpl<RepeatingCallback<R(Args...)>> : std::true_type {};

template <typename T>
struct IsOnceCallbackImpl : std::false_type {};

template <typename R, typename... Args>
struct IsOnceCallbackImpl<OnceCallback<R(Args...)>> : std::true_type {};

}  // namespace internal

///
/// IsBaseCallback<T>::value is true when T is any of the Closure or Callback
/// family of types.
///
template <typename T>
using IsBaseCallback = internal::IsBaseCallbackImpl<std::decay_t<T>>;

///
/// IsOnceCallback<T>::value is true when T is a OnceClosure or OnceCallback
/// type.
///
template <typename T>
using IsOnceCallback = internal::IsOnceCallbackImpl<std::decay_t<T>>;

///
/// SFINAE friendly enabler allowing to overload methods for both Repeating and
/// OnceCallbacks.
///
/// Usage:
/// <pre>
///   template <template <typename> class CallbackType,
///             ... other template args ...,
///             typename = EnableIfIsBaseCallback<CallbackType>>
///   void DoStuff(CallbackType<...> cb, ...);
/// </pre>
///
template <template <typename> class CallbackType>
using EnableIfIsBaseCallback =
    std::enable_if_t<IsBaseCallback<CallbackType<void()>>::value>;

namespace internal {

template <typename... Args>
class OnceCallbackHolder final {
 public:
  OnceCallbackHolder(OnceCallback<void(Args...)> callback,
                     bool ignore_extra_runs)
      : callback_(std::move(callback)), ignore_extra_runs_(ignore_extra_runs) {
    DCHECK(callback_);
  }
  OnceCallbackHolder(const OnceCallbackHolder&) = delete;
  OnceCallbackHolder& operator=(const OnceCallbackHolder&) = delete;

  void Run(Args... args) {
    if (has_run_.exchange(true)) {
      CHECK(ignore_extra_runs_) << "Both OnceCallbacks returned by "
                                   "base::SplitOnceCallback() were run. "
                                   "At most one of the pair should be run.";
      return;
    }
    DCHECK(callback_);
    std::move(callback_).Run(std::forward<Args>(args)...);
  }

 private:
  volatile std::atomic_bool has_run_{false};
  base::OnceCallback<void(Args...)> callback_;
  const bool ignore_extra_runs_;
};

}  // namespace internal

///
/// Wraps the given OnceCallback into a RepeatingCallback that relays its
/// invocation to the original OnceCallback on the first invocation. The
/// following invocations are just ignored.
///
/// Note that this deliberately subverts the Once/Repeating paradigm of
/// Callbacks but helps ease the migration from old-style Callbacks. Avoid if
/// possible; use if necessary for migration.
///
// TODO(tzik): Remove it. https://crbug.com/730593
template <typename... Args>
RepeatingCallback<void(Args...)> AdaptCallbackForRepeating(
    OnceCallback<void(Args...)> callback) {
  using Helper = internal::OnceCallbackHolder<Args...>;
  return base::BindRepeating(
      &Helper::Run, std::make_unique<Helper>(std::move(callback),
                                             /*ignore_extra_runs=*/true));
}

///
/// Wraps the given OnceCallback and returns two OnceCallbacks with an identical
/// signature. On first invokation of either returned callbacks, the original
/// callback is invoked. Invoking the remaining callback results in a crash.
///
template <typename... Args>
std::pair<OnceCallback<void(Args...)>, OnceCallback<void(Args...)>>
SplitOnceCallback(OnceCallback<void(Args...)> callback) {
  using Helper = internal::OnceCallbackHolder<Args...>;
  auto wrapped_once = base::BindRepeating(
      &Helper::Run, std::make_unique<Helper>(std::move(callback),
                                             /*ignore_extra_runs=*/false));
  return std::make_pair(wrapped_once, wrapped_once);
}

///
/// ScopedClosureRunner is akin to std::unique_ptr<> for Closures. It ensures
/// that the Closure is executed no matter how the current scope exits.
/// If you are looking for "ScopedCallback", "CallbackRunner", or
/// "CallbackScoper" this is the class you want.
///
class ScopedClosureRunner {
 public:
  ScopedClosureRunner();
  explicit ScopedClosureRunner(OnceClosure closure);
  ScopedClosureRunner(ScopedClosureRunner&& other);
  // Runs the current closure if it's set, then replaces it with the closure
  // from |other|. This is akin to how unique_ptr frees the contained pointer in
  // its move assignment operator. If you need to explicitly avoid running any
  // current closure, use ReplaceClosure().
  ScopedClosureRunner& operator=(ScopedClosureRunner&& other);
  ~ScopedClosureRunner();

  explicit operator bool() const { return !!closure_; }

  // Calls the current closure and resets it, so it wont be called again.
  void RunAndReset();

  // Replaces closure with the new one releasing the old one without calling it.
  void ReplaceClosure(OnceClosure closure);

  // Releases the Closure without calling.
  [[nodiscard]] OnceClosure Release();

 private:
  OnceClosure closure_;
};

///
/// Creates a null callback.
///
class NullCallback {
 public:
  template <typename R, typename... Args>
  operator RepeatingCallback<R(Args...)>() const {
    return RepeatingCallback<R(Args...)>();
  }
  template <typename R, typename... Args>
  operator OnceCallback<R(Args...)>() const {
    return OnceCallback<R(Args...)>();
  }
};

///
/// Creates a callback that does nothing when called.
///
class DoNothing {
 public:
  template <typename... Args>
  operator RepeatingCallback<void(Args...)>() const {
    return Repeatedly<Args...>();
  }
  template <typename... Args>
  operator OnceCallback<void(Args...)>() const {
    return Once<Args...>();
  }
  // Explicit way of specifying a specific callback type when the compiler can't
  // deduce it.
  template <typename... Args>
  static RepeatingCallback<void(Args...)> Repeatedly() {
    return BindRepeating([](Args... args) {});
  }
  template <typename... Args>
  static OnceCallback<void(Args...)> Once() {
    return BindOnce([](Args... args) {});
  }
};

///
/// Useful for creating a Closure that will delete a pointer when invoked. Only
/// use this when necessary. In most cases MessageLoop::DeleteSoon() is a better
/// fit.
///
template <typename T>
void DeletePointer(T* obj) {
  delete obj;
}

}  // namespace base

#endif  // !USING_CHROMIUM_INCLUDES

#endif  // CEF_INCLUDE_BASE_CEF_CALLBACK_HELPERS_H_
