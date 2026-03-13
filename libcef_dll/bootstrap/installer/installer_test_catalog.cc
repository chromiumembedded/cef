// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_test_catalog.h"

#include <windows.h>

#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/registry.h"

namespace cef_installer {
namespace test {

namespace {

// Find the x64 bin directory for the newest installed Windows SDK.
// Checks WindowsSdkDir + WindowsSDKVersion env vars first (set by
// vcvarsall.bat), then falls back to the registry.
const base::FilePath& FindSdkBinDir() {
  static const base::NoDestructor<base::FilePath> dir([]() -> base::FilePath {
    auto env = base::Environment::Create();

    // Check WindowsSdkDir + WindowsSDKVersion first (set by vcvarsall.bat).
    auto sdk_dir = env->GetVar("WindowsSdkDir");
    auto sdk_ver = env->GetVar("WindowsSDKVersion");
    if (sdk_dir.has_value() && !sdk_dir->empty() && sdk_ver.has_value() &&
        !sdk_ver->empty()) {
      // WindowsSDKVersion may have a trailing backslash.
      std::string ver = *sdk_ver;
      if (!ver.empty() && ver.back() == '\\') {
        ver.pop_back();
      }
      base::FilePath candidate = base::FilePath::FromUTF8Unsafe(*sdk_dir)
                                     .Append(L"bin")
                                     .AppendASCII(ver)
                                     .Append(L"x64");
      if (base::DirectoryExists(candidate)) {
        return candidate;
      }
    }

    // Fall back to the registry. Read KitsRoot10 for the install path and
    // enumerate version subkeys to find the newest installed SDK.
    base::win::RegKey key;
    if (key.Open(HKEY_LOCAL_MACHINE,
                 L"SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots",
                 KEY_READ) == ERROR_SUCCESS) {
      std::wstring kits_root;
      if (key.ReadValue(L"KitsRoot10", &kits_root) == ERROR_SUCCESS) {
        base::FilePath bin_root = base::FilePath(kits_root).Append(L"bin");
        std::wstring best_ver;
        for (base::win::RegistryKeyIterator it(
                 HKEY_LOCAL_MACHINE,
                 L"SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots");
             it.Valid(); ++it) {
          std::wstring ver = it.Name();
          if (base::DirectoryExists(bin_root.Append(ver).Append(L"x64")) &&
              (best_ver.empty() || ver > best_ver)) {
            best_ver = ver;
          }
        }
        if (!best_ver.empty()) {
          return bin_root.Append(best_ver).Append(L"x64");
        }
      }
    }

    return base::FilePath();
  }());
  return *dir;
}

// Find a Windows SDK tool (e.g., signtool.exe, makecat.exe).
base::FilePath FindSdkTool(const std::wstring& tool_name) {
  const base::FilePath& bin_dir = FindSdkBinDir();
  if (bin_dir.empty()) {
    return base::FilePath();
  }
  base::FilePath tool_path = bin_dir.Append(tool_name);
  if (!base::PathExists(tool_path)) {
    return base::FilePath();
  }
  return tool_path;
}

// Run a Windows SDK tool and capture combined stdout+stderr output.
// Returns the process exit code, or -1 if the process could not be launched.
// The command line string is passed directly to CreateProcess to avoid
// base::CommandLine reinterpreting / prefixed arguments as switches.
int RunSdkTool(const std::wstring& cmdline, std::string* output) {
  base::LaunchOptions options;
  options.start_hidden = true;

  HANDLE read_pipe = nullptr;
  HANDLE write_pipe = nullptr;
  SECURITY_ATTRIBUTES sa = {sizeof(sa), nullptr, TRUE};
  if (!CreatePipe(&read_pipe, &write_pipe, &sa, 0)) {
    return -1;
  }
  SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);
  options.stdout_handle = write_pipe;
  options.stderr_handle = write_pipe;
  options.handles_to_inherit.push_back(write_pipe);

  base::Process process = base::LaunchProcess(cmdline, options);
  CloseHandle(write_pipe);

  if (!process.IsValid()) {
    CloseHandle(read_pipe);
    return -1;
  }

  output->clear();
  char buf[4096];
  DWORD bytes_read;
  while (ReadFile(read_pipe, buf, sizeof(buf), &bytes_read, nullptr) &&
         bytes_read > 0) {
    output->append(buf, bytes_read);
  }
  CloseHandle(read_pipe);

