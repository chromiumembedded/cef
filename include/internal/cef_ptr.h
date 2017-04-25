// Copyright (c) 2014 Marshall A. Greenblatt. All rights reserved.
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


#ifndef CEF_INCLUDE_INTERNAL_CEF_PTR_H_
#define CEF_INCLUDE_INTERNAL_CEF_PTR_H_
#pragma once

#include "include/base/cef_build.h"
#include "include/base/cef_ref_counted.h"

#if defined(USING_CHROMIUM_INCLUDES)
#include <memory>  // For std::unique_ptr.
#else
#include "include/base/cef_scoped_ptr.h"
#endif

///
// Smart pointer implementation that is an alias of scoped_refptr from
// include/base/cef_ref_counted.h.
// <p>
// A smart pointer class for reference counted objects.  Use this class instead
// of calling AddRef and Release manually on a reference counted object to
// avoid common memory leaks caused by forgetting to Release an object
// reference.  Sample usage:
// <pre>
//   class MyFoo : public CefBaseRefCounted {
//    ...
//   };
//
//   void some_function() {
//     // The MyFoo object that |foo| represents starts with a single
//     // reference.
//     CefRefPtr&lt;MyFoo&gt; foo = new MyFoo();
//     foo-&gt;Method(param);
//     // |foo| is released when this function returns
//   }
//
//   void some_other_function() {
//     CefRefPtr&lt;MyFoo&gt; foo = new MyFoo();
//     ...
//     foo = NULL;  // explicitly releases |foo|
//     ...
//     if (foo)
//       foo-&gt;Method(param);
//   }
// </pre>
// The above examples show how CefRefPtr&lt;T&gt; acts like a pointer to T.
// Given two CefRefPtr&lt;T&gt; classes, it is also possible to exchange
// references between the two objects, like so:
// <pre>
//   {
//     CefRefPtr&lt;MyFoo&gt; a = new MyFoo();
//     CefRefPtr&lt;MyFoo&gt; b;
//
//     b.swap(a);
//     // now, |b| references the MyFoo object, and |a| references NULL.
//   }
// </pre>
// To make both |a| and |b| in the above example reference the same MyFoo
// object, simply use the assignment operator:
// <pre>
//   {
//     CefRefPtr&lt;MyFoo&gt; a = new MyFoo();
//     CefRefPtr&lt;MyFoo&gt; b;
//
//     b = a;
//     // now, |a| and |b| each own a reference to the same MyFoo object.
//     // the reference count of the underlying MyFoo object will be 2.
//   }
// </pre>
// Reference counted objects can also be passed as function parameters and
// used as function return values:
// <pre>
//   void some_func_with_param(CefRefPtr&lt;MyFoo&gt; param) {
//     // A reference is added to the MyFoo object that |param| represents
//     // during the scope of some_func_with_param() and released when
//     // some_func_with_param() goes out of scope.
//   }
//
//   CefRefPtr&lt;MyFoo&gt; some_func_with_retval() {
//     // The MyFoo object that |foox| represents starts with a single
//     // reference.
//     CefRefPtr&lt;MyFoo&gt; foox = new MyFoo();
//
//     // Creating the return value adds an additional reference.
//     return foox;
//
//     // When some_func_with_retval() goes out of scope the original |foox|
//     // reference is released.
//   }
//
//   void and_another_function() {
//     CefRefPtr&lt;MyFoo&gt; foo = new MyFoo();
//
//     // pass |foo| as a parameter.
//     some_function(foo);
//
//     CefRefPtr&lt;MyFoo&gt; foo2 = some_func_with_retval();
//     // Now, since we kept a reference to the some_func_with_retval() return
//     // value, |foo2| is the only class pointing to the MyFoo object created
//     in some_func_with_retval(), and it has a reference count of 1.
//
//     some_func_with_retval();
//     // Now, since we didn't keep a reference to the some_func_with_retval()
//     // return value, the MyFoo object created in some_func_with_retval()
//     // will automatically be released.
//   }
// </pre>
// And in standard containers:
// <pre>
//   {
//      // Create a vector that holds MyFoo objects.
//      std::vector&lt;CefRefPtr&lt;MyFoo&gt; &gt; MyFooVec;
//
//     // The MyFoo object that |foo| represents starts with a single
//     // reference.
//     CefRefPtr&lt;MyFoo&gt; foo = new MyFoo();
//
//     // When the MyFoo object is added to |MyFooVec| the reference count
//     // is increased to 2.
//     MyFooVec.push_back(foo);
//   }
// </pre>
// </p>
///
#if defined(HAS_CPP11_TEMPLATE_ALIAS_SUPPORT)
template <class T>
using CefRefPtr = scoped_refptr<T>;
#else
// When template aliases are not supported use a define instead of subclassing
// because it's otherwise hard to get the constructors to behave correctly.
#define CefRefPtr scoped_refptr
#endif


///
// A CefOwnPtr<T> is like a T*, except that the destructor of CefOwnPtr<T>
// automatically deletes the pointer it holds (if any). That is, CefOwnPtr<T>
// owns the T object that it points to. Like a T*, a CefOwnPtr<T> may hold
// either NULL or a pointer to a T object. Also like T*, CefOwnPtr<T> is
// thread-compatible, and once you dereference it, you get the thread safety
// guarantees of T.
///
#if defined(USING_CHROMIUM_INCLUDES)
// Implementation-side code uses std::unique_ptr instead of scoped_ptr.
template <class T, class D = std::default_delete<T>>
using CefOwnPtr = std::unique_ptr<T, D>;
#elif defined(HAS_CPP11_TEMPLATE_ALIAS_SUPPORT)
template <class T, class D = base::DefaultDeleter<T>>
using CefOwnPtr = scoped_ptr<T, D>;
#else
// When template aliases are not supported use a define instead of subclassing
// because it's otherwise hard to get the constructors to behave correctly.
#define CefOwnPtr scoped_ptr
#endif


///
// A CefRawPtr<T> is the same as T*
///
#if defined(HAS_CPP11_TEMPLATE_ALIAS_SUPPORT)
#define CEF_RAW_PTR_GET(r) r
template <class T>
using CefRawPtr = T*;
#else
// Simple wrapper implementation that behaves as much like T* as possible.
// CEF_RAW_PTR_GET is required for VS2008 compatibility (Issue #2155).
#define CEF_RAW_PTR_GET(r) r.get()
template <class T>
class CefRawPtr {
 public:
  CefRawPtr() : ptr_(nullptr) {}
  CefRawPtr(T* p) : ptr_(p) {}
  CefRawPtr(const CefRawPtr& r) : ptr_(r.ptr_) {}

  template <typename U>
  CefRawPtr(const CefRawPtr<U>& r) : ptr_(r.get()) {}

  T* get() const { return ptr_; }

  // Allow CefRawPtr to be used in boolean expression and comparison operations.
  operator T*() const { return ptr_; }

  T* operator->() const {
    assert(ptr_ != NULL);
    return ptr_;
  }

  CefRawPtr<T>& operator=(T* p) {
    ptr_ = p;
    return *this;
  }

  CefRawPtr<T>& operator=(const CefRawPtr<T>& r) {
    return *this = r.ptr_;
  }

  template <typename U>
  CefRawPtr<T>& operator=(const CefRawPtr<U>& r) {
    return *this = r.get();
  }

 private:
  T* ptr_;
};
#endif

#endif  // CEF_INCLUDE_INTERNAL_CEF_PTR_H_
