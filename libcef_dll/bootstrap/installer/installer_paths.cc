// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_paths.h"

#include <windows.h>

#include <shlobj.h>

#include <algorithm>
#include <optional>

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "base/process/process_info.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "build/build_config.h"
#include "cef/libcef_dll/bootstrap/installer/installer_constants.h"
#include "cef/libcef_dll/bootstrap/installer/installer_file_ops.h"
#include "cef/libcef_dll/bootstrap/installer/installer_registry.h"

namespace cef_installer {

namespace {

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
size_t g_directory_mutation_probe_count = 0;
#endif

// Registry key for CEF install location (machine-wide only for security)
constexpr wchar_t kCefRegistryKey[] = L"SOFTWARE\\CEF";
constexpr wchar_t kInstallLocationValue[] = L"InstallLocation";

// Try to use or create a directory. Returns true if successful.
// Security: Rejects reparse points (symlinks/junctions) to prevent attackers
// from redirecting install location to sensitive directories.
bool TryUseDirectory(const base::FilePath& path) {
  if (path.empty()) {
    return false;
  }

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
  ++g_directory_mutation_probe_count;
#endif

  // Check if it exists
  if (base::DirectoryExists(path)) {
    // Security: Reject reparse points (symlinks/junctions)
    // An attacker with write access could create a junction at the expected
    // install location pointing to a sensitive directory.
    if (IsReparsePoint(path)) {
      return false;
    }

    // Check if writable by trying to create a unique temp file.
    // Use PID + TID to avoid sharing violations when multiple installer
    // instances probe the same directory concurrently.
    std::wstring probe_name = L".write_test_" +
                              std::to_wstring(::GetCurrentProcessId()) + L"_" +
                              std::to_wstring(::GetCurrentThreadId());
    base::FilePath test_file = path.Append(probe_name);
    if (base::WriteFile(test_file, "")) {
      base::DeleteFile(test_file);
      return true;
    }
    return false;
  }

  // Try to create it
  return base::CreateDirectory(path);
}

// Validate a path read from the registry. Must be absolute and must not
// contain parent references (e.g. "..") that could redirect to arbitrary
// locations. HKLM requires admin to write, but defense in depth.
bool IsRegistryPathValid(const base::FilePath& path) {
  return path.IsAbsolute() && !path.ReferencesParent();
}

// Resolve candidate path from HKLM registry. Returns empty if not configured
// or invalid.
base::FilePath ResolveRegistryPath() {
  base::win::RegKey key;
  if (OpenSharedMachineRegistryKey(kCefRegistryKey, KEY_READ, &key) !=
      ERROR_SUCCESS) {
    return base::FilePath();
  }

  std::wstring path_value;
  if (key.ReadValue(kInstallLocationValue, &path_value) != ERROR_SUCCESS) {
    return base::FilePath();
  }

  if (path_value.empty()) {
    return base::FilePath();
  }

  base::FilePath candidate(path_value);
  if (!IsRegistryPathValid(candidate)) {
    return base::FilePath();
  }

  return candidate;
}

// Resolve candidate path for %ProgramFiles%\CEF.
base::FilePath ResolveProgramFilesPath() {
  wchar_t path[MAX_PATH];
  if (FAILED(SHGetFolderPathW(nullptr, CSIDL_PROGRAM_FILES, nullptr,
                              SHGFP_TYPE_CURRENT, path))) {
    return base::FilePath();
  }

  return base::FilePath(path).Append(kCefSubdirectory);
}

// Resolve candidate path for %LocalAppData%\CEF.
base::FilePath ResolveLocalAppDataPath() {
  wchar_t path[MAX_PATH];
  if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr,
                              SHGFP_TYPE_CURRENT, path))) {
    return base::FilePath();
  }

  return base::FilePath(path).Append(kCefSubdirectory);
}

