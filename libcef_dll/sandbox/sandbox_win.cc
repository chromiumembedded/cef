// Copyright 2013 The Chromium Embedded Framework Authors. Portions Copyright
// 2011 the Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#include "sandbox/win/src/process_mitigations.h"
#include "sandbox/win/src/sandbox_factory.h"

#include "include/cef_sandbox_win.h"

namespace {

// From content/app/startup_helper_win.cc:
void InitializeSandboxInfo(sandbox::SandboxInterfaceInfo* info) {
  info->broker_services = sandbox::SandboxFactory::GetBrokerServices();
  if (!info->broker_services) {
    info->target_services = sandbox::SandboxFactory::GetTargetServices();
  } else {
    // Ensure the proper mitigations are enforced for the browser process.
    sandbox::ApplyProcessMitigationsToCurrentProcess(
        sandbox::MITIGATION_DEP | sandbox::MITIGATION_DEP_NO_ATL_THUNK |
        sandbox::MITIGATION_HARDEN_TOKEN_IL_POLICY);
    // Note: these mitigations are "post-startup".  Some mitigations that need
    // to be enabled sooner (e.g. MITIGATION_EXTENSION_POINT_DISABLE) are done
    // so in Chrome_ELF.
  }
}

}  // namespace

void* cef_sandbox_info_create() {
  sandbox::SandboxInterfaceInfo* info = new sandbox::SandboxInterfaceInfo();
  memset(info, 0, sizeof(sandbox::SandboxInterfaceInfo));
  InitializeSandboxInfo(info);
  return info;
}

void cef_sandbox_info_destroy(void* sandbox_info) {
  delete static_cast<sandbox::SandboxInterfaceInfo*>(sandbox_info);
}
