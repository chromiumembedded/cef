// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_BROWSER_MESSAGE_FILTER_H_
#define CEF_LIBCEF_BROWSER_BROWSER_MESSAGE_FILTER_H_

#include <stdint.h>

#include <string>

#include "ipc/ipc_channel_proxy.h"
#include "ipc/message_filter.h"

namespace content {
class RenderProcessHost;
}

struct CefProcessHostMsg_GetNewBrowserInfo_Params;
struct CefProcessHostMsg_GetNewRenderThreadInfo_Params;

// This class sends and receives control messages on the browser process.
class CefBrowserMessageFilter : public IPC::MessageFilter {
 public:
  explicit CefBrowserMessageFilter(int render_process_id);
  ~CefBrowserMessageFilter() override;

  // IPC::ChannelProxy::MessageFilter implementation.
  void OnFilterRemoved() override;
  bool OnMessageReceived(const IPC::Message& message) override;

  bool Send(IPC::Message* message);

 private:
  // Message handlers.
  void OnGetNewRenderThreadInfo(
      CefProcessHostMsg_GetNewRenderThreadInfo_Params* params);
  void OnGetNewBrowserInfo(
      int render_view_routing_id,
      int render_frame_routing_id,
      IPC::Message* reply_msg);
  void OnFrameFocused(int32_t render_frame_routing_id);

  int render_process_id_;

  DISALLOW_COPY_AND_ASSIGN(CefBrowserMessageFilter);
};


#endif  // CEF_LIBCEF_BROWSER_BROWSER_MESSAGE_FILTER_H_
