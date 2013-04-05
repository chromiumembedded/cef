// Copyright (c) 2010 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/browser_settings.h"

#include <string>

#include "libcef/common/cef_switches.h"

#include "base/command_line.h"
#include "include/internal/cef_types_wrappers.h"
#include "webkit/glue/webpreferences.h"

using webkit_glue::WebPreferences;

// Set default preferences based on CEF command-line flags. Chromium command-
// line flags should not exist for these preferences.
void SetDefaults(WebPreferences& web) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(switches::kDefaultEncoding)) {
    web.default_encoding =
        command_line.GetSwitchValueASCII(switches::kDefaultEncoding);
  }

  if (command_line.HasSwitch(switches::kUserStyleSheetLocation)) {
    web.user_style_sheet_location = GURL(
        command_line.GetSwitchValueASCII(switches::kUserStyleSheetLocation));
    if (!web.user_style_sheet_location.is_empty())
      web.user_style_sheet_enabled = true;
  }

  web.javascript_can_open_windows_automatically =
      !command_line.HasSwitch(switches::kDisableJavascriptOpenWindows);
  web.allow_scripts_to_close_windows =
      !command_line.HasSwitch(switches::kDisableJavascriptCloseWindows);
  web.javascript_can_access_clipboard =
      !command_line.HasSwitch(switches::kDisableJavascriptAccessClipboard);
  web.dom_paste_enabled =
      !command_line.HasSwitch(switches::kDisableJavascriptDomPaste);
  web.caret_browsing_enabled =
      command_line.HasSwitch(switches::kEnableCaretBrowsing);
  web.allow_universal_access_from_file_urls =
      command_line.HasSwitch(switches::kAllowUniversalAccessFromFileUrls);
  web.loads_images_automatically =
      !command_line.HasSwitch(switches::kDisableImageLoading);
  web.shrinks_standalone_images_to_fit =
      command_line.HasSwitch(switches::kImageShrinkStandaloneToFit);
  web.text_areas_are_resizable =
      !command_line.HasSwitch(switches::kDisableTextAreaResize);
  web.tabs_to_links =
      !command_line.HasSwitch(switches::kDisableTabToLinks);
  web.author_and_user_styles_enabled =
      !command_line.HasSwitch(switches::kDisableAuthorAndUserStyles);
  web.developer_extras_enabled =
      !command_line.HasSwitch(switches::kDisableDeveloperTools);
}

// Helper macro for setting a WebPreferences variable based on the value of a
// CefBrowserSettings variable.
#define SET_STATE(cef_var, web_var) \
    if (cef_var == STATE_ENABLED) \
      web_var = true; \
    else if (cef_var == STATE_DISABLED) \
      web_var = false;

// Use the preferences from WebContentsImpl::GetWebkitPrefs and the
// WebPreferences constructor by default. Only override features that are
// explicitly enabled or disabled.
void BrowserToWebSettings(const CefBrowserSettings& cef, WebPreferences& web) {
  SetDefaults(web);

  if (cef.standard_font_family.length > 0) {
    web.standard_font_family_map[WebPreferences::kCommonScript] =
        CefString(&cef.standard_font_family);
  }
  if (cef.fixed_font_family.length > 0) {
    web.fixed_font_family_map[WebPreferences::kCommonScript] =
        CefString(&cef.fixed_font_family);
  }
  if (cef.serif_font_family.length > 0) {
    web.serif_font_family_map[WebPreferences::kCommonScript] =
        CefString(&cef.serif_font_family);
  }
  if (cef.sans_serif_font_family.length > 0) {
    web.sans_serif_font_family_map[WebPreferences::kCommonScript] =
        CefString(&cef.sans_serif_font_family);
  }
  if (cef.cursive_font_family.length > 0) {
    web.cursive_font_family_map[WebPreferences::kCommonScript] =
        CefString(&cef.cursive_font_family);
  }
  if (cef.fantasy_font_family.length > 0) {
    web.fantasy_font_family_map[WebPreferences::kCommonScript] =
        CefString(&cef.fantasy_font_family);
  }

  if (cef.default_font_size > 0)
    web.default_font_size = cef.default_font_size;
  if (cef.default_fixed_font_size > 0)
    web.default_fixed_font_size = cef.default_fixed_font_size;
  if (cef.minimum_font_size > 0)
    web.minimum_font_size = cef.minimum_font_size;
  if (cef.minimum_logical_font_size > 0)
    web.minimum_logical_font_size = cef.minimum_logical_font_size;

  if (cef.default_encoding.length > 0)
    web.default_encoding = CefString(&cef.default_encoding);

  if (cef.user_style_sheet_location.length > 0) {
    web.user_style_sheet_enabled = true;
    web.user_style_sheet_location =
        GURL(CefString(&cef.user_style_sheet_location).ToString());
  }

  SET_STATE(cef.remote_fonts, web.remote_fonts_enabled);
  SET_STATE(cef.javascript, web.javascript_enabled);
  SET_STATE(cef.javascript_open_windows,
      web.javascript_can_open_windows_automatically);
  SET_STATE(cef.javascript_close_windows, web.allow_scripts_to_close_windows);
  SET_STATE(cef.javascript_access_clipboard,
      web.javascript_can_access_clipboard);
  SET_STATE(cef.javascript_dom_paste, web.dom_paste_enabled);
  SET_STATE(cef.caret_browsing, web.caret_browsing_enabled);
  SET_STATE(cef.java, web.java_enabled);
  SET_STATE(cef.plugins, web.plugins_enabled);
  SET_STATE(cef.universal_access_from_file_urls,
      web.allow_universal_access_from_file_urls);
  SET_STATE(cef.file_access_from_file_urls,
      web.allow_file_access_from_file_urls);
  SET_STATE(cef.web_security, web.web_security_enabled);
  SET_STATE(cef.image_loading, web.loads_images_automatically);
  SET_STATE(cef.image_shrink_standalone_to_fit,
      web.shrinks_standalone_images_to_fit);
  SET_STATE(cef.text_area_resize, web.text_areas_are_resizable);
  SET_STATE(cef.page_cache, web.uses_page_cache);
  SET_STATE(cef.tab_to_links, web.tabs_to_links);
  SET_STATE(cef.author_and_user_styles, web.author_and_user_styles_enabled);
  SET_STATE(cef.local_storage, web.local_storage_enabled);
  SET_STATE(cef.databases, web.databases_enabled);
  SET_STATE(cef.application_cache, web.application_cache_enabled);

  // Never explicitly enable GPU-related functions in this method because the
  // GPU blacklist is not being checked here.
  if (cef.webgl == STATE_DISABLED)
    web.experimental_webgl_enabled = false;
  if (cef.accelerated_compositing == STATE_DISABLED)
    web.accelerated_compositing_enabled = false;

  SET_STATE(cef.developer_tools, web.developer_extras_enabled);
}
