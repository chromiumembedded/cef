// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_REQUEST_CONTEXT_IMPL_H_
#define CEF_LIBCEF_REQUEST_CONTEXT_IMPL_H_
#pragma once

#include "include/cef_request_context.h"

class CefBrowserContext;

class CefRequestContextImpl : public CefRequestContext {
 public:
  explicit CefRequestContextImpl(CefBrowserContext* browser_context);
  explicit CefRequestContextImpl(CefRefPtr<CefRequestContextHandler> handler);
  virtual ~CefRequestContextImpl();

  CefBrowserContext* GetOrCreateBrowserContext();

  virtual bool IsSame(CefRefPtr<CefRequestContext> other) OVERRIDE;
  virtual bool IsGlobal() OVERRIDE;
  virtual CefRefPtr<CefRequestContextHandler> GetHandler() OVERRIDE;

 protected:
  CefBrowserContext* browser_context_;
  CefRefPtr<CefRequestContextHandler> handler_;

  // Used to uniquely identify CefRequestContext objects before an associated
  // CefBrowserContext has been created.
  int unique_id_;

  IMPLEMENT_REFCOUNTING(CefRequestContextImpl);
  DISALLOW_COPY_AND_ASSIGN(CefRequestContextImpl);
};

#endif  // CEF_LIBCEF_REQUEST_CONTEXT_IMPL_H_
