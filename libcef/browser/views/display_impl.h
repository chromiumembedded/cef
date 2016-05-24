// Copyright (c) 2016 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_VIEWS_DISPLAY_IMPL_H_
#define CEF_LIBCEF_BROWSER_VIEWS_DISPLAY_IMPL_H_
#pragma once

#include "include/views/cef_display.h"
#include "libcef/browser/thread_util.h"

#include "ui/display/display.h"

class CefDisplayImpl : public CefDisplay {
 public:
  explicit CefDisplayImpl(const display::Display& display);
  ~CefDisplayImpl() override;

  // CefDisplay methods:
  int64 GetID() override;
  float GetDeviceScaleFactor() override;
  void ConvertPointToPixels(CefPoint& point) override;
  void ConvertPointFromPixels(CefPoint& point) override;
  CefRect GetBounds() override;
  CefRect GetWorkArea() override;
  int GetRotation() override;

  const display::Display& display() const { return display_; }

 private:
  display::Display display_;

  IMPLEMENT_REFCOUNTING_DELETE_ON_UIT(CefDisplayImpl);
  DISALLOW_COPY_AND_ASSIGN(CefDisplayImpl);
};

#endif  // CEF_LIBCEF_BROWSER_VIEWS_DISPLAY_IMPL_H_
