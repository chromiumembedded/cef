// Copyright 2014 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "build/build_config.h"

// Enable deprecation warnings on Windows. See http://crbug.com/585142.
#if BUILDFLAG(IS_WIN)
#if defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wdeprecated-declarations"
#else
#pragma warning(push)
#pragma warning(default : 4996)
#endif
#endif

#include "libcef/renderer/render_frame_observer.h"

#include "libcef/common/app_manager.h"
#include "libcef/renderer/blink_glue.h"
#include "libcef/renderer/browser_impl.h"
#include "libcef/renderer/dom_document_impl.h"
#include "libcef/renderer/v8_impl.h"

#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_node.h"

CefRenderFrameObserver::CefRenderFrameObserver(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {}

CefRenderFrameObserver::~CefRenderFrameObserver() = default;

void CefRenderFrameObserver::DidCommitProvisionalLoad(
    ui::PageTransition transition) {
  if (!frame_) {
    return;
  }

  frame_->OnDidCommitProvisionalLoad();

  if (frame_->GetParent() == nullptr) {
    blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
    CefRefPtr<CefBrowserImpl> browserPtr =
        CefBrowserImpl::GetBrowserForMainFrame(frame->Top());
    browserPtr->OnLoadingStateChange(true);
  }
  OnLoadStart();
}

void CefRenderFrameObserver::WasShown() {
  if (frame_) {
    frame_->OnWasShown();
  }
}

void CefRenderFrameObserver::DidFailProvisionalLoad() {
  if (frame_) {
    OnLoadError();
  }
}

void CefRenderFrameObserver::DidFinishLoad() {
  if (frame_) {
    frame_->OnDidFinishLoad();
  }
}

void CefRenderFrameObserver::WillDetach(blink::DetachReason detach_reason) {
  if (frame_) {
    frame_->OnDetached();
    frame_ = nullptr;
  }
}

void CefRenderFrameObserver::FocusedElementChanged(
    const blink::WebElement& element) {
  if (!frame_) {
    return;
  }

  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  CefRefPtr<CefBrowserImpl> browserPtr =
      CefBrowserImpl::GetBrowserForMainFrame(frame->Top());
  if (!browserPtr) {
    return;
  }

  CefRefPtr<CefRenderProcessHandler> handler;
  CefRefPtr<CefApp> application = CefAppManager::Get()->GetApplication();
  if (application) {
    handler = application->GetRenderProcessHandler();
  }
  if (!handler) {
    return;
  }

  CefRefPtr<CefFrameImpl> framePtr = browserPtr->GetWebFrameImpl(frame);

  if (element.IsNull()) {
    handler->OnFocusedNodeChanged(browserPtr.get(), framePtr.get(), nullptr);
    return;
  }

  if (element.GetDocument().IsNull()) {
    return;
  }

  CefRefPtr<CefDOMDocumentImpl> documentImpl =
      new CefDOMDocumentImpl(browserPtr.get(), frame);
  handler->OnFocusedNodeChanged(browserPtr.get(), framePtr.get(),
                                documentImpl->GetOrCreateNode(element));
  documentImpl->Detach();
}

void CefRenderFrameObserver::DraggableRegionsChanged() {
  if (frame_) {
    frame_->OnDraggableRegionsChanged();
  }
}

void CefRenderFrameObserver::DidCreateScriptContext(
    v8::Handle<v8::Context> context,
    int world_id) {
  if (!frame_) {
    return;
  }

  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  CefRefPtr<CefBrowserImpl> browserPtr =
      CefBrowserImpl::GetBrowserForMainFrame(frame->Top());
  if (!browserPtr) {
    return;
  }

  CefRefPtr<CefRenderProcessHandler> handler;
  CefRefPtr<CefApp> application = CefAppManager::Get()->GetApplication();
  if (application) {
    handler = application->GetRenderProcessHandler();
  }

  CefRefPtr<CefFrameImpl> framePtr = browserPtr->GetWebFrameImpl(frame);

  if (handler) {
    v8::Isolate* isolate = context->GetIsolate();
    v8::HandleScope handle_scope(isolate);
    v8::Context::Scope scope(context);
    v8::MicrotasksScope microtasks_scope(isolate,
                                         v8::MicrotasksScope::kRunMicrotasks);

    CefRefPtr<CefV8Context> contextPtr(new CefV8ContextImpl(isolate, context));

    handler->OnContextCreated(browserPtr.get(), framePtr.get(), contextPtr);
  }

  // Do this last, in case the client callback modified the window object.
  framePtr->OnContextCreated(context);
}

void CefRenderFrameObserver::WillReleaseScriptContext(
    v8::Handle<v8::Context> context,
    int world_id) {
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  CefRefPtr<CefBrowserImpl> browserPtr =
      CefBrowserImpl::GetBrowserForMainFrame(frame->Top());
  if (!browserPtr) {
    return;
  }

  CefRefPtr<CefRenderProcessHandler> handler;
  CefRefPtr<CefApp> application = CefAppManager::Get()->GetApplication();
  if (application) {
    handler = application->GetRenderProcessHandler();
  }

  CefRefPtr<CefFrameImpl> framePtr = browserPtr->GetWebFrameImpl(frame);

  if (handler) {
    v8::Isolate* isolate = context->GetIsolate();
    v8::HandleScope handle_scope(isolate);

    // The released context should not be used for script execution.
    // Depending on how the context is released this may or may not already
    // be set.
    blink_glue::CefScriptForbiddenScope forbidScript;

    CefRefPtr<CefV8Context> contextPtr(new CefV8ContextImpl(isolate, context));

    handler->OnContextReleased(browserPtr.get(), framePtr.get(), contextPtr);
  }

  framePtr->OnContextReleased();

  CefV8ReleaseContext(context);
}

void CefRenderFrameObserver::OnDestruct() {
  delete this;
}

void CefRenderFrameObserver::OnInterfaceRequestForFrame(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle* interface_pipe) {
  registry_.TryBindInterface(interface_name, interface_pipe);
}

bool CefRenderFrameObserver::OnAssociatedInterfaceRequestForFrame(
    const std::string& interface_name,
    mojo::ScopedInterfaceEndpointHandle* handle) {
  return associated_interfaces_.TryBindInterface(interface_name, handle);
}

void CefRenderFrameObserver::AttachFrame(CefFrameImpl* frame) {
  DCHECK(frame);
  DCHECK(!frame_);
  frame_ = frame;
  frame_->OnAttached();
}

void CefRenderFrameObserver::OnLoadStart() {
  CefRefPtr<CefApp> app = CefAppManager::Get()->GetApplication();
  if (app.get()) {
    CefRefPtr<CefRenderProcessHandler> handler = app->GetRenderProcessHandler();
    if (handler.get()) {
      CefRefPtr<CefLoadHandler> load_handler = handler->GetLoadHandler();
      if (load_handler.get()) {
        blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
        CefRefPtr<CefBrowserImpl> browserPtr =
            CefBrowserImpl::GetBrowserForMainFrame(frame->Top());
        load_handler->OnLoadStart(browserPtr.get(), frame_, TT_EXPLICIT);
      }
    }
  }
}

void CefRenderFrameObserver::OnLoadError() {
  CefRefPtr<CefApp> app = CefAppManager::Get()->GetApplication();
  if (app.get()) {
    CefRefPtr<CefRenderProcessHandler> handler = app->GetRenderProcessHandler();
    if (handler.get()) {
      CefRefPtr<CefLoadHandler> load_handler = handler->GetLoadHandler();
      // Error codes were removed from DidFailProvisionalLoad() so we now always
      // pass the same value.
      if (load_handler.get()) {
        const cef_errorcode_t errorCode =
            static_cast<cef_errorcode_t>(net::ERR_ABORTED);
        const std::string& errorText = net::ErrorToString(errorCode);
        blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
        CefRefPtr<CefBrowserImpl> browserPtr =
            CefBrowserImpl::GetBrowserForMainFrame(frame->Top());
        load_handler->OnLoadError(browserPtr.get(), frame_, errorCode,
                                  errorText, frame_->GetURL());
      }
    }
  }
}

// Enable deprecation warnings on Windows. See http://crbug.com/585142.
#if BUILDFLAG(IS_WIN)
#if defined(__clang__)
#pragma GCC diagnostic pop
#else
#pragma warning(pop)
#endif
#endif
