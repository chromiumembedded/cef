// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_RENDERER_RENDER_MESSAGE_FILTER_H_
#define CEF_LIBCEF_RENDERER_RENDER_MESSAGE_FILTER_H_

#include <stdint.h>

#include <string>

#include "ipc/ipc_channel_proxy.h"
#include "ipc/message_filter.h"

// This class sends and receives control messages on the renderer process.
class CefRenderMessageFilter : public IPC::MessageFilter {
 public:
  CefRenderMessageFilter();
  ~CefRenderMessageFilter() override;

  // IPC::ChannelProxy::MessageFilter implementation.
  void OnFilterAdded(IPC::Channel* channel) override;
  void OnFilterRemoved() override;
  bool OnMessageReceived(const IPC::Message& message) override;

  bool Send(IPC::Message* message);

 private:
  // Message handlers called on the IO thread.
  void OnDevToolsAgentAttach(const std::string& host_id, int session_id);
  void OnDevToolsAgentDetach(int32_t routing_id);
  void OnIsCrashReportingEnabled(bool* enabled);

  void OnDevToolsAgentAttach_RT();
  void OnDevToolsAgentDetach_RT(int32_t routing_id);

  IPC::Channel* channel_;

  DISALLOW_COPY_AND_ASSIGN(CefRenderMessageFilter);
};


#endif  // CEF_LIBCEF_RENDERER_RENDER_MESSAGE_FILTER_H_
