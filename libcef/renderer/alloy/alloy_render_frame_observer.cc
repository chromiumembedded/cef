// Copyright 2014 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "cef/libcef/renderer/alloy/alloy_render_frame_observer.h"

AlloyRenderFrameObserver::AlloyRenderFrameObserver(
    content::RenderFrame* render_frame)
    : CefRenderFrameObserver(render_frame) {}

void AlloyRenderFrameObserver::OnInterfaceRequestForFrame(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle* interface_pipe) {
  registry_.TryBindInterface(interface_name, interface_pipe);
}

bool AlloyRenderFrameObserver::OnAssociatedInterfaceRequestForFrame(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle* handle) {
  return associated_interfaces_.TryBindInterface(interface_name, handle);
}
