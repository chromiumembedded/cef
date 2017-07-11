// Copyright 2017 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "include/cef_file_util.h"

#include "libcef/browser/context.h"
#include "libcef/browser/thread_util.h"

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "net/cert/crl_set.h"
#include "net/cert/crl_set_storage.h"
#include "net/ssl/ssl_config_service.h"

namespace {

// Based on chrome/browser/net/crl_set_fetcher.cc.

void SetCRLSetIfNewer(scoped_refptr<net::CRLSet> crl_set) {
  CEF_REQUIRE_IOT();

  scoped_refptr<net::CRLSet> old_crl_set(net::SSLConfigService::GetCRLSet());
  if (old_crl_set.get() && old_crl_set->sequence() > crl_set->sequence()) {
    LOG(WARNING) << "Refusing to downgrade CRL set from #"
                 << old_crl_set->sequence() << "to #" << crl_set->sequence();
  } else {
    net::SSLConfigService::SetCRLSet(crl_set);
    VLOG(1) << "Installed CRL set #" << crl_set->sequence();
  }
}

void LoadFromDisk(const base::FilePath& path) {
  base::ThreadRestrictions::AssertIOAllowed();

  std::string crl_set_bytes;
  if (!base::ReadFileToString(path, &crl_set_bytes)) {
    LOG(WARNING) << "Failed to read CRL set from " << path.MaybeAsASCII();
    return;
  }

  scoped_refptr<net::CRLSet> crl_set;
  if (!net::CRLSetStorage::Parse(crl_set_bytes, &crl_set)) {
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

  base::PostTaskWithTraits(FROM_HERE,
                           {base::TaskPriority::BACKGROUND, base::MayBlock()},
                           base::BindOnce(&LoadFromDisk, path));
}
