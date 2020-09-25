// Copyright (c) 2013 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_RENDERER_RENDER_THREAD_OBSERVER_H_
#define CEF_LIBCEF_RENDERER_RENDER_THREAD_OBSERVER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "content/public/renderer/render_thread_observer.h"

struct Cef_CrossOriginWhiteListEntry_Params;

// This class sends and receives control messages in the renderer process.
class CefRenderThreadObserver : public content::RenderThreadObserver {
 public:
  CefRenderThreadObserver();
  ~CefRenderThreadObserver() override;

 private:
  // content::RenderThreadObserver:
  bool OnControlMessageReceived(const IPC::Message& message) override;

  // Message handlers called on the render thread.
  void OnModifyCrossOriginWhitelistEntry(
      bool add,
      const Cef_CrossOriginWhiteListEntry_Params& params);
  void OnClearCrossOriginWhitelist();

  DISALLOW_COPY_AND_ASSIGN(CefRenderThreadObserver);
};

#endif  // CEF_LIBCEF_RENDERER_RENDER_THREAD_OBSERVER_H_
