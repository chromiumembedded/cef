// Copyright (c) 2022 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_PROCESS_MESSAGE_SMR_IMPL_H_
#define CEF_LIBCEF_COMMON_PROCESS_MESSAGE_SMR_IMPL_H_
#pragma once

#include "include/cef_process_message.h"
#include "include/cef_shared_process_message_builder.h"

#include "base/memory/writable_shared_memory_region.h"

class CefProcessMessageSMRImpl final : public CefProcessMessage {
 public:
  CefProcessMessageSMRImpl(const CefString& name,
                           base::WritableSharedMemoryRegion&& region);
  CefProcessMessageSMRImpl(const CefProcessMessageSMRImpl&) = delete;
  CefProcessMessageSMRImpl& operator=(const CefProcessMessageSMRImpl&) = delete;
  ~CefProcessMessageSMRImpl() override;

  // CefProcessMessage methods.
  bool IsValid() override;
  bool IsReadOnly() override { return true; }
  CefRefPtr<CefProcessMessage> Copy() override { return nullptr; }
  CefString GetName() override;
  CefRefPtr<CefListValue> GetArgumentList() override { return nullptr; }
  CefRefPtr<CefSharedMemoryRegion> GetSharedMemoryRegion() override;
  [[nodiscard]] base::WritableSharedMemoryRegion TakeRegion();

 private:
  const CefString name_;
  base::WritableSharedMemoryRegion region_;

  IMPLEMENT_REFCOUNTING(CefProcessMessageSMRImpl);
};

class CefSharedProcessMessageBuilderImpl final
    : public CefSharedProcessMessageBuilder {
 public:
  CefSharedProcessMessageBuilderImpl(const CefString& name, size_t byte_size);
  CefSharedProcessMessageBuilderImpl(const CefProcessMessageSMRImpl&) = delete;
  CefSharedProcessMessageBuilderImpl& operator=(
      const CefSharedProcessMessageBuilderImpl&) = delete;

  bool IsValid() override;
  size_t Size() override;
  void* Memory() override;
  CefRefPtr<CefProcessMessage> Build() override;

 private:
  const CefString name_;
  base::WritableSharedMemoryRegion region_;
  base::WritableSharedMemoryMapping mapping_;

  IMPLEMENT_REFCOUNTING(CefSharedProcessMessageBuilderImpl);
};

#endif  // CEF_LIBCEF_COMMON_PROCESS_MESSAGE_SMR_IMPL_H_
