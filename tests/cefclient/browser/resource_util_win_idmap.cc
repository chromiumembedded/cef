// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <cstring>

#include "tests/cefclient/browser/resource.h"

namespace client {

int GetResourceId(const char* resource_name) {
  // Map of resource labels to BINARY id values.
  static struct _resource_map {
    const char* name;
    int id;
  } resource_map[] = {
      {"binding.html", IDS_BINDING_HTML},
      {"dialogs.html", IDS_DIALOGS_HTML},
      {"draggable.html", IDS_DRAGGABLE_HTML},
      {"extensions/set_page_color/icon.png",
       IDS_EXTENSIONS_SET_PAGE_COLOR_ICON_PNG},
      {"extensions/set_page_color/manifest.json",
       IDS_EXTENSIONS_SET_PAGE_COLOR_MANIFEST_JSON},
      {"extensions/set_page_color/popup.html",
       IDS_EXTENSIONS_SET_PAGE_COLOR_POPUP_HTML},
      {"extensions/set_page_color/popup.js",
       IDS_EXTENSIONS_SET_PAGE_COLOR_POPUP_JS},
      {"localstorage.html", IDS_LOCALSTORAGE_HTML},
      {"logo.png", IDS_LOGO_PNG},
      {"media_router.html", IDS_MEDIA_ROUTER_HTML},
      {"menu_icon.1x.png", IDS_MENU_ICON_1X_PNG},
      {"menu_icon.2x.png", IDS_MENU_ICON_2X_PNG},
      {"osr_test.html", IDS_OSRTEST_HTML},
      {"other_tests.html", IDS_OTHER_TESTS_HTML},
      {"pdf.html", IDS_PDF_HTML},
      {"pdf.pdf", IDS_PDF_PDF},
      {"performance.html", IDS_PERFORMANCE_HTML},
      {"performance2.html", IDS_PERFORMANCE2_HTML},
      {"preferences.html", IDS_PREFERENCES_HTML},
      {"response_filter.html", IDS_RESPONSE_FILTER_HTML},
      {"server.html", IDS_SERVER_HTML},
      {"transparency.html", IDS_TRANSPARENCY_HTML},
      {"urlrequest.html", IDS_URLREQUEST_HTML},
      {"websocket.html", IDS_WEBSOCKET_HTML},
      {"window.html", IDS_WINDOW_HTML},
      {"window_icon.1x.png", IDS_WINDOW_ICON_1X_PNG},
      {"window_icon.2x.png", IDS_WINDOW_ICON_2X_PNG},
      {"xmlhttprequest.html", IDS_XMLHTTPREQUEST_HTML},
  };

  for (size_t i = 0; i < sizeof(resource_map) / sizeof(_resource_map); ++i) {
    if (!strcmp(resource_map[i].name, resource_name))
      return resource_map[i].id;
  }

  return 0;
}

}  // namespace client
