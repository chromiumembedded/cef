// Copyright (c) 2010 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/prefs/renderer_prefs.h"

#include <string>

#include "libcef/browser/alloy/alloy_browser_host_impl.h"
#include "libcef/browser/context.h"
#include "libcef/browser/extensions/browser_extensions_util.h"
#include "libcef/common/cef_switches.h"
#include "libcef/common/extensions/extensions_util.h"
#include "libcef/features/runtime_checks.h"

#include "base/command_line.h"
#include "base/i18n/character_encoding.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chrome/browser/accessibility/animation_policy_prefs.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/extensions/extension_webkit_preferences.h"
#include "chrome/browser/font_family_cache.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/command_line_pref_store.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_store.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/constants.h"
#include "media/media_buildflags.h"
#include "third_party/blink/public/common/peerconnection/webrtc_ip_handling_policy.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "ui/color/color_provider_key.h"
#include "ui/native_theme/native_theme.h"

namespace renderer_prefs {

namespace {

// Chrome preferences.
// Should match ChromeContentBrowserClient::OverrideWebkitPrefs.
void SetChromePrefs(Profile* profile, blink::web_pref::WebPreferences& web) {
  PrefService* prefs = profile->GetPrefs();

  // Fill per-script font preferences.
  FontFamilyCache::FillFontFamilyMap(profile,
                                     prefs::kWebKitStandardFontFamilyMap,
                                     &web.standard_font_family_map);
  FontFamilyCache::FillFontFamilyMap(profile, prefs::kWebKitFixedFontFamilyMap,
                                     &web.fixed_font_family_map);
  FontFamilyCache::FillFontFamilyMap(profile, prefs::kWebKitSerifFontFamilyMap,
                                     &web.serif_font_family_map);
  FontFamilyCache::FillFontFamilyMap(profile,
                                     prefs::kWebKitSansSerifFontFamilyMap,
                                     &web.sans_serif_font_family_map);
  FontFamilyCache::FillFontFamilyMap(profile,
                                     prefs::kWebKitCursiveFontFamilyMap,
                                     &web.cursive_font_family_map);
  FontFamilyCache::FillFontFamilyMap(profile,
                                     prefs::kWebKitFantasyFontFamilyMap,
                                     &web.fantasy_font_family_map);

  web.default_font_size = prefs->GetInteger(prefs::kWebKitDefaultFontSize);
  web.default_fixed_font_size =
      prefs->GetInteger(prefs::kWebKitDefaultFixedFontSize);
  web.minimum_font_size = prefs->GetInteger(prefs::kWebKitMinimumFontSize);
  web.minimum_logical_font_size =
      prefs->GetInteger(prefs::kWebKitMinimumLogicalFontSize);

  web.default_encoding = prefs->GetString(prefs::kDefaultCharset);

  web.dom_paste_enabled = prefs->GetBoolean(prefs::kWebKitDomPasteEnabled);
  web.tabs_to_links = prefs->GetBoolean(prefs::kWebkitTabsToLinks);

  if (!prefs->GetBoolean(prefs::kWebKitJavascriptEnabled)) {
    web.javascript_enabled = false;
  }
  if (!prefs->GetBoolean(prefs::kWebKitWebSecurityEnabled)) {
    web.web_security_enabled = false;
  }
  if (!prefs->GetBoolean(prefs::kWebKitPluginsEnabled)) {
    web.plugins_enabled = false;
  }
  web.loads_images_automatically =
      prefs->GetBoolean(prefs::kWebKitLoadsImagesAutomatically);

  if (prefs->GetBoolean(prefs::kDisable3DAPIs)) {
    web.webgl1_enabled = false;
    web.webgl2_enabled = false;
  }

  web.allow_running_insecure_content =
      prefs->GetBoolean(prefs::kWebKitAllowRunningInsecureContent);

  web.password_echo_enabled = false;

  web.text_areas_are_resizable =
      prefs->GetBoolean(prefs::kWebKitTextAreasAreResizable);
  web.hyperlink_auditing_enabled =
      prefs->GetBoolean(prefs::kEnableHyperlinkAuditing);

  if (extensions::ExtensionsEnabled()) {
    std::string image_animation_policy =
        prefs->GetString(prefs::kAnimationPolicy);
    if (image_animation_policy == kAnimationPolicyOnce) {
      web.animation_policy =
          blink::mojom::ImageAnimationPolicy::kImageAnimationPolicyAnimateOnce;
    } else if (image_animation_policy == kAnimationPolicyNone) {
      web.animation_policy =
          blink::mojom::ImageAnimationPolicy::kImageAnimationPolicyNoAnimation;
    } else {
      web.animation_policy =
          blink::mojom::ImageAnimationPolicy::kImageAnimationPolicyAllowed;
    }
  }

  // Make sure we will set the default_encoding with canonical encoding name.
  web.default_encoding =
      base::GetCanonicalEncodingNameByAliasName(web.default_encoding);
  if (web.default_encoding.empty()) {
    prefs->ClearPref(prefs::kDefaultCharset);
    web.default_encoding = prefs->GetString(prefs::kDefaultCharset);
  }
  DCHECK(!web.default_encoding.empty());

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnablePotentiallyAnnoyingSecurityFeatures)) {
    web.disable_reading_from_canvas = true;
    web.strict_mixed_content_checking = true;
    web.strict_powerful_feature_restrictions = true;
  }
}

