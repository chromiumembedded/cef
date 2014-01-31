// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_RENDERER_RENDER_MESSAGE_FILTER_H_
#define CEF_LIBCEF_RENDERER_RENDER_MESSAGE_FILTER_H_

#include <string>
#include "ipc/ipc_channel_proxy.h"

// This class sends and receives control messages on the renderer process.
class CefRenderMessageFilter : public IPC::ChannelProxy::MessageFilter {
 public:
  CefRenderMessageFilter();
  virtual ~CefRenderMessageFilter();

  // IPC::ChannelProxy::MessageFilter implementation.
  virtual void OnFilterAdded(IPC::Channel* channel) OVERRIDE;
  virtual void OnFilterRemoved() OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  // Message handlers called on the IO thread.
  void OnDevToolsAgentAttach();
  void OnDevToolsAgentDetach();

  void OnDevToolsAgentAttach_RT();
  void OnDevToolsAgentDetach_IOT();
  void OnDevToolsAgentDetach_RT();

  IPC::Channel* channel_;

  DISALLOW_COPY_AND_ASSIGN(CefRenderMessageFilter);
};


#endif  // CEF_LIBCEF_RENDERER_RENDER_MESSAGE_FILTER_H_
