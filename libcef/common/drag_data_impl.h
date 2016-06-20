// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_DRAG_DATA_IMPL_H_
#define CEF_LIBCEF_COMMON_DRAG_DATA_IMPL_H_
#pragma once

#include "include/cef_drag_data.h"

#include <vector>

#include "base/synchronization/lock.h"
#include "content/public/common/drop_data.h"

// Implementation of CefDragData.
class CefDragDataImpl : public CefDragData {
 public:
  CefDragDataImpl();
  explicit CefDragDataImpl(const content::DropData& data);

  CefRefPtr<CefDragData> Clone() override;
  bool IsReadOnly() override;
  bool IsLink() override;
  bool IsFragment() override;
  bool IsFile() override;
  CefString GetLinkURL() override;
  CefString GetLinkTitle() override;
  CefString GetLinkMetadata() override;
  CefString GetFragmentText() override;
  CefString GetFragmentHtml() override;
  CefString GetFragmentBaseURL() override;
  CefString GetFileName() override;
  size_t GetFileContents(CefRefPtr<CefStreamWriter> writer) override;
  bool GetFileNames(std::vector<CefString>& names) override;
  void SetLinkURL(const CefString& url) override;
  void SetLinkTitle(const CefString& title) override;
  void SetLinkMetadata(const CefString& data) override;
  void SetFragmentText(const CefString& text) override;
  void SetFragmentHtml(const CefString& fragment) override;
  void SetFragmentBaseURL(const CefString& fragment) override;
  void ResetFileContents() override;
  void AddFile(const CefString& path, const CefString& display_name) override;

  // This method is not safe. Use Lock/Unlock to get mutually exclusive access.
  content::DropData* drop_data() {
    return &data_;
  }

  void SetReadOnly(bool read_only);

  base::Lock& lock() { return lock_; }

 private:
  content::DropData data_;

  // True if this object is read-only.
  bool read_only_;

  base::Lock lock_;

  IMPLEMENT_REFCOUNTING(CefDragDataImpl);
};

#endif  // CEF_LIBCEF_COMMON_DRAG_DATA_IMPL_H_
