// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/common/process_message_impl.h"
#include "libcef/common/values_impl.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"

// static
CefRefPtr<CefProcessMessage> CefProcessMessage::Create(const CefString& name) {
  return new CefProcessMessageImpl(name, CefListValue::Create());
}

CefProcessMessageImpl::CefProcessMessageImpl(const CefString& name,
                                             CefRefPtr<CefListValue> arguments)
    : name_(name), arguments_(arguments) {
  DCHECK(!name_.empty());
  DCHECK(arguments_);
}

CefProcessMessageImpl::CefProcessMessageImpl(const CefString& name,
                                             base::ListValue* arguments,
                                             bool read_only)
    : name_(name),
      arguments_(
          new CefListValueImpl(arguments, /*will_delete=*/false, read_only)),
      should_detach_(true) {
  DCHECK(!name_.empty());
}

CefProcessMessageImpl::~CefProcessMessageImpl() {
  DCHECK(!should_detach_ || !arguments_->IsValid());
}

void CefProcessMessageImpl::Detach() {
  DCHECK(IsValid());
  DCHECK(should_detach_);
  CefListValueImpl* value_impl =
      static_cast<CefListValueImpl*>(arguments_.get());
  ignore_result(value_impl->Detach(nullptr));
}

base::ListValue CefProcessMessageImpl::TakeArgumentList() {
  DCHECK(IsValid());
  CefListValueImpl* value_impl =
      static_cast<CefListValueImpl*>(arguments_.get());
  auto value = base::WrapUnique(value_impl->CopyOrDetachValue(nullptr));
  return std::move(*value);
}

bool CefProcessMessageImpl::IsValid() {
  return arguments_->IsValid();
}

bool CefProcessMessageImpl::IsReadOnly() {
  return arguments_->IsReadOnly();
}

CefRefPtr<CefProcessMessage> CefProcessMessageImpl::Copy() {
  if (!IsValid())
    return nullptr;
  return new CefProcessMessageImpl(name_, arguments_->Copy());
}

CefString CefProcessMessageImpl::GetName() {
  return name_;
}

CefRefPtr<CefListValue> CefProcessMessageImpl::GetArgumentList() {
  return arguments_;
}
