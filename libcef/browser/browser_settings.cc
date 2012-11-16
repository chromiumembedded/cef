// Copyright (c) 2010 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/browser_settings.h"

#include <string>

#include "include/internal/cef_types_wrappers.h"
#include "webkit/glue/webpreferences.h"

using webkit_glue::WebPreferences;

// Use the preferences from WebContentsImpl::GetWebkitPrefs and the
// WebPreferences constructor by default. Only override features that are
// explicitly enabled or disabled.
void BrowserToWebSettings(const CefBrowserSettings& cef, WebPreferences& web) {
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

  if (cef.javascript_disabled)
    web.javascript_enabled = false;
  if (cef.web_security_disabled)
    web.web_security_enabled = false;
  if (cef.javascript_open_windows_disallowed)
    web.javascript_can_open_windows_automatically = false;
  if (cef.image_load_disabled)
    web.loads_images_automatically = false;
  if (cef.plugins_disabled)
    web.plugins_enabled = false;
  if (cef.dom_paste_disabled)
    web.dom_paste_enabled = false;
  if (cef.developer_tools_disabled)
    web.developer_extras_enabled = false;
  if (cef.site_specific_quirks_disabled)
    web.site_specific_quirks_enabled = false;
  if (cef.shrink_standalone_images_to_fit)
    web.shrinks_standalone_images_to_fit = true;
  if (cef.encoding_detector_enabled)
    web.uses_universal_detector = true;
  if (cef.text_area_resize_disabled)
    web.text_areas_are_resizable = false;
  if (cef.java_disabled)
     web.java_enabled = false;
  if (cef.javascript_close_windows_disallowed)
    web.allow_scripts_to_close_windows = false;
  if (cef.page_cache_disabled)
    web.uses_page_cache = false;
  if (cef.remote_fonts_disabled)
    web.remote_fonts_enabled = true;
  if (cef.javascript_access_clipboard_disallowed)
    web.javascript_can_access_clipboard = false;
  if (cef.xss_auditor_enabled)
    web.xss_auditor_enabled = true;
  if (cef.local_storage_disabled)
    web.local_storage_enabled = false;
  if (cef.databases_disabled)
    web.databases_enabled = false;
  if (cef.application_cache_disabled)
    web.application_cache_enabled = false;
  if (cef.tab_to_links_disabled)
    web.tabs_to_links = false;
  if (cef.caret_browsing_enabled)
    web.caret_browsing_enabled = true;
  if (cef.hyperlink_auditing_disabled)
    web.hyperlink_auditing_enabled = true;
  if (cef.user_style_sheet_enabled &&
      cef.user_style_sheet_location.length > 0) {
    web.user_style_sheet_enabled = true;
    web.user_style_sheet_location =
        GURL(std::string(CefString(&cef.user_style_sheet_location)));
  }
  if (cef.author_and_user_styles_disabled)
    web.author_and_user_styles_enabled = false;
  if (cef.universal_access_from_file_urls_allowed)
    web.allow_universal_access_from_file_urls = true;
  if (cef.file_access_from_file_urls_allowed)
    web.allow_file_access_from_file_urls = true;

  // Never explicitly enable GPU-related functions in this method because the
  // GPU blacklist is not being checked here.
  if (cef.webgl_disabled) {
    web.experimental_webgl_enabled = false;
    web.gl_multisampling_enabled = false;
  }
  if (cef.accelerated_compositing_disabled)
    web.accelerated_compositing_enabled = false;
  if (cef.accelerated_layers_disabled) {
    web.accelerated_compositing_for_3d_transforms_enabled = false;
    web.accelerated_compositing_for_animation_enabled = false;
  }
  if (cef.accelerated_video_disabled)
    web.accelerated_compositing_for_video_enabled = false;
  if (cef.accelerated_2d_canvas_disabled)
    web.accelerated_2d_canvas_enabled = false;
  if (cef.accelerated_plugins_disabled)
    web.accelerated_compositing_for_plugins_enabled = false;
}
