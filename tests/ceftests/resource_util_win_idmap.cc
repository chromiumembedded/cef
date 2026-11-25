// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <cstring>

#include "tests/ceftests/resource.h"

namespace client {

int GetResourceId(const char* resource_name) {
  // Map of resource labels to BINARY id values.
  static struct _resource_map {
    const char* name;
    int id;
  } resource_map[] = {
      {"frame_id_25fps.webm", IDS_FRAME_ID_25FPS_WEBM},
      {"frame_id_50fps.webm", IDS_FRAME_ID_50FPS_WEBM},
      {"frame_id_60fps.webm", IDS_FRAME_ID_60FPS_WEBM},
      {"osr_test.html", IDS_OSRTEST_HTML},
      {"pdf.html", IDS_PDF_HTML},
      {"pdf.pdf", IDS_PDF_PDF},
      {"window_icon.1x.png", IDS_WINDOW_ICON_1X_PNG},
      {"window_icon.2x.png", IDS_WINDOW_ICON_2X_PNG},
  };

  for (auto& i : resource_map) {
    if (!strcmp(i.name, resource_name)) {
      return i.id;
    }
  }

  return 0;
}

}  // namespace client
