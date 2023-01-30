// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/common/process_message_impl.h"

#include <memory>

#include "libcef/common/values_impl.h"

#include "base/logging.h"

// static
CefRefPtr<CefProcessMessage> CefProcessMessage::Create(const CefString& name) {
  return new CefProcessMessageImpl(name, CefListValue::Create());
}

CefProcessMessageImpl::CefProcessMessageImpl(const CefString& name,
                                             CefRefPtr<CefListValue> arguments)
    : name_(name), arguments_(arguments) {
  DCHECK(!name_.empty());
  DCHECK(arguments_ && arguments_->IsValid());
}

CefProcessMessageImpl::CefProcessMessageImpl(const CefString& name,
                                             base::Value::List arguments,
                                             bool read_only)
    : name_(name) {
  DCHECK(!name_.empty());

  arguments_ = new CefListValueImpl(std::move(arguments), read_only);
}

CefProcessMessageImpl::~CefProcessMessageImpl() = default;

base::Value::List CefProcessMessageImpl::TakeArgumentList() {
  DCHECK(IsValid());
  CefListValueImpl* value_impl =
      static_cast<CefListValueImpl*>(arguments_.get());
  auto value = value_impl->CopyOrDetachValue(nullptr);
  return std::move(value->GetList());
}

bool CefProcessMessageImpl::IsValid() {
  return arguments_->IsValid();
}

bool CefProcessMessageImpl::IsReadOnly() {
  return arguments_->IsReadOnly();
}

CefRefPtr<CefProcessMessage> CefProcessMessageImpl::Copy() {
  if (!IsValid()) {
    return nullptr;
  }
  return new CefProcessMessageImpl(name_, arguments_->Copy());
}

CefString CefProcessMessageImpl::GetName() {
  return name_;
}

CefRefPtr<CefListValue> CefProcessMessageImpl::GetArgumentList() {
  return arguments_;
}
