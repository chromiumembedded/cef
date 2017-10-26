// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/browser_message_filter.h"

#include "libcef/browser/browser_info_manager.h"
#include "libcef/browser/origin_whitelist_impl.h"
#include "libcef/common/cef_messages.h"
#include "libcef/common/content_client.h"
#include "libcef/common/values_impl.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "content/public/common/child_process_host.h"

CefBrowserMessageFilter::CefBrowserMessageFilter(int render_process_id)
    : content::BrowserMessageFilter(ExtensionMsgStart),
      render_process_id_(render_process_id) {}

CefBrowserMessageFilter::~CefBrowserMessageFilter() {}

void CefBrowserMessageFilter::OnFilterRemoved() {
  render_process_id_ = content::ChildProcessHost::kInvalidUniqueID;
  content::BrowserMessageFilter::OnFilterRemoved();
}

bool CefBrowserMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;

  IPC_BEGIN_MESSAGE_MAP(CefBrowserMessageFilter, message)
    IPC_MESSAGE_HANDLER(CefProcessHostMsg_GetNewRenderThreadInfo,
                        OnGetNewRenderThreadInfo)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(CefProcessHostMsg_GetNewBrowserInfo,
                                    OnGetNewBrowserInfo)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void CefBrowserMessageFilter::OnGetNewRenderThreadInfo(
    CefProcessHostMsg_GetNewRenderThreadInfo_Params* params) {
  GetCrossOriginWhitelistEntries(&params->cross_origin_whitelist_entries);

  CefRefPtr<CefApp> app = CefContentClient::Get()->application();
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

void CefBrowserMessageFilter::OnGetNewBrowserInfo(int render_frame_routing_id,
                                                  IPC::Message* reply_msg) {
  if (render_process_id_ != content::ChildProcessHost::kInvalidUniqueID) {
    CefBrowserInfoManager::GetInstance()->OnGetNewBrowserInfo(
        render_process_id_, render_frame_routing_id, reply_msg);
  } else {
    delete reply_msg;
  }
}
