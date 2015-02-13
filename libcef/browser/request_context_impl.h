// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_REQUEST_CONTEXT_IMPL_H_
#define CEF_LIBCEF_REQUEST_CONTEXT_IMPL_H_
#pragma once

#include "include/cef_request_context.h"
#include "libcef/browser/browser_context.h"

class CefRequestContextImpl : public CefRequestContext {
 public:
  explicit CefRequestContextImpl(
      scoped_refptr<CefBrowserContext> browser_context);
  explicit CefRequestContextImpl(CefRefPtr<CefRequestContextHandler> handler);
  ~CefRequestContextImpl() override;

  scoped_refptr<CefBrowserContext> GetOrCreateBrowserContext();

  bool IsSame(CefRefPtr<CefRequestContext> other) override;
  bool IsGlobal() override;
  CefRefPtr<CefRequestContextHandler> GetHandler() override;

 protected:
  scoped_refptr<CefBrowserContext> browser_context_;
  CefRefPtr<CefRequestContextHandler> handler_;

  // Used to uniquely identify CefRequestContext objects before an associated
  // CefBrowserContext has been created.
  int unique_id_;

  IMPLEMENT_REFCOUNTING(CefRequestContextImpl);
  DISALLOW_COPY_AND_ASSIGN(CefRequestContextImpl);
};

#endif  // CEF_LIBCEF_REQUEST_CONTEXT_IMPL_H_
