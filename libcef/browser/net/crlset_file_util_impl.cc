// Copyright 2017 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "include/cef_file_util.h"

#include "libcef/browser/context.h"
#include "libcef/browser/thread_util.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/threading/thread_restrictions.h"
#include "net/cert/crl_set.h"
#include "net/ssl/ssl_config_service.h"

namespace {

// Based on chrome/browser/net/crl_set_fetcher.cc.

void SetCRLSetIfNewer(scoped_refptr<net::CRLSet> crl_set) {
  CEF_REQUIRE_IOT();
  // TODO(cef): Re-implement via NetworkService.
}

void LoadFromDisk(const base::FilePath& path) {
  CEF_REQUIRE_BLOCKING();

  std::string crl_set_bytes;
  if (!base::ReadFileToString(path, &crl_set_bytes)) {
    LOG(WARNING) << "Failed to read CRL set from " << path.MaybeAsASCII();
    return;
  }

  scoped_refptr<net::CRLSet> crl_set;
  if (!net::CRLSet::Parse(crl_set_bytes, &crl_set)) {
    LOG(WARNING) << "Failed to parse CRL set from " << path.MaybeAsASCII();
    return;
  }

  VLOG(1) << "Loaded " << crl_set_bytes.size() << " bytes of CRL set from disk";
  CEF_POST_TASK(CEF_IOT, base::BindOnce(&SetCRLSetIfNewer, crl_set));
}

}  // namespace

void CefLoadCRLSetsFile(const CefString& path) {
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return;
  }

  CEF_POST_USER_VISIBLE_TASK(base::BindOnce(&LoadFromDisk, path));
}