// Extension preferences.
// Should match ChromeContentBrowserClientExtensionsPart::OverrideWebkitPrefs.
void SetExtensionPrefs(content::WebContents* web_contents,
                       content::RenderViewHost* rvh,
                       blink::web_pref::WebPreferences& web) {
  if (!extensions::ExtensionsEnabled()) {
    return;
  }

  const extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(
          rvh->GetProcess()->GetBrowserContext());
  if (!registry) {
    return;
  }

  // Note: it's not possible for kExtensionsScheme to change during the lifetime
  // of the process.
  //
  // Ensure that we are only granting extension preferences to URLs with the
  // correct scheme. Without this check, chrome-guest:// schemes used by webview
  // tags as well as hosts that happen to match the id of an installed extension
  // would get the wrong preferences.
  const GURL& site_url =
      web_contents->GetPrimaryMainFrame()->GetSiteInstance()->GetSiteURL();
  if (!site_url.SchemeIs(extensions::kExtensionScheme)) {
    return;
  }

  const extensions::Extension* extension =
      registry->enabled_extensions().GetByID(site_url.host());
  extension_webkit_preferences::SetPreferences(extension, &web);
}

void SetString(CommandLinePrefStore* prefs,
               const std::string& key,
               const std::string& value) {
  prefs->SetValue(key, base::Value(value),
                  WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
}

void SetBool(CommandLinePrefStore* prefs, const std::string& key, bool value) {
  prefs->SetValue(key, base::Value(value),
                  WriteablePrefStore::DEFAULT_PREF_WRITE_FLAGS);
}

blink::mojom::PreferredColorScheme ToBlinkPreferredColorScheme(
    ui::NativeTheme::PreferredColorScheme native_theme_scheme) {
  switch (native_theme_scheme) {
    case ui::NativeTheme::PreferredColorScheme::kDark:
      return blink::mojom::PreferredColorScheme::kDark;
    case ui::NativeTheme::PreferredColorScheme::kLight:
      return blink::mojom::PreferredColorScheme::kLight;
  }

  DCHECK(false);
}

// From chrome/browser/chrome_content_browser_client.cc
// Returns true if preferred color scheme is modified based on at least one of
// the following -
// |url| - Last committed url.
// |native_theme| - For other platforms based on native theme scheme.
bool UpdatePreferredColorScheme(blink::web_pref::WebPreferences* web_prefs,
                                const GURL& url,
                                content::WebContents* web_contents,
                                const ui::NativeTheme* native_theme) {
  auto old_preferred_color_scheme = web_prefs->preferred_color_scheme;

  // Update based on native theme scheme.
  web_prefs->preferred_color_scheme =
      ToBlinkPreferredColorScheme(native_theme->GetPreferredColorScheme());

  if (url.SchemeIs(content::kChromeUIScheme)) {
    // WebUI should track the color mode of the ColorProvider associated with
    // |web_contents|.
    web_prefs->preferred_color_scheme =
        web_contents->GetColorMode() == ui::ColorProviderKey::ColorMode::kLight
            ? blink::mojom::PreferredColorScheme::kLight
            : blink::mojom::PreferredColorScheme::kDark;
  }

  return old_preferred_color_scheme != web_prefs->preferred_color_scheme;
}

}  // namespace

