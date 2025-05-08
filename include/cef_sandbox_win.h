// Copyright (c) 2013 Marshall A. Greenblatt. All rights reserved.
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

#ifndef CEF_INCLUDE_CEF_SANDBOX_WIN_H_
#define CEF_INCLUDE_CEF_SANDBOX_WIN_H_
#pragma once

#if !defined(GENERATING_CEF_API_HASH)
#include "include/base/cef_build.h"
#endif

#if defined(OS_WIN)

#if !defined(GENERATING_CEF_API_HASH)
#include <windows.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

///
/// \file
/// The sandbox is used to restrict sub-processes (renderer, GPU, etc) from
/// directly accessing system resources. This helps to protect the user from
/// untrusted and potentially malicious Web content. See
/// http://www.chromium.org/developers/design-documents/sandbox for complete
/// details.
///
/// To enable the sandbox on Windows the same executable must be used for all
/// processes (browser process and sub-processes). This executable must link the
/// cef_sandbox static library and initialize the sandbox by calling
/// cef_sandbox_info_create. The resulting |sandbox_info| value must then be
/// passed to CefExecuteProcess and CefInitialize.
///
/// Beginning with M138 the cef_sandbox static library can only be linked with
/// applications built as part of the CEF/Chromium build. This is due to
/// unavoidable dependencies on Chromium's bundled Clang/LLVM/libc++ toolchain.
/// Client applications therefore have 3 options for sandbox integration:
///
/// 1. Build the client application (or a custom bootstrap executable) as part
///    of the CEF/Chromium build using Chromium's bundled Clang/LLVM/libc++
///    toolchain. For details of this option see
///    https://bitbucket.org/chromiumembedded/cef/wiki/SandboxSetup.md
/// 2. Build the client application as a DLL using any toolchain and run using
///    the provided bootstrap.exe or bootstrapc.exe. The DLL implements
///    RunWinMain or RunConsoleMain respectively and gets passed the
///    |sandbox_info| parameter which it then forwards to CefExecuteProcess
///    and CefInitialize. The provided bootstrap executables can optionally be
///    renamed or modified [1] to meet client branding needs.
/// 3. Build the client application as an executable using any toolchain with
///    the sandbox disabled. Pass nullptr as the |sandbox_info| parameter to
///    CefExecuteProcess and CefInitialize.
///
/// [1] Embedded executable resources such as icons and file properties can be
///     modified using Visual Studio or Resource Hacker tools. Be sure to code
///     sign all binaries after modification and before distribution to users.
///

///
/// Create the sandbox information object for this process. It is safe to create
/// multiple of this object and to destroy the object immediately after passing
/// into the CefExecuteProcess() and/or CefInitialize() functions.
///
void* cef_sandbox_info_create(void);

///
/// Destroy the specified sandbox information object.
///
void cef_sandbox_info_destroy(void* sandbox_info);

#ifdef __cplusplus
}

///
/// Manages the life span of a sandbox information object.
///
class CefScopedSandboxInfo {
 public:
  CefScopedSandboxInfo() { sandbox_info_ = cef_sandbox_info_create(); }
  ~CefScopedSandboxInfo() { cef_sandbox_info_destroy(sandbox_info_); }

  void* sandbox_info() const { return sandbox_info_; }

 private:
  void* sandbox_info_;
};
#endif  // __cplusplus

#if defined(CEF_BUILD_BOOTSTRAP)
#define CEF_BOOTSTRAP_EXPORT __declspec(dllimport)
#else
#define CEF_BOOTSTRAP_EXPORT __declspec(dllexport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

///
/// Entry point to be implemented by client DLLs using bootstrap.exe for
/// windows (/SUBSYSTEM:WINDOWS) applications.
///
CEF_BOOTSTRAP_EXPORT int RunWinMain(HINSTANCE hInstance,
                                    LPTSTR lpCmdLine,
                                    int nCmdShow,
                                    void* sandbox_info);

///
/// Entry point to be implemented by client DLLs using bootstrapc.exe for
/// console (/SUBSYSTEM:CONSOLE) applications.
///
CEF_BOOTSTRAP_EXPORT int RunConsoleMain(int argc,
                                        char* argv[],
                                        void* sandbox_info);

#ifdef __cplusplus
}
#endif

#endif  // defined(OS_WIN)

#endif  // CEF_INCLUDE_CEF_SANDBOX_WIN_H_
