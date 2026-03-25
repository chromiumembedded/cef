// Copyright (c) 2026 Marshall A. Greenblatt. All rights reserved.
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
//
// ---------------------------------------------------------------------------
//
// The contents of this file must follow a specific format in order to
// support the CEF translator tool. See the translator.README.txt file in the
// tools directory for more information.
//

#ifndef CEF_INCLUDE_CEF_BROWSER_SECURITY_H_
#define CEF_INCLUDE_CEF_BROWSER_SECURITY_H_
#pragma once

#include "include/cef_base.h"
#include "include/cef_callback.h"

///
/// Callback interface for CefBrowserSecurityPolicy::SetSettings.
///
/*--cef(source=client)--*/
class CefBrowserSecurityCallback : public virtual CefBaseRefCounted {
 public:
  ///
  /// Called on the UI thread after a security policy operation completes.
  /// |success| will be true if the operation succeeded. |message| may contain
  /// additional implementation-defined details.
  ///
  /*--cef(optional_param=message)--*/
  virtual void OnComplete(bool success, const CefString& message) = 0;
};

///
/// Per-request-context browser security policy settings.
///
/*--cef(source=library)--*/
class CefBrowserSecurityPolicy : public virtual CefBaseRefCounted {
 public:
  ///
  /// Returns a copy of the current security settings.
  ///
  /*--cef()--*/
  virtual CefBrowserSecuritySettings GetSettings() = 0;

  ///
  /// Updates the current security settings. If |callback| is non-NULL it will
  /// be executed asynchronously on the UI thread after completion.
  ///
  /*--cef(optional_param=callback)--*/
  virtual void SetSettings(const CefBrowserSecuritySettings& settings,
                           CefRefPtr<CefBrowserSecurityCallback> callback) = 0;

  ///
  /// Returns the current allowed domain patterns as a comma-separated string.
  ///
  /*--cef()--*/
  virtual CefString GetAllowedDomains() = 0;

  ///
  /// Returns the current confirmation-required action categories as a
  /// comma-separated string.
  ///
  /*--cef()--*/
  virtual CefString GetConfirmActionCategories() = 0;
};

#endif  // CEF_INCLUDE_CEF_BROWSER_SECURITY_H_
