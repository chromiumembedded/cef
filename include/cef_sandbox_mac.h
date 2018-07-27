// Copyright (c) 2018 Marshall A. Greenblatt. All rights reserved.
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

#ifndef CEF_INCLUDE_CEF_SANDBOX_MAC_H_
#define CEF_INCLUDE_CEF_SANDBOX_MAC_H_
#pragma once

#include "include/base/cef_build.h"

#if defined(OS_MACOSX)

#ifdef __cplusplus
extern "C" {
#endif

// The sandbox is used to restrict sub-processes (renderer, plugin, GPU, etc)
// from directly accessing system resources. This helps to protect the user
// from untrusted and potentially malicious Web content.
// See http://www.chromium.org/developers/design-documents/sandbox for
// complete details.
//
// To enable the sandbox on macOS the following requirements must be met:
// 1. Link the helper process executable with the cef_sandbox static library.
// 2. Call the cef_sandbox_initialize() function at the beginning of the
//    helper executable main() function and before loading the CEF framework
//    library. See include/wrapper/cef_library_loader.h for example usage.

///
// Initialize the sandbox for this process. Returns the sandbox context
// handle on success or NULL on failure. The returned handle should be
// passed to cef_sandbox_destroy() immediately before process termination.
///
void* cef_sandbox_initialize(int argc, char** argv);

///
// Destroy the specified sandbox context handle.
///
void cef_sandbox_destroy(void* sandbox_context);

#ifdef __cplusplus
}

///
// Scoped helper for managing the life span of a sandbox context handle.
///
class CefScopedSandboxContext {
 public:
  CefScopedSandboxContext();
  ~CefScopedSandboxContext();

  // Load the sandbox for this process. Returns true on success.
  bool Initialize(int argc, char** argv);

 private:
  void* sandbox_context_;
};
#endif  // __cplusplus

#endif  // defined(OS_MACOSX)

#endif  // CEF_INCLUDE_CEF_SANDBOX_MAC_H_
