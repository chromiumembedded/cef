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

CefResourceContext::CefResourceContext(bool is_off_the_record)
    : is_off_the_record_(is_off_the_record) {}

CefResourceContext::~CefResourceContext() {}

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

void CefResourceContext::AddHandler(
    int render_process_id,
    int render_frame_id,
    CefRefPtr<CefRequestContextHandler> handler) {
  CEF_REQUIRE_IOT();
  DCHECK_GE(render_process_id, 0);
  DCHECK(handler);

  handler_map_.insert(std::make_pair(
      std::make_pair(render_process_id, render_frame_id), handler));
}

void CefResourceContext::RemoveHandler(int render_process_id,
                                       int render_frame_id) {
  CEF_REQUIRE_IOT();
  DCHECK_GE(render_process_id, 0);

  HandlerMap::iterator it =
      handler_map_.find(std::make_pair(render_process_id, render_frame_id));
  if (it != handler_map_.end())
    handler_map_.erase(it);
}

CefRefPtr<CefRequestContextHandler> CefResourceContext::GetHandler(
    int render_process_id,
    int render_frame_id,
    bool require_frame_match) {
  CEF_REQUIRE_IOT();
  DCHECK_GE(render_process_id, 0);

  HandlerMap::const_iterator it =
      handler_map_.find(std::make_pair(render_process_id, render_frame_id));
  if (it != handler_map_.end())
    return it->second;

  if (!require_frame_match) {
    // Choose an arbitrary handler for the same process.
    for (auto& kv : handler_map_) {
      if (kv.first.first == render_process_id)
        return kv.second;
    }
  }

  return nullptr;
}

void CefResourceContext::AddPluginLoadDecision(
    int render_process_id,
    const base::FilePath& plugin_path,
    bool is_main_frame,
    const url::Origin& main_frame_origin,
    chrome::mojom::PluginStatus status) {
  CEF_REQUIRE_IOT();
  DCHECK_GE(render_process_id, 0);
  DCHECK(!plugin_path.empty());

  plugin_load_decision_map_.insert(std::make_pair(
      std::make_pair(std::make_pair(render_process_id, plugin_path),
                     std::make_pair(is_main_frame, main_frame_origin)),
      status));
}

bool CefResourceContext::HasPluginLoadDecision(
    int render_process_id,
    const base::FilePath& plugin_path,
    bool is_main_frame,
    const url::Origin& main_frame_origin,
    chrome::mojom::PluginStatus* status) const {
  CEF_REQUIRE_IOT();
  DCHECK_GE(render_process_id, 0);
  DCHECK(!plugin_path.empty());

  PluginLoadDecisionMap::const_iterator it = plugin_load_decision_map_.find(
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
