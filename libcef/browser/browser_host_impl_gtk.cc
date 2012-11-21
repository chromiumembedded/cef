// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/browser_host_impl.h"

#include <gtk/gtk.h>

#include "libcef/browser/thread_util.h"

#include "base/bind.h"
#include "base/utf_string_conversions.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/file_chooser_params.h"
#include "content/public/common/renderer_preferences.h"
#include "grit/cef_strings.h"
#include "grit/ui_strings.h"
#include "net/base/mime_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

void DestroyBrowser(CefRefPtr<CefBrowserHostImpl> browser) {
  browser->DestroyBrowser();
  browser->Release();
}

void window_destroyed(GtkWidget* widget, CefBrowserHostImpl* browser) {
  // Destroy the browser host after window destruction is complete.
  CEF_POST_TASK(CEF_UIT, base::Bind(DestroyBrowser, browser));
}

std::string GetDescriptionFromMimeType(const std::string& mime_type) {
  // Check for wild card mime types and return an appropriate description.
  static const struct {
    const char* mime_type;
    int string_id;
  } kWildCardMimeTypes[] = {
    { "audio", IDS_APP_AUDIO_FILES },
    { "image", IDS_APP_IMAGE_FILES },
    { "text", IDS_APP_TEXT_FILES },
    { "video", IDS_APP_VIDEO_FILES },
  };

  for (size_t i = 0;
       i < sizeof(kWildCardMimeTypes) / sizeof(kWildCardMimeTypes[0]); ++i) {
    if (mime_type == std::string(kWildCardMimeTypes[i].mime_type) + "/*")
      return l10n_util::GetStringUTF8(kWildCardMimeTypes[i].string_id);
  }

  return std::string();
}

void AddFiltersForAcceptTypes(GtkFileChooser* chooser,
                              const std::vector<string16>& accept_types,
                              bool include_all_files) {
  bool has_filter = false;

  for (size_t i = 0; i < accept_types.size(); ++i) {
    std::string ascii_type = UTF16ToASCII(accept_types[i]);
    if (ascii_type.length()) {
      // Just treat as extension if contains '.' as the first character.
      if (ascii_type[0] == '.') {
        GtkFileFilter* filter = gtk_file_filter_new();
        std::string pattern = "*" + ascii_type;
        gtk_file_filter_add_pattern(filter, pattern.c_str());
        gtk_file_filter_set_name(filter, pattern.c_str());
        gtk_file_chooser_add_filter(chooser, filter);
        if (!has_filter)
          has_filter = true;
      } else {
        // Otherwise convert mime type to one or more extensions.
        GtkFileFilter* filter = NULL;
        std::string description = GetDescriptionFromMimeType(ascii_type);
        bool description_from_ext = description.empty();

        std::vector<FilePath::StringType> ext;
        net::GetExtensionsForMimeType(ascii_type, &ext);
        for (size_t x = 0; x < ext.size(); ++x) {
          if (!filter)
            filter = gtk_file_filter_new();
          std::string pattern = "*." + ext[x];
          gtk_file_filter_add_pattern(filter, pattern.c_str());

          if (description_from_ext) {
            if (x != 0)
              description += ";";
            description += pattern;
          }
        }

        if (filter) {
          gtk_file_filter_set_name(filter, description.c_str());
          gtk_file_chooser_add_filter(chooser, filter);
          if (!has_filter)
            has_filter = true;
        }
      }
    }
  }

  // Add the *.* filter, but only if we have added other filters (otherwise it
  // is implied).
  if (include_all_files && has_filter) {
    GtkFileFilter* filter = gtk_file_filter_new();
    gtk_file_filter_add_pattern(filter, "*");
    gtk_file_filter_set_name(filter,
        l10n_util::GetStringUTF8(IDS_SAVEAS_ALL_FILES).c_str());
    gtk_file_chooser_add_filter(chooser, filter);
  }
}

