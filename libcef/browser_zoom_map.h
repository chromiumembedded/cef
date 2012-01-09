// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_ZOOM_MAP_H_
#define CEF_LIBCEF_BROWSER_ZOOM_MAP_H_
#pragma once

#include <map>
#include <string>

#include "include/internal/cef_string.h"
#include "base/memory/singleton.h"
#include "googleurl/src/gurl.h"

// Maps the host/domain of a URL to a zoom value.
// NOTE: This class is not thread-safe. It is assumed that the methods will be
// called from the UI thread.
class ZoomMap {
 public:
  // Returns the static ZoomMap instance.
  static ZoomMap* GetInstance();

  // Store |zoomLevel| with key |url|.
  void set(const GURL& url, double zoomLevel);

  // Returns true if there is a |zoomLevel| keyed with |url|, false otherwise.
  // |zoomLevel| is the "out" variable.
  bool get(const GURL& url, double& zoomLevel);

 private:
  typedef std::map<std::string, double> Map;
  Map map_;

  friend struct DefaultSingletonTraits<ZoomMap>;

  ZoomMap() {}
  virtual ~ZoomMap() {}

  DISALLOW_COPY_AND_ASSIGN(ZoomMap);
};

#endif  // CEF_LIBCEF_BROWSER_ZOOM_MAP_H_
