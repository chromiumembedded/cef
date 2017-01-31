// Copyright 2015 The Chromium Embedded Framework Authors.
// Portions copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_PLUGINS_PLUGIN_INFO_MESSAGE_FILTER_H_
#define CEF_LIBCEF_BROWSER_PLUGINS_PLUGIN_INFO_MESSAGE_FILTER_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner_helpers.h"
#include "chrome/browser/plugins/plugin_metadata.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/keyed_service/core/keyed_service_shutdown_notifier.h"
#include "components/prefs/pref_member.h"
#include "content/public/browser/browser_message_filter.h"
#include "extensions/features/features.h"
#include "ppapi/features/features.h"

class CefBrowserContext;
class CefResourceContext;
struct CefViewHostMsg_GetPluginInfo_Output;
enum class CefViewHostMsg_GetPluginInfo_Status;
class GURL;

namespace content {
struct WebPluginInfo;
}

namespace extensions {
class ExtensionRegistry;
}

namespace url {
class Origin;
}

// This class filters out incoming IPC messages requesting plugin information.
class CefPluginInfoMessageFilter : public content::BrowserMessageFilter {
 public:
  struct GetPluginInfo_Params;

  // Contains all the information needed by the CefPluginInfoMessageFilter.
  class Context {
   public:
    Context(int render_process_id, CefBrowserContext* profile);

    ~Context();
    void ShutdownOnUIThread();

    void DecidePluginStatus(
        const GetPluginInfo_Params& params,
        const content::WebPluginInfo& plugin,
        const PluginMetadata* plugin_metadata,
        CefViewHostMsg_GetPluginInfo_Status* status) const;
    bool FindEnabledPlugin(
        const GetPluginInfo_Params& params,
        CefViewHostMsg_GetPluginInfo_Status* status,
        content::WebPluginInfo* plugin,
        std::string* actual_mime_type,
        std::unique_ptr<PluginMetadata>* plugin_metadata) const;
    void GetPluginContentSetting(const content::WebPluginInfo& plugin,
                                 const GURL& policy_url,
                                 const GURL& plugin_url,
                                 const std::string& resource,
                                 ContentSetting* setting,
                                 bool* is_default,
                                 bool* is_managed) const;
    bool IsPluginEnabled(const content::WebPluginInfo& plugin) const;

   private:
    int render_process_id_;
    CefResourceContext* resource_context_;
#if BUILDFLAG(ENABLE_EXTENSIONS)
    extensions::ExtensionRegistry* extension_registry_;
#endif
    const HostContentSettingsMap* host_content_settings_map_;

    BooleanPrefMember allow_outdated_plugins_;
    BooleanPrefMember always_authorize_plugins_;
  };

  CefPluginInfoMessageFilter(int render_process_id,
                             CefBrowserContext* profile);

  // content::BrowserMessageFilter methods:
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnDestruct() const override;

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<CefPluginInfoMessageFilter>;

  void ShutdownOnUIThread();
  ~CefPluginInfoMessageFilter() override;

  void OnGetPluginInfo(int render_frame_id,
                       const GURL& url,
                       bool is_main_frame,
                       const url::Origin& main_frame_origin,
                       const std::string& mime_type,
                       IPC::Message* reply_msg);

  // |params| wraps the parameters passed to |OnGetPluginInfo|, because
  // |base::Bind| doesn't support the required arity <http://crbug.com/98542>.
  void PluginsLoaded(const GetPluginInfo_Params& params,
                     IPC::Message* reply_msg,
                     const std::vector<content::WebPluginInfo>& plugins);

#if BUILDFLAG(ENABLE_PEPPER_CDMS)
  // Returns whether any internal plugin supporting |mime_type| is registered
  // and enabled. Does not determine whether the plugin can actually be
  // instantiated (e.g. whether it has all its dependencies).
  // When the returned *|is_available| is true, |additional_param_names| and
  // |additional_param_values| contain the name-value pairs, if any, specified
  // for the *first* non-disabled plugin found that is registered for
  // |mime_type|.
  void OnIsInternalPluginAvailableForMimeType(
      const std::string& mime_type,
      bool* is_available,
      std::vector<base::string16>* additional_param_names,
      std::vector<base::string16>* additional_param_values);
#endif

  Context context_;
  std::unique_ptr<KeyedServiceShutdownNotifier::Subscription>
      shutdown_notifier_;

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  base::WeakPtrFactory<CefPluginInfoMessageFilter> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CefPluginInfoMessageFilter);
};

#endif  // CEF_LIBCEF_BROWSER_PLUGINS_PLUGIN_INFO_MESSAGE_FILTER_H_
