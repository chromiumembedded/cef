// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_VERSION_H_
#define CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_VERSION_H_

#include <string>

#include "base/version.h"

namespace cef_installer {

// Wrapper around base::Version for CEF-specific version handling.
// CEF versions follow the format "major.minor.patch" (e.g., "137.3.5").
class Version {
 public:
  Version();
  ~Version();

  Version(const Version& other);
  Version& operator=(const Version& other);
  Version(Version&& other) noexcept;
  Version& operator=(Version&& other) noexcept;

  // Parse version string like "137.3.5". Returns invalid version on failure.
  static Version Parse(const std::string& version_str);

  // Convert API version integer (e.g., 14601) to Version (e.g., "146.1").
  static Version FromApiVersion(int api_version);

  // Check validity
  bool IsValid() const;

  // Comparison operators (delegates to base::Version)
  bool operator<(const Version& other) const;
  bool operator<=(const Version& other) const;
  bool operator>(const Version& other) const;
  bool operator>=(const Version& other) const;
  bool operator==(const Version& other) const;
  bool operator!=(const Version& other) const;

  // Get milestone (major version component). E.g., "137.3.5" -> 137
  // Returns 0 if version is invalid.
  int GetMilestone() const;

  // Get string representation
  std::string ToString() const;

  // Check if this version is within [vmin, vmax] range.
  // - vmin is required and inclusive
  // - vmax is optional (empty string means no upper bound)
  // - vmax is inclusive: IsInRange("137.1", "137.99") -> true for "137.3.5"
  // - vmax is a literal bound, NOT a wildcard: "137.99" means <= 137.99
  //   (so "137.100" would be OUT of range if vmax="137.99")
  bool IsInRange(const std::string& vmin, const std::string& vmax) const;

 private:
  explicit Version(base::Version version);

  base::Version version_;
};

}  // namespace cef_installer

#endif  // CEF_LIBCEF_DLL_BOOTSTRAP_INSTALLER_INSTALLER_VERSION_H_