bool RunFileDialog(const content::FileChooserParams& params,
                   CefWindowHandle widget,
                   std::vector<FilePath>* files) {
  GtkFileChooserAction action;
  const gchar* accept_button;
  if (params.mode == content::FileChooserParams::Open ||
      params.mode == content::FileChooserParams::OpenMultiple) {
    action = GTK_FILE_CHOOSER_ACTION_OPEN;
    accept_button = GTK_STOCK_OPEN;
  } else if (params.mode == content::FileChooserParams::Save) {
    action = GTK_FILE_CHOOSER_ACTION_SAVE;
    accept_button = GTK_STOCK_SAVE;
  } else {
    NOTREACHED();
    return false;
  }

  // Consider default file name if any.
  FilePath default_file_name(params.default_file_name);

  std::string base_name;
  if (!default_file_name.empty())
    base_name = default_file_name.BaseName().value();

  std::string title;
  if (!params.title.empty()) {
    title = UTF16ToUTF8(params.title);
  } else {
    int string_id = 0;
    switch (params.mode) {
      case content::FileChooserParams::Open:
        string_id = IDS_OPEN_FILE_DIALOG_TITLE;
        break;
      case content::FileChooserParams::OpenMultiple:
        string_id = IDS_OPEN_FILES_DIALOG_TITLE;
        break;
      case content::FileChooserParams::Save:
        string_id = IDS_SAVE_AS_DIALOG_TITLE;
        break;
      default:
        break;
    }
    title = l10n_util::GetStringUTF8(string_id);
  }

  GtkWidget* window = gtk_widget_get_toplevel(GTK_WIDGET(widget));
  GtkWidget* dialog = gtk_file_chooser_dialog_new(
      title.c_str(),
      GTK_WINDOW(window),
      action,
      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
      accept_button, GTK_RESPONSE_ACCEPT,
      NULL);

  if (params.mode == content::FileChooserParams::OpenMultiple) {
    gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), TRUE);
  } else if (params.mode == content::FileChooserParams::Save) {
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog),
                                                   TRUE);
  }

  if (params.mode == content::FileChooserParams::Save && !base_name.empty()) {
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog),
                                      base_name.c_str());
  }

  AddFiltersForAcceptTypes(GTK_FILE_CHOOSER(dialog), params.accept_types, true);

  bool success = false;

  if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
    if (params.mode == content::FileChooserParams::Open ||
        params.mode == content::FileChooserParams::Save) {
      char* filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
      files->push_back(FilePath(filename));
      success = true;
    } else if (params.mode == content::FileChooserParams::OpenMultiple) {
      GSList* filenames =
          gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog));
      if (filenames) {
        for (GSList* iter = filenames; iter != NULL;
             iter = g_slist_next(iter)) {
          FilePath path(static_cast<char*>(iter->data));
          g_free(iter->data);
          files->push_back(path);
        }
        g_slist_free(filenames);
        success = true;
      }
    }
  }

  gtk_widget_destroy(dialog);

  return success;
}

}  // namespace

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
  window_info_.widget = web_contents_->GetView()->GetNativeView();
  gtk_container_add(GTK_CONTAINER(window_info_.parent_widget),
                    window_info_.widget);

  g_signal_connect(G_OBJECT(window_info_.widget), "destroy",
                   G_CALLBACK(window_destroyed), this);

  // As an additional requirement on Linux, we must set the colors for the
  // render widgets in webkit.
  content::RendererPreferences* prefs =
      web_contents_->GetMutableRendererPrefs();
  prefs->focus_ring_color = SkColorSetARGB(255, 229, 151, 0);
  prefs->thumb_active_color = SkColorSetRGB(244, 244, 244);
  prefs->thumb_inactive_color = SkColorSetRGB(234, 234, 234);
  prefs->track_color = SkColorSetRGB(211, 211, 211);

  prefs->active_selection_bg_color = SkColorSetRGB(30, 144, 255);
  prefs->active_selection_fg_color = SK_ColorWHITE;
  prefs->inactive_selection_bg_color = SkColorSetRGB(200, 200, 200);
  prefs->inactive_selection_fg_color = SkColorSetRGB(50, 50, 50);

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

void CefBrowserHostImpl::PlatformHandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  // TODO(cef): Is something required here to handle shortcut keys?
}

void CefBrowserHostImpl::PlatformRunFileChooser(
    const content::FileChooserParams& params,
    RunFileChooserCallback callback) {
  std::vector<FilePath> files;

  if (params.mode == content::FileChooserParams::Open ||
      params.mode == content::FileChooserParams::OpenMultiple ||
      params.mode == content::FileChooserParams::Save) {
    ::RunFileDialog(params, PlatformGetWindowHandle(), &files);
  } else {
    NOTIMPLEMENTED();
  }

  callback.Run(files);
}

void CefBrowserHostImpl::PlatformHandleExternalProtocol(const GURL& url) {
}

//static
bool CefBrowserHostImpl::IsWindowRenderingDisabled(const CefWindowInfo& info) {
  // TODO(port): Implement this method as part of off-screen rendering support.
  return false;
}

// static
bool CefBrowserHostImpl::PlatformTranslateKeyEvent(
    gfx::NativeEvent& native_event, const CefKeyEvent& event) {
  // TODO(port): Implement this method as part of off-screen rendering support.
  NOTIMPLEMENTED();
  return false;
}

// static
bool CefBrowserHostImpl::PlatformTranslateClickEvent(
    WebKit::WebMouseEvent& ev, 
    int x, int y, MouseButtonType type, 
    bool mouseUp, int clickCount) {
  // TODO(port): Implement this method as part of off-screen rendering support. 
  NOTIMPLEMENTED();
  return false;
}

// static
bool CefBrowserHostImpl::PlatformTranslateMoveEvent(
    WebKit::WebMouseEvent& ev,
    int x, int y, bool mouseLeave) {
  // TODO(port): Implement this method as part of off-screen rendering support.
  NOTIMPLEMENTED();
  return false;
}

// static
bool CefBrowserHostImpl::PlatformTranslateWheelEvent(
    WebKit::WebMouseWheelEvent& ev,
    int x, int y, int deltaX, int deltaY) {
  // TODO(port): Implement this method as part of off-screen rendering support.
  NOTIMPLEMENTED();
  return false;
}
