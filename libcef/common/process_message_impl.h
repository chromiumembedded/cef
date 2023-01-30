// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_PROCESS_MESSAGE_IMPL_H_
#define CEF_LIBCEF_COMMON_PROCESS_MESSAGE_IMPL_H_
#pragma once

#include "include/cef_process_message.h"

#include "base/values.h"

// CefProcessMessage implementation.
class CefProcessMessageImpl : public CefProcessMessage {
 public:
  // Constructor for referencing existing |arguments|.
  CefProcessMessageImpl(const CefString& name,
                        CefRefPtr<CefListValue> arguments);

  // Constructor for creating a new CefListValue that takes ownership of
  // |arguments|.
  CefProcessMessageImpl(const CefString& name,
                        base::Value::List arguments,
                        bool read_only);

  CefProcessMessageImpl(const CefProcessMessageImpl&) = delete;
  CefProcessMessageImpl& operator=(const CefProcessMessageImpl&) = delete;

  ~CefProcessMessageImpl() override;

  // Transfer ownership of the underlying argument list to the caller, or create
  // a copy if the argument list is already owned by something else.
  // TODO: Pass by reference instead of ownership if/when Mojo adds support
  // for that.
  [[nodiscard]] base::Value::List TakeArgumentList();

  // CefProcessMessage methods.
  bool IsValid() override;
  bool IsReadOnly() override;
  CefRefPtr<CefProcessMessage> Copy() override;
  CefString GetName() override;
  CefRefPtr<CefListValue> GetArgumentList() override;
  CefRefPtr<CefSharedMemoryRegion> GetSharedMemoryRegion() override {
    return nullptr;
  }

 private:
  const CefString name_;
  CefRefPtr<CefListValue> arguments_;

  IMPLEMENT_REFCOUNTING(CefProcessMessageImpl);
};

#endif  // CEF_LIBCEF_COMMON_PROCESS_MESSAGE_IMPL_H_
