// Copyright 2014 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.


#include "base/compiler_specific.h"

MSVC_PUSH_WARNING_LEVEL(0);
#include "platform/ScriptForbiddenScope.h"
MSVC_POP_WARNING();

// Enable deprecation warnings for MSVC. See http://crbug.com/585142.
#if defined(OS_WIN)
#pragma warning(push)
#pragma warning(default:4996)
#endif

#include "libcef/renderer/render_frame_observer.h"

#include "libcef/common/content_client.h"
#include "libcef/renderer/content_renderer_client.h"
#include "libcef/renderer/v8_impl.h"

#include "content/public/renderer/render_frame.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebKit.h"

CefRenderFrameObserver::CefRenderFrameObserver(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {
}

CefRenderFrameObserver::~CefRenderFrameObserver() {
}

void CefRenderFrameObserver::DidCreateScriptContext(
    v8::Handle<v8::Context> context,
    int world_id) {
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();

  CefRefPtr<CefBrowserImpl> browserPtr =
      CefBrowserImpl::GetBrowserForMainFrame(frame->Top());
  if (!browserPtr.get())
    return;

  CefRefPtr<CefRenderProcessHandler> handler;
  CefRefPtr<CefApp> application = CefContentClient::Get()->application();
  if (application.get())
    handler = application->GetRenderProcessHandler();
  if (!handler.get())
    return;

  CefRefPtr<CefFrameImpl> framePtr = browserPtr->GetWebFrameImpl(frame);

  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope scope(context);
  v8::MicrotasksScope microtasks_scope(isolate,
                                       v8::MicrotasksScope::kRunMicrotasks);

  CefRefPtr<CefV8Context> contextPtr(new CefV8ContextImpl(isolate, context));

  handler->OnContextCreated(browserPtr.get(), framePtr.get(), contextPtr);
}

void CefRenderFrameObserver::WillReleaseScriptContext(
    v8::Handle<v8::Context> context,
    int world_id) {
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();

  CefRefPtr<CefBrowserImpl> browserPtr =
      CefBrowserImpl::GetBrowserForMainFrame(frame->Top());
  if (browserPtr.get()) {
    CefRefPtr<CefApp> application = CefContentClient::Get()->application();
    if (application.get()) {
      CefRefPtr<CefRenderProcessHandler> handler =
          application->GetRenderProcessHandler();
      if (handler.get()) {
        CefRefPtr<CefFrameImpl> framePtr = browserPtr->GetWebFrameImpl(frame);

        v8::Isolate* isolate = blink::MainThreadIsolate();
        v8::HandleScope handle_scope(isolate);

        // The released context should not be used for script execution.
        // Depending on how the context is released this may or may not already
        // be set.
        blink::ScriptForbiddenScope forbidScript;

        CefRefPtr<CefV8Context> contextPtr(
            new CefV8ContextImpl(isolate, context));

        handler->OnContextReleased(browserPtr.get(), framePtr.get(),
                                   contextPtr);
      }
    }
  }

  CefV8ReleaseContext(context);
}

void CefRenderFrameObserver::OnDestruct() {
  delete this;
}


// Enable deprecation warnings for MSVC. See http://crbug.com/585142.
#if defined(OS_WIN)
#pragma warning(pop)
#endif