void SetCommandLinePrefDefaults(CommandLinePrefStore* prefs) {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  if (command_line->HasSwitch(switches::kDefaultEncoding)) {
    SetString(prefs, prefs::kDefaultCharset,
              command_line->GetSwitchValueASCII(switches::kDefaultEncoding));
  }

  if (command_line->HasSwitch(switches::kDisableJavascriptDomPaste)) {
    SetBool(prefs, prefs::kWebKitDomPasteEnabled, false);
  }
  if (command_line->HasSwitch(switches::kDisableImageLoading)) {
    SetBool(prefs, prefs::kWebKitLoadsImagesAutomatically, false);
  }
  if (command_line->HasSwitch(switches::kDisableTabToLinks)) {
    SetBool(prefs, prefs::kWebkitTabsToLinks, false);
  }
}

void SetDefaultPrefs(blink::web_pref::WebPreferences& web) {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  web.javascript_enabled =
      !command_line->HasSwitch(switches::kDisableJavascript);
  web.allow_scripts_to_close_windows =
      !command_line->HasSwitch(switches::kDisableJavascriptCloseWindows);
  web.javascript_can_access_clipboard =
      !command_line->HasSwitch(switches::kDisableJavascriptAccessClipboard);
  web.allow_universal_access_from_file_urls =
      command_line->HasSwitch(switches::kAllowUniversalAccessFromFileUrls);
  web.shrinks_standalone_images_to_fit =
      command_line->HasSwitch(switches::kImageShrinkStandaloneToFit);
  web.text_areas_are_resizable =
      !command_line->HasSwitch(switches::kDisableTextAreaResize);
}

// Helper macro for setting a WebPreferences variable based on the value of a
// CefBrowserSettings variable.
#define SET_STATE(cef_var, web_var)   \
  if (cef_var == STATE_ENABLED)       \
    web_var = true;                   \
  else if (cef_var == STATE_DISABLED) \
    web_var = false;

void SetCefPrefs(const CefBrowserSettings& cef,
                 blink::web_pref::WebPreferences& web) {
  if (cef.standard_font_family.length > 0) {
    web.standard_font_family_map[blink::web_pref::kCommonScript] =
        CefString(&cef.standard_font_family);
  }
  if (cef.fixed_font_family.length > 0) {
    web.fixed_font_family_map[blink::web_pref::kCommonScript] =
        CefString(&cef.fixed_font_family);
  }
  if (cef.serif_font_family.length > 0) {
    web.serif_font_family_map[blink::web_pref::kCommonScript] =
        CefString(&cef.serif_font_family);
  }
  if (cef.sans_serif_font_family.length > 0) {
    web.sans_serif_font_family_map[blink::web_pref::kCommonScript] =
        CefString(&cef.sans_serif_font_family);
  }
  if (cef.cursive_font_family.length > 0) {
    web.cursive_font_family_map[blink::web_pref::kCommonScript] =
        CefString(&cef.cursive_font_family);
  }
  if (cef.fantasy_font_family.length > 0) {
    web.fantasy_font_family_map[blink::web_pref::kCommonScript] =
        CefString(&cef.fantasy_font_family);
  }

  if (cef.default_font_size > 0) {
    web.default_font_size = cef.default_font_size;
  }
  if (cef.default_fixed_font_size > 0) {
    web.default_fixed_font_size = cef.default_fixed_font_size;
  }
  if (cef.minimum_font_size > 0) {
    web.minimum_font_size = cef.minimum_font_size;
  }
  if (cef.minimum_logical_font_size > 0) {
    web.minimum_logical_font_size = cef.minimum_logical_font_size;
  }

  if (cef.default_encoding.length > 0) {
    web.default_encoding = CefString(&cef.default_encoding);
  }

  SET_STATE(cef.remote_fonts, web.remote_fonts_enabled);
  SET_STATE(cef.javascript, web.javascript_enabled);
  SET_STATE(cef.javascript_close_windows, web.allow_scripts_to_close_windows);
  SET_STATE(cef.javascript_access_clipboard,
            web.javascript_can_access_clipboard);
  SET_STATE(cef.javascript_dom_paste, web.dom_paste_enabled);
  SET_STATE(cef.image_loading, web.loads_images_automatically);
  SET_STATE(cef.image_shrink_standalone_to_fit,
            web.shrinks_standalone_images_to_fit);
  SET_STATE(cef.text_area_resize, web.text_areas_are_resizable);
  SET_STATE(cef.tab_to_links, web.tabs_to_links);
  SET_STATE(cef.local_storage, web.local_storage_enabled);
  SET_STATE(cef.databases, web.databases_enabled);

  // Never explicitly enable GPU-related functions in this method because the
  // GPU blacklist is not being checked here.
  if (cef.webgl == STATE_DISABLED) {
    web.webgl1_enabled = false;
    web.webgl2_enabled = false;
  }
}

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry,
                          const std::string& locale) {
  PrefsTabHelper::RegisterProfilePrefs(registry, locale);
  RegisterAnimationPolicyPrefs(registry);

  // From chrome/browser/ui/browser_ui_prefs.cc RegisterBrowserUserPrefs.
  registry->RegisterBooleanPref(prefs::kCaretBrowsingEnabled, false);

  registry->RegisterStringPref(prefs::kWebRTCIPHandlingPolicy,
                               blink::kWebRTCIPHandlingDefault);
  registry->RegisterStringPref(prefs::kWebRTCUDPPortRange, std::string());

#if !BUILDFLAG(IS_MAC)
  registry->RegisterBooleanPref(prefs::kFullscreenAllowed, true);
#endif

  // From ChromeContentBrowserClient::RegisterProfilePrefs.
  registry->RegisterBooleanPref(prefs::kDisable3DAPIs, false);
  registry->RegisterBooleanPref(prefs::kEnableHyperlinkAuditing, true);

  // From Profile::RegisterProfilePrefs.
  registry->RegisterDictionaryPref(prefs::kPartitionDefaultZoomLevel);
  registry->RegisterDictionaryPref(prefs::kPartitionPerHostZoomLevels);
}

