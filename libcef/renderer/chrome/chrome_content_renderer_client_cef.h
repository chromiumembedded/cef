// Copyright 2020 The Chromium Embedded Framework Authors.
// Portions copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_RENDERER_CHROME_CHROME_CONTENT_RENDERER_CLIENT_CEF_
#define CEF_LIBCEF_RENDERER_CHROME_CHROME_CONTENT_RENDERER_CLIENT_CEF_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/renderer/chrome_content_renderer_client.h"

class CefRenderManager;

// CEF override of ChromeContentRendererClient.
class ChromeContentRendererClientCef : public ChromeContentRendererClient {
 public:
  ChromeContentRendererClientCef();

  ChromeContentRendererClientCef(const ChromeContentRendererClientCef&) =
      delete;
  ChromeContentRendererClientCef& operator=(
      const ChromeContentRendererClientCef&) = delete;

  ~ChromeContentRendererClientCef() override;

  // Render thread task runner.
  base::SingleThreadTaskRunner* render_task_runner() const {
    return render_task_runner_.get();
  }

  // Returns the task runner for the current thread. Returns NULL if the current
  // thread is not the main render process thread.
  scoped_refptr<base::SingleThreadTaskRunner> GetCurrentTaskRunner();

  // ChromeContentRendererClient overrides.
  void RenderThreadStarted() override;
  void RenderThreadConnected() override;
  void RenderFrameCreated(content::RenderFrame* render_frame) override;
  void WebViewCreated(blink::WebView* web_view,
                      bool was_created_by_renderer,
                      const url::Origin* outermost_origin) override;
  void DevToolsAgentAttached() override;
  void DevToolsAgentDetached() override;
  void ExposeInterfacesToBrowser(mojo::BinderMap* binders) override;

 private:
  std::unique_ptr<CefRenderManager> render_manager_;

  scoped_refptr<base::SingleThreadTaskRunner> render_task_runner_;
};

#endif  // CEF_LIBCEF_RENDERER_CHROME_CHROME_CONTENT_RENDERER_CLIENT_CEF_
