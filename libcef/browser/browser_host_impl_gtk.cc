// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/browser_host_impl.h"

#include <gtk/gtk.h>
#include <sys/sysinfo.h>

#include "libcef/browser/thread_util.h"

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/file_chooser_params.h"
#include "content/public/common/renderer_preferences.h"
#include "grit/cef_strings.h"
#include "grit/ui_strings.h"
#include "net/base/mime_util.h"
#include "third_party/WebKit/public/web/gtk/WebInputEventFactory.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

void DestroyBrowser(CefRefPtr<CefBrowserHostImpl> browser) {
  // Force the browser to be destroyed and release the reference added in
  // PlatformCreateWindow().
  browser->WindowDestroyed();
}

void browser_destroy(GtkWidget* widget, CefBrowserHostImpl* browser) {
  // Destroy the browser host after window destruction is complete.
  CEF_POST_TASK(CEF_UIT, base::Bind(DestroyBrowser, browser));
}

void window_destroy(GtkWidget* widget, gpointer data) {
}

gboolean window_delete_event(GtkWidget* widget, GdkEvent* event,
                             CefBrowserHostImpl* browser) {
  // Protect against multiple requests to close while the close is pending.
  if (browser && browser->destruction_state() <=
      CefBrowserHostImpl::DESTRUCTION_STATE_PENDING) {
    if (browser->destruction_state() ==
        CefBrowserHostImpl::DESTRUCTION_STATE_NONE) {
      // Request that the browser close.
      browser->CloseBrowser(false);
    }

    // Cancel the close.
    return TRUE;
  }

  // Allow the close.
  return FALSE;
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

        std::vector<base::FilePath::StringType> ext;
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
                   std::vector<base::FilePath>* files) {
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
  base::FilePath default_file_name(params.default_file_name);

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
      files->push_back(base::FilePath(filename));
      success = true;
    } else if (params.mode == content::FileChooserParams::OpenMultiple) {
      GSList* filenames =
          gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dialog));
      if (filenames) {
        for (GSList* iter = filenames; iter != NULL;
             iter = g_slist_next(iter)) {
          base::FilePath path(static_cast<char*>(iter->data));
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

// Returns the number of seconds since system boot.
long GetSystemUptime() {
  struct sysinfo info;
  if (sysinfo(&info) == 0)
    return info.uptime;
  return 0;
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

    g_signal_connect(G_OBJECT(window), "destroy",
                     G_CALLBACK(window_destroy), NULL);
    g_signal_connect(G_OBJECT(window), "delete_event",
                     G_CALLBACK(window_delete_event), this);

    window_info_.parent_widget = parentView;
  }

  // Add a reference that will be released in the destroy handler.
  AddRef();

  // Parent the TabContents to the browser window.
  window_info_.widget = web_contents_->GetView()->GetNativeView();
  gtk_container_add(GTK_CONTAINER(window_info_.parent_widget),
                    window_info_.widget);

  g_signal_connect(G_OBJECT(window_info_.widget), "destroy",
                   G_CALLBACK(browser_destroy), this);

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

    // Send the "delete_event" signal.
    GdkEvent event;
    memset(&event, 0, sizeof(GdkEvent));
    event.any.type = GDK_DELETE;
    event.any.send_event = TRUE;
    event.any.window = window->window;
    gtk_main_do_event(&event);
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
  return IsWindowRenderingDisabled() ?
      window_info_.parent_widget :
      window_info_.widget;
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
  std::vector<base::FilePath> files;

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

// static
bool CefBrowserHostImpl::IsWindowRenderingDisabled(const CefWindowInfo& info) {
  return info.window_rendering_disabled ? true : false;
}

bool CefBrowserHostImpl::IsTransparent() {
  return window_info_.transparent_painting != 0;
}

void CefBrowserHostImpl::PlatformTranslateKeyEvent(
    content::NativeWebKeyboardEvent& result,
    const CefKeyEvent& key_event) {
  // Use a synthetic GdkEventKey in order to obtain the windowsKeyCode member
  // from the NativeWebKeyboardEvent constructor. This is the only member
  // which cannot be easily translated (without hardcoding keyCodes).

  guint state = 0;
  if (key_event.modifiers & EVENTFLAG_SHIFT_DOWN)
    state |= GDK_SHIFT_MASK;
  if (key_event.modifiers & EVENTFLAG_CAPS_LOCK_ON)
    state |= GDK_LOCK_MASK;
  if (key_event.modifiers & EVENTFLAG_CONTROL_DOWN)
    state |= GDK_CONTROL_MASK;
  if (key_event.modifiers & EVENTFLAG_ALT_DOWN)
    state |= GDK_MOD1_MASK;
  if (key_event.modifiers & EVENTFLAG_LEFT_MOUSE_BUTTON)
    state |= GDK_BUTTON1_MASK;
  if (key_event.modifiers & EVENTFLAG_MIDDLE_MOUSE_BUTTON)
    state |= GDK_BUTTON2_MASK;
  if (key_event.modifiers & EVENTFLAG_RIGHT_MOUSE_BUTTON)
    state |= GDK_BUTTON3_MASK;

  GdkKeymap* keymap = gdk_keymap_get_for_display(gdk_display_get_default());

  GdkKeymapKey *keys = NULL;
  gint n_keys = 0;
  if (gdk_keymap_get_entries_for_keyval(keymap, key_event.native_key_code,
                                        &keys, &n_keys)) {
    GdkEventKey event;
    event.type = GDK_KEY_PRESS;
    event.window = NULL;
    event.send_event = 0;
    event.time = 0;
    event.state = state;
    event.keyval = key_event.native_key_code;
    event.length = 0;
    event.string = NULL;
    event.hardware_keycode = keys[0].keycode;
    event.group = keys[0].group;
    event.is_modifier = 0;
    g_free(keys);
    result = content::NativeWebKeyboardEvent(
        reinterpret_cast<GdkEvent*>(&event));
  }

  result.timeStampSeconds = GetSystemUptime();

  switch (key_event.type) {
  case KEYEVENT_RAWKEYDOWN:
  case KEYEVENT_KEYDOWN:
    result.type = WebKit::WebInputEvent::RawKeyDown;
    break;
  case KEYEVENT_KEYUP:
    result.type = WebKit::WebInputEvent::KeyUp;
    break;
  case KEYEVENT_CHAR:
    result.type = WebKit::WebInputEvent::Char;
    break;
  default:
    NOTREACHED();
  }
}

void CefBrowserHostImpl::PlatformTranslateClickEvent(
    WebKit::WebMouseEvent& result,
    const CefMouseEvent& mouse_event,
    MouseButtonType type,
    bool mouseUp, int clickCount) {
  PlatformTranslateMouseEvent(result, mouse_event);

  switch (type) {
  case MBT_LEFT:
    result.type = mouseUp ? WebKit::WebInputEvent::MouseUp :
                            WebKit::WebInputEvent::MouseDown;
    result.button = WebKit::WebMouseEvent::ButtonLeft;
    break;
  case MBT_MIDDLE:
    result.type = mouseUp ? WebKit::WebInputEvent::MouseUp :
                            WebKit::WebInputEvent::MouseDown;
    result.button = WebKit::WebMouseEvent::ButtonMiddle;
    break;
  case MBT_RIGHT:
    result.type = mouseUp ? WebKit::WebInputEvent::MouseUp :
                            WebKit::WebInputEvent::MouseDown;
    result.button = WebKit::WebMouseEvent::ButtonRight;
    break;
  default:
    NOTREACHED();
  }

  result.clickCount = clickCount;
}

void CefBrowserHostImpl::PlatformTranslateMoveEvent(
    WebKit::WebMouseEvent& result,
    const CefMouseEvent& mouse_event,
    bool mouseLeave) {
  PlatformTranslateMouseEvent(result, mouse_event);

  if (!mouseLeave) {
    result.type = WebKit::WebInputEvent::MouseMove;
    if (mouse_event.modifiers & EVENTFLAG_LEFT_MOUSE_BUTTON)
      result.button = WebKit::WebMouseEvent::ButtonLeft;
    else if (mouse_event.modifiers & EVENTFLAG_MIDDLE_MOUSE_BUTTON)
      result.button = WebKit::WebMouseEvent::ButtonMiddle;
    else if (mouse_event.modifiers & EVENTFLAG_RIGHT_MOUSE_BUTTON)
      result.button = WebKit::WebMouseEvent::ButtonRight;
    else
      result.button = WebKit::WebMouseEvent::ButtonNone;
  } else {
    result.type = WebKit::WebInputEvent::MouseLeave;
    result.button = WebKit::WebMouseEvent::ButtonNone;
  }

  result.clickCount = 0;
}

void CefBrowserHostImpl::PlatformTranslateWheelEvent(
    WebKit::WebMouseWheelEvent& result,
    const CefMouseEvent& mouse_event,
    int deltaX, int deltaY) {
  result = WebKit::WebMouseWheelEvent();
  PlatformTranslateMouseEvent(result, mouse_event);

  result.type = WebKit::WebInputEvent::MouseWheel;

  static const double scrollbarPixelsPerGtkTick = 40.0;
  result.deltaX = deltaX;
  result.deltaY = deltaY;
  result.wheelTicksX = result.deltaX / scrollbarPixelsPerGtkTick;
  result.wheelTicksY = result.deltaY / scrollbarPixelsPerGtkTick;
  result.hasPreciseScrollingDeltas = true;

  // Unless the phase and momentumPhase are passed in as parameters to this
  // function, there is no way to know them
  result.phase = WebKit::WebMouseWheelEvent::PhaseNone;
  result.momentumPhase = WebKit::WebMouseWheelEvent::PhaseNone;

  if (mouse_event.modifiers & EVENTFLAG_LEFT_MOUSE_BUTTON)
    result.button = WebKit::WebMouseEvent::ButtonLeft;
  else if (mouse_event.modifiers & EVENTFLAG_MIDDLE_MOUSE_BUTTON)
    result.button = WebKit::WebMouseEvent::ButtonMiddle;
  else if (mouse_event.modifiers & EVENTFLAG_RIGHT_MOUSE_BUTTON)
    result.button = WebKit::WebMouseEvent::ButtonRight;
  else
    result.button = WebKit::WebMouseEvent::ButtonNone;
}

void CefBrowserHostImpl::PlatformTranslateMouseEvent(
    WebKit::WebMouseEvent& result,
    const CefMouseEvent& mouse_event) {
  // position
  result.x = mouse_event.x;
  result.y = mouse_event.y;
  result.windowX = result.x;
  result.windowY = result.y;
  result.globalX = result.x;
  result.globalY = result.y;

  // global position
  if (IsWindowRenderingDisabled()) {
    GetClient()->GetRenderHandler()->GetScreenPoint(GetBrowser(),
        result.x, result.y,
        result.globalX, result.globalY);
  } else {
    GtkWidget* window = gtk_widget_get_toplevel(GetWindowHandle());
    GdkWindow* gdk_window = gtk_widget_get_window(window);
    gint xorigin, yorigin;
    gdk_window_get_root_origin(gdk_window, &xorigin, &yorigin);
    result.globalX = xorigin + result.x;
    result.globalY = yorigin + result.y;
  }

  // modifiers
  result.modifiers |= TranslateModifiers(mouse_event.modifiers);

  // timestamp
  result.timeStampSeconds = GetSystemUptime();
}
