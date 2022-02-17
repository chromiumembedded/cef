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

#ifndef CEF_INCLUDE_WRAPPER_CEF_LIBRARY_LOADER_H_
#define CEF_INCLUDE_WRAPPER_CEF_LIBRARY_LOADER_H_
#pragma once

#include "include/base/cef_build.h"

#ifdef __cplusplus
#include <string>

extern "C" {
#endif  // __cplusplus

///
// Load the CEF library at the specified |path|. Returns true (1) on
// success and false (0) on failure.
///
int cef_load_library(const char* path);

///
// Unload the CEF library that was previously loaded. Returns true (1)
// on success and false (0) on failure.
///
int cef_unload_library(void);

#ifdef __cplusplus
}

#if defined(OS_MAC)

///
// Scoped helper for loading and unloading the CEF framework library at
// runtime from the expected location in the app bundle. Loading at runtime
// instead of linking directly is a requirement of the macOS sandbox
// implementation.
//
// Example usage in the main process:
//
//   #include "include/wrapper/cef_library_loader.h"
//
//   int main(int argc, char* argv[]) {
//     // Dynamically load the CEF framework library.
//     CefScopedLibraryLoader library_loader;
//     if (!library_loader.LoadInMain())
//       return 1;
//
//     // Continue with CEF initialization...
//   }
//
// Example usage in the helper process:
//
//   #include "include/cef_sandbox_mac.h"
//   #include "include/wrapper/cef_library_loader.h"
//
//   int main(int argc, char* argv[]) {
//     // Initialize the macOS sandbox for this helper process.
//     CefScopedSandboxContext sandbox_context;
//     if (!sandbox_context.Initialize(argc, argv))
//       return 1;
//
//     // Dynamically load the CEF framework library.
//     CefScopedLibraryLoader library_loader;
//     if (!library_loader.LoadInHelper())
//       return 1;
//
//     // Continue with CEF initialization...
//   }
///
class CefScopedLibraryLoader {
 public:
  CefScopedLibraryLoader();

  CefScopedLibraryLoader(const CefScopedLibraryLoader&) = delete;
  CefScopedLibraryLoader& operator=(const CefScopedLibraryLoader&) = delete;

  ~CefScopedLibraryLoader();

  ///
  // Load the CEF framework in the main process from the expected app
  // bundle location relative to the executable. Returns true if the
  // load succeeds.
  ///
  bool LoadInMain() { return Load(false); }

  ///
  // Load the CEF framework in the helper process from the expected app
  // bundle location relative to the executable. Returns true if the
  // load succeeds.
  ///
  bool LoadInHelper() { return Load(true); }

 private:
  bool Load(bool helper);

  bool loaded_;
};

#endif  // defined(OS_MAC)
#endif  // __cplusplus

#endif  // CEF_INCLUDE_WRAPPER_CEF_LIBRARY_LOADER_H_
