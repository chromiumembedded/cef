// Copyright (c) 2022 Marshall A. Greenblatt. All rights reserved.
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

#ifndef CEF_INCLUDE_CEF_PREFERENCE_H_
#define CEF_INCLUDE_CEF_PREFERENCE_H_
#pragma once

#include <vector>

#include "include/cef_base.h"
#include "include/cef_values.h"

///
/// Class that manages custom preference registrations.
///
/*--cef(source=library)--*/
class CefPreferenceRegistrar : public CefBaseScoped {
 public:
  ///
  /// Register a preference with the specified |name| and |default_value|. To
  /// avoid conflicts with built-in preferences the |name| value should contain
  /// an application-specific prefix followed by a period (e.g. "myapp.value").
  /// The contents of |default_value| will be copied. The data type for the
  /// preference will be inferred from |default_value|'s type and cannot be
  /// changed after registration. Returns true on success. Returns false if
  /// |name| is already registered or if |default_value| has an invalid type.
  /// This method must be called from within the scope of the
  /// CefBrowserProcessHandler::OnRegisterCustomPreferences callback.
  ///
  /*--cef()--*/
  virtual bool AddPreference(const CefString& name,
                             CefRefPtr<CefValue> default_value) = 0;
};

///
/// Manage access to preferences. Many built-in preferences are registered by
/// Chromium. Custom preferences can be registered in
/// CefBrowserProcessHandler::OnRegisterCustomPreferences.
///
/*--cef(source=library,no_debugct_check)--*/
class CefPreferenceManager : public virtual CefBaseRefCounted {
 public:
  ///
  /// Returns the global preference manager object.
  ///
  /*--cef()--*/
  static CefRefPtr<CefPreferenceManager> GetGlobalPreferenceManager();

  ///
  /// Returns true if a preference with the specified |name| exists. This method
  /// must be called on the browser process UI thread.
  ///
  /*--cef()--*/
  virtual bool HasPreference(const CefString& name) = 0;

  ///
  /// Returns the value for the preference with the specified |name|. Returns
  /// NULL if the preference does not exist. The returned object contains a copy
  /// of the underlying preference value and modifications to the returned
  /// object will not modify the underlying preference value. This method must
  /// be called on the browser process UI thread.
  ///
  /*--cef()--*/
  virtual CefRefPtr<CefValue> GetPreference(const CefString& name) = 0;

  ///
  /// Returns all preferences as a dictionary. If |include_defaults| is true
  /// then preferences currently at their default value will be included. The
  /// returned object contains a copy of the underlying preference values and
  /// modifications to the returned object will not modify the underlying
  /// preference values. This method must be called on the browser process UI
  /// thread.
  ///
  /*--cef()--*/
  virtual CefRefPtr<CefDictionaryValue> GetAllPreferences(
      bool include_defaults) = 0;

  ///
  /// Returns true if the preference with the specified |name| can be modified
  /// using SetPreference. As one example preferences set via the command-line
  /// usually cannot be modified. This method must be called on the browser
  /// process UI thread.
  ///
  /*--cef()--*/
  virtual bool CanSetPreference(const CefString& name) = 0;

  ///
  /// Set the |value| associated with preference |name|. Returns true if the
  /// value is set successfully and false otherwise. If |value| is NULL the
  /// preference will be restored to its default value. If setting the
  /// preference fails then |error| will be populated with a detailed
  /// description of the problem. This method must be called on the browser
  /// process UI thread.
  ///
  /*--cef(optional_param=value)--*/
  virtual bool SetPreference(const CefString& name,
                             CefRefPtr<CefValue> value,
                             CefString& error) = 0;
};

#endif  // CEF_INCLUDE_CEF_PREFERENCE_H_