std::optional<base::FilePath> ResolvePathIdentity(const base::FilePath& input) {
  if (input.empty() || !input.IsAbsolute()) {
    return std::nullopt;
  }

  base::FilePath existing =
      input.NormalizePathSeparators().StripTrailingSeparators();
  std::vector<base::FilePath::StringType> missing_components;
  constexpr size_t kMaxAncestorDepth = 128;
  while (!base::PathExists(existing)) {
    if (missing_components.size() == kMaxAncestorDepth) {
      return std::nullopt;
    }
    const base::FilePath parent = existing.DirName();
    if (parent == existing) {
      return std::nullopt;
    }
    missing_components.push_back(existing.BaseName().value());
    existing = parent;
  }

  base::win::ScopedHandle handle(::CreateFileW(
      existing.value().c_str(), FILE_READ_ATTRIBUTES,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
      OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
  if (!handle.is_valid()) {
    return std::nullopt;
  }

  constexpr DWORD kMaxFinalPathLength = 32768;
  std::wstring resolved(MAX_PATH, L'\0');
  DWORD length = ::GetFinalPathNameByHandleW(
      handle.get(), resolved.data(), static_cast<DWORD>(resolved.size()),
      FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
  if (length == 0 || length >= kMaxFinalPathLength) {
    return std::nullopt;
  }
  if (length >= resolved.size()) {
    resolved.resize(length + 1);
    length = ::GetFinalPathNameByHandleW(
        handle.get(), resolved.data(), static_cast<DWORD>(resolved.size()),
        FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (length == 0 || length >= resolved.size()) {
      return std::nullopt;
    }
  }
  resolved.resize(length);
  base::FilePath result(std::move(resolved));
  for (auto it = missing_components.rbegin(); it != missing_components.rend();
       ++it) {
    result = result.Append(*it);
  }
  return result.NormalizePathSeparators().StripTrailingSeparators();
}

bool PathsHaveSameIdentity(const base::FilePath& lhs,
                           const base::FilePath& rhs) {
  if (lhs.empty() || rhs.empty()) {
    return false;
  }
  const base::FilePath normalized_lhs =
      lhs.NormalizePathSeparators().StripTrailingSeparators();
  const base::FilePath normalized_rhs =
      rhs.NormalizePathSeparators().StripTrailingSeparators();
  if (base::FilePath::CompareEqualIgnoreCase(normalized_lhs.value(),
                                             normalized_rhs.value())) {
    return true;
  }
  const std::optional<base::FilePath> resolved_lhs = ResolvePathIdentity(lhs);
  const std::optional<base::FilePath> resolved_rhs = ResolvePathIdentity(rhs);
  if (resolved_lhs && resolved_rhs &&
      base::FilePath::CompareEqualIgnoreCase(resolved_lhs->value(),
                                             resolved_rhs->value())) {
    return true;
  }
  return base::PathExists(lhs) && base::PathExists(rhs) &&
         IsSameDirectory(lhs, rhs);
}

// Test-only overrides for directory resolution.
// No-op in official release builds.
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
struct DirectoryOverrides {
  std::vector<base::FilePath> readable_dirs;
  std::optional<base::FilePath> writable_dir;
  std::optional<std::vector<internal::TestDirectoryCandidate>> candidates;
  std::optional<bool> elevated;
  std::optional<bool> admin_mutation_allowed;
};

std::optional<DirectoryOverrides>& GetTestDirectoryOverrides() {
  static base::NoDestructor<std::optional<DirectoryOverrides>> instance;
  return *instance;
}
#endif

}  // namespace

namespace internal {

PathContainment GetPhysicalPathContainment(const base::FilePath& directory,
                                           const base::FilePath& path) {
  if (directory.empty() || path.empty() || !directory.IsAbsolute() ||
      !path.IsAbsolute() || !base::DirectoryExists(directory) ||
      !base::PathExists(path)) {
    return PathContainment::kIndeterminate;
  }

  const std::optional<base::FilePath> resolved_directory =
      ResolvePathIdentity(directory);
  const std::optional<base::FilePath> resolved_path = ResolvePathIdentity(path);
  if (!resolved_directory || !resolved_path) {
    return PathContainment::kIndeterminate;
  }

  if (*resolved_directory == *resolved_path ||
      resolved_directory->IsParent(*resolved_path)) {
    return PathContainment::kContained;
  }
  return PathContainment::kOutside;
}

#if defined(OFFICIAL_BUILD) && defined(NDEBUG)
void OverrideInstallDirectoriesForTesting(
    std::vector<base::FilePath> readable_dirs,
    std::optional<base::FilePath> writable_dir) {}
void ClearInstallDirectoryOverridesForTesting() {}
void OverrideInstallDirectoryCandidatesForTesting(
    std::vector<TestDirectoryCandidate> candidates) {}
size_t GetInstallDirectoryMutationProbeCountForTesting() {
  return 0;
}
void OverrideProcessElevationForTesting(std::optional<bool> elevated) {}
void OverrideAdminMutationAllowedForTesting(std::optional<bool> allowed) {}
#else
void OverrideInstallDirectoriesForTesting(
    std::vector<base::FilePath> readable_dirs,
    std::optional<base::FilePath> writable_dir) {
  GetTestDirectoryOverrides() = DirectoryOverrides{
      std::move(readable_dirs), std::move(writable_dir), std::nullopt};
}

void ClearInstallDirectoryOverridesForTesting() {
  GetTestDirectoryOverrides().reset();
  g_directory_mutation_probe_count = 0;
}

void OverrideInstallDirectoryCandidatesForTesting(
    std::vector<TestDirectoryCandidate> candidates) {
  GetTestDirectoryOverrides() =
      DirectoryOverrides{{}, std::nullopt, std::move(candidates)};
}

size_t GetInstallDirectoryMutationProbeCountForTesting() {
  return g_directory_mutation_probe_count;
}

void OverrideProcessElevationForTesting(std::optional<bool> elevated) {
  if (!GetTestDirectoryOverrides()) {
    GetTestDirectoryOverrides().emplace();
  }
  GetTestDirectoryOverrides()->elevated = elevated;
}

void OverrideAdminMutationAllowedForTesting(std::optional<bool> allowed) {
  if (!GetTestDirectoryOverrides()) {
    GetTestDirectoryOverrides().emplace();
  }
  GetTestDirectoryOverrides()->admin_mutation_allowed = allowed;
}
#endif

}  // namespace internal

bool IsCurrentProcessElevated() {
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
  if (auto& o = GetTestDirectoryOverrides(); o && o->elevated.has_value()) {
    return *o->elevated;
  }
#endif
  const base::IntegrityLevel level = base::GetCurrentProcessIntegrityLevel();
  // Fail closed: an unknown integrity level must not enable traversal into
  // the lower-integrity per-user store.
  return level == base::HIGH_INTEGRITY || level == base::INTEGRITY_UNKNOWN;
}

bool IsAdminMutationAllowed(bool automatic_startup,
                            bool enable_explicit_modes) {
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
  if (auto& o = GetTestDirectoryOverrides();
      o && o->admin_mutation_allowed.has_value()) {
    return *o->admin_mutation_allowed;
  }
  return true;
#else
  return !automatic_startup || enable_explicit_modes;
#endif
}

bool IsUserRetentionEligible(DirectoryRole role) {
  return role == DirectoryRole::kPerUserDefault ||
         role == DirectoryRole::kCustom;
}

InstallDirectories ResolveInstallDirectories(
    const std::string& custom_path,
    const DirectoryResolutionContext& context) {
  InstallDirectories result;

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
  if (auto& o = GetTestDirectoryOverrides()) {
    if (!o->candidates.has_value()) {
      if (o->writable_dir.has_value()) {
        result.writable_dir = *o->writable_dir;
        result.writable_role = DirectoryRole::kCustom;
        result.write_error = PathError::kSuccess;
      }
      result.readable_dirs = o->readable_dirs;
      result.readable_roles.assign(result.readable_dirs.size(),
                                   DirectoryRole::kCustom);
      return result;
    }
  }
#endif

  // install_path: exclusive; preserve a safe existing read-only store.
  if (!custom_path.empty()) {
    base::FilePath custom(base::FilePath::FromUTF8Unsafe(custom_path));
    if (!custom.IsAbsolute() || custom.ReferencesParent()) {
      result.write_error = PathError::kInvalidPath;
      return result;
    }

    std::vector<base::FilePath> standard_candidates;
#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
    if (auto& o = GetTestDirectoryOverrides(); o && o->candidates.has_value()) {
      for (const auto& candidate : *o->candidates) {
        if (candidate.role != DirectoryRole::kCustom) {
          standard_candidates.push_back(candidate.path);
        }
      }
    }
#endif
    if (standard_candidates.empty()) {
      standard_candidates = {ResolveRegistryPath(), ResolveProgramFilesPath(),
                             ResolveLocalAppDataPath()};
    }
    for (const auto& standard : standard_candidates) {
      if (PathsHaveSameIdentity(custom, standard)) {
        result.write_error = PathError::kInvalidPath;
        return result;
      }
    }

    const bool exists = base::PathExists(custom);
    if (exists && !base::DirectoryExists(custom)) {
      result.write_error = PathError::kInvalidPath;
      return result;
    }
    if (exists && !IsReadableDirectory(custom)) {
      result.write_error = PathError::kAccessDenied;
      return result;
    }

    if (context.mutation_capable && TryUseDirectory(custom)) {
      result.writable_dir = custom;
      result.writable_role = DirectoryRole::kCustom;
      result.write_error = PathError::kSuccess;
      result.readable_dirs.push_back(custom);
      result.readable_roles.push_back(DirectoryRole::kCustom);
    } else if (exists) {
      result.write_error = PathError::kAccessDenied;
      result.readable_dirs.push_back(custom);
      result.readable_roles.push_back(DirectoryRole::kCustom);
    } else {
      result.write_error = context.mutation_capable ? PathError::kAccessDenied
                                                    : PathError::kNotFound;
    }
    return result;  // exclusive — never fall through
  }

  // Default search: registry → ProgramFiles → LocalAppData.

  // Preserve the earliest source role before performing any permission or
  // mutation check. A lower-priority alias must never weaken the gate applied
  // to the same path through an earlier admin-default source.
  std::vector<std::pair<base::FilePath, DirectoryRole>> seen_candidates;
  auto get_effective_role = [&seen_candidates](const base::FilePath& path,
                                               DirectoryRole role) {
    for (const auto& [seen_path, seen_role] : seen_candidates) {
      if (IsSameDirectory(seen_path, path)) {
        return seen_role;
      }
    }
    seen_candidates.emplace_back(path, role);
    return role;
  };

  // Collect readable dirs. Deduplicate by canonical path.
  auto add_if_unique = [&result](const base::FilePath& path,
                                 DirectoryRole role) {
    for (size_t i = 0; i < result.readable_dirs.size(); ++i) {
      if (IsSameDirectory(result.readable_dirs[i], path)) {
        return result.readable_roles[i];
      }
    }
    result.readable_dirs.push_back(path);
    result.readable_roles.push_back(role);
    return role;
  };

  // Try a candidate path: stop if writable, otherwise add to readable.
  // Returns true if writable (search should stop).
  auto try_candidate = [&](const base::FilePath& candidate, DirectoryRole role,
                           std::optional<bool> readable_override = std::nullopt,
                           std::optional<bool> writable_override =
                               std::nullopt) -> bool {
    if (candidate.empty()) {
      return false;
    }

    const DirectoryRole effective_role = get_effective_role(candidate, role);
    const bool is_admin = effective_role == DirectoryRole::kHklmDefault ||
                          effective_role == DirectoryRole::kProgramFilesDefault;
    const bool may_mutate =
        context.mutation_capable &&
        (!is_admin || (context.is_elevated && context.allow_admin_mutation));

    // Treat an unknown writability result as potentially writable. Before the
    // first write probe, prove that stopping at this candidate would preserve
    // every directory used by the read-only selection. A known non-writable
    // test candidate cannot truncate the set and may be skipped safely.
    if (may_mutate && writable_override != false) {
      std::vector<base::FilePath> prospective_readable = result.readable_dirs;
      bool candidate_present = false;
      for (const auto& readable : prospective_readable) {
        if (IsSameDirectory(readable, candidate)) {
          candidate_present = true;
          break;
        }
      }
      if (!candidate_present) {
        prospective_readable.push_back(candidate);
      }
      for (const auto& required : context.required_readable_dirs) {
        bool found = false;
        for (const auto& readable : prospective_readable) {
          if (IsSameDirectory(readable, required)) {
            found = true;
            break;
          }
        }
        if (!found) {
          result.mutation_blocked_by_required_readable_dirs = true;
          return true;
        }
      }
    }
    const bool writable = may_mutate && (writable_override.has_value()
                                             ? *writable_override
                                             : TryUseDirectory(candidate));
    if (writable) {
      // Added as both readable and writable.
      add_if_unique(candidate, effective_role);
      result.writable_dir = candidate;
      result.writable_role = effective_role;
      result.write_error = PathError::kSuccess;
      return true;  // stop
    }

    const bool readable = readable_override.has_value()
                              ? *readable_override
                              : IsReadableDirectory(candidate);
    if (readable) {
      // Added as readable only; continue searching.
      add_if_unique(candidate, effective_role);
    }

    return false;  // continue
  };

#if !(defined(OFFICIAL_BUILD) && defined(NDEBUG))
  if (auto& o = GetTestDirectoryOverrides(); o && o->candidates.has_value()) {
    for (const auto& candidate : *o->candidates) {
      if (candidate.role == DirectoryRole::kPerUserDefault &&
          (context.is_elevated || !context.allow_shared_user_store)) {
        continue;
      }
      if (try_candidate(candidate.path, candidate.role, candidate.readable,
                        candidate.writable)) {
        return result;
      }
    }
    return result;
  }
#endif

  if (try_candidate(ResolveRegistryPath(), DirectoryRole::kHklmDefault)) {
    return result;
  }
  if (try_candidate(ResolveProgramFilesPath(),
                    DirectoryRole::kProgramFilesDefault)) {
    return result;
  }
  if (!context.is_elevated && context.allow_shared_user_store &&
      try_candidate(ResolveLocalAppDataPath(),
                    DirectoryRole::kPerUserDefault)) {
    return result;
  }

  return result;
}

base::FilePath GetDatabasePath(const base::FilePath& install_dir) {
  return install_dir.Append(kDatabaseFilename);
}

base::FilePath GetLibcefPath(const base::FilePath& version_dir) {
  return version_dir.Append(kReleaseSubdirectory).Append(kLibcefFilename);
}

std::string GetCurrentPlatform() {
#if defined(ARCH_CPU_ARM64)
  return "windowsarm64";
#elif defined(ARCH_CPU_X86_64)
  return "windows64";
#elif defined(ARCH_CPU_X86)
  return "windows32";
#else
#error "Unknown CPU architecture"
#endif
}

base::FilePath GetVersionPath(const base::FilePath& install_dir,
                              const Version& version,
                              const std::string& platform) {
  return install_dir.Append(kVersionsSubdirectory)
      .Append(base::FilePath::FromUTF8Unsafe(version.ToString()))
      .Append(base::FilePath::FromUTF8Unsafe(platform));
}

base::FilePath GetVersionPath(const base::FilePath& install_dir,
                              const Version& version) {
  return GetVersionPath(install_dir, version, GetCurrentPlatform());
}

std::vector<Version> ScanInstalledVersions(const base::FilePath& install_dir) {
  std::vector<Version> versions;

  base::FilePath versions_dir = install_dir.Append(kVersionsSubdirectory);
  if (!base::DirectoryExists(versions_dir)) {
    return versions;
  }

  // Reject if the Versions/ directory itself is a reparse point
  // (symlink/junction) to prevent enumerating attacker-controlled content.
  if (IsReparsePoint(versions_dir)) {
    return versions;
  }

  std::string current_platform = GetCurrentPlatform();

  base::FileEnumerator enumerator(versions_dir, false,
                                  base::FileEnumerator::DIRECTORIES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    std::string dir_name = path.BaseName().AsUTF8Unsafe();
    Version version = Version::Parse(dir_name);
    if (version.IsValid()) {
      // Reject reparse points (symlinks/junctions)
      if (IsReparsePoint(path)) {
        continue;
      }
      // Check if this version has the current platform installed
      base::FilePath platform_path =
          path.Append(base::FilePath::FromUTF8Unsafe(current_platform));
      if (base::DirectoryExists(platform_path) &&
          !IsReparsePoint(platform_path)) {
        versions.push_back(std::move(version));
      }
    }
  }

  // Sort newest-first (descending order)
  std::sort(versions.begin(), versions.end(),
            [](const Version& a, const Version& b) { return a > b; });

  return versions;
}

base::FilePath ResolvePathRelativeTo(const std::string& path_utf8,
                                     const base::FilePath& base_dir) {
  base::FilePath path = base::FilePath::FromUTF8Unsafe(path_utf8);
  if (path.IsAbsolute()) {
    return base::MakeAbsoluteFilePath(path);
  }
  return base::MakeAbsoluteFilePath(base_dir.Append(path));
}

base::FilePath GetTempDirectoryPath() {
  wchar_t temp_path[MAX_PATH];
  DWORD len = GetTempPathW(MAX_PATH, temp_path);
  if (len == 0 || len > MAX_PATH) {
    return base::FilePath();
  }
  return base::FilePath(temp_path).StripTrailingSeparators();
}

bool IsRunningFromTempDirectory(const base::FilePath& exe_path) {
  const base::FilePath temp_dir = GetTempDirectoryPath();
  if (temp_dir.empty()) {
    return false;
  }

  return internal::GetPhysicalPathContainment(temp_dir, exe_path) ==
         internal::PathContainment::kContained;
}

const char* PathErrorToString(PathError error) {
  switch (error) {
    case PathError::kSuccess:
      return "Success";
    case PathError::kNotFound:
      return "No valid install directory found";
    case PathError::kAccessDenied:
      return "Directory exists but not writable";
    case PathError::kInvalidPath:
      return "Path exists but is not a directory";
  }
  return "Unknown error";
}

}  // namespace cef_installer
