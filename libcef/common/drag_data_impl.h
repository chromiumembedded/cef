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
  virtual CefRefPtr<CefDragData> Clone();
  virtual bool IsReadOnly();

  virtual bool IsLink();
  virtual bool IsFragment();
  virtual bool IsFile();
  virtual CefString GetLinkURL();
  virtual CefString GetLinkTitle();
  virtual CefString GetLinkMetadata();
  virtual CefString GetFragmentText();
  virtual CefString GetFragmentHtml();
  virtual CefString GetFragmentBaseURL();
  virtual CefString GetFileName();
  virtual size_t GetFileContents(CefRefPtr<CefStreamWriter> writer);
  virtual bool GetFileNames(std::vector<CefString>& names);

  virtual void SetLinkURL(const CefString& url);
  virtual void SetLinkTitle(const CefString& title);
  virtual void SetLinkMetadata(const CefString& data);
  virtual void SetFragmentText(const CefString& text);
  virtual void SetFragmentHtml(const CefString& fragment);
  virtual void SetFragmentBaseURL(const CefString& fragment);
  virtual void ResetFileContents();
  virtual void AddFile(const CefString& path, const CefString& display_name);

  // This method is not safe. Use Lock/Unlock to get mutually exclusive access.
  const content::DropData& drop_data() {
    return data_;
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
