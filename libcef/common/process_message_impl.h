// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_PROCESS_MESSAGE_IMPL_H_
#define CEF_LIBCEF_COMMON_PROCESS_MESSAGE_IMPL_H_
#pragma once

#include "include/cef_process_message.h"

namespace base {
class ListValue;
}

// CefProcessMessage implementation.
class CefProcessMessageImpl : public CefProcessMessage {
 public:
  // Constructor for an owned list of arguments.
  CefProcessMessageImpl(const CefString& name,
                        CefRefPtr<CefListValue> arguments);

  // Constructor for an unowned list of arguments.
  // Call Detach() when |arguments| is no longer valid.
  CefProcessMessageImpl(const CefString& name,
                        base::ListValue* arguments,
                        bool read_only);

  ~CefProcessMessageImpl() override;

  // Stop referencing the underlying argument list (which we never owned).
  void Detach();

  // Transfer ownership of the underlying argument list to the caller.
  // TODO: Pass by reference instead of ownership if/when Mojo adds support
  // for that.
  base::ListValue TakeArgumentList() WARN_UNUSED_RESULT;

  // CefProcessMessage methods.
  bool IsValid() override;
  bool IsReadOnly() override;
  CefRefPtr<CefProcessMessage> Copy() override;
  CefString GetName() override;
  CefRefPtr<CefListValue> GetArgumentList() override;

 private:
  const CefString name_;
  CefRefPtr<CefListValue> arguments_;
  const bool should_detach_ = false;

  IMPLEMENT_REFCOUNTING(CefProcessMessageImpl);
  DISALLOW_COPY_AND_ASSIGN(CefProcessMessageImpl);
};

#endif  // CEF_LIBCEF_COMMON_PROCESS_MESSAGE_IMPL_H_
