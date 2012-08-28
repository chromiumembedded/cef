// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/download_manager_delegate.h"

#include <gtk/gtk.h>

#include "base/string_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"

// static
FilePath CefDownloadManagerDelegate::PlatformChooseDownloadPath(
    content::WebContents* web_contents,
    const FilePath& suggested_path) {
  FilePath result;

  gfx::NativeWindow parent_window =
      web_contents->GetView()->GetTopLevelNativeWindow();
  std::string base_name = suggested_path.BaseName().value();

  GtkWidget* dialog = gtk_file_chooser_dialog_new(
      "Save File",
       parent_window,
       GTK_FILE_CHOOSER_ACTION_SAVE,
       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
       GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
       NULL);
  gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog),
                                                 TRUE);
  gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog),
                                    base_name.c_str());

  if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
    char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
    result = FilePath(filename);
  }
  gtk_widget_destroy(dialog);

  return result;
}
