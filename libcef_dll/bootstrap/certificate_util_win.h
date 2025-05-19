// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_BOOTSTRAP_CERTIFICATE_UTIL_WIN_H_
#define CEF_LIBCEF_DLL_BOOTSTRAP_CERTIFICATE_UTIL_WIN_H_
#pragma once

#include <windows.h>

#include <string>
#include <vector>

namespace certificate_util {

struct ThumbprintsInfo {
 public:
  bool IsSignedAndValid() const {
    return !valid_thumbprints.empty() && errors.empty();
  }

  bool IsUnsignedOrValid() const {
    return !has_signature || IsSignedAndValid();
  }

  bool IsSame(const ThumbprintsInfo& other, bool allow_unsigned) const {
    if (allow_unsigned && !has_signature && !other.has_signature) {
      return true;
    }

    // Returns true if both are valid and have the same primary thumbprint.
    return IsSignedAndValid() && other.IsSignedAndValid() &&
           valid_thumbprints[0] == other.valid_thumbprints[0];
  }

  // True if a primary signature exists, irrespective of validity.
  bool has_signature = false;

  // Thumbprints for signatures, if any, that passed verification.
  std::vector<std::string> valid_thumbprints;

  // Thumbprints for signatures, if any, that failed verification. Will not be
  // populated if |verify_binary=true| was passed to GetClientThumbprints().
  std::vector<std::string> invalid_thumbprints;

  // Errors (newline delimited) if any signatures failed verification.
  std::wstring errors;
};

// Process client signatures for the binary at |binary_path| and populate
// |info|. If |verify_binary| is true and the primary signature fails
// verification then no further signatures will be processed.
void GetClientThumbprints(const std::wstring& binary_path,
                          bool verify_binary,
                          ThumbprintsInfo& info);

}  // namespace certificate_util

#endif  // CEF_LIBCEF_DLL_BOOTSTRAP_CERTIFICATE_UTIL_WIN_H_
