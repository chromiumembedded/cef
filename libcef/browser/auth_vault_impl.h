// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_AUTH_VAULT_IMPL_H_
#define CEF_LIBCEF_BROWSER_AUTH_VAULT_IMPL_H_
#pragma once

#include "base/files/file_path.h"
#include "cef/include/cef_auth_vault.h"

struct AuthVaultActionResult;
struct AuthVaultReadResult;
struct AuthVaultListResult;

// Implementation of the CefAuthVault interface. Methods may be called on any
// browser process thread unless otherwise indicated.
class CefAuthVaultImpl : public CefAuthVault {
 public:
  CefAuthVaultImpl() = default;

  CefAuthVaultImpl(const CefAuthVaultImpl&) = delete;
  CefAuthVaultImpl& operator=(const CefAuthVaultImpl&) = delete;

  // CefAuthVault methods.
  CefString GetVaultPath() override;
  CefString GetEncryptionKeyPath() override;
  void SaveProfile(CefRefPtr<CefDictionaryValue> profile,
                   CefRefPtr<CefAuthVaultActionCallback> callback) override;
  void ReadProfile(const CefString& name,
                   CefRefPtr<CefAuthVaultReadCallback> callback) override;
  void DeleteProfile(
      const CefString& name,
      CefRefPtr<CefAuthVaultActionCallback> callback) override;
  void VisitProfiles(CefRefPtr<CefAuthProfileVisitor> visitor) override;

 private:
  ~CefAuthVaultImpl() override = default;

  void OnActionComplete(CefRefPtr<CefAuthVaultActionCallback> callback,
                        AuthVaultActionResult result);
  void OnReadComplete(CefRefPtr<CefAuthVaultReadCallback> callback,
                      AuthVaultReadResult result);
  void OnVisitComplete(CefRefPtr<CefAuthProfileVisitor> visitor,
                       AuthVaultListResult result);

  void RunActionCallback(CefRefPtr<CefAuthVaultActionCallback> callback,
                         bool success,
                         const std::string& error,
                         const base::FilePath& path);
  base::FilePath GetVaultPathInternal() const;
  base::FilePath GetEncryptionKeyPathInternal() const;

  IMPLEMENT_REFCOUNTING(CefAuthVaultImpl);
};

#endif  // CEF_LIBCEF_BROWSER_AUTH_VAULT_IMPL_H_
