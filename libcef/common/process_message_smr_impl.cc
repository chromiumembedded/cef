// Copyright (c) 2022 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/common/process_message_smr_impl.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory_mapping.h"

namespace {

class CefSharedMemoryRegionImpl final : public CefSharedMemoryRegion {
 public:
  explicit CefSharedMemoryRegionImpl(
      base::WritableSharedMemoryMapping&& mapping)
      : mapping_(std::move(mapping)) {}
  CefSharedMemoryRegionImpl(const CefSharedMemoryRegionImpl&) = delete;
  CefSharedMemoryRegionImpl& operator=(const CefSharedMemoryRegionImpl&) =
      delete;

  // CefSharedMemoryRegion methods
  bool IsValid() override { return mapping_.IsValid(); }
  size_t Size() override { return IsValid() ? mapping_.size() : 0; }
  void* Memory() override { return mapping_.memory(); }

 private:
  base::WritableSharedMemoryMapping mapping_;
  IMPLEMENT_REFCOUNTING(CefSharedMemoryRegionImpl);
};

}  // namespace

CefProcessMessageSMRImpl::CefProcessMessageSMRImpl(
    const CefString& name,
    base::WritableSharedMemoryRegion&& region)
    : name_(name), region_(std::move(region)) {
  DCHECK(!name_.empty());
  DCHECK(region_.IsValid());
}

CefProcessMessageSMRImpl::~CefProcessMessageSMRImpl() = default;

bool CefProcessMessageSMRImpl::IsValid() {
  return region_.IsValid();
}

CefString CefProcessMessageSMRImpl::GetName() {
  return name_;
}

CefRefPtr<CefSharedMemoryRegion>
CefProcessMessageSMRImpl::GetSharedMemoryRegion() {
  return new CefSharedMemoryRegionImpl(region_.Map());
}

base::WritableSharedMemoryRegion CefProcessMessageSMRImpl::TakeRegion() {
  return std::move(region_);
}

// static
CefRefPtr<CefSharedProcessMessageBuilder>
CefSharedProcessMessageBuilder::Create(const CefString& name,
                                       size_t byte_size) {
  return new CefSharedProcessMessageBuilderImpl(name, byte_size);
}

CefSharedProcessMessageBuilderImpl::CefSharedProcessMessageBuilderImpl(
    const CefString& name,
    size_t byte_size)
    : name_(name),
      region_(base::WritableSharedMemoryRegion::Create(byte_size)),
      mapping_(region_.Map()) {}

bool CefSharedProcessMessageBuilderImpl::IsValid() {
  return region_.IsValid() && mapping_.IsValid();
}

size_t CefSharedProcessMessageBuilderImpl::Size() {
  return !IsValid() ? 0 : region_.GetSize();
}

void* CefSharedProcessMessageBuilderImpl::Memory() {
  return !IsValid() ? nullptr : mapping_.memory();
}

CefRefPtr<CefProcessMessage> CefSharedProcessMessageBuilderImpl::Build() {
  if (!IsValid()) {
    return nullptr;
  }

  // Invalidate mappping
  mapping_ = base::WritableSharedMemoryMapping();
  return new CefProcessMessageSMRImpl(name_, std::move(region_));
}
