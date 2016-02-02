// Copyright 2015 The Chromium Embedded Framework Authors.
// Portions copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/plugins/plugin_info_message_filter.h"

#include <utility>

#include "libcef/browser/browser_context.h"
#include "libcef/browser/plugins/plugin_service_filter.h"
#include "libcef/browser/web_plugin_impl.h"
#include "libcef/common/cef_messages.h"
#include "libcef/common/extensions/extensions_util.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/plugins/plugin_finder.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/plugins_field_trial.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/plugin_service_filter.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/webplugininfo.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/webview_info.h"
#include "url/gurl.h"

#include "widevine_cdm_version.h"  // In SHARED_INTERMEDIATE_DIR.

#if !defined(DISABLE_NACL)
#include "components/nacl/common/nacl_constants.h"
#endif

using content::PluginService;
using content::WebPluginInfo;

namespace {

#if defined(OS_WIN) || defined(OS_MACOSX)
// These are the mime-types of plugins which are known to have PPAPI versions.
const char* kPepperPluginMimeTypes[] = {
    "application/pdf",
    "application/x-google-chrome-pdf",
    "application/x-nacl",
    "application/x-pnacl",
    "application/vnd.chromium.remoting-viewer",
    "application/x-shockwave-flash",
    "application/futuresplash",
};
#endif

// For certain sandboxed Pepper plugins, use the JavaScript Content Settings.
bool ShouldUseJavaScriptSettingForPlugin(const WebPluginInfo& plugin) {
  if (plugin.type != WebPluginInfo::PLUGIN_TYPE_PEPPER_IN_PROCESS &&
      plugin.type != WebPluginInfo::PLUGIN_TYPE_PEPPER_OUT_OF_PROCESS) {
    return false;
  }

#if !defined(DISABLE_NACL)
  // Treat Native Client invocations like JavaScript.
  if (plugin.name == base::ASCIIToUTF16(nacl::kNaClPluginName))
    return true;
#endif

#if defined(WIDEVINE_CDM_AVAILABLE) && defined(ENABLE_PEPPER_CDMS)
  // Treat CDM invocations like JavaScript.
  if (plugin.name == base::ASCIIToUTF16(kWidevineCdmDisplayName)) {
    DCHECK(plugin.type == WebPluginInfo::PLUGIN_TYPE_PEPPER_OUT_OF_PROCESS);
    return true;
  }
#endif  // defined(WIDEVINE_CDM_AVAILABLE) && defined(ENABLE_PEPPER_CDMS)

  return false;
}

#if defined(ENABLE_PEPPER_CDMS)

enum PluginAvailabilityStatusForUMA {
  PLUGIN_NOT_REGISTERED,
  PLUGIN_AVAILABLE,
  PLUGIN_DISABLED,
  PLUGIN_AVAILABILITY_STATUS_MAX
};

static void SendPluginAvailabilityUMA(const std::string& mime_type,
                                      PluginAvailabilityStatusForUMA status) {
#if defined(WIDEVINE_CDM_AVAILABLE)
  // Only report results for Widevine CDM.
  if (mime_type != kWidevineCdmPluginMimeType)
    return;

  UMA_HISTOGRAM_ENUMERATION("Plugin.AvailabilityStatus.WidevineCdm",
                            status, PLUGIN_AVAILABILITY_STATUS_MAX);
#endif  // defined(WIDEVINE_CDM_AVAILABLE)
}

#endif  // defined(ENABLE_PEPPER_CDMS)

void ReportMetrics(const std::string& mime_type,
                   const GURL& url,
                   const GURL& origin_url) {
}

#if defined(ENABLE_EXTENSIONS)
// Returns whether a request from a plugin to load |resource| from a renderer
// with process id |process_id| is a request for an internal resource by an app
// listed in |accessible_resources| in its manifest.
bool IsPluginLoadingAccessibleResourceInWebView(
    extensions::ExtensionRegistry* extension_registry,
    int process_id,
    const GURL& resource) {
  extensions::WebViewRendererState* renderer_state =
      extensions::WebViewRendererState::GetInstance();
  std::string partition_id;
  if (!renderer_state->IsGuest(process_id) ||
      !renderer_state->GetPartitionID(process_id, &partition_id)) {
    return false;
  }

  const std::string extension_id = resource.host();
  const extensions::Extension* extension = extension_registry->GetExtensionById(
      extension_id, extensions::ExtensionRegistry::ENABLED);
  if (!extension || !extensions::WebviewInfo::IsResourceWebviewAccessible(
          extension, partition_id, resource.path())) {
    return false;
  }

  // Make sure the renderer making the request actually belongs to the
  // same extension.
  std::string owner_extension;
  return renderer_state->GetOwnerInfo(process_id, nullptr, &owner_extension) &&
         owner_extension == extension_id;
}
#endif  // defined(ENABLE_EXTENSIONS)

}  // namespace

