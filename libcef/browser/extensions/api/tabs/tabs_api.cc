// Copyright 2017 The Chromium Embedded Framework Authors. Portions copyright
// 2012 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#include "libcef/browser/extensions/api/tabs/tabs_api.h"

#include "libcef/browser/extensions/extension_web_contents_observer.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/page_zoom.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/extension_zoom_request_client.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/permissions_data.h"

namespace extensions {
namespace cef {

namespace keys = extensions::tabs_constants;
namespace tabs = api::tabs;

using api::extension_types::InjectDetails;

namespace {

const char kNotImplementedError[] = "Not implemented";

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

template <typename T>
void AssignOptionalValue(const std::unique_ptr<T>& source,
                         std::unique_ptr<T>& destination) {
  if (source.get()) {
    destination.reset(new T(*source));
  }
}

}  // namespace

ExtensionFunction::ResponseAction TabsGetFunction::Run() {
  return RespondNow(Error(kNotImplementedError));
}

TabsCreateFunction::TabsCreateFunction() : cef_details_(this) {}

ExtensionFunction::ResponseAction TabsCreateFunction::Run() {
  std::unique_ptr<tabs::Create::Params> params(
      tabs::Create::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  CefExtensionFunctionDetails::OpenTabParams options;
  AssignOptionalValue(params->create_properties.window_id, options.window_id);
  AssignOptionalValue(params->create_properties.opener_tab_id,
                      options.opener_tab_id);
  AssignOptionalValue(params->create_properties.selected, options.active);
  // The 'active' property has replaced the 'selected' property.
  AssignOptionalValue(params->create_properties.active, options.active);
  AssignOptionalValue(params->create_properties.pinned, options.pinned);
  AssignOptionalValue(params->create_properties.index, options.index);
  AssignOptionalValue(params->create_properties.url, options.url);

  std::string error;
  std::unique_ptr<base::DictionaryValue> result(
      cef_details_.OpenTab(options, user_gesture(), &error));
  if (!result)
    return RespondNow(Error(error));

  // Return data about the newly created tab.
  return RespondNow(has_callback() ? OneArgument(std::move(result))
                                   : NoArguments());
}

ExecuteCodeInTabFunction::ExecuteCodeInTabFunction()
    : cef_details_(this), execute_tab_id_(-1) {}

ExecuteCodeInTabFunction::~ExecuteCodeInTabFunction() {}

ExecuteCodeFunction::InitResult ExecuteCodeInTabFunction::Init() {
  if (init_result_)
    return init_result_.value();

  // |tab_id| is optional so it's ok if it's not there.
  int tab_id = -1;
  if (args_->GetInteger(0, &tab_id) && tab_id < 0)
    return set_init_result(VALIDATION_FAILURE);

  // |details| are not optional.
  base::DictionaryValue* details_value = NULL;
  if (!args_->GetDictionary(1, &details_value))
    return set_init_result(VALIDATION_FAILURE);
  std::unique_ptr<InjectDetails> details(new InjectDetails());
  if (!InjectDetails::Populate(*details_value, details.get()))
    return set_init_result(VALIDATION_FAILURE);

  // Find a browser that we can access, or fail with error.
  std::string error;
  CefRefPtr<CefBrowserHostImpl> browser =
      cef_details_.GetBrowserForTabIdFirstTime(tab_id, &error);
  if (!browser)
    return set_init_result_error(error);

  execute_tab_id_ = browser->GetIdentifier();
  details_ = std::move(details);
  set_host_id(HostID(HostID::EXTENSIONS, extension()->id()));
  return set_init_result(SUCCESS);
}

bool ExecuteCodeInTabFunction::CanExecuteScriptOnPage(std::string* error) {
  CHECK_GE(execute_tab_id_, 0);

  CefRefPtr<CefBrowserHostImpl> browser =
      cef_details_.GetBrowserForTabIdAgain(execute_tab_id_, error);
  if (!browser)
    return false;

  int frame_id = details_->frame_id ? *details_->frame_id
                                    : ExtensionApiFrameIdMap::kTopFrameId;
  content::RenderFrameHost* rfh =
      ExtensionApiFrameIdMap::GetRenderFrameHostById(browser->web_contents(),
                                                     frame_id);
  if (!rfh) {
    *error = ErrorUtils::FormatErrorMessage(
        keys::kFrameNotFoundError, base::NumberToString(frame_id),
        base::NumberToString(execute_tab_id_));
    return false;
  }

  // Content scripts declared in manifest.json can access frames at about:-URLs
  // if the extension has permission to access the frame's origin, so also allow
  // programmatic content scripts at about:-URLs for allowed origins.
  GURL effective_document_url(rfh->GetLastCommittedURL());
  bool is_about_url = effective_document_url.SchemeIs(url::kAboutScheme);
  if (is_about_url && details_->match_about_blank &&
      *details_->match_about_blank) {
    effective_document_url = GURL(rfh->GetLastCommittedOrigin().Serialize());
  }

  if (!effective_document_url.is_valid()) {
    // Unknown URL, e.g. because no load was committed yet. Allow for now, the
    // renderer will check again and fail the injection if needed.
    return true;
  }

  // NOTE: This can give the wrong answer due to race conditions, but it is OK,
  // we check again in the renderer.
  if (!extension()->permissions_data()->CanAccessPage(effective_document_url,
                                                      execute_tab_id_, error)) {
    if (is_about_url &&
        extension()->permissions_data()->active_permissions().HasAPIPermission(
            APIPermission::kTab)) {
      *error = ErrorUtils::FormatErrorMessage(
          manifest_errors::kCannotAccessAboutUrl,
          rfh->GetLastCommittedURL().spec(),
          rfh->GetLastCommittedOrigin().Serialize());
    }
    return false;
  }

  return true;
}

ScriptExecutor* ExecuteCodeInTabFunction::GetScriptExecutor(
    std::string* error) {
  CHECK_GE(execute_tab_id_, 0);

  CefRefPtr<CefBrowserHostImpl> browser =
      cef_details_.GetBrowserForTabIdAgain(execute_tab_id_, error);
  if (!browser)
    return nullptr;

  return CefExtensionWebContentsObserver::FromWebContents(
             browser->web_contents())
      ->script_executor();
}

bool ExecuteCodeInTabFunction::IsWebView() const {
  return false;
}

const GURL& ExecuteCodeInTabFunction::GetWebViewSrc() const {
  return GURL::EmptyGURL();
}

bool ExecuteCodeInTabFunction::LoadFile(const std::string& file,
                                        std::string* error) {
  if (cef_details_.LoadFile(
          file, base::BindOnce(&ExecuteCodeInTabFunction::LoadFileComplete,
                               this, file))) {
    return true;
  }

  // Default handling.
  return ExecuteCodeFunction::LoadFile(file, error);
}

void ExecuteCodeInTabFunction::LoadFileComplete(
    const std::string& file,
    std::unique_ptr<std::string> data) {
  const bool success = !!data.get();
  DidLoadAndLocalizeFile(file, success, std::move(data));
}

bool TabsExecuteScriptFunction::ShouldInsertCSS() const {
  return false;
}

bool TabsInsertCSSFunction::ShouldInsertCSS() const {
  return true;
}

ZoomAPIFunction::ZoomAPIFunction() : cef_details_(this) {}

content::WebContents* ZoomAPIFunction::GetWebContents(int tab_id) {
  // Find a browser that we can access, or set |error_| and return nullptr.
  CefRefPtr<CefBrowserHostImpl> browser =
      cef_details_.GetBrowserForTabIdFirstTime(tab_id, &error_);
  if (!browser)
    return nullptr;

  return browser->web_contents();
}

void ZoomAPIFunction::SendResponse(bool success) {
  ResponseValue response;
  if (success) {
    response = ArgumentList(std::move(results_));
  } else {
    response = results_ ? ErrorWithArguments(std::move(results_), error_)
                        : Error(error_);
  }
  Respond(std::move(response));
}

ExtensionFunction::ResponseAction ZoomAPIFunction::Run() {
  if (RunAsync())
    return RespondLater();
  // TODO(devlin): Track these down and eliminate them if possible. We
  // shouldn't return results and an error.
  if (results_)
    return RespondNow(ErrorWithArguments(std::move(results_), error_));
  return RespondNow(Error(error_));
}

bool TabsSetZoomFunction::RunAsync() {
  std::unique_ptr<tabs::SetZoom::Params> params(
      tabs::SetZoom::Params::Create(*args_));
  EXTENSION_FUNCTION_PRERUN_VALIDATE(params);

  int tab_id = params->tab_id ? *params->tab_id : -1;
  content::WebContents* web_contents = GetWebContents(tab_id);
  if (!web_contents)
    return false;

  GURL url(web_contents->GetVisibleURL());
  if (extension()->permissions_data()->IsRestrictedUrl(url, &error_))
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
  EXTENSION_FUNCTION_PRERUN_VALIDATE(params);

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
  EXTENSION_FUNCTION_PRERUN_VALIDATE(params);

  int tab_id = params->tab_id ? *params->tab_id : -1;
  content::WebContents* web_contents = GetWebContents(tab_id);
  if (!web_contents)
    return false;

  GURL url(web_contents->GetVisibleURL());
  if (extension()->permissions_data()->IsRestrictedUrl(url, &error_))
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
  EXTENSION_FUNCTION_PRERUN_VALIDATE(params);

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
