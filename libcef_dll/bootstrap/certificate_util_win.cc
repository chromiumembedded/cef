// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/certificate_util_win.h"

#include <windows.h>

#include <Softpub.h>
#include <wincrypt.h>
#include <wintrust.h>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "cef/libcef_dll/bootstrap/bootstrap_util_win.h"

namespace certificate_util {

namespace {

bool InitCryptProviderStructs(WINTRUST_DATA& win_trust_data,
                              CRYPT_PROVIDER_DATA*& prov_data_ptr,
                              CRYPT_PROVIDER_SGNR*& prov_signer_ptr,
                              CRYPT_PROVIDER_CERT*& prov_cert_ptr) {
  prov_data_ptr = WTHelperProvDataFromStateData(win_trust_data.hWVTStateData);
  if (prov_data_ptr) {
    prov_signer_ptr = WTHelperGetProvSignerFromChain(
        (PCRYPT_PROVIDER_DATA)prov_data_ptr, 0, FALSE, 0);
    if (prov_signer_ptr) {
      prov_cert_ptr = WTHelperGetProvCertFromChain(prov_signer_ptr, 0);
      if (prov_cert_ptr) {
        return true;
      }
    }
  }

  return false;
}

std::string BytesToHexString(const void* bytes, size_t length) {
  const unsigned char* bytes_c = reinterpret_cast<const unsigned char*>(bytes);

  std::string hex_string;
  hex_string.reserve(length * 2);
  for (size_t index = 0; index < length; ++index) {
    hex_string.append(base::StringPrintf("%02x", bytes_c[index]));
  }

  return hex_string;
}

}  // namespace

void GetClientThumbprints(const std::wstring& binary_path,
                          bool verify_binary,
                          ThumbprintsInfo& info) {
  const HWND wvt_handle = static_cast<HWND>(INVALID_HANDLE_VALUE);
  GUID wvt_policy = WINTRUST_ACTION_GENERIC_VERIFY_V2;

  WINTRUST_FILE_INFO file_info = {};
  file_info.cbStruct = sizeof(file_info);
  file_info.pcwszFilePath = binary_path.c_str();

  WINTRUST_SIGNATURE_SETTINGS sig_settings = {};
  sig_settings.cbStruct = sizeof(sig_settings);
  // We will verify each signature separately, but also get the number of
  // secondary signatures present in the file.
  sig_settings.dwFlags = WSS_GET_SECONDARY_SIG_COUNT | WSS_VERIFY_SPECIFIC;

  // cSecondarySigs starts off as 0. We assume we have one primary signature.
  // After the first WinVerifyTrust call succeeds, we will continue inspecting
  // the rest of the signatures.
  for (DWORD i = 0; i < sig_settings.cSecondarySigs + 1; ++i) {
    const auto& error_prefix =
        base::ASCIIToWide(base::StringPrintf("\nCertificate %d: ", i));

    WINTRUST_DATA win_trust_data = {0};
    win_trust_data.cbStruct = sizeof(win_trust_data);
    win_trust_data.dwUIChoice = WTD_UI_NONE;
    // No revocation checking.
    win_trust_data.fdwRevocationChecks = WTD_REVOKE_NONE;
    // Prevent revocation checks over the network.
    win_trust_data.dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL;
    win_trust_data.dwUnionChoice = WTD_CHOICE_FILE;
    win_trust_data.dwStateAction = WTD_STATEACTION_VERIFY;
    win_trust_data.pFile = &file_info;
    win_trust_data.pSignatureSettings = &sig_settings;

    sig_settings.dwIndex = i;

    const auto status =
        WinVerifyTrust(wvt_handle, &wvt_policy, &win_trust_data);
    const bool valid = status == ERROR_SUCCESS;
    if (!valid) {
      if (i == 0 && status == TRUST_E_NOSIGNATURE) {
        const auto last_error = static_cast<HRESULT>(::GetLastError());
        if (TRUST_E_NOSIGNATURE == last_error ||
            TRUST_E_SUBJECT_FORM_UNKNOWN == last_error ||
            TRUST_E_PROVIDER_UNKNOWN == last_error) {
          // The file is not signed.
          return;
        }
      }

      info.errors += error_prefix + TEXT("WinVerifyTrust failed: ") +
                     bootstrap_util::GetLastErrorAsString();

      // WinVerifyTrust will fail if the signing certificates can't be verified,
      // but it will still provide information about them in the StateData
      // structure. We only continue if the method asks for this.
      if (verify_binary) {
        // If the primary signature fails, we will return and not inspect the
        // rest of the signatures.
        if (i == 0) {
          info.has_signature = true;
          return;
        }
        continue;
      }
    }

    if (!win_trust_data.hWVTStateData) {
      info.errors += error_prefix + TEXT("No WinVerifyTrust data");
      continue;
    }

    CRYPT_PROVIDER_DATA* prov_data = NULL;
    CRYPT_PROVIDER_SGNR* prov_signer = NULL;
    CRYPT_PROVIDER_CERT* prov_cert = NULL;
    if (InitCryptProviderStructs(win_trust_data, prov_data, prov_signer,
                                 prov_cert)) {
      // Using SHA1 hash here because: (a) SHA1 is used internally by default in
      // most tools that inspect certificates, (b) the SHA1 value is more likely
      // to aleady be cached, (c) SHA1 is faster to compute than SHA256 if not
      // already cached, and (d) SHA1 is still resistant to preimage attacks
      // (e.g. trying to match specific hashes), particularly when used on DEC
      // formatted certificates as in this case.
      // SHA1 hash = 20 bytes.
      BYTE sha1_bytes[20] = {};
      DWORD sha1_bytes_count = sizeof(sha1_bytes);

      // Read or compute the SHA1 hash of the certificate (thumbprint), and
      // convert it to a hex string.
      if (CertGetCertificateContextProperty(prov_cert->pCert,
                                            CERT_SHA1_HASH_PROP_ID, sha1_bytes,
                                            &sha1_bytes_count)) {
        auto& thumbprints =
            valid ? info.valid_thumbprints : info.invalid_thumbprints;
        thumbprints.emplace_back(
            BytesToHexString(sha1_bytes, sha1_bytes_count));
      } else {
        info.errors += error_prefix +
                       TEXT("CertGetCertificateContextProperty failed: ") +
                       bootstrap_util::GetLastErrorAsString();
      }
    } else {
      info.errors += error_prefix + TEXT("Invalid WinVerifyTrust data");
    }

    win_trust_data.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(wvt_handle, &wvt_policy, &win_trust_data);
  }

  info.has_signature = true;
}

}  // namespace certificate_util
