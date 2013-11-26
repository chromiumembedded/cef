// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cefsimple/simple_handler.h"

#include <gtk/gtk.h>
#include <string>

#include "cefsimple/util.h"
#include "include/cef_browser.h"

void SimpleHandler::OnTitleChange(CefRefPtr<CefBrowser> browser,
                                  const CefString& title) {
  REQUIRE_UI_THREAD();

  GtkWidget* window = gtk_widget_get_ancestor(
      GTK_WIDGET(browser->GetHost()->GetWindowHandle()),
      GTK_TYPE_WINDOW);
  std::string titleStr(title);
  gtk_window_set_title(GTK_WINDOW(window), titleStr.c_str());
}
