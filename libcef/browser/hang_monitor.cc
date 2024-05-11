// Copyright 2024 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "cef/libcef/browser/hang_monitor.h"

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "cef/include/cef_client.h"
#include "cef/libcef/browser/browser_host_base.h"
#include "chrome/browser/hang_monitor/hang_crash_dump.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/common/result_codes.h"

namespace hang_monitor {

namespace {

// Based on HungRendererDialogView::ForceCrashHungRenderer.
void ForceCrashHungRenderer(content::RenderWidgetHost* render_widget_host) {
  content::RenderProcessHost* rph = render_widget_host->GetProcess();
  if (rph) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    // A generic |CrashDumpHungChildProcess()| is not implemented for Linux.
    // Instead we send an explicit IPC to crash on the renderer's IO thread.
    rph->ForceCrash();
#else
    // Try to generate a crash report for the hung process.
    CrashDumpHungChildProcess(rph->GetProcess().Handle());
    rph->Shutdown(content::RESULT_CODE_HUNG);
#endif
  }
}

class CefUnresponsiveProcessCallbackImpl
    : public CefUnresponsiveProcessCallback {
 public:
  CefUnresponsiveProcessCallbackImpl(
      content::RenderWidgetHost* render_widget_host,
      base::RepeatingClosure hang_monitor_restarter)
      : render_widget_host_(render_widget_host),
        hang_monitor_restarter_(hang_monitor_restarter) {}

  CefUnresponsiveProcessCallbackImpl(
      const CefUnresponsiveProcessCallbackImpl&) = delete;
  CefUnresponsiveProcessCallbackImpl& operator=(
      const CefUnresponsiveProcessCallbackImpl&) = delete;

  ~CefUnresponsiveProcessCallbackImpl() override {
    // Do nothing on destruction.
  }

  void Wait() override { ContinueNow(true); }

  void Terminate() override { ContinueNow(false); }

  void Detach() {
    render_widget_host_ = nullptr;
    hang_monitor_restarter_.Reset();
  }

  bool IsDetached() const { return !render_widget_host_; }

 private:
  void ContinueNow(bool wait) {
    if (CEF_CURRENTLY_ON_UIT()) {
      if (!IsDetached()) {
        RunNow(render_widget_host_, hang_monitor_restarter_, wait);
        Detach();
      }
    } else {
      CEF_POST_TASK(
          CEF_UIT,
          base::BindOnce(&CefUnresponsiveProcessCallbackImpl::ContinueNow, this,
                         wait));
    }
  }

  static void RunNow(content::RenderWidgetHost* render_widget_host,
                     base::RepeatingClosure hang_monitor_restarter,
                     bool wait) {
    CEF_REQUIRE_UIT();
    if (wait) {
      hang_monitor_restarter.Run();
    } else {
      ForceCrashHungRenderer(render_widget_host);
    }
  }

  raw_ptr<content::RenderWidgetHost> render_widget_host_;
  base::RepeatingClosure hang_monitor_restarter_;

  IMPLEMENT_REFCOUNTING(CefUnresponsiveProcessCallbackImpl);
};

bool ResetRendererCallback(CefBrowserHostBase* browser) {
  CEF_REQUIRE_UIT();
  if (auto callback = browser->unresponsive_process_callback()) {
    Detach(callback);
    browser->set_unresponsive_process_callback(nullptr);
    return true;
  }
  return false;
}

CefRefPtr<CefRequestHandler> GetRequestHandler(CefBrowserHostBase* browser) {
  if (auto client = browser->GetClient()) {
    return client->GetRequestHandler();
  }
  return nullptr;
}

}  // namespace

bool RendererUnresponsive(CefBrowserHostBase* browser,
                          content::RenderWidgetHost* render_widget_host,
                          base::RepeatingClosure hang_monitor_restarter) {
  // There should be no callback currently.
  DCHECK(!browser->unresponsive_process_callback());

  if (auto handler = GetRequestHandler(browser)) {
    CefRefPtr<CefUnresponsiveProcessCallbackImpl> callbackImpl(
        new CefUnresponsiveProcessCallbackImpl(render_widget_host,
                                               hang_monitor_restarter));
    if (!handler->OnRenderProcessUnresponsive(browser, callbackImpl.get())) {
      if (!callbackImpl->IsDetached()) {
        // Proceed with default handling.
        callbackImpl->Detach();
        return false;
      } else {
        LOG(ERROR) << "Should return true from OnRenderProcessUnresponsive "
                      "when executing the callback";
      }
    }

    // Proceed with client handling. The callback may already be executed, but
    // we still want to wait for RendererResponsive.
    browser->set_unresponsive_process_callback(callbackImpl.get());
    return true;
  }

  // Proceed with default handling.
  return false;
}

bool RendererResponsive(CefBrowserHostBase* browser,
                        content::RenderWidgetHost* render_widget_host) {
  // |handled| will be true if the client handled OnRenderProcessUnresponsive.
  bool handled = ResetRendererCallback(browser);

  // Always execute the client callback.
  if (auto handler = GetRequestHandler(browser)) {
    handler->OnRenderProcessResponsive(browser);
  }

  return handled;
}

void Detach(CefRefPtr<CefUnresponsiveProcessCallback> callback) {
  CEF_REQUIRE_UIT();
  auto* callback_impl =
      static_cast<CefUnresponsiveProcessCallbackImpl*>(callback.get());
  callback_impl->Detach();
}

}  // namespace hang_monitor
