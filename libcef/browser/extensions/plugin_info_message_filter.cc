// Copyright 2015 The Chromium Embedded Framework Authors.
// Portions copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/extensions/plugin_info_message_filter.h"

#include "libcef/common/cef_messages.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/plugin_service_filter.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/webplugininfo.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "url/gurl.h"

using content::PluginService;
using content::WebPluginInfo;

namespace extensions {

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

}  // namespace

CefPluginInfoMessageFilter::Context::Context(
    int render_process_id,
    content::BrowserContext* browser_context)
    : render_process_id_(render_process_id),
      resource_context_(browser_context->GetResourceContext()) {
}

CefPluginInfoMessageFilter::Context::~Context() {
}

CefPluginInfoMessageFilter::CefPluginInfoMessageFilter(
    int render_process_id,
    content::BrowserContext* browser_context)
    : BrowserMessageFilter(ExtensionMsgStart),
      context_(render_process_id, browser_context),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_ptr_factory_(this) {
}

bool CefPluginInfoMessageFilter::OnMessageReceived(const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(CefPluginInfoMessageFilter, message)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(CefViewHostMsg_GetPluginInfo,
                                    OnGetPluginInfo)
    IPC_MESSAGE_HANDLER(CefViewHostMsg_IsInternalPluginAvailableForMimeType,
                        OnIsInternalPluginAvailableForMimeType)
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
  // This also fills in |actual_mime_type|.
  if (context_.FindEnabledPlugin(params.render_frame_id, params.url,
                                 params.top_origin_url, params.mime_type,
                                 &output.status, &output.plugin,
                                 &output.actual_mime_type)) {
    context_.DecidePluginStatus(params, output.plugin, &output.status);
  }

  CefViewHostMsg_GetPluginInfo::WriteReplyParams(reply_msg, output);
  Send(reply_msg);
}

void CefPluginInfoMessageFilter::Context::DecidePluginStatus(
    const GetPluginInfo_Params& params,
    const WebPluginInfo& plugin,
    CefViewHostMsg_GetPluginInfo_Status* status) const {
  if (plugin.type == WebPluginInfo::PLUGIN_TYPE_NPAPI) {
    CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
    // NPAPI plugins are not supported inside <webview> guests.
    if (extensions::WebViewRendererState::GetInstance()->IsGuest(
        render_process_id_)) {
      *status = CefViewHostMsg_GetPluginInfo_Status::kNPAPINotSupported;
      return;
    }
  }

  // Check if the plugin is crashing too much.
  if (PluginService::GetInstance()->IsPluginUnstable(plugin.path)) {
    *status = CefViewHostMsg_GetPluginInfo_Status::kUnauthorized;
    return;
  }

  if (*status == CefViewHostMsg_GetPluginInfo_Status::kAllowed) {
    // Allow an embedder of <webview> to block a plugin from being loaded inside
    // the guest. In order to do this, set the status to 'Unauthorized' here,
    // and update the status as appropriate depending on the response from the
    // embedder.
    if (extensions::WebViewRendererState::GetInstance()->IsGuest(
        render_process_id_)) {
      *status = CefViewHostMsg_GetPluginInfo_Status::kUnauthorized;
    }
  }
}

bool CefPluginInfoMessageFilter::Context::FindEnabledPlugin(
    int render_frame_id,
    const GURL& url,
    const GURL& top_origin_url,
    const std::string& mime_type,
    CefViewHostMsg_GetPluginInfo_Status* status,
    WebPluginInfo* plugin,
    std::string* actual_mime_type) const {
  *status = CefViewHostMsg_GetPluginInfo_Status::kAllowed;

  bool allow_wildcard = true;
  std::vector<WebPluginInfo> matching_plugins;
  std::vector<std::string> mime_types;
  PluginService::GetInstance()->GetPluginInfoArray(
      url, mime_type, allow_wildcard, &matching_plugins, &mime_types);
  if (matching_plugins.empty()) {
    *status = CefViewHostMsg_GetPluginInfo_Status::kNotFound;
#if defined(OS_WIN) || defined(OS_MACOSX)
    if (!PluginService::GetInstance()->NPAPIPluginsSupported()) {
      // At this point it is not known for sure this is an NPAPI plugin as it
      // could be a not-yet-installed Pepper plugin. To avoid notifying on
      // these types, bail early based on a blacklist of pepper mime types.
      for (auto pepper_mime_type : kPepperPluginMimeTypes)
        if (pepper_mime_type == mime_type)
          return false;
    }
#endif
    return false;
  }

  content::PluginServiceFilter* filter =
      PluginService::GetInstance()->GetFilter();
  size_t i = 0;
  for (; i < matching_plugins.size(); ++i) {
    if (!filter || filter->IsPluginAvailable(render_process_id_,
                                             render_frame_id,
                                             resource_context_,
                                             url,
                                             top_origin_url,
                                             &matching_plugins[i])) {
      break;
    }
  }

  // If we broke out of the loop, we have found an enabled plugin.
  bool enabled = i < matching_plugins.size();
  if (!enabled) {
    // Otherwise, we only found disabled plugins, so we take the first one.
    i = 0;
    *status = CefViewHostMsg_GetPluginInfo_Status::kDisabled;
  }

  *plugin = matching_plugins[i];
  *actual_mime_type = mime_types[i];

  return enabled;
}

void CefPluginInfoMessageFilter::OnIsInternalPluginAvailableForMimeType(
    const std::string& mime_type,
    bool* is_available,
    std::vector<base::string16>* additional_param_names,
    std::vector<base::string16>* additional_param_values) {
  std::vector<WebPluginInfo> plugins;
  PluginService::GetInstance()->GetInternalPlugins(&plugins);

  for (size_t i = 0; i < plugins.size(); ++i) {
    const WebPluginInfo& plugin = plugins[i];
    const std::vector<content::WebPluginMimeType>& mime_types =
        plugin.mime_types;
    for (size_t j = 0; j < mime_types.size(); ++j) {
      if (mime_types[j].mime_type == mime_type) {
        // TODO(cef): Maybe allow plugins to be disabled here.
        *is_available = true;
        *additional_param_names = mime_types[j].additional_param_names;
        *additional_param_values = mime_types[j].additional_param_values;
        return;
      }
    }
  }

  *is_available = false;
}

}  // namespace extensions
