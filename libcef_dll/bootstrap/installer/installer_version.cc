// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_version.h"

#include <utility>

namespace cef_installer {

Version::Version() = default;
Version::~Version() = default;

Version::Version(const Version& other) = default;
Version& Version::operator=(const Version& other) = default;
Version::Version(Version&& other) noexcept = default;
Version& Version::operator=(Version&& other) noexcept = default;

Version::Version(base::Version version) : version_(std::move(version)) {}

// static
Version Version::Parse(const std::string& version_str) {
  base::Version parsed(version_str);
  // base::Version supports any number of components, but CEF uses at most 4.
  // Reject versions with more than 4 components or invalid formats.
  if (!parsed.IsValid() || parsed.components().size() > 4) {
    return Version();
  }
  return Version(std::move(parsed));
}

// static
Version Version::FromApiVersion(int api_version) {
  std::string version_str = std::to_string(api_version / 100) + "." +
                            std::to_string(api_version % 100);
  return Parse(version_str);
}

bool Version::IsValid() const {
  return version_.IsValid();
}

bool Version::operator<(const Version& other) const {
  return version_ < other.version_;
}

bool Version::operator<=(const Version& other) const {
  return version_ <= other.version_;
}

bool Version::operator>(const Version& other) const {
  return version_ > other.version_;
}

bool Version::operator>=(const Version& other) const {
  return version_ >= other.version_;
}

bool Version::operator==(const Version& other) const {
  return version_ == other.version_;
}

bool Version::operator!=(const Version& other) const {
  return version_ != other.version_;
}

int Version::GetMilestone() const {
  if (!version_.IsValid() || version_.components().empty()) {
    return 0;
  }
  return static_cast<int>(version_.components()[0]);
}

std::string Version::ToString() const {
  if (!version_.IsValid()) {
    return std::string();
  }
  return version_.GetString();
}

bool Version::IsInRange(const std::string& vmin,
                        const std::string& vmax) const {
  if (!IsValid()) {
    return false;
  }

  // Parse minimum version (required)
  Version min_version = Parse(vmin);
  if (!min_version.IsValid()) {
    return false;
  }

  // Check lower bound
  if (*this < min_version) {
    return false;
  }

  // If vmax is empty, no upper bound
  if (vmax.empty()) {
    return true;
  }

  // Parse maximum version
  Version max_version = Parse(vmax);
  if (!max_version.IsValid()) {
    return false;
  }

  // Check upper bound (inclusive)
  return *this <= max_version;
}

}  // namespace cef_installer
