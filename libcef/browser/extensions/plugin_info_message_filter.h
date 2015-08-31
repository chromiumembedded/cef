// Copyright 2015 The Chromium Embedded Framework Authors.
// Portions copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_EXTENSIONS_PLUGIN_INFO_MESSAGE_FILTER_H_
#define CEF_LIBCEF_BROWSER_EXTENSIONS_PLUGIN_INFO_MESSAGE_FILTER_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_member.h"
#include "base/sequenced_task_runner_helpers.h"
#include "content/public/browser/browser_message_filter.h"

struct CefViewHostMsg_GetPluginInfo_Output;
enum class CefViewHostMsg_GetPluginInfo_Status;
class GURL;

namespace content {
class BrowserContext;
class ResourceContext;
struct WebPluginInfo;
}

namespace extensions {

// This class filters out incoming IPC messages requesting plugin information.
class CefPluginInfoMessageFilter : public content::BrowserMessageFilter {
 public:
  struct GetPluginInfo_Params;

  // Contains all the information needed by the CefPluginInfoMessageFilter.
  class Context {
   public:
    Context(int render_process_id, content::BrowserContext* browser_context);

    ~Context();

    void DecidePluginStatus(
        const GetPluginInfo_Params& params,
        const content::WebPluginInfo& plugin,
        CefViewHostMsg_GetPluginInfo_Status* status) const;
    bool FindEnabledPlugin(int render_frame_id,
                           const GURL& url,
                           const GURL& top_origin_url,
                           const std::string& mime_type,
                           CefViewHostMsg_GetPluginInfo_Status* status,
                           content::WebPluginInfo* plugin,
                           std::string* actual_mime_type) const;

   private:
    int render_process_id_;
    content::ResourceContext* resource_context_;
  };

  CefPluginInfoMessageFilter(int render_process_id,
                             content::BrowserContext* browser_context);

  // content::BrowserMessageFilter methods:
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnDestruct() const override;

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<CefPluginInfoMessageFilter>;

  ~CefPluginInfoMessageFilter() override;

  void OnGetPluginInfo(int render_frame_id,
                       const GURL& url,
                       const GURL& top_origin_url,
                       const std::string& mime_type,
                       IPC::Message* reply_msg);
  void OnIsInternalPluginAvailableForMimeType(
      const std::string& mime_type,
      bool* is_available,
      std::vector<base::string16>* additional_param_names,
      std::vector<base::string16>* additional_param_values);

  // |params| wraps the parameters passed to |OnGetPluginInfo|, because
  // |base::Bind| doesn't support the required arity <http://crbug.com/98542>.
  void PluginsLoaded(const GetPluginInfo_Params& params,
                     IPC::Message* reply_msg,
                     const std::vector<content::WebPluginInfo>& plugins);

  Context context_;

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  base::WeakPtrFactory<CefPluginInfoMessageFilter> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CefPluginInfoMessageFilter);
};

}  // namespace extensions

#endif  // CEF_LIBCEF_BROWSER_EXTENSIONS_PLUGIN_INFO_MESSAGE_FILTER_H_
