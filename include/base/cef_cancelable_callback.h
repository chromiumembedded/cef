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

// CancelableCallback is a wrapper around base::Callback that allows
// cancellation of a callback. CancelableCallback takes a reference on the
// wrapped callback until this object is destroyed or Reset()/Cancel() are
// called.
//
// NOTE:
//
// Calling CancelableCallback::Cancel() brings the object back to its natural,
// default-constructed state, i.e., CancelableCallback::callback() will return
// a null callback.
//
// THREAD-SAFETY:
//
// CancelableCallback objects must be created on, posted to, cancelled on, and
// destroyed on the same thread.
//
//
// EXAMPLE USAGE:
//
// In the following example, the test is verifying that RunIntensiveTest()
// Quit()s the message loop within 4 seconds. The cancelable callback is posted
// to the message loop, the intensive test runs, the message loop is run,
// then the callback is cancelled.
//
// RunLoop run_loop;
//
// void TimeoutCallback(const std::string& timeout_message) {
//   FAIL() << timeout_message;
//   run_loop.QuitWhenIdle();
// }
//
// CancelableOnceClosure timeout(
//     base::BindOnce(&TimeoutCallback, "Test timed out."));
// ThreadTaskRunnerHandle::Get()->PostDelayedTask(FROM_HERE, timeout.callback(),
//                                                TimeDelta::FromSeconds(4));
// RunIntensiveTest();
// run_loop.Run();
// timeout.Cancel();  // Hopefully this is hit before the timeout callback runs.

#ifndef CEF_INCLUDE_BASE_CEF_CANCELABLE_CALLBACK_H_
#define CEF_INCLUDE_BASE_CEF_CANCELABLE_CALLBACK_H_
#pragma once

#if defined(USING_CHROMIUM_INCLUDES)
// When building CEF include the Chromium header directly.
#include "base/cancelable_callback.h"
#else  // !USING_CHROMIUM_INCLUDES
// The following is substantially similar to the Chromium implementation.
// If the Chromium implementation diverges the below implementation should be
// updated to match.

#include <utility>

#include "include/base/cef_bind.h"
#include "include/base/cef_callback.h"
#include "include/base/cef_compiler_specific.h"
#include "include/base/cef_logging.h"
#include "include/base/cef_weak_ptr.h"
#include "include/base/internal/cef_callback_internal.h"

namespace base {
namespace internal {

template <typename CallbackType>
class CancelableCallbackImpl {
 public:
  CancelableCallbackImpl() = default;
  CancelableCallbackImpl(const CancelableCallbackImpl&) = delete;
  CancelableCallbackImpl& operator=(const CancelableCallbackImpl&) = delete;

  // |callback| must not be null.
  explicit CancelableCallbackImpl(CallbackType callback)
      : callback_(std::move(callback)) {
    DCHECK(callback_);
  }

  ~CancelableCallbackImpl() = default;

  // Cancels and drops the reference to the wrapped callback.
  void Cancel() {
    weak_ptr_factory_.InvalidateWeakPtrs();
    callback_.Reset();
  }

  // Returns true if the wrapped callback has been cancelled.
  bool IsCancelled() const { return callback_.is_null(); }

  // Sets |callback| as the closure that may be cancelled. |callback| may not
  // be null. Outstanding and any previously wrapped callbacks are cancelled.
  void Reset(CallbackType callback) {
    DCHECK(callback);
    // Outstanding tasks (e.g., posted to a message loop) must not be called.
    Cancel();
    callback_ = std::move(callback);
  }

  // Returns a callback that can be disabled by calling Cancel().
  CallbackType callback() const {
    if (!callback_)
      return CallbackType();
    CallbackType forwarder;
    MakeForwarder(&forwarder);
    return forwarder;
  }

 private:
  template <typename... Args>
  void MakeForwarder(RepeatingCallback<void(Args...)>* out) const {
    using ForwarderType = void (CancelableCallbackImpl::*)(Args...);
    ForwarderType forwarder = &CancelableCallbackImpl::ForwardRepeating;
    *out = BindRepeating(forwarder, weak_ptr_factory_.GetWeakPtr());
  }

  template <typename... Args>
  void MakeForwarder(OnceCallback<void(Args...)>* out) const {
    using ForwarderType = void (CancelableCallbackImpl::*)(Args...);
    ForwarderType forwarder = &CancelableCallbackImpl::ForwardOnce;
    *out = BindOnce(forwarder, weak_ptr_factory_.GetWeakPtr());
  }

  template <typename... Args>
  void ForwardRepeating(Args... args) {
    callback_.Run(std::forward<Args>(args)...);
  }

  template <typename... Args>
  void ForwardOnce(Args... args) {
    weak_ptr_factory_.InvalidateWeakPtrs();
    std::move(callback_).Run(std::forward<Args>(args)...);
  }

  // The stored closure that may be cancelled.
  CallbackType callback_;
  mutable base::WeakPtrFactory<CancelableCallbackImpl> weak_ptr_factory_{this};
};

}  // namespace internal

// Consider using base::WeakPtr directly instead of base::CancelableCallback for
// the task cancellation.
template <typename Signature>
using CancelableOnceCallback =
    internal::CancelableCallbackImpl<OnceCallback<Signature>>;
using CancelableOnceClosure = CancelableOnceCallback<void()>;

template <typename Signature>
using CancelableRepeatingCallback =
    internal::CancelableCallbackImpl<RepeatingCallback<Signature>>;
using CancelableRepeatingClosure = CancelableRepeatingCallback<void()>;

}  // namespace base

#endif  // !USING_CHROMIUM_INCLUDES

#endif  // CEF_INCLUDE_BASE_CEF_CANCELABLE_CALLBACK_H_
