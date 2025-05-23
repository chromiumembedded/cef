// Copyright (c) 2025 Marshall A. Greenblatt. All rights reserved.
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
// The contents of this file are only available to applications that link
// against the libcef_dll_wrapper target.
//

#ifndef CEF_INCLUDE_WRAPPER_CEF_CERTIFICATE_UTIL_WIN_H_
#define CEF_INCLUDE_WRAPPER_CEF_CERTIFICATE_UTIL_WIN_H_
#pragma once

#include <windows.h>

#include <string>
#include <vector>

namespace cef_certificate_util {

// SHA1 upper-case hex encoded = 40 characters.
inline constexpr size_t kThumbprintLength = 40U;

///
/// Structure populated by GetClientThumbprints().
///
struct ThumbprintsInfo {
  ///
  /// True if one or more signatures exist and all are valid.
  ///
  bool IsSignedAndValid() const {
    return !valid_thumbprints.empty() && errors.empty();
  }

  ///
  /// True if unsigned, or if one or more signatures exist and all are valid.
  ///
  bool IsUnsignedOrValid() const {
    return !has_signature || IsSignedAndValid();
  }

  ///
  /// True if this and |other| have the same signature status. If
  /// |allow_unsigned| is true then both may be unsigned. Otherwise, one or more
  /// signatures must exist, all must be valid, and the primary fingerprint must
  /// be the same for both.
  ///
  bool IsSame(const ThumbprintsInfo& other, bool allow_unsigned) const {
    if (allow_unsigned && !has_signature && !other.has_signature) {
      return true;
    }

    return IsSignedAndValid() &&
           other.HasPrimaryThumbprint(valid_thumbprints[0]);
  }

  ///
  /// True if a valid primary signature exists and it matches the specified
  /// |thumbprint|.
  ///
  bool HasPrimaryThumbprint(const std::string& thumbprint) const {
    return IsSignedAndValid() && valid_thumbprints[0] == thumbprint;
  }

  ///
  /// True if a primary signature exists, irrespective of validity.
  ///
  bool has_signature = false;

  ///
  /// Thumbprints for signatures, if any, that passed verification.
  ///
  std::vector<std::string> valid_thumbprints;

  ///
  /// Thumbprints for signatures, if any, that failed verification. Will not be
  /// populated if |verify_binary=true| was passed to GetClientThumbprints().
  ///
  std::vector<std::string> invalid_thumbprints;

  ///
  /// Errors (newline delimited) if any signatures failed verification.
  ///
  std::wstring errors;
};

///
/// Process client signatures for the binary at the specified abolute
/// |binary_path| and populate |info|. If |verify_binary| is true and the
/// primary signature fails verification then no further signatures will be
/// processed. For a code signing example and usage details see
/// https://github.com/chromiumembedded/cef/issues/3824#issuecomment-2892139995
///
void GetClientThumbprints(const std::wstring& binary_path,
                          bool verify_binary,
                          ThumbprintsInfo& info);

///
/// Evaluate the binary at the specified absolute |binary_path| for common
/// requirements and populate |info|. If the binary is code signed then all
/// signatures must be valid. If |thumbprint| is a SHA1 hash (e.g. 40 character
/// upper-case hex-encoded value) then the primary signature must match that
/// thumbprint. If |allow_unsigned| is true and |thumbprint| is nullptr then the
/// binary may be unsigned, otherwise it must be validly signed. Returns true if
/// all requirements are met.
///
bool ValidateCodeSigning(const std::wstring& binary_path,
                         const char* thumbprint,
                         bool allow_unsigned,
                         ThumbprintsInfo& info);

///
/// Same as ValidateCodeSigning, but failures result in a FATAL error and
/// application termination. Optionally populate |info| is validation succeeds.
/// Usage must be protected by cef::logging::ScopedEarlySupport if called prior
/// to libcef loading.
///
void ValidateCodeSigningAssert(const std::wstring& binary_path,
                               const char* thumbprint,
                               bool allow_unsigned,
                               ThumbprintsInfo* info = nullptr);

}  // namespace cef_certificate_util

#endif  // CEF_INCLUDE_WRAPPER_CEF_CERTIFICATE_UTIL_WIN_H_
