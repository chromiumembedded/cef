// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/extensions/api/tabs/tabs_api.h"

#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/browser_info_manager.h"
#include "libcef/browser/request_context_impl.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/common/page_zoom.h"
#include "extensions/browser/extension_zoom_request_client.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/permissions/permissions_data.h"

namespace extensions {
namespace cef {

namespace keys = extensions::tabs_constants;
namespace tabs = api::tabs;

namespace {

const char kNotImplementedError[] = "Not implemented";

// Any out parameter (|browser|, |contents|, & |tab_index|) may be NULL and will
// not be set within the function.
// Based on ExtensionTabUtil::GetTabById().
bool GetTabById(int tab_id,
                content::BrowserContext* browser_context,
                bool include_incognito,
                CefRefPtr<CefBrowserHostImpl>* browser,
                content::WebContents** contents) {
  if (tab_id == api::tabs::TAB_ID_NONE)
    return false;

  // As an extra security check make sure we're operating in the same
  // BrowserContext.
  Profile* profile = Profile::FromBrowserContext(browser_context);
  Profile* incognito_profile =
      include_incognito && profile->HasOffTheRecordProfile() ?
          profile->GetOffTheRecordProfile() : NULL;

  CefBrowserInfoManager::BrowserInfoList list;
  CefBrowserInfoManager::GetInstance()->GetBrowserInfoList(list);
  for (auto browser_info : list) {
    CefRefPtr<CefBrowserHostImpl> cef_browser = browser_info->browser();
    if (!cef_browser)
      continue;
    CefRefPtr<CefRequestContext> request_context =
        cef_browser->GetRequestContext();
    if (!request_context)
      continue;
    CefRefPtr<CefRequestContextImpl> request_context_impl =
        CefRequestContextImpl::GetOrCreateForRequestContext(request_context);
    if (!request_context_impl)
      continue;
    CefBrowserContext* browser_context =
        request_context_impl->GetBrowserContext();
    if (!browser_context)
      continue;

    if (browser_context == profile ||
        browser_context == incognito_profile) {
      content::WebContents* web_contents = cef_browser->web_contents();
      if (SessionTabHelper::IdForTab(web_contents) == tab_id) {
        if (browser)
          *browser = cef_browser;
        if (contents)
          *contents = web_contents;
        return true;
      }
    }
  }
  return false;
}

// |error_message| can optionally be passed in and will be set with an
// appropriate message if the tab cannot be found by id.
bool GetTabById(int tab_id,
                content::BrowserContext* browser_context,
                bool include_incognito,
                CefRefPtr<CefBrowserHostImpl>* browser,
                content::WebContents** contents,
                std::string* error_message) {
  if (GetTabById(tab_id, browser_context, include_incognito, browser,
                 contents)) {
    return true;
  }

  if (error_message) {
    *error_message = ErrorUtils::FormatErrorMessage(
        keys::kTabNotFoundError, base::IntToString(tab_id));
  }

  return false;
}

void ZoomModeToZoomSettings(zoom::ZoomController::ZoomMode zoom_mode,
                            api::tabs::ZoomSettings* zoom_settings) {
  DCHECK(zoom_settings);
  switch (zoom_mode) {
    case zoom::ZoomController::ZOOM_MODE_DEFAULT:
      zoom_settings->mode = api::tabs::ZOOM_SETTINGS_MODE_AUTOMATIC;
      zoom_settings->scope = api::tabs::ZOOM_SETTINGS_SCOPE_PER_ORIGIN;
      break;
    case zoom::ZoomController::ZOOM_MODE_ISOLATED:
      zoom_settings->mode = api::tabs::ZOOM_SETTINGS_MODE_AUTOMATIC;
      zoom_settings->scope = api::tabs::ZOOM_SETTINGS_SCOPE_PER_TAB;
      break;
    case zoom::ZoomController::ZOOM_MODE_MANUAL:
      zoom_settings->mode = api::tabs::ZOOM_SETTINGS_MODE_MANUAL;
      zoom_settings->scope = api::tabs::ZOOM_SETTINGS_SCOPE_PER_TAB;
      break;
    case zoom::ZoomController::ZOOM_MODE_DISABLED:
      zoom_settings->mode = api::tabs::ZOOM_SETTINGS_MODE_DISABLED;
      zoom_settings->scope = api::tabs::ZOOM_SETTINGS_SCOPE_PER_TAB;
      break;
  }
}

}  // namespace

ExtensionFunction::ResponseAction TabsGetFunction::Run() {
  return RespondNow(Error(kNotImplementedError));
}

content::WebContents* ZoomAPIFunction::GetWebContents(int tab_id) {
  content::WebContents* web_contents = NULL;
  if (tab_id != -1) {
    // We assume this call leaves web_contents unchanged if it is unsuccessful.
    GetTabById(tab_id,
               context_,
               include_incognito(),
               NULL /* ignore CefBrowserHostImpl* output */,
               &web_contents,
               &error_);
  } else {
    // Use the sender as the default.
    web_contents = GetSenderWebContents();
  }
  return web_contents;
}

bool TabsSetZoomFunction::RunAsync() {
  std::unique_ptr<tabs::SetZoom::Params> params(
      tabs::SetZoom::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  int tab_id = params->tab_id ? *params->tab_id : -1;
  content::WebContents* web_contents = GetWebContents(tab_id);
  if (!web_contents)
    return false;

  GURL url(web_contents->GetVisibleURL());
  if (extensions::PermissionsData::IsRestrictedUrl(url, extension(), &error_))
    return false;

  zoom::ZoomController* zoom_controller =
      zoom::ZoomController::FromWebContents(web_contents);
  double zoom_level = params->zoom_factor > 0
                          ? content::ZoomFactorToZoomLevel(params->zoom_factor)
                          : zoom_controller->GetDefaultZoomLevel();

  scoped_refptr<extensions::ExtensionZoomRequestClient> client(
      new extensions::ExtensionZoomRequestClient(extension()));
  if (!zoom_controller->SetZoomLevelByClient(zoom_level, client)) {
    // Tried to zoom a tab in disabled mode.
    error_ = keys::kCannotZoomDisabledTabError;
    return false;
  }

  SendResponse(true);
  return true;
}

bool TabsGetZoomFunction::RunAsync() {
  std::unique_ptr<tabs::GetZoom::Params> params(
      tabs::GetZoom::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  int tab_id = params->tab_id ? *params->tab_id : -1;
  content::WebContents* web_contents = GetWebContents(tab_id);
  if (!web_contents)
    return false;

  double zoom_level =
      zoom::ZoomController::FromWebContents(web_contents)->GetZoomLevel();
  double zoom_factor = content::ZoomLevelToZoomFactor(zoom_level);
  results_ = tabs::GetZoom::Results::Create(zoom_factor);
  SendResponse(true);
  return true;
}

bool TabsSetZoomSettingsFunction::RunAsync() {
  using api::tabs::ZoomSettings;

  std::unique_ptr<tabs::SetZoomSettings::Params> params(
      tabs::SetZoomSettings::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  int tab_id = params->tab_id ? *params->tab_id : -1;
  content::WebContents* web_contents = GetWebContents(tab_id);
  if (!web_contents)
    return false;

  GURL url(web_contents->GetVisibleURL());
  if (PermissionsData::IsRestrictedUrl(url, extension(), &error_))
    return false;

  // "per-origin" scope is only available in "automatic" mode.
  if (params->zoom_settings.scope == tabs::ZOOM_SETTINGS_SCOPE_PER_ORIGIN &&
      params->zoom_settings.mode != tabs::ZOOM_SETTINGS_MODE_AUTOMATIC &&
      params->zoom_settings.mode != tabs::ZOOM_SETTINGS_MODE_NONE) {
    error_ = keys::kPerOriginOnlyInAutomaticError;
    return false;
  }

  // Determine the correct internal zoom mode to set |web_contents| to from the
  // user-specified |zoom_settings|.
  zoom::ZoomController::ZoomMode zoom_mode =
      zoom::ZoomController::ZOOM_MODE_DEFAULT;
  switch (params->zoom_settings.mode) {
    case tabs::ZOOM_SETTINGS_MODE_NONE:
    case tabs::ZOOM_SETTINGS_MODE_AUTOMATIC:
      switch (params->zoom_settings.scope) {
        case tabs::ZOOM_SETTINGS_SCOPE_NONE:
        case tabs::ZOOM_SETTINGS_SCOPE_PER_ORIGIN:
          zoom_mode = zoom::ZoomController::ZOOM_MODE_DEFAULT;
          break;
        case tabs::ZOOM_SETTINGS_SCOPE_PER_TAB:
          zoom_mode = zoom::ZoomController::ZOOM_MODE_ISOLATED;
      }
      break;
    case tabs::ZOOM_SETTINGS_MODE_MANUAL:
      zoom_mode = zoom::ZoomController::ZOOM_MODE_MANUAL;
      break;
    case tabs::ZOOM_SETTINGS_MODE_DISABLED:
      zoom_mode = zoom::ZoomController::ZOOM_MODE_DISABLED;
  }

  zoom::ZoomController::FromWebContents(web_contents)->SetZoomMode(zoom_mode);

  SendResponse(true);
  return true;
}

bool TabsGetZoomSettingsFunction::RunAsync() {
  std::unique_ptr<tabs::GetZoomSettings::Params> params(
      tabs::GetZoomSettings::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  int tab_id = params->tab_id ? *params->tab_id : -1;
  content::WebContents* web_contents = GetWebContents(tab_id);
  if (!web_contents)
    return false;
  zoom::ZoomController* zoom_controller =
      zoom::ZoomController::FromWebContents(web_contents);

  zoom::ZoomController::ZoomMode zoom_mode = zoom_controller->zoom_mode();
  api::tabs::ZoomSettings zoom_settings;
  ZoomModeToZoomSettings(zoom_mode, &zoom_settings);
  zoom_settings.default_zoom_factor.reset(new double(
      content::ZoomLevelToZoomFactor(zoom_controller->GetDefaultZoomLevel())));

  results_ = api::tabs::GetZoomSettings::Results::Create(zoom_settings);
  SendResponse(true);
  return true;
}

}  // namespace cef
}  // namespace extensions
