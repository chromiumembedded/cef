/// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/browser_message_filter.h"

#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/content_browser_client.h"
#include "libcef/browser/context.h"
#include "libcef/browser/origin_whitelist_impl.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/cef_messages.h"
#include "libcef/common/values_impl.h"

#include "base/compiler_specific.h"
#include "base/bind.h"
#include "content/public/browser/render_process_host.h"

CefBrowserMessageFilter::CefBrowserMessageFilter(
    content::RenderProcessHost* host)
    : host_(host),
      channel_(NULL) {
}

CefBrowserMessageFilter::~CefBrowserMessageFilter() {
}

void CefBrowserMessageFilter::OnFilterAdded(IPC::Channel* channel) {
  channel_ = channel;
}

void CefBrowserMessageFilter::OnFilterRemoved() {
}

bool CefBrowserMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CefBrowserMessageFilter, message)
    IPC_MESSAGE_HANDLER(CefProcessHostMsg_GetNewRenderThreadInfo,
                        OnGetNewRenderThreadInfo)
    IPC_MESSAGE_HANDLER(CefProcessHostMsg_GetNewBrowserInfo,
                        OnGetNewBrowserInfo)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool CefBrowserMessageFilter::Send(IPC::Message* message) {
  return host_->Send(message);
}

void CefBrowserMessageFilter::OnGetNewRenderThreadInfo(
    CefProcessHostMsg_GetNewRenderThreadInfo_Params* params) {
  GetCrossOriginWhitelistEntries(&params->cross_origin_whitelist_entries);

  CefRefPtr<CefApp> app = _Context->application();
  if (app.get()) {
    CefRefPtr<CefBrowserProcessHandler> handler =
        app->GetBrowserProcessHandler();
    if (handler.get()) {
      CefRefPtr<CefListValueImpl> listValuePtr(
        new CefListValueImpl(&params->extra_info, false, false));
      handler->OnRenderProcessThreadCreated(listValuePtr.get());
      listValuePtr->Detach(NULL);
    }
  }
}

void CefBrowserMessageFilter::OnGetNewBrowserInfo(
    int routing_id, CefProcessHostMsg_GetNewBrowserInfo_Params* params) {
  CefRefPtr<CefBrowserHostImpl> browser =
      CefBrowserHostImpl::GetBrowserByRoutingID(host_->GetID(), routing_id);
  if (browser.get()) {
    params->browser_id = browser->GetIdentifier();
    params->is_popup = browser->IsPopup();
  } else {
    CefContentBrowserClient::NewPopupBrowserInfo info;
    CefContentBrowserClient::Get()->GetNewPopupBrowserInfo(host_->GetID(),
                                                           routing_id,
                                                           &info);
    DCHECK_GT(info.browser_id, 0);
    params->browser_id = info.browser_id;
    params->is_popup = true;
  }
}