CefPluginInfoMessageFilter::Context::Context(
    int render_process_id,
    CefBrowserContext* profile)
    : render_process_id_(render_process_id),
      resource_context_(profile->GetResourceContext()),
      host_content_settings_map_(profile->GetHostContentSettingsMap()) {
#if defined(ENABLE_EXTENSIONS)
  if (extensions::ExtensionsEnabled())
    extension_registry_ = extensions::ExtensionRegistry::Get(profile);
#endif

  allow_outdated_plugins_.Init(prefs::kPluginsAllowOutdated,
                               profile->GetPrefs());
  allow_outdated_plugins_.MoveToThread(
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::IO));
  always_authorize_plugins_.Init(prefs::kPluginsAlwaysAuthorize,
                                 profile->GetPrefs());
  always_authorize_plugins_.MoveToThread(
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::IO));
}

CefPluginInfoMessageFilter::Context::~Context() {
}

CefPluginInfoMessageFilter::CefPluginInfoMessageFilter(
    int render_process_id,
    CefBrowserContext* profile)
    : BrowserMessageFilter(ExtensionMsgStart),
      browser_context_(profile),
      context_(render_process_id, profile),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_ptr_factory_(this) {
}

bool CefPluginInfoMessageFilter::OnMessageReceived(
    const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(CefPluginInfoMessageFilter, message)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(CefViewHostMsg_GetPluginInfo,
                                    OnGetPluginInfo)
#if defined(ENABLE_PEPPER_CDMS)
    IPC_MESSAGE_HANDLER(
        CefViewHostMsg_IsInternalPluginAvailableForMimeType,
        OnIsInternalPluginAvailableForMimeType)
#endif
    IPC_MESSAGE_UNHANDLED(return false)
  IPC_END_MESSAGE_MAP()
  return true;
}

void CefPluginInfoMessageFilter::OnDestruct() const {
  const_cast<CefPluginInfoMessageFilter*>(this)->
      weak_ptr_factory_.InvalidateWeakPtrs();

  // Destroy on the UI thread because we contain a |PrefMember|.
  content::BrowserThread::DeleteOnUIThread::Destruct(this);
}

CefPluginInfoMessageFilter::~CefPluginInfoMessageFilter() {}

struct CefPluginInfoMessageFilter::GetPluginInfo_Params {
  int render_frame_id;
  GURL url;
  GURL top_origin_url;
  std::string mime_type;
};

void CefPluginInfoMessageFilter::OnGetPluginInfo(
    int render_frame_id,
    const GURL& url,
    const GURL& top_origin_url,
    const std::string& mime_type,
    IPC::Message* reply_msg) {
  GetPluginInfo_Params params = {
    render_frame_id,
    url,
    top_origin_url,
    mime_type
  };
  PluginService::GetInstance()->GetPlugins(
      base::Bind(&CefPluginInfoMessageFilter::PluginsLoaded,
                 weak_ptr_factory_.GetWeakPtr(),
                 params, reply_msg));
}

void CefPluginInfoMessageFilter::PluginsLoaded(
    const GetPluginInfo_Params& params,
    IPC::Message* reply_msg,
    const std::vector<WebPluginInfo>& plugins) {
  CefViewHostMsg_GetPluginInfo_Output output;
  CefRefPtr<CefRequestContextHandler> handler =
        browser_context_->GetHandler();

  // This also fills in |actual_mime_type|.
  scoped_ptr<PluginMetadata> plugin_metadata;
  context_.FindEnabledPlugin(params, handler.get(),
                             &output.status, &output.plugin,
                             &output.actual_mime_type,
                             &plugin_metadata);

  if (plugin_metadata) {
    output.group_identifier = plugin_metadata->identifier();
    output.group_name = plugin_metadata->name();
  }

  CefViewHostMsg_GetPluginInfo::WriteReplyParams(reply_msg, output);
  Send(reply_msg);
  if (output.status !=
      CefViewHostMsg_GetPluginInfo_Status::kNotFound) {
    main_thread_task_runner_->PostTask(
        FROM_HERE, base::Bind(&ReportMetrics, output.actual_mime_type,
                              params.url, params.top_origin_url));
  }
}

