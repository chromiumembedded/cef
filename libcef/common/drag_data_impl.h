// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_DRAG_DATA_IMPL_H_
#define CEF_LIBCEF_COMMON_DRAG_DATA_IMPL_H_
#pragma once

#include <vector>

#include "include/cef_drag_data.h"
#include "content/public/common/drop_data.h"

// Implementation of CefDragData.
class CefDragDataImpl : public CefDragData {
 public:
  explicit CefDragDataImpl(const content::DropData& data);

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
  virtual bool GetFileNames(std::vector<CefString>& names);

 protected:
  content::DropData data_;

  IMPLEMENT_REFCOUNTING(CefDragDataImpl);
};

#endif  // CEF_LIBCEF_COMMON_DRAG_DATA_IMPL_H_
