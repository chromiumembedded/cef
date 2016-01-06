/// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/browser_message_filter.h"

#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/browser_info_manager.h"
#include "libcef/browser/context.h"
#include "libcef/browser/origin_whitelist_impl.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/cef_messages.h"
#include "libcef/common/content_client.h"
#include "libcef/common/values_impl.h"

#include "base/compiler_specific.h"
#include "base/bind.h"
#include "content/common/frame_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/render_process_host.h"

CefBrowserMessageFilter::CefBrowserMessageFilter(
    content::RenderProcessHost* host)
    : host_(host),
      sender_(NULL) {
}

CefBrowserMessageFilter::~CefBrowserMessageFilter() {
}

void CefBrowserMessageFilter::OnFilterAdded(IPC::Sender* sender) {
  sender_ = sender;
}

void CefBrowserMessageFilter::OnFilterRemoved() {
  host_ = NULL;
  sender_ = NULL;
}

bool CefBrowserMessageFilter::OnMessageReceived(const IPC::Message& message) {
  if (message.type() == FrameHostMsg_FrameFocused::ID) {
    // Observe but don't handle this message.
    OnFrameFocused(message.routing_id());
    return false;
  }

  bool handled = true;
  if (message.type() == ViewHostMsg_CreateWindow::ID) {
    // Observe but don't handle this message.
    handled = false;
  }

  IPC_BEGIN_MESSAGE_MAP(CefBrowserMessageFilter, message)
    IPC_MESSAGE_HANDLER(CefProcessHostMsg_GetNewRenderThreadInfo,
                        OnGetNewRenderThreadInfo)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(CefProcessHostMsg_GetNewBrowserInfo,
                                    OnGetNewBrowserInfo)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(ViewHostMsg_CreateWindow, OnCreateWindow)
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

void CefBrowserMessageFilter::OnGetNewBrowserInfo(
    int render_view_routing_id,
    int render_frame_routing_id,
    IPC::Message* reply_msg) {
  CefBrowserInfoManager::GetInstance()->OnGetNewBrowserInfo(
      host_,
      render_view_routing_id,
      render_frame_routing_id,
      reply_msg);
}

void CefBrowserMessageFilter::OnCreateWindow(
    const ViewHostMsg_CreateWindow_Params& params,
    IPC::Message* reply_msg) {
  CefBrowserInfoManager::GetInstance()->OnCreateWindow(host_, params);

  // Reply message is not used.
  delete reply_msg;
}

void CefBrowserMessageFilter::OnFrameFocused(int32_t render_frame_routing_id) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefBrowserMessageFilter::OnFrameFocused, this,
                   render_frame_routing_id));
    return;
  }

  if (!host_)
    return;

  CefRefPtr<CefBrowserHostImpl> browser =
      CefBrowserHostImpl::GetBrowserForFrame(host_->GetID(),
                                             render_frame_routing_id);
  if (browser.get())
    browser->SetFocusedFrame(render_frame_routing_id);
}
