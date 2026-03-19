// Copyright 2020 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/common/app_manager.h"

#include "base/logging.h"
#include "cef/libcef/common/net/scheme_info.h"
#include "cef/libcef/common/scheme_registrar_impl.h"
#include "content/public/browser/child_process_security_policy.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "base/path_service.h"
#endif

namespace {

CefAppManager* g_manager = nullptr;

}  // namespace

// static
CefAppManager* CefAppManager::Get() {
  return g_manager;
}

CefAppManager::CefAppManager() {
  // Only a single instance should exist.
  DCHECK(!g_manager);
  g_manager = this;
}

CefAppManager::~CefAppManager() {
  g_manager = nullptr;
}

void CefAppManager::AddCustomScheme(const CefSchemeInfo* scheme_info) {
  DCHECK(!scheme_info_list_locked_);
  scheme_info_list_.push_back(*scheme_info);

  // Custom schemes are registered with ChildProcessSecurityPolicy later, after
  // FeatureList initialization, via RegisterCustomSchemesWithPolicy().
}

void CefAppManager::RegisterCustomSchemesWithPolicy() {
  DCHECK(scheme_info_list_locked_);
  content::ChildProcessSecurityPolicy* policy =
      content::ChildProcessSecurityPolicy::GetInstance();
  for (const auto& scheme_info : scheme_info_list_) {
    if (!policy->IsWebSafeScheme(scheme_info.scheme_name)) {
      policy->RegisterWebSafeScheme(scheme_info.scheme_name);
    }
  }
}

bool CefAppManager::HasCustomScheme(const std::string& scheme_name) {
  DCHECK(scheme_info_list_locked_);
  if (scheme_info_list_.empty()) {
    return false;
  }

  for (const auto& info : scheme_info_list_) {
    if (info.scheme_name == scheme_name) {
      return true;
    }
  }

  return false;
}

const CefAppManager::SchemeInfoList* CefAppManager::GetCustomSchemes() {
  DCHECK(scheme_info_list_locked_);
  return &scheme_info_list_;
}

void CefAppManager::AddAdditionalSchemes(
    content::ContentClient::Schemes* schemes) {
  DCHECK(!scheme_info_list_locked_);

  auto application = GetApplication();
  if (application) {
    CefSchemeRegistrarImpl schemeRegistrar;
    application->OnRegisterCustomSchemes(&schemeRegistrar);
    schemeRegistrar.GetSchemes(schemes);
  }

  scheme_info_list_locked_ = true;
}

#if BUILDFLAG(IS_WIN)
const wchar_t* CefAppManager::GetResourceDllName() {
  static wchar_t file_path[MAX_PATH + 1] = {0};

  if (file_path[0] == 0) {
    // Retrieve the module path (usually libcef.dll).
    base::FilePath module;
    base::PathService::Get(base::FILE_MODULE, &module);
    const std::wstring wstr = module.value();
    size_t count = std::min(static_cast<size_t>(MAX_PATH), wstr.size());
    wcsncpy(file_path, wstr.c_str(), count);
    file_path[count] = 0;
  }

  return file_path;
}
#endif  // BUILDFLAG(IS_WIN)
