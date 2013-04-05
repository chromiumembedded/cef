// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/origin_whitelist_impl.h"

#include <string>
#include <vector>

#include "include/cef_origin_whitelist.h"
#include "libcef/browser/context.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/cef_messages.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/synchronization/lock.h"
#include "content/public/browser/render_process_host.h"
#include "googleurl/src/gurl.h"

namespace {

// Class that manages cross-origin whitelist registrations.
class CefOriginWhitelistManager {
 public:
  CefOriginWhitelistManager() {}

  // Retrieve the singleton instance.
  static CefOriginWhitelistManager* GetInstance();

  bool AddOriginEntry(const std::string& source_origin,
                      const std::string& target_protocol,
                      const std::string& target_domain,
                      bool allow_target_subdomains) {
    Cef_CrossOriginWhiteListEntry_Params info;
    info.source_origin = source_origin;
    info.target_protocol = target_protocol;
    info.target_domain = target_domain;
    info.allow_target_subdomains = allow_target_subdomains;

    {
      base::AutoLock lock_scope(lock_);

      // Verify that the origin entry doesn't already exist.
      OriginList::const_iterator it = origin_list_.begin();
      for (; it != origin_list_.end(); ++it) {
        if (IsEqual(*it, info))
          return false;
      }

      origin_list_.push_back(info);
    }

    SendModifyCrossOriginWhitelistEntry(true, info);
    return true;
  }

  bool RemoveOriginEntry(const std::string& source_origin,
                         const std::string& target_protocol,
                         const std::string& target_domain,
                         bool allow_target_subdomains) {
    Cef_CrossOriginWhiteListEntry_Params info;
    info.source_origin = source_origin;
    info.target_protocol = target_protocol;
    info.target_domain = target_domain;
    info.allow_target_subdomains = allow_target_subdomains;

    bool found = false;

    {
      base::AutoLock lock_scope(lock_);

      OriginList::iterator it = origin_list_.begin();
      for (; it != origin_list_.end(); ++it) {
        if (IsEqual(*it, info)) {
          origin_list_.erase(it);
          found = true;
          break;
        }
      }
    }

    if (!found)
      return false;

    SendModifyCrossOriginWhitelistEntry(false, info);
    return true;
  }

  void ClearOrigins() {
    {
      base::AutoLock lock_scope(lock_);
      origin_list_.clear();
    }

    SendClearCrossOriginWhitelist();
  }

  void GetCrossOriginWhitelistEntries(
      std::vector<Cef_CrossOriginWhiteListEntry_Params>* entries) {
    base::AutoLock lock_scope(lock_);

    if (origin_list_.empty())
      return;
    entries->insert(entries->end(), origin_list_.begin(), origin_list_.end());
  }

 private:
  // Send the modify cross-origin whitelist entry message to all currently
  // existing hosts.
  static void SendModifyCrossOriginWhitelistEntry(
      bool add,
      Cef_CrossOriginWhiteListEntry_Params& params) {
    CEF_REQUIRE_UIT();

    content::RenderProcessHost::iterator i(
        content::RenderProcessHost::AllHostsIterator());
    for (; !i.IsAtEnd(); i.Advance()) {
      i.GetCurrentValue()->Send(
          new CefProcessMsg_ModifyCrossOriginWhitelistEntry(add, params));
    }
  }

  // Send the clear cross-origin whitelists message to all currently existing
  // hosts.
  static void SendClearCrossOriginWhitelist() {
    CEF_REQUIRE_UIT();

    content::RenderProcessHost::iterator i(
        content::RenderProcessHost::AllHostsIterator());
    for (; !i.IsAtEnd(); i.Advance()) {
      i.GetCurrentValue()->Send(new CefProcessMsg_ClearCrossOriginWhitelist);
    }
  }

  static bool IsEqual(const Cef_CrossOriginWhiteListEntry_Params& param1,
                      const Cef_CrossOriginWhiteListEntry_Params& param2) {
    return (param1.source_origin == param2.source_origin &&
            param1.target_protocol == param2.target_protocol &&
            param1.target_domain == param2.target_domain &&
            param1.allow_target_subdomains == param2.allow_target_subdomains);
  }

  base::Lock lock_;

  // List of registered origins. Access must be protected by |lock_|.
  typedef std::vector<Cef_CrossOriginWhiteListEntry_Params> OriginList;
  OriginList origin_list_;

  DISALLOW_EVIL_CONSTRUCTORS(CefOriginWhitelistManager);
};

base::LazyInstance<CefOriginWhitelistManager> g_manager =
    LAZY_INSTANCE_INITIALIZER;

CefOriginWhitelistManager* CefOriginWhitelistManager::GetInstance() {
  return g_manager.Pointer();
}

}  // namespace

bool CefAddCrossOriginWhitelistEntry(const CefString& source_origin,
                                     const CefString& target_protocol,
                                     const CefString& target_domain,
                                     bool allow_target_subdomains) {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED();
    return false;
  }

  std::string source_url = source_origin;
  GURL gurl = GURL(source_url);
  if (gurl.is_empty() || !gurl.is_valid()) {
    NOTREACHED() << "Invalid source_origin URL: " << source_url;
    return false;
  }

  if (CEF_CURRENTLY_ON_UIT()) {
    return CefOriginWhitelistManager::GetInstance()->AddOriginEntry(
        source_origin, target_protocol, target_domain, allow_target_subdomains);
  } else {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(base::IgnoreResult(&CefAddCrossOriginWhitelistEntry),
                   source_origin, target_protocol, target_domain,
                   allow_target_subdomains));
  }

  return true;
}

bool CefRemoveCrossOriginWhitelistEntry(const CefString& source_origin,
                                        const CefString& target_protocol,
                                        const CefString& target_domain,
                                        bool allow_target_subdomains) {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED();
    return false;
  }

  std::string source_url = source_origin;
  GURL gurl = GURL(source_url);
  if (gurl.is_empty() || !gurl.is_valid()) {
    NOTREACHED() << "Invalid source_origin URL: " << source_url;
    return false;
  }

  if (CEF_CURRENTLY_ON_UIT()) {
    return CefOriginWhitelistManager::GetInstance()->RemoveOriginEntry(
        source_origin, target_protocol, target_domain, allow_target_subdomains);
  } else {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(base::IgnoreResult(&CefRemoveCrossOriginWhitelistEntry),
                   source_origin, target_protocol, target_domain,
                   allow_target_subdomains));
  }

  return true;
}

bool CefClearCrossOriginWhitelist() {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED();
    return false;
  }

  if (CEF_CURRENTLY_ON_UIT()) {
    CefOriginWhitelistManager::GetInstance()->ClearOrigins();
  } else {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(base::IgnoreResult(&CefClearCrossOriginWhitelist)));
  }

  return true;
}

void GetCrossOriginWhitelistEntries(
    std::vector<Cef_CrossOriginWhiteListEntry_Params>* entries) {
  CefOriginWhitelistManager::GetInstance()->GetCrossOriginWhitelistEntries(
      entries);
}
