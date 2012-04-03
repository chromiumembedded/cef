// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/browser_host_impl.h"

#include <gtk/gtk.h>

#include "libcef/browser/thread_util.h"

#include "base/bind.h"
#include "content/public/browser/web_contents_view.h"

namespace {

void DestroyBrowser(CefRefPtr<CefBrowserHostImpl> browser) {
  browser->DestroyBrowser();
  browser->Release();
}

void window_destroyed(GtkWidget* widget, CefBrowserHostImpl* browser) {
  // Destroy the browser host after window destruction is complete.
  CEF_POST_TASK(CEF_UIT, base::Bind(DestroyBrowser, browser));
}

}  // namespace

bool CefBrowserHostImpl::PlatformViewText(const std::string& text) {
  CEF_REQUIRE_UIT();

  char buff[] = "/tmp/CEFSourceXXXXXX";
  int fd = mkstemp(buff);

  if (fd == -1)
    return false;

  FILE* srcOutput = fdopen(fd, "w+");
  if (!srcOutput)
    return false;

  if (fputs(text.c_str(), srcOutput) < 0) {
    fclose(srcOutput);
    return false;
  }

  fclose(srcOutput);

  std::string newName(buff);
  newName.append(".txt");
  if (rename(buff, newName.c_str()) != 0)
    return false;

  std::string openCommand("xdg-open ");
  openCommand += newName;

  if (system(openCommand.c_str()) != 0)
    return false;

  return true;
}

bool CefBrowserHostImpl::PlatformCreateWindow() {
  GtkWidget* window;
  GtkWidget* parentView = window_info_.parent_widget;

  if (parentView == NULL) {
    // Create a new window.
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);

    parentView = gtk_vbox_new(FALSE, 0);

    gtk_container_add(GTK_CONTAINER(window), parentView);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_widget_show_all(GTK_WIDGET(window));

    window_info_.parent_widget = parentView;
  }

  // Add a reference that will be released in the destroy handler.
  AddRef();

  // Parent the TabContents to the browser window.
  window_info_.widget = tab_contents_->GetView()->GetNativeView();
  gtk_container_add(GTK_CONTAINER(window_info_.parent_widget),
                    window_info_.widget);

  g_signal_connect(G_OBJECT(window_info_.widget), "destroy",
                   G_CALLBACK(window_destroyed), this);

  return true;
}

void CefBrowserHostImpl::PlatformCloseWindow() {
  if (window_info_.widget != NULL) {
    GtkWidget* window =
        gtk_widget_get_toplevel(GTK_WIDGET(window_info_.widget));
    gtk_widget_destroy(window);
  }
}

void CefBrowserHostImpl::PlatformSizeTo(int width, int height) {
  if (window_info_.widget != NULL) {
    GtkWidget* window =
        gtk_widget_get_toplevel(GTK_WIDGET(window_info_.widget));
    gtk_widget_set_size_request(window, width, height);
  }
}

CefWindowHandle CefBrowserHostImpl::PlatformGetWindowHandle() {
  return window_info_.widget;
}
