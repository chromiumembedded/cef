// Copyright (c) 2016 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/views/display_impl.h"

#include "libcef/browser/views/view_util.h"

#include "ui/display/screen.h"

// static
CefRefPtr<CefDisplay> CefDisplay::GetPrimaryDisplay() {
  CEF_REQUIRE_UIT_RETURN(nullptr);
  return new CefDisplayImpl(display::Screen::GetScreen()->GetPrimaryDisplay());
}

// static
CefRefPtr<CefDisplay> CefDisplay::GetDisplayNearestPoint(
    const CefPoint& point,
    bool input_pixel_coords) {
  CEF_REQUIRE_UIT_RETURN(nullptr);
  return new CefDisplayImpl(view_util::GetDisplayNearestPoint(
      gfx::Point(point.x, point.y), input_pixel_coords));
}

// static
CefRefPtr<CefDisplay> CefDisplay::GetDisplayMatchingBounds(
    const CefRect& bounds,
    bool input_pixel_coords) {
  CEF_REQUIRE_UIT_RETURN(nullptr);
  return new CefDisplayImpl(view_util::GetDisplayMatchingBounds(
      gfx::Rect(bounds.x, bounds.y, bounds.width, bounds.height),
      input_pixel_coords));
}

// static
size_t CefDisplay::GetDisplayCount() {
  CEF_REQUIRE_UIT_RETURN(0U);
  return static_cast<size_t>(display::Screen::GetScreen()->GetNumDisplays());
}

// static
void CefDisplay::GetAllDisplays(std::vector<CefRefPtr<CefDisplay> >& displays) {
  CEF_REQUIRE_UIT_RETURN_VOID();

  displays.clear();

  typedef std::vector<display::Display> DisplayVector;
  DisplayVector vec = display::Screen::GetScreen()->GetAllDisplays();
  for (size_t i = 0; i < vec.size(); ++i)
    displays.push_back(new CefDisplayImpl(vec[i]));
}

CefDisplayImpl::CefDisplayImpl(const display::Display& display)
    : display_(display) {
  CEF_REQUIRE_UIT();
}

CefDisplayImpl::~CefDisplayImpl() {
  CEF_REQUIRE_UIT();
}

int64 CefDisplayImpl::GetID() {
  CEF_REQUIRE_UIT_RETURN(-1);
  return display_.id();
}

float CefDisplayImpl::GetDeviceScaleFactor() {
  CEF_REQUIRE_UIT_RETURN(0.0f);
  return display_.device_scale_factor();
}

void CefDisplayImpl::ConvertPointToPixels(CefPoint& point) {
  CEF_REQUIRE_UIT_RETURN_VOID();
  gfx::Point gfx_point(point.x, point.y);
  view_util::ConvertPointToPixels(&gfx_point, display_.device_scale_factor());
  point = CefPoint(gfx_point.x(), gfx_point.y());
}

void CefDisplayImpl::ConvertPointFromPixels(CefPoint& point) {
  CEF_REQUIRE_UIT_RETURN_VOID();
  gfx::Point gfx_point(point.x, point.y);
  view_util::ConvertPointFromPixels(&gfx_point, display_.device_scale_factor());
  point = CefPoint(gfx_point.x(), gfx_point.y());
}

CefRect CefDisplayImpl::GetBounds() {
  CEF_REQUIRE_UIT_RETURN(CefRect());
  const gfx::Rect& gfx_rect = display_.bounds();
  return CefRect(gfx_rect.x(), gfx_rect.y(),
                 gfx_rect.width(), gfx_rect.height());
}

CefRect CefDisplayImpl::GetWorkArea() {
  CEF_REQUIRE_UIT_RETURN(CefRect());
  const gfx::Rect& gfx_rect = display_.work_area();
  return CefRect(gfx_rect.x(), gfx_rect.y(),
                 gfx_rect.width(), gfx_rect.height());
}

int CefDisplayImpl::GetRotation() {
  CEF_REQUIRE_UIT_RETURN(0);
  return display_.RotationAsDegree();
}
