// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/resource_context.h"

#include "libcef/browser/net/url_request_context_getter.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/net/scheme_registration.h"

#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
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

CefResourceContext::~CefResourceContext() {
  // This is normally called in the parent ResourceContext destructor, but we
  // want to call it here so that this CefResourceContext object is still
  // valid when CefNetworkDelegate::OnCompleted is called via the URLRequest
  // destructor.
  if (content::ResourceDispatcherHostImpl::Get())
    content::ResourceDispatcherHostImpl::Get()->CancelRequestsForContext(this);
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

void CefResourceContext::AddHandler(
    int render_process_id,
    int render_frame_id,
    int frame_tree_node_id,
    CefRefPtr<CefRequestContextHandler> handler) {
  CEF_REQUIRE_IOT();
  DCHECK_GE(render_process_id, 0);
  DCHECK_GE(render_frame_id, 0);
  DCHECK_GE(frame_tree_node_id, 0);
  DCHECK(handler);

  render_id_handler_map_.insert(std::make_pair(
      std::make_pair(render_process_id, render_frame_id), handler));
  node_id_handler_map_.insert(std::make_pair(frame_tree_node_id, handler));
}

void CefResourceContext::RemoveHandler(int render_process_id,
                                       int render_frame_id,
                                       int frame_tree_node_id) {
  CEF_REQUIRE_IOT();
  DCHECK_GE(render_process_id, 0);
  DCHECK_GE(render_frame_id, 0);
  DCHECK_GE(frame_tree_node_id, 0);

  auto it1 = render_id_handler_map_.find(
      std::make_pair(render_process_id, render_frame_id));
  if (it1 != render_id_handler_map_.end())
    render_id_handler_map_.erase(it1);

  auto it2 = node_id_handler_map_.find(frame_tree_node_id);
  if (it2 != node_id_handler_map_.end())
    node_id_handler_map_.erase(it2);
}

CefRefPtr<CefRequestContextHandler> CefResourceContext::GetHandler(
    int render_process_id,
    int render_frame_id,
    int frame_tree_node_id,
    bool require_frame_match) {
  CEF_REQUIRE_IOT();

  if (render_process_id >= 0 && render_frame_id >= 0) {
    const auto it1 = render_id_handler_map_.find(
        std::make_pair(render_process_id, render_frame_id));
    if (it1 != render_id_handler_map_.end())
      return it1->second;
  }

  if (frame_tree_node_id >= 0) {
    const auto it2 = node_id_handler_map_.find(frame_tree_node_id);
    if (it2 != node_id_handler_map_.end())
      return it2->second;
  }

  if (render_process_id >= 0 && !require_frame_match) {
    // Choose an arbitrary handler for the same process.
    for (auto& kv : render_id_handler_map_) {
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

void CefResourceContext::RegisterSchemeHandlerFactory(
    const std::string& scheme_name,
    const std::string& domain_name,
    CefRefPtr<CefSchemeHandlerFactory> factory) {
  CEF_REQUIRE_IOT();

  const std::string& scheme_lower = base::ToLowerASCII(scheme_name);
  std::string domain_lower;

  // Hostname is only supported for standard schemes.
  if (scheme::IsStandardScheme(scheme_lower)) {
    // Hostname might contain Unicode characters.
    domain_lower =
        base::UTF16ToUTF8(base::i18n::ToLower(base::UTF8ToUTF16(domain_name)));
  }

  const auto key = std::make_pair(scheme_lower, domain_lower);

  if (factory) {
    // Add or replace the factory.
    scheme_handler_factory_map_[key] = factory;
  } else {
    // Remove the existing factory, if any.
    auto it = scheme_handler_factory_map_.find(key);
    if (it != scheme_handler_factory_map_.end())
      scheme_handler_factory_map_.erase(it);
  }
}

void CefResourceContext::ClearSchemeHandlerFactories() {
  CEF_REQUIRE_IOT();
  scheme_handler_factory_map_.clear();
}

CefRefPtr<CefSchemeHandlerFactory> CefResourceContext::GetSchemeHandlerFactory(
    const GURL& url) {
  CEF_REQUIRE_IOT();

  if (scheme_handler_factory_map_.empty())
    return nullptr;

  const std::string& scheme_lower = url.scheme();
  const std::string& domain_lower =
      url.IsStandard() ? url.host() : std::string();

  if (!domain_lower.empty()) {
    // Sanity check.
    DCHECK(scheme::IsStandardScheme(scheme_lower)) << scheme_lower;

    // Try for a match with hostname first.
    const auto it = scheme_handler_factory_map_.find(
        std::make_pair(scheme_lower, domain_lower));
    if (it != scheme_handler_factory_map_.end())
      return it->second;
  }

  // Try for a match with no specified hostname.
  const auto it = scheme_handler_factory_map_.find(
      std::make_pair(scheme_lower, std::string()));
  if (it != scheme_handler_factory_map_.end())
    return it->second;

  return nullptr;
}
