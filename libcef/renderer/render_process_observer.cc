/// Copyright (c) 2013 The Chromium Embedded Framework Authors.
// Portions (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/renderer/render_process_observer.h"
#include "libcef/common/cef_messages.h"
#include "libcef/renderer/content_renderer_client.h"

#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityPolicy.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURL.h"

CefRenderProcessObserver::CefRenderProcessObserver() {
}

CefRenderProcessObserver::~CefRenderProcessObserver() {
}

bool CefRenderProcessObserver::OnControlMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CefRenderProcessObserver, message)
    IPC_MESSAGE_HANDLER(CefProcessMsg_ModifyCrossOriginWhitelistEntry,
                        OnModifyCrossOriginWhitelistEntry)
    IPC_MESSAGE_HANDLER(CefProcessMsg_ClearCrossOriginWhitelist,
                        OnClearCrossOriginWhitelist)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void CefRenderProcessObserver::WebKitInitialized() {
  CefContentRendererClient::Get()->WebKitInitialized();
}

void CefRenderProcessObserver::OnRenderProcessShutdown() {
  CefContentRendererClient::Get()->OnRenderProcessShutdown();
}

void CefRenderProcessObserver::OnModifyCrossOriginWhitelistEntry(
    bool add,
    const Cef_CrossOriginWhiteListEntry_Params& params) {
  GURL gurl = GURL(params.source_origin);
  if (add) {
    WebKit::WebSecurityPolicy::addOriginAccessWhitelistEntry(
        gurl,
        WebKit::WebString::fromUTF8(params.target_protocol),
        WebKit::WebString::fromUTF8(params.target_domain),
        params.allow_target_subdomains);
  } else {
    WebKit::WebSecurityPolicy::removeOriginAccessWhitelistEntry(
        gurl,
        WebKit::WebString::fromUTF8(params.target_protocol),
        WebKit::WebString::fromUTF8(params.target_domain),
        params.allow_target_subdomains);
  }
}

void CefRenderProcessObserver::OnClearCrossOriginWhitelist() {
  WebKit::WebSecurityPolicy::resetOriginAccessWhitelists();
}
