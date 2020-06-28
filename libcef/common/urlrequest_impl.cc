// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "include/cef_urlrequest.h"
#include "libcef/browser/net_service/browser_urlrequest_impl.h"
#include "libcef/common/app_manager.h"
#include "libcef/common/task_runner_impl.h"
#include "libcef/renderer/render_urlrequest_impl.h"
#include "libcef/features/runtime_checks.h"

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "content/public/common/content_client.h"

// static
CefRefPtr<CefURLRequest> CefURLRequest::Create(
    CefRefPtr<CefRequest> request,
    CefRefPtr<CefURLRequestClient> client,
    CefRefPtr<CefRequestContext> request_context) {
  // TODO(chrome-runtime): Add support for this method.
  REQUIRE_ALLOY_RUNTIME();

  if (!request.get() || !client.get()) {
    NOTREACHED() << "called with invalid parameters";
    return nullptr;
  }

  if (!CefTaskRunnerImpl::GetCurrentTaskRunner()) {
    NOTREACHED() << "called on invalid thread";
    return nullptr;
  }

  auto content_client = CefAppManager::Get()->GetContentClient();
  if (content_client->browser()) {
    // In the browser process.
    CefRefPtr<CefBrowserURLRequest> impl =
        new CefBrowserURLRequest(nullptr, request, client, request_context);
    if (impl->Start())
      return impl.get();
    return nullptr;
  } else if (content_client->renderer()) {
    // In the render process.
    CefRefPtr<CefRenderURLRequest> impl =
        new CefRenderURLRequest(nullptr, request, client);
    if (impl->Start())
      return impl.get();
    return nullptr;
  } else {
    NOTREACHED() << "called in unsupported process";
    return nullptr;
  }
}