void PopulateWebPreferences(content::RenderViewHost* rvh,
                            blink::web_pref::WebPreferences& web,
                            SkColor& base_background_color) {
  REQUIRE_ALLOY_RUNTIME();
  CefRefPtr<AlloyBrowserHostImpl> browser = static_cast<AlloyBrowserHostImpl*>(
      extensions::GetOwnerBrowserForHost(rvh, nullptr).get());

  // Set defaults for preferences that are not handled by PrefService.
  SetDefaultPrefs(web);

  // Set preferences based on the context's PrefService.
  if (browser) {
    auto profile = Profile::FromBrowserContext(
        browser->web_contents()->GetBrowserContext());
    SetChromePrefs(profile, web);
  }

  auto* native_theme = ui::NativeTheme::GetInstanceForWeb();
  switch (native_theme->GetPreferredColorScheme()) {
    case ui::NativeTheme::PreferredColorScheme::kDark:
      web.preferred_color_scheme = blink::mojom::PreferredColorScheme::kDark;
      break;
    case ui::NativeTheme::PreferredColorScheme::kLight:
      web.preferred_color_scheme = blink::mojom::PreferredColorScheme::kLight;
      break;
  }

  switch (native_theme->GetPreferredContrast()) {
    case ui::NativeTheme::PreferredContrast::kNoPreference:
      web.preferred_contrast = blink::mojom::PreferredContrast::kNoPreference;
      break;
    case ui::NativeTheme::PreferredContrast::kMore:
      web.preferred_contrast = blink::mojom::PreferredContrast::kMore;
      break;
    case ui::NativeTheme::PreferredContrast::kLess:
      web.preferred_contrast = blink::mojom::PreferredContrast::kLess;
      break;
    case ui::NativeTheme::PreferredContrast::kCustom:
      web.preferred_contrast = blink::mojom::PreferredContrast::kCustom;
      break;
  }

  auto web_contents = content::WebContents::FromRenderViewHost(rvh);
  UpdatePreferredColorScheme(
      &web,
      web_contents->GetPrimaryMainFrame()->GetSiteInstance()->GetSiteURL(),
      web_contents, native_theme);

  // Set preferences based on the extension.
  SetExtensionPrefs(web_contents, rvh, web);

  if (browser) {
    // Set preferences based on CefBrowserSettings.
    SetCefPrefs(browser->settings(), web);

    web.picture_in_picture_enabled = browser->IsPictureInPictureSupported();

    // Set the background color for the WebView.
    base_background_color = browser->GetBackgroundColor();
  } else {
    // We don't know for sure that the browser will be windowless but assume
    // that the global windowless state is likely to be accurate.
    base_background_color =
        CefContext::Get()->GetBackgroundColor(nullptr, STATE_DEFAULT);
  }
}

bool PopulateWebPreferencesAfterNavigation(
    content::WebContents* web_contents,
    blink::web_pref::WebPreferences& web) {
  auto* native_theme = ui::NativeTheme::GetInstanceForWeb();
  return UpdatePreferredColorScheme(&web, web_contents->GetLastCommittedURL(),
                                    web_contents, native_theme);
}

}  // namespace renderer_prefs