  int exit_code = -1;
  process.WaitForExit(&exit_code);
  return exit_code;
}

// Write a CDF (Catalog Definition File) for the given files.
// CDF format reference:
// https://learn.microsoft.com/en-us/windows/win32/seccrypto/makecat
bool WriteCdfFile(const base::FilePath& cdf_path,
                  const base::FilePath& catalog_path,
                  const std::vector<base::FilePath>& files) {
  std::string cdf_content;
  cdf_content += "[CatalogHeader]\n";
  cdf_content += "Name=" + catalog_path.BaseName().AsUTF8Unsafe() + "\n";
  cdf_content += "ResultDir=" + catalog_path.DirName().AsUTF8Unsafe() + "\n";
  cdf_content += "PublicVersion=0x00000200\n";
  cdf_content += "EncodingType=0x00010001\n";
  cdf_content += "CatalogVersion=2\n";
  cdf_content += "HashAlgorithms=SHA256\n";
  cdf_content += "CATATTR1=0x10010001:OSAttr:2:6.0\n";
  cdf_content += "[CatalogFiles]\n";

  for (const auto& file : files) {
    // <HASH> prefix makes the reference tag be the file's hash in ASCII,
    // which is required for CatalogVersion=2 catalogs.
    cdf_content += "<HASH>" + file.BaseName().AsUTF8Unsafe() + "=" +
                   file.AsUTF8Unsafe() + "\n";
  }

  return base::WriteFile(cdf_path, cdf_content);
}

}  // namespace

CatalogError CreateSignedCatalog(const std::vector<base::FilePath>& files,
                                 const base::FilePath& catalog_path,
                                 const base::FilePath& pfx_path,
                                 const std::string& pfx_password) {
  for (const auto& file : files) {
    if (!base::PathExists(file)) {
      LOG(ERROR) << "File not found: " << file;
      return CatalogError::kFileNotFound;
    }
  }

  if (!base::PathExists(pfx_path)) {
    return CatalogError::kCertificateNotFound;
  }

  // Find SDK tools.
  base::FilePath makecat = FindSdkTool(L"makecat.exe");
  base::FilePath signtool = FindSdkTool(L"signtool.exe");
  if (makecat.empty() || signtool.empty()) {
    LOG(ERROR) << "Windows SDK tools not found (makecat=" << makecat
               << ", signtool=" << signtool << ")";
    return CatalogError::kCatalogCreationFailed;
  }

  // Write a CDF (Catalog Definition File) describing the catalog contents.
  base::FilePath cdf_path = catalog_path.ReplaceExtension(L".cdf");
  if (!WriteCdfFile(cdf_path, catalog_path, files)) {
    LOG(ERROR) << "Failed to write CDF file";
    return CatalogError::kCatalogCreationFailed;
  }

  // Create catalog using makecat.exe. The in-process CryptCATCDF APIs
  // fail to produce v2 catalogs (CatalogVersion=2, HashAlgorithms=SHA256).
  std::string makecat_output;
  std::wstring makecat_cmd =
      L"\"" + makecat.value() + L"\" \"" + cdf_path.value() + L"\"";
  RunSdkTool(makecat_cmd, &makecat_output);
  // makecat.exe returns exit code 1 even on success; check file instead.

  // Clean up CDF file.
  base::DeleteFile(cdf_path);

  if (!base::PathExists(catalog_path) ||
      base::GetFileSize(catalog_path).value_or(0) == 0) {
    LOG(ERROR) << "makecat.exe failed to create catalog: " << makecat_output;
    return CatalogError::kCatalogCreationFailed;
  }

  // Sign the catalog using signtool.exe. The in-process SignerSignEx API
  // crashes in release builds and SignerSignEx2 returns E_INVALIDARG.
  std::string signtool_output;
  std::wstring signtool_cmd = L"\"" + signtool.value() + L"\" sign" +
                              L" /f \"" + pfx_path.value() + L"\" /p " +
                              base::UTF8ToWide(pfx_password) +
                              L" /fd SHA256 \"" + catalog_path.value() + L"\"";
  int sign_exit = RunSdkTool(signtool_cmd, &signtool_output);

  if (sign_exit != 0) {
    LOG(ERROR) << "signtool.exe failed (exit " << sign_exit
               << "): " << signtool_output;
    return CatalogError::kSigningFailed;
  }

  return CatalogError::kSuccess;
}

const char* CatalogErrorToString(CatalogError error) {
  switch (error) {
    case CatalogError::kSuccess:
      return "Success";
    case CatalogError::kFileNotFound:
      return "File not found";
    case CatalogError::kCatalogCreationFailed:
      return "Catalog creation failed";
    case CatalogError::kSigningFailed:
      return "Signing failed";
    case CatalogError::kCertificateNotFound:
      return "Certificate not found";
  }
  return "Unknown error";
}

}  // namespace test
}  // namespace cef_installer
