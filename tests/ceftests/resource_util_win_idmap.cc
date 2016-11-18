// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <cstring>

#include "tests/ceftests/resource.h"

namespace client {

int GetResourceId(const char* resource_name) {
  // Map of resource labels to BINARY id values.
  static struct _resource_map {
    char* name;
    int id;
  } resource_map[] = {
    {"osr_test.html", IDS_OSRTEST_HTML},
    {"pdf.html", IDS_PDF_HTML},
    {"pdf.pdf", IDS_PDF_PDF},
    {"window_icon.1x.png", IDS_WINDOW_ICON_1X_PNG},
    {"window_icon.2x.png", IDS_WINDOW_ICON_2X_PNG},
  };

  for (int i = 0; i < sizeof(resource_map)/sizeof(_resource_map); ++i) {
    if (!strcmp(resource_map[i].name, resource_name))
      return resource_map[i].id;
  }
  
  return 0;
}

}  // namespace client
