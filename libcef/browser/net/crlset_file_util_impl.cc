// Copyright 2017 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "include/cef_file_util.h"

#include "libcef/browser/context.h"
#include "libcef/browser/thread_util.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "content/public/browser/network_service_instance.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"

namespace {

void UpdateCRLSet(const std::string& crl_set_bytes) {
  CEF_REQUIRE_UIT();
  content::GetCertVerifierServiceFactory()->UpdateCRLSet(
      base::as_bytes(base::make_span(crl_set_bytes)), base::DoNothing());
}

void LoadFromDisk(const base::FilePath& path) {
  CEF_REQUIRE_BLOCKING();

  std::string crl_set_bytes;
  if (!base::ReadFileToString(path, &crl_set_bytes)) {
    LOG(WARNING) << "Failed to read CRL set from " << path.MaybeAsASCII();
    return;
  }

  VLOG(1) << "Loading " << crl_set_bytes.size()
          << " bytes of CRL set from disk";
  CEF_POST_TASK(CEF_UIT, base::BindOnce(&UpdateCRLSet, crl_set_bytes));
}

}  // namespace

void CefLoadCRLSetsFile(const CefString& path) {
  if (!CONTEXT_STATE_VALID()) {
    DCHECK(false) << "context not valid";
    return;
  }

  CEF_POST_USER_VISIBLE_TASK(base::BindOnce(&LoadFromDisk, path));
}
