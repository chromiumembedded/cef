// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/resource_context.h"

#include "libcef/browser/net/url_request_context_getter.h"
#include "libcef/browser/thread_util.h"

#include "base/logging.h"
#include "content/browser/resource_context_impl.h"
#include "content/public/browser/browser_thread.h"

#if defined(USE_NSS_CERTS)
#include "net/ssl/client_cert_store_nss.h"
#endif

#if defined(OS_WIN)
#include "net/ssl/client_cert_store_win.h"
#endif

#if defined(OS_MACOSX)
#include "net/ssl/client_cert_store_mac.h"
#endif

namespace {

bool ShouldProxyUserData(const void* key) {
  // If this value is not proxied WebUI will fail to load.
  if (key == content::GetURLDataManagerBackendUserDataKey())
    return true;

  return false;
}

}  // namespace

CefResourceContext::CefResourceContext(
    bool is_off_the_record,
    CefRefPtr<CefRequestContextHandler> handler)
    : parent_(nullptr),
      is_off_the_record_(is_off_the_record),
      handler_(handler) {
}

CefResourceContext::~CefResourceContext() {
}

base::SupportsUserData::Data* CefResourceContext::GetUserData(const void* key)
    const {
  if (parent_ && ShouldProxyUserData(key))
    return parent_->GetUserData(key);
  return content::ResourceContext::GetUserData(key);
}

void CefResourceContext::SetUserData(const void* key, Data* data) {
  if (parent_ && ShouldProxyUserData(key))
    parent_->SetUserData(key, data);
  else
    content::ResourceContext::SetUserData(key, data);
}

void CefResourceContext::SetUserData(const void* key,
                                     std::unique_ptr<Data> data) {
  if (parent_ && ShouldProxyUserData(key))
    parent_->SetUserData(key, std::move(data));
  else
    content::ResourceContext::SetUserData(key, std::move(data));
}

void CefResourceContext::RemoveUserData(const void* key) {
  if (parent_ && ShouldProxyUserData(key))
    parent_->RemoveUserData(key);
  else
    content::ResourceContext::RemoveUserData(key);
}

net::HostResolver* CefResourceContext::GetHostResolver() {
  CHECK(getter_.get());
  return getter_->GetHostResolver();
}

net::URLRequestContext* CefResourceContext::GetRequestContext() {
  CHECK(getter_.get());
  return getter_->GetURLRequestContext();
}

std::unique_ptr<net::ClientCertStore>
    CefResourceContext::CreateClientCertStore() {
#if defined(USE_NSS_CERTS)
  return std::unique_ptr<net::ClientCertStore>(new net::ClientCertStoreNSS(
      net::ClientCertStoreNSS::PasswordDelegateFactory()));
#elif defined(OS_WIN)
  return std::unique_ptr<net::ClientCertStore>(new net::ClientCertStoreWin());
#elif defined(OS_MACOSX)
  return std::unique_ptr<net::ClientCertStore>(new net::ClientCertStoreMac());
#elif defined(USE_OPENSSL)
  // OpenSSL does not use the ClientCertStore infrastructure. On Android client
  // cert matching is done by the OS as part of the call to show the cert
  // selection dialog.
  return std::unique_ptr<net::ClientCertStore>();
#else
#error Unknown platform.
#endif
}

void CefResourceContext::set_extensions_info_map(
    extensions::InfoMap* extensions_info_map) {
  DCHECK(!extension_info_map_);
  extension_info_map_ = extensions_info_map;
}

void CefResourceContext::set_url_request_context_getter(
    CefURLRequestContextGetter* getter) {
  DCHECK(!getter_.get());
  getter_ = getter;
}

void CefResourceContext::set_parent(CefResourceContext* parent) {
  DCHECK(!parent_);
  DCHECK(parent);
  parent_ = parent;
}

void CefResourceContext::AddPluginLoadDecision(
    int render_process_id,
    const base::FilePath& plugin_path,
    bool is_main_frame,
    const url::Origin& main_frame_origin,
    CefViewHostMsg_GetPluginInfo_Status status) {
  CEF_REQUIRE_IOT();
  DCHECK_GE(render_process_id, 0);
  DCHECK(!plugin_path.empty());

  plugin_load_decision_map_.insert(
      std::make_pair(
          std::make_pair(std::make_pair(render_process_id, plugin_path),
                         std::make_pair(is_main_frame, main_frame_origin)),
          status));
}

bool CefResourceContext::HasPluginLoadDecision(
    int render_process_id,
    const base::FilePath& plugin_path,
    bool is_main_frame,
    const url::Origin& main_frame_origin,
    CefViewHostMsg_GetPluginInfo_Status* status) const {
  CEF_REQUIRE_IOT();
  DCHECK_GE(render_process_id, 0);
  DCHECK(!plugin_path.empty());

  PluginLoadDecisionMap::const_iterator it =
      plugin_load_decision_map_.find(
          std::make_pair(std::make_pair(render_process_id, plugin_path),
                         std::make_pair(is_main_frame, main_frame_origin)));
  if (it == plugin_load_decision_map_.end())
    return false;

  *status = it->second;
  return true;
}

void CefResourceContext::ClearPluginLoadDecision(int render_process_id) {
  CEF_REQUIRE_IOT();

  if (render_process_id == -1) {
    plugin_load_decision_map_.clear();
  } else {
    PluginLoadDecisionMap::iterator it = plugin_load_decision_map_.begin();
    while (it != plugin_load_decision_map_.end()) {
      if (it->first.first.first == render_process_id)
        it = plugin_load_decision_map_.erase(it);
      else
        ++it;
    }
  }
}
