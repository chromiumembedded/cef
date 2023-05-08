// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "include/cef_urlrequest.h"
#include "libcef/browser/net_service/browser_urlrequest_impl.h"
#include "libcef/common/app_manager.h"
#include "libcef/common/task_runner_impl.h"
#include "libcef/features/runtime_checks.h"

#include "base/logging.h"
#include "base/notreached.h"
#include "content/public/common/content_client.h"

// static
CefRefPtr<CefURLRequest> CefURLRequest::Create(
    CefRefPtr<CefRequest> request,
    CefRefPtr<CefURLRequestClient> client,
    CefRefPtr<CefRequestContext> request_context) {
  if (!request.get() || !client.get()) {
    DCHECK(false) << "called with invalid parameters";
    return nullptr;
  }

  if (!CefTaskRunnerImpl::GetCurrentTaskRunner()) {
    DCHECK(false) << "called on invalid thread";
    return nullptr;
  }

  auto content_client = CefAppManager::Get()->GetContentClient();
  if (content_client->browser()) {
    // In the browser process.
    CefRefPtr<CefBrowserURLRequest> impl =
        new CefBrowserURLRequest(nullptr, request, client, request_context);
    if (impl->Start()) {
      return impl.get();
    }
    return nullptr;
  } else {
    DCHECK(false) << "called in unsupported process";
    return nullptr;
  }
}
