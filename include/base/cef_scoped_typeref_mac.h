// Copyright (c) 2021 Marshall A. Greenblatt. Portions copyright (c) 2013
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

#ifndef CEF_INCLUDE_BASE_CEF_SCOPED_TYPEREF_MAC_H_
#define CEF_INCLUDE_BASE_CEF_SCOPED_TYPEREF_MAC_H_
#pragma once

#if defined(USING_CHROMIUM_INCLUDES)
// When building CEF include the Chromium header directly.
#include "base/mac/scoped_typeref.h"
#else  // !USING_CHROMIUM_INCLUDES
// The following is substantially similar to the Chromium implementation.
// If the Chromium implementation diverges the below implementation should be
// updated to match.

#include "include/base/cef_logging.h"
#include "include/base/internal/cef_scoped_policy.h"

namespace base {

template <typename T>
struct ScopedTypeRefTraits;

///
/// ScopedTypeRef<> is patterned after std::unique_ptr<>, but maintains
/// ownership of a reference to any type that is maintained by Retain and
/// Release methods.
///
/// The Traits structure must provide the Retain and Release methods for type T.
/// A default ScopedTypeRefTraits is used but not defined, and should be defined
/// for each type to use this interface. For example, an appropriate definition
/// of ScopedTypeRefTraits for CGLContextObj would be:
///
/// <pre>
///   template<>
///   struct ScopedTypeRefTraits<CGLContextObj> {
///     static CGLContextObj InvalidValue() { return nullptr; }
///     static CGLContextObj Retain(CGLContextObj object) {
///       CGLContextRetain(object);
///       return object;
///     }
///     static void Release(CGLContextObj object) { CGLContextRelease(object); }
///   };
/// </pre>
///
/// For the many types that have pass-by-pointer create functions, the function
/// InitializeInto() is provided to allow direct initialization and assumption
/// of ownership of the object. For example, continuing to use the above
/// CGLContextObj specialization:
///
/// <pre>
///   base::ScopedTypeRef<CGLContextObj> context;
///   CGLCreateContext(pixel_format, share_group, context.InitializeInto());
/// </pre>
///
/// For initialization with an existing object, the caller may specify whether
/// the ScopedTypeRef<> being initialized is assuming the caller's existing
/// ownership of the object (and should not call Retain in initialization) or if
/// it should not assume this ownership and must create its own (by calling
/// Retain in initialization). This behavior is based on the |policy| parameter,
/// with |ASSUME| for the former and |RETAIN| for the latter. The default policy
/// is to |ASSUME|.
///
template <typename T, typename Traits = ScopedTypeRefTraits<T>>
class ScopedTypeRef {
 public:
  using element_type = T;

  explicit constexpr ScopedTypeRef(
      element_type object = Traits::InvalidValue(),
      base::scoped_policy::OwnershipPolicy policy = base::scoped_policy::ASSUME)
      : object_(object) {
    if (object_ && policy == base::scoped_policy::RETAIN) {
      object_ = Traits::Retain(object_);
    }
  }

  ScopedTypeRef(const ScopedTypeRef<T, Traits>& that) : object_(that.object_) {
    if (object_) {
      object_ = Traits::Retain(object_);
    }
  }

  // This allows passing an object to a function that takes its superclass.
  template <typename R, typename RTraits>
  explicit ScopedTypeRef(const ScopedTypeRef<R, RTraits>& that_as_subclass)
      : object_(that_as_subclass.get()) {
    if (object_) {
      object_ = Traits::Retain(object_);
    }
  }

  ScopedTypeRef(ScopedTypeRef<T, Traits>&& that) : object_(that.object_) {
    that.object_ = Traits::InvalidValue();
  }

  ~ScopedTypeRef() {
    if (object_) {
      Traits::Release(object_);
    }
  }

  ScopedTypeRef& operator=(const ScopedTypeRef<T, Traits>& that) {
    reset(that.get(), base::scoped_policy::RETAIN);
    return *this;
  }

  // This is to be used only to take ownership of objects that are created
  // by pass-by-pointer create functions. To enforce this, require that the
  // object be reset to NULL before this may be used.
  [[nodiscard]] element_type* InitializeInto() {
    DCHECK(!object_);
    return &object_;
  }

  void reset(const ScopedTypeRef<T, Traits>& that) {
    reset(that.get(), base::scoped_policy::RETAIN);
  }

  void reset(element_type object = Traits::InvalidValue(),
             base::scoped_policy::OwnershipPolicy policy =
                 base::scoped_policy::ASSUME) {
    if (object && policy == base::scoped_policy::RETAIN) {
      object = Traits::Retain(object);
    }
    if (object_) {
      Traits::Release(object_);
    }
    object_ = object;
  }

  bool operator==(const element_type& that) const { return object_ == that; }

  bool operator!=(const element_type& that) const { return object_ != that; }

  operator element_type() const { return object_; }

  element_type get() const { return object_; }

  void swap(ScopedTypeRef& that) {
    element_type temp = that.object_;
    that.object_ = object_;
    object_ = temp;
  }

  // ScopedTypeRef<>::release() is like std::unique_ptr<>::release.  It is NOT
  // a wrapper for Release().  To force a ScopedTypeRef<> object to call
  // Release(), use ScopedTypeRef<>::reset().
  [[nodiscard]] element_type release() {
    element_type temp = object_;
    object_ = Traits::InvalidValue();
    return temp;
  }

 private:
  element_type object_;
};

}  // namespace base

#endif  // !USING_CHROMIUM_INCLUDES

#endif  // CEF_INCLUDE_BASE_CEF_SCOPED_TYPEREF_MAC_H_
