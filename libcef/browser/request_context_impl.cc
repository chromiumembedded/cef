// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/request_context_impl.h"
#include "libcef/browser/browser_context_proxy.h"
#include "libcef/browser/content_browser_client.h"
#include "libcef/browser/context.h"
#include "libcef/browser/thread_util.h"
#include "base/atomic_sequence_num.h"
#include "base/logging.h"

namespace {

void RemoveContextRef(CefBrowserContext* browser_context) {
  CefContentBrowserClient::Get()->RemoveBrowserContextReference(
      browser_context);
}

base::StaticAtomicSequenceNumber g_next_id;

}  // namespace

// Static functions

CefRefPtr<CefRequestContext> CefRequestContext::GetGlobalContext() {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return NULL;
  }

  return new CefRequestContextImpl(
      CefContentBrowserClient::Get()->browser_context());
}

CefRefPtr<CefRequestContext> CefRequestContext::CreateContext(
      CefRefPtr<CefRequestContextHandler> handler) {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return NULL;
  }

  return new CefRequestContextImpl(handler);
}

// CefBrowserContextImpl

CefRequestContextImpl::CefRequestContextImpl(
    CefBrowserContext* browser_context)
    : browser_context_(browser_context),
      unique_id_(0) {
  DCHECK(browser_context);
  if (!IsGlobal()) {
    CEF_REQUIRE_UIT();
    CefBrowserContextProxy* proxy =
        static_cast<CefBrowserContextProxy*>(browser_context);
    handler_ = proxy->handler();
    CefContentBrowserClient::Get()->AddBrowserContextReference(browser_context);
  }
}

CefRequestContextImpl::CefRequestContextImpl(
    CefRefPtr<CefRequestContextHandler> handler)
    : browser_context_(NULL),
      handler_(handler),
      unique_id_(g_next_id.GetNext()) {
}

CefRequestContextImpl::~CefRequestContextImpl() {
  if (browser_context_) {
    if (CEF_CURRENTLY_ON_UIT())
      RemoveContextRef(browser_context_);
    else
      CEF_POST_TASK(CEF_UIT, base::Bind(RemoveContextRef, browser_context_));
  }
}

CefBrowserContext* CefRequestContextImpl::GetOrCreateBrowserContext() {
  CEF_REQUIRE_UIT();
  if (!browser_context_) {
    browser_context_ =
        CefContentBrowserClient::Get()->CreateBrowserContextProxy(handler_);
  }
  return browser_context_;
}

bool CefRequestContextImpl::IsSame(CefRefPtr<CefRequestContext> other) {
  CefRequestContextImpl* impl =
      static_cast<CefRequestContextImpl*>(other.get());
  if (!impl)
    return false;

  // Compare CefBrowserContext points if one has been associated.
  if (browser_context_ && impl->browser_context_)
    return (browser_context_ == impl->browser_context_);
  else if (browser_context_ || impl->browser_context_)
    return false;

  // Otherwise compare unique IDs.
  return (unique_id_ == impl->unique_id_);
}

bool CefRequestContextImpl::IsGlobal() {
  return (browser_context_ ==
          CefContentBrowserClient::Get()->browser_context());
}

CefRefPtr<CefRequestContextHandler> CefRequestContextImpl::GetHandler() {
  return handler_;
}
