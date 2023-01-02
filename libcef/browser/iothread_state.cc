// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/iothread_state.h"

#include "libcef/browser/net/scheme_handler.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/net/scheme_registration.h"

#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/resource_context_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"

CefIOThreadState::CefIOThreadState() {
  // Using base::Unretained() is safe because both this callback and possible
  // deletion of |this| will execute on the IO thread, and this callback will
  // be executed first.
  CEF_POST_TASK(CEF_IOT, base::BindOnce(&CefIOThreadState::InitOnIOThread,
                                        base::Unretained(this)));
}

CefIOThreadState::~CefIOThreadState() {
  CEF_REQUIRE_IOT();
}

void CefIOThreadState::AddHandler(
    const content::GlobalRenderFrameHostId& global_id,
    CefRefPtr<CefRequestContextHandler> handler) {
  CEF_REQUIRE_IOT();
  handler_map_.AddHandler(global_id, handler);
}

void CefIOThreadState::RemoveHandler(
    const content::GlobalRenderFrameHostId& global_id) {
  CEF_REQUIRE_IOT();
  handler_map_.RemoveHandler(global_id);
}

CefRefPtr<CefRequestContextHandler> CefIOThreadState::GetHandler(
    const content::GlobalRenderFrameHostId& global_id,
    bool require_frame_match) const {
  CEF_REQUIRE_IOT();
  return handler_map_.GetHandler(global_id, require_frame_match);
}

void CefIOThreadState::RegisterSchemeHandlerFactory(
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
    if (it != scheme_handler_factory_map_.end()) {
      scheme_handler_factory_map_.erase(it);
    }
  }
}

void CefIOThreadState::ClearSchemeHandlerFactories() {
  CEF_REQUIRE_IOT();

  scheme_handler_factory_map_.clear();

  // Restore the default internal handlers.
  scheme::RegisterInternalHandlers(this);
}

CefRefPtr<CefSchemeHandlerFactory> CefIOThreadState::GetSchemeHandlerFactory(
    const GURL& url) {
  CEF_REQUIRE_IOT();

  if (scheme_handler_factory_map_.empty()) {
    return nullptr;
  }

  const std::string& scheme_lower = url.scheme();
  const std::string& domain_lower =
      url.IsStandard() ? url.host() : std::string();

  if (!domain_lower.empty()) {
    // Sanity check.
    DCHECK(scheme::IsStandardScheme(scheme_lower)) << scheme_lower;

    // Try for a match with hostname first.
    const auto it = scheme_handler_factory_map_.find(
        std::make_pair(scheme_lower, domain_lower));
    if (it != scheme_handler_factory_map_.end()) {
      return it->second;
    }
  }

  // Try for a match with no specified hostname.
  const auto it = scheme_handler_factory_map_.find(
      std::make_pair(scheme_lower, std::string()));
  if (it != scheme_handler_factory_map_.end()) {
    return it->second;
  }

  return nullptr;
}

void CefIOThreadState::InitOnIOThread() {
  CEF_REQUIRE_IOT();

  // Add the default internal handlers.
  scheme::RegisterInternalHandlers(this);
}