#if defined(ENABLE_PEPPER_CDMS)

void CefPluginInfoMessageFilter::OnIsInternalPluginAvailableForMimeType(
    const std::string& mime_type,
    bool* is_available,
    std::vector<base::string16>* additional_param_names,
    std::vector<base::string16>* additional_param_values) {
  std::vector<WebPluginInfo> plugins;
  PluginService::GetInstance()->GetInternalPlugins(&plugins);

  bool is_plugin_disabled = false;
  for (size_t i = 0; i < plugins.size(); ++i) {
    const WebPluginInfo& plugin = plugins[i];
    const std::vector<content::WebPluginMimeType>& mime_types =
        plugin.mime_types;
    for (size_t j = 0; j < mime_types.size(); ++j) {
      if (mime_types[j].mime_type == mime_type) {
        if (!context_.IsPluginEnabled(plugin)) {
          is_plugin_disabled = true;
          break;
        }

        *is_available = true;
        *additional_param_names = mime_types[j].additional_param_names;
        *additional_param_values = mime_types[j].additional_param_values;
        SendPluginAvailabilityUMA(mime_type, PLUGIN_AVAILABLE);
        return;
      }
    }
  }

  *is_available = false;
  SendPluginAvailabilityUMA(
      mime_type, is_plugin_disabled ? PLUGIN_DISABLED : PLUGIN_NOT_REGISTERED);
}

#endif // defined(ENABLE_PEPPER_CDMS)

