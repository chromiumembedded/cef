// Copyright (c) 2013 Google Inc. All rights reserved.
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

// Do not include this header file directly. Use base/mac/scoped_block.h
// instead.

#ifndef CEF_INCLUDE_BASE_INTERNAL_CEF_SCOPED_BLOCK_MAC_H_
#define CEF_INCLUDE_BASE_INTERNAL_CEF_SCOPED_BLOCK_MAC_H_

#include <Block.h>

#include "include/base/cef_scoped_typeref_mac.h"

#if defined(__has_feature) && __has_feature(objc_arc)
#error \
    "Cannot include include/base/internal/cef_scoped_block_mac.h in file built with ARC."
#endif

namespace base {
namespace mac {

namespace cef_internal {

template <typename B>
struct ScopedBlockTraits {
  static B InvalidValue() { return nullptr; }
  static B Retain(B block) { return Block_copy(block); }
  static void Release(B block) { Block_release(block); }
};

}  // namespace cef_internal

// ScopedBlock<> is patterned after ScopedCFTypeRef<>, but uses Block_copy() and
// Block_release() instead of CFRetain() and CFRelease().
template <typename B>
using ScopedBlock = ScopedTypeRef<B, cef_internal::ScopedBlockTraits<B>>;

}  // namespace mac
}  // namespace base

#endif  // CEF_INCLUDE_BASE_INTERNAL_CEF_SCOPED_BLOCK_MAC_H_
