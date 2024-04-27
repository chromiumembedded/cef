// Copyright 2014 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef LIBCEF_RENDERER_ALLOY_ALLOY_RENDER_FRAME_OBSERVER_H_
#define LIBCEF_RENDERER_ALLOY_ALLOY_RENDER_FRAME_OBSERVER_H_

#include "libcef/renderer/render_frame_observer.h"

#include "services/service_manager/public/cpp/binder_registry.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"

class AlloyRenderFrameObserver : public CefRenderFrameObserver {
 public:
  explicit AlloyRenderFrameObserver(content::RenderFrame* render_frame);

  AlloyRenderFrameObserver(const AlloyRenderFrameObserver&) = delete;
  AlloyRenderFrameObserver& operator=(const AlloyRenderFrameObserver&) = delete;

  // RenderFrameObserver methods:
  void OnInterfaceRequestForFrame(
      const std::string& interface_name,
      mojo::ScopedMessagePipeHandle* interface_pipe) override;
  bool OnAssociatedInterfaceRequestForFrame(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle* handle) override;

  service_manager::BinderRegistry* registry() { return &registry_; }
  blink::AssociatedInterfaceRegistry* associated_interfaces() {
    return &associated_interfaces_;
  }

 private:
  service_manager::BinderRegistry registry_;

  // For interfaces which must be associated with some IPC::ChannelProxy,
  // meaning that messages on the interface retain FIFO with respect to legacy
  // Chrome IPC messages sent or dispatched on the channel.
  blink::AssociatedInterfaceRegistry associated_interfaces_;
};

#endif  // LIBCEF_RENDERER_ALLOY_ALLOY_RENDER_FRAME_OBSERVER_H_