void CefPluginInfoMessageFilter::Context::DecidePluginStatus(
    const GetPluginInfo_Params& params,
    const WebPluginInfo& plugin,
    const PluginMetadata* plugin_metadata,
    CefViewHostMsg_GetPluginInfo_Status* status) const {
  if (plugin.type == WebPluginInfo::PLUGIN_TYPE_NPAPI) {
    CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
    // NPAPI plugins are not supported inside <webview> guests.
#if defined(ENABLE_EXTENSIONS)
    if (extensions::WebViewRendererState::GetInstance()->IsGuest(
        render_process_id_)) {
      *status = CefViewHostMsg_GetPluginInfo_Status::kNPAPINotSupported;
      return;
    }
#endif
  }

  ContentSetting plugin_setting = CONTENT_SETTING_DEFAULT;
  bool uses_default_content_setting = true;
  bool is_managed = false;
  // Check plugin content settings. The primary URL is the top origin URL and
  // the secondary URL is the plugin URL.
  GetPluginContentSetting(plugin, params.top_origin_url, params.url,
                          plugin_metadata->identifier(), &plugin_setting,
                          &uses_default_content_setting, &is_managed);

  // TODO(tommycli): Remove once we deprecate the plugin ASK policy.
  bool legacy_ask_user = plugin_setting == CONTENT_SETTING_ASK;
  plugin_setting = content_settings::PluginsFieldTrial::EffectiveContentSetting(
      CONTENT_SETTINGS_TYPE_PLUGINS, plugin_setting);

  DCHECK(plugin_setting != CONTENT_SETTING_DEFAULT);
  DCHECK(plugin_setting != CONTENT_SETTING_ASK);

  PluginMetadata::SecurityStatus plugin_status =
      plugin_metadata->GetSecurityStatus(plugin);
#if defined(ENABLE_PLUGIN_INSTALLATION)
  // Check if the plugin is outdated.
  if (plugin_status == PluginMetadata::SECURITY_STATUS_OUT_OF_DATE &&
      !allow_outdated_plugins_.GetValue()) {
    if (allow_outdated_plugins_.IsManaged()) {
      *status = CefViewHostMsg_GetPluginInfo_Status::kOutdatedDisallowed;
    } else {
      *status = CefViewHostMsg_GetPluginInfo_Status::kOutdatedBlocked;
    }
    return;
  }
#endif
  // Check if the plugin requires authorization.
  if (plugin_status ==
          PluginMetadata::SECURITY_STATUS_REQUIRES_AUTHORIZATION &&
      plugin.type != WebPluginInfo::PLUGIN_TYPE_PEPPER_IN_PROCESS &&
      plugin.type != WebPluginInfo::PLUGIN_TYPE_PEPPER_OUT_OF_PROCESS &&
      plugin.type != WebPluginInfo::PLUGIN_TYPE_BROWSER_PLUGIN &&
      !always_authorize_plugins_.GetValue() &&
      plugin_setting != CONTENT_SETTING_BLOCK &&
      uses_default_content_setting) {
    *status = CefViewHostMsg_GetPluginInfo_Status::kUnauthorized;
    return;
  }

  // Check if the plugin is crashing too much.
  if (PluginService::GetInstance()->IsPluginUnstable(plugin.path) &&
      !always_authorize_plugins_.GetValue() &&
      plugin_setting != CONTENT_SETTING_BLOCK &&
      uses_default_content_setting) {
    *status = CefViewHostMsg_GetPluginInfo_Status::kUnauthorized;
    return;
  }

#if defined(ENABLE_EXTENSIONS)
  // If an app has explicitly made internal resources available by listing them
  // in |accessible_resources| in the manifest, then allow them to be loaded by
  // plugins inside a guest-view.
  if (extensions::ExtensionsEnabled() &&
      params.url.SchemeIs(extensions::kExtensionScheme) && !is_managed &&
      plugin_setting == CONTENT_SETTING_BLOCK &&
      IsPluginLoadingAccessibleResourceInWebView(
          extension_registry_, render_process_id_, params.url)) {
    plugin_setting = CONTENT_SETTING_ALLOW;
  }
#endif  // defined(ENABLE_EXTENSIONS)

  if (plugin_setting == CONTENT_SETTING_DETECT_IMPORTANT_CONTENT) {
    *status = CefViewHostMsg_GetPluginInfo_Status::kPlayImportantContent;
  } else if (plugin_setting == CONTENT_SETTING_BLOCK) {
    // For managed users with the ASK policy, we allow manually running plugins
    // via context menu. This is the closest to admin intent.
    *status = is_managed && !legacy_ask_user
                  ? CefViewHostMsg_GetPluginInfo_Status::kBlockedByPolicy
                  : CefViewHostMsg_GetPluginInfo_Status::kBlocked;
  }

#if defined(ENABLE_EXTENSIONS)
  // Allow an embedder of <webview> to block a plugin from being loaded inside
  // the guest. In order to do this, set the status to 'Unauthorized' here,
  // and update the status as appropriate depending on the response from the
  // embedder.
  if (*status == CefViewHostMsg_GetPluginInfo_Status::kAllowed ||
      *status == CefViewHostMsg_GetPluginInfo_Status::kBlocked ||
      *status ==
          CefViewHostMsg_GetPluginInfo_Status::kPlayImportantContent) {
    if (extensions::WebViewRendererState::GetInstance()->IsGuest(
            render_process_id_)) {
      *status = CefViewHostMsg_GetPluginInfo_Status::kUnauthorized;
    }
  }
#endif
}

