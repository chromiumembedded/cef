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
  } resource_map[] = {{"binary_transfer.html", IDS_BINARY_TRANSFER_HTML},
                      {"binding.html", IDS_BINDING_HTML},
                      {"config.html", IDS_CONFIG_HTML},
                      {"dialogs.html", IDS_DIALOGS_HTML},
                      {"draggable.html", IDS_DRAGGABLE_HTML},
                      {"hang.html", IDS_HANG_HTML},
                      {"ipc_performance.html", IDS_IPC_PERFORMANCE_HTML},
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
                      {"task_manager.html", IDS_TASK_MANAGER_HTML},
                      {"transparency.html", IDS_TRANSPARENCY_HTML},
                      {"urlrequest.html", IDS_URLREQUEST_HTML},
                      {"websocket.html", IDS_WEBSOCKET_HTML},
                      {"window.html", IDS_WINDOW_HTML},
                      {"window_icon.1x.png", IDS_WINDOW_ICON_1X_PNG},
                      {"window_icon.2x.png", IDS_WINDOW_ICON_2X_PNG},
                      {"xmlhttprequest.html", IDS_XMLHTTPREQUEST_HTML},
                      {"xmlhttprequest.html", IDS_XMLHTTPREQUEST_HTML}};

  for (auto& i : resource_map) {
    if (!strcmp(i.name, resource_name)) {
      return i.id;
    }
  }

  return 0;
}

}  // namespace client
