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
  void OnRegisterScheme(const std::string& scheme_name,
                        bool is_standard,
                        bool is_local,
                        bool is_display_isolated);

  void RegisterSchemeOnRenderThread(const std::string& scheme_name,
                                    bool is_local,
                                    bool is_display_isolated);

  IPC::Channel* channel_;

  DISALLOW_COPY_AND_ASSIGN(CefRenderMessageFilter);
};


#endif  // CEF_LIBCEF_RENDERER_RENDER_MESSAGE_FILTER_H_
