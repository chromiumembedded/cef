// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_WEB_URLREQUEST_IMPL_H_
#define CEF_LIBCEF_WEB_URLREQUEST_IMPL_H_
#pragma once

#include "include/cef_web_urlrequest.h"
#include "base/memory/ref_counted.h"

class CefWebURLRequestImpl : public CefWebURLRequest {
 public:
  class Context;

  explicit CefWebURLRequestImpl(
      CefRefPtr<CefWebURLRequestClient> handler);
  virtual ~CefWebURLRequestImpl();

  // Can be called on any thread.
  virtual RequestState GetState() OVERRIDE;
  virtual void Cancel() OVERRIDE;

  // Can only be called on the UI thread.
  void DoSend(CefRefPtr<CefRequest> request);
  void DoCancel();
  void DoStateChange(RequestState newState);

  CefRefPtr<CefWebURLRequestClient> GetHandler() { return handler_; }

  static bool ImplementsThreadSafeReferenceCounting() { return true; }

 protected:
  CefRefPtr<CefWebURLRequestClient> handler_;

  // The below parameters are only modified on the UI thread.
  RequestState state_;
  scoped_refptr<Context> context_;

  IMPLEMENT_REFCOUNTING(CefWebURLRequestImpl);
  IMPLEMENT_LOCKING(CefWebURLRequestImpl);
};

#endif  // CEF_LIBCEF_WEB_URLREQUEST_IMPL_H_
