// Copyright (c) 2013 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_RENDERER_RENDER_THREAD_OBSERVER_H_
#define CEF_LIBCEF_RENDERER_RENDER_THREAD_OBSERVER_H_

#include "base/compiler_specific.h"
#include "content/public/renderer/render_thread_observer.h"

struct Cef_CrossOriginWhiteListEntry_Params;

// This class sends and receives control messages on the renderer process.
class CefRenderThreadObserver : public content::RenderThreadObserver {
 public:
  CefRenderThreadObserver();
  ~CefRenderThreadObserver() override;

  static bool is_incognito_process() { return is_incognito_process_; }

  // RenderThreadObserver implementation.
  bool OnControlMessageReceived(const IPC::Message& message) override;
  void OnRenderProcessShutdown() override;

 private:
  // Message handlers called on the render thread.
  void OnSetIsIncognitoProcess(bool is_incognito_process);
  void OnModifyCrossOriginWhitelistEntry(
      bool add,
      const Cef_CrossOriginWhiteListEntry_Params& params);
  void OnClearCrossOriginWhitelist();

  static bool is_incognito_process_;

  DISALLOW_COPY_AND_ASSIGN(CefRenderThreadObserver);
};

#endif  // CEF_LIBCEF_RENDERER_RENDER_THREAD_OBSERVER_H_
