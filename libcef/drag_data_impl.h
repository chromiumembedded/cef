// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef _DRAG_DATA_IMPL_H
#define _DRAG_DATA_IMPL_H

#include "../include/cef.h"
#include "webkit/glue/webdropdata.h"

// Implementation of CefDragData.
class CefDragDataImpl : public CefDragData
{
public:
  explicit CefDragDataImpl(const WebDropData& data);

  virtual bool IsLink();
  virtual bool IsFragment();
  virtual bool IsFile();
  virtual CefString GetLinkURL();
  virtual CefString GetLinkTitle();
  virtual CefString GetLinkMetadata();
  virtual CefString GetFragmentText();
  virtual CefString GetFragmentHtml();
  virtual CefString GetFragmentBaseURL();
  virtual CefString GetFileExtension();
  virtual CefString GetFileName();
  virtual bool GetFileNames(std::vector<CefString>& names);

protected:
  WebDropData data_;

  IMPLEMENT_REFCOUNTING(CefDragDataImpl);
};

#endif // _DRAG_DATA_IMPL_H