bool CefPluginInfoMessageFilter::Context::FindEnabledPlugin(
    const GetPluginInfo_Params& params,
    CefRequestContextHandler* handler,
    CefViewHostMsg_GetPluginInfo_Status* status,
    WebPluginInfo* plugin,
    std::string* actual_mime_type,
    scoped_ptr<PluginMetadata>* plugin_metadata) const {
  *status = CefViewHostMsg_GetPluginInfo_Status::kAllowed;

  bool allow_wildcard = true;
  std::vector<WebPluginInfo> matching_plugins;
  std::vector<std::string> mime_types;
  PluginService::GetInstance()->GetPluginInfoArray(
      params.url, params.mime_type, allow_wildcard, &matching_plugins,
      &mime_types);
  if (matching_plugins.empty()) {
    *status = CefViewHostMsg_GetPluginInfo_Status::kNotFound;
#if defined(OS_WIN) || defined(OS_MACOSX)
    if (!PluginService::GetInstance()->NPAPIPluginsSupported()) {
      // At this point it is not known for sure this is an NPAPI plugin as it
      // could be a not-yet-installed Pepper plugin. To avoid notifying on
      // these types, bail early based on a blacklist of pepper mime types.
      for (auto pepper_mime_type : kPepperPluginMimeTypes)
        if (pepper_mime_type == params.mime_type)
          return false;
    }
#endif
    return false;
  }

  CefPluginServiceFilter* filter = static_cast<CefPluginServiceFilter*>(
      PluginService::GetInstance()->GetFilter());
  DCHECK(filter);

  CefViewHostMsg_GetPluginInfo_Status first_status = *status;

  size_t i = 0;
  for (; i < matching_plugins.size(); ++i) {
    *plugin = matching_plugins[i];
    *actual_mime_type = mime_types[i];
    *plugin_metadata = PluginFinder::GetInstance()->GetPluginMetadata(*plugin);

    DecidePluginStatus(params, *plugin, (*plugin_metadata).get(), status);
    if (filter->IsPluginAvailable(handler,
                                  params.url,
                                  params.top_origin_url,
                                  plugin,
                                  status)) {
      break;
    }

    if (i == 0)
      first_status = *status;
  }

  // If we broke out of the loop, we have found an enabled plugin.
  bool enabled = i < matching_plugins.size();
  if (!enabled) {
    // Otherwise, we only found disabled/not-found plugins, so we take the first
    // one.
    *plugin = matching_plugins[0];
    *actual_mime_type = mime_types[0];
    *plugin_metadata = PluginFinder::GetInstance()->GetPluginMetadata(*plugin);
    *status = first_status;
  }

  return enabled;
}

void CefPluginInfoMessageFilter::Context::GetPluginContentSetting(
    const WebPluginInfo& plugin,
    const GURL& policy_url,
    const GURL& plugin_url,
    const std::string& resource,
    ContentSetting* setting,
    bool* uses_default_content_setting,
    bool* is_managed) const {
  scoped_ptr<base::Value> value;
  content_settings::SettingInfo info;
  bool uses_plugin_specific_setting = false;
  if (ShouldUseJavaScriptSettingForPlugin(plugin)) {
    value = host_content_settings_map_->GetWebsiteSetting(
        policy_url,
        policy_url,
        CONTENT_SETTINGS_TYPE_JAVASCRIPT,
        std::string(),
        &info);
  } else {
    content_settings::SettingInfo specific_info;
    scoped_ptr<base::Value> specific_setting =
        host_content_settings_map_->GetWebsiteSetting(
            policy_url,
            plugin_url,
            CONTENT_SETTINGS_TYPE_PLUGINS,
            resource,
            &specific_info);
    content_settings::SettingInfo general_info;
    scoped_ptr<base::Value> general_setting =
        host_content_settings_map_->GetWebsiteSetting(
            policy_url,
            plugin_url,
            CONTENT_SETTINGS_TYPE_PLUGINS,
            std::string(),
            &general_info);

    // If there is a plugin-specific setting, we use it, unless the general
    // setting was set by policy, in which case it takes precedence.
    // TODO(tommycli): Remove once we deprecate the plugin ASK policy.
    bool legacy_ask_user = content_settings::ValueToContentSetting(
                               general_setting.get()) == CONTENT_SETTING_ASK;
    bool use_policy =
        general_info.source == content_settings::SETTING_SOURCE_POLICY &&
        !legacy_ask_user;
    uses_plugin_specific_setting = specific_setting && !use_policy;
    if (uses_plugin_specific_setting) {
      value = std::move(specific_setting);
      info = specific_info;
    } else {
      value = std::move(general_setting);
      info = general_info;
    }
  }
  *setting = content_settings::ValueToContentSetting(value.get());
  *uses_default_content_setting =
      !uses_plugin_specific_setting &&
      info.primary_pattern == ContentSettingsPattern::Wildcard() &&
      info.secondary_pattern == ContentSettingsPattern::Wildcard();
  *is_managed = info.source == content_settings::SETTING_SOURCE_POLICY;
}

bool CefPluginInfoMessageFilter::Context::IsPluginEnabled(
    const content::WebPluginInfo& plugin) const {
  return true;
}
