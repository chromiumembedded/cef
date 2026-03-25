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

#ifndef CEF_INCLUDE_CEF_AUTH_VAULT_H_
#define CEF_INCLUDE_CEF_AUTH_VAULT_H_
#pragma once

#include <vector>

#include "include/cef_base.h"
#include "include/cef_values.h"

class CefAuthProfileVisitor;
class CefAuthVaultActionCallback;
class CefAuthVaultReadCallback;

///
/// Implemented by the client to receive authentication profile metadata during
/// asynchronous enumeration.
///
/*--cef(source=client)--*/
class CefAuthProfileVisitor : public virtual CefBaseRefCounted {
 public:
  ///
  /// Method that will be called once for each available profile. |count| is
  /// the 0-based index for the current profile and |total| is the total number
  /// of profiles. Return false to stop visiting profiles.
  ///
  /*--cef()--*/
  virtual bool Visit(const CefAuthProfile& profile, int count, int total) = 0;
};

///
/// Callback interface for CefAuthVault write/delete operations.
///
/*--cef(source=client)--*/
class CefAuthVaultActionCallback : public virtual CefBaseRefCounted {
 public:
  ///
  /// Method that will be called upon completion. |success| will be true if the
  /// requested operation completed successfully. |path| is the affected vault
  /// file path when available. |error| will contain a description if the
  /// operation failed.
  ///
  /*--cef(optional_param=path,optional_param=error)--*/
  virtual void OnComplete(bool success,
                          const CefString& path,
                          const CefString& error) = 0;
};

///
/// Callback interface for CefAuthVault read operations.
///
/*--cef(source=client)--*/
class CefAuthVaultReadCallback : public virtual CefBaseRefCounted {
 public:
  ///
  /// Method that will be called upon completion. |success| will be true if the
  /// requested profile was read successfully. |profile| will be nullptr on
  /// failure. |error| will contain a description if the operation failed.
  ///
  /*--cef(optional_param=profile,optional_param=error)--*/
  virtual void OnComplete(bool success,
                          CefRefPtr<CefDictionaryValue> profile,
                          const CefString& error) = 0;
};

///
/// Global service for managing encrypted authentication profile metadata.
/// Methods may be called on any browser process thread unless otherwise
/// indicated.
///
/*--cef(source=library)--*/
class CefAuthVault : public virtual CefBaseRefCounted {
 public:
  ///
  /// Returns the global auth vault service.
  ///
  /*--cef()--*/
  static CefRefPtr<CefAuthVault> GetGlobalVault();

  ///
  /// Returns the default directory where authentication profiles are stored.
  ///
  /*--cef()--*/
  virtual CefString GetVaultPath() = 0;

  ///
  /// Returns the default encryption key path used by this vault.
  ///
  /*--cef()--*/
  virtual CefString GetEncryptionKeyPath() = 0;

  ///
  /// Asynchronously saves an authentication profile. |profile| should contain
  /// metadata such as "name", "url", "username", optional selector values, and
  /// optional timestamp fields. The dictionary contents will be copied.
  ///
  /*--cef(optional_param=callback)--*/
  virtual void SaveProfile(CefRefPtr<CefDictionaryValue> profile,
                           CefRefPtr<CefAuthVaultActionCallback> callback) = 0;

  ///
  /// Asynchronously reads an authentication profile by |name|.
  ///
  /*--cef(optional_param=callback)--*/
  virtual void ReadProfile(const CefString& name,
                           CefRefPtr<CefAuthVaultReadCallback> callback) = 0;

  ///
  /// Asynchronously deletes an authentication profile by |name|.
  ///
  /*--cef(optional_param=callback)--*/
  virtual void DeleteProfile(
      const CefString& name,
      CefRefPtr<CefAuthVaultActionCallback> callback) = 0;

  ///
  /// Asynchronously visits all available authentication profile metadata.
  ///
  /*--cef()--*/
  virtual void VisitProfiles(CefRefPtr<CefAuthProfileVisitor> visitor) = 0;
};

#endif  // CEF_INCLUDE_CEF_AUTH_VAULT_H_
