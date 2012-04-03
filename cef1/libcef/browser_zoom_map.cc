// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser_zoom_map.h"
#include "libcef/cef_thread.h"

ZoomMap* ZoomMap::GetInstance() {
  return Singleton<ZoomMap>::get();
}

void ZoomMap::set(const GURL& url, double zoomLevel) {
  REQUIRE_UIT();

  if (zoomLevel == 0.) {
    // Remove the entry for this host.
    Map::iterator iter = map_.find(url.host());
    if (iter != map_.end())
      map_.erase(iter);
  } else {
    // Update the entry for this host.
    map_[url.host()] = zoomLevel;
  }
}

bool ZoomMap::get(const GURL& url, double& zoomLevel) {
  REQUIRE_UIT();

  Map::const_iterator iter = map_.find(url.host());
  if (iter == map_.end())
    return false;

  zoomLevel = iter->second;
  return true;
}
