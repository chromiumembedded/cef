// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/renderer/content_renderer_client.h"

#include "libcef/common/cef_messages.h"
#include "libcef/common/content_client.h"
#include "libcef/renderer/browser_impl.h"
#include "libcef/renderer/render_message_filter.h"
#include "libcef/renderer/render_process_observer.h"
#include "libcef/renderer/thread_util.h"
#include "libcef/renderer/v8_impl.h"

#include "content/common/child_thread.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "ipc/ipc_sync_channel.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "v8/include/v8.h"

CefContentRendererClient::CefContentRendererClient() {
}

CefContentRendererClient::~CefContentRendererClient() {
}

// static
CefContentRendererClient* CefContentRendererClient::Get() {
  return static_cast<CefContentRendererClient*>(
      content::GetContentClient()->renderer());
}

CefRefPtr<CefBrowserImpl> CefContentRendererClient::GetBrowserForView(
    content::RenderView* view) {
  CEF_REQUIRE_RT_RETURN(NULL);

  BrowserMap::const_iterator it = browsers_.find(view);
  if (it != browsers_.end())
    return it->second;
  return NULL;
}

CefRefPtr<CefBrowserImpl> CefContentRendererClient::GetBrowserForMainFrame(
    WebKit::WebFrame* frame) {
  CEF_REQUIRE_RT_RETURN(NULL);

  BrowserMap::const_iterator it = browsers_.begin();
  for (; it != browsers_.end(); ++it) {
    content::RenderView* render_view = it->second->render_view();
    if (render_view && render_view->GetWebView() &&
        render_view->GetWebView()->mainFrame() == frame) {
      return it->second;
    }
  }

  return NULL;
}

void CefContentRendererClient::OnBrowserDestroyed(CefBrowserImpl* browser) {
  BrowserMap::iterator it = browsers_.begin();
  for (; it != browsers_.end(); ++it) {
    if (it->second.get() == browser) {
      browsers_.erase(it);
      return;
    }
  }

  // No browser was found in the map.
  NOTREACHED();
}

void CefContentRendererClient::RenderThreadStarted() {
  render_loop_ = base::MessageLoopProxy::current();
  observer_.reset(new CefRenderProcessObserver());

  content::RenderThread* thread = content::RenderThread::Get();
  thread->AddObserver(observer_.get());
  thread->GetChannel()->AddFilter(new CefRenderMessageFilter);

  thread->Send(new CefProcessHostMsg_RenderThreadStarted);

  // Notify the render process handler.
  CefRefPtr<CefApp> application = CefContentClient::Get()->application();
  if (application.get()) {
    CefRefPtr<CefRenderProcessHandler> handler =
        application->GetRenderProcessHandler();
    if (handler.get())
      handler->OnRenderThreadCreated();
  }
}

void CefContentRendererClient::RenderViewCreated(
    content::RenderView* render_view) {
  CefRefPtr<CefBrowserImpl> browser = new CefBrowserImpl(render_view);
  browsers_.insert(std::make_pair(render_view, browser));
}

void CefContentRendererClient::SetNumberOfViews(int number_of_views) {
}

SkBitmap* CefContentRendererClient::GetSadPluginBitmap() {
  return NULL;
}

std::string CefContentRendererClient::GetDefaultEncoding() {
  return std::string();
}

bool CefContentRendererClient::OverrideCreatePlugin(
    content::RenderView* render_view,
    WebKit::WebFrame* frame,
    const WebKit::WebPluginParams& params,
    WebKit::WebPlugin** plugin) {
  return false;
}

bool CefContentRendererClient::HasErrorPage(int http_status_code,
                                            std::string* error_domain) {
  return false;
}

void CefContentRendererClient::GetNavigationErrorStrings(
    const WebKit::WebURLRequest& failed_request,
    const WebKit::WebURLError& error,
    std::string* error_html,
    string16* error_description) {
}

webkit_media::WebMediaPlayerImpl*
CefContentRendererClient::OverrideCreateWebMediaPlayer(
    content::RenderView* render_view,
    WebKit::WebFrame* frame,
    WebKit::WebMediaPlayerClient* client,
    base::WeakPtr<webkit_media::WebMediaPlayerDelegate> delegate,
    media::FilterCollection* collection,
    WebKit::WebAudioSourceProvider* audio_source_provider,
    media::MessageLoopFactory* message_loop_factory,
    webkit_media::MediaStreamClient* media_stream_client,
    media::MediaLog* media_log) {
  return NULL;
}

bool CefContentRendererClient::RunIdleHandlerWhenWidgetsHidden() {
  return true;
}

bool CefContentRendererClient::AllowPopup(const GURL& creator) {
  return false;
}

bool CefContentRendererClient::ShouldFork(WebKit::WebFrame* frame,
                                          const GURL& url,
                                          bool is_initial_navigation,
                                          bool* send_referrer) {
  return false;
}

bool CefContentRendererClient::WillSendRequest(WebKit::WebFrame* frame,
                                               const GURL& url,
                                               GURL* new_url) {
  return false;
}

bool CefContentRendererClient::ShouldPumpEventsDuringCookieMessage() {
  return false;
}

void CefContentRendererClient::DidCreateScriptContext(
    WebKit::WebFrame* frame, v8::Handle<v8::Context> context,
    int extension_group, int world_id) {
  // Notify the render process handler.
  CefRefPtr<CefApp> application = CefContentClient::Get()->application();
  if (!application.get())
    return;

  CefRefPtr<CefRenderProcessHandler> handler =
      application->GetRenderProcessHandler();
  if (!handler.get())
    return;

  CefRefPtr<CefBrowserImpl> browserPtr =
      CefBrowserImpl::GetBrowserForMainFrame(frame->top());
  DCHECK(browserPtr.get());
  if (!browserPtr.get())
    return;

  CefRefPtr<CefFrameImpl> framePtr = browserPtr->GetWebFrameImpl(frame);

  v8::HandleScope handle_scope;
  v8::Context::Scope scope(context);

  CefRefPtr<CefV8Context> contextPtr(new CefV8ContextImpl(context));

  handler->OnContextCreated(browserPtr.get(), framePtr.get(), contextPtr);
}

void CefContentRendererClient::WillReleaseScriptContext(
    WebKit::WebFrame* frame, v8::Handle<v8::Context> context, int world_id) {
  // Notify the render process handler.
  CefRefPtr<CefApp> application = CefContentClient::Get()->application();
  if (!application.get())
    return;

  CefRefPtr<CefRenderProcessHandler> handler =
      application->GetRenderProcessHandler();
  if (!handler.get())
    return;

  CefRefPtr<CefBrowserImpl> browserPtr =
      CefBrowserImpl::GetBrowserForMainFrame(frame->top());
  DCHECK(browserPtr.get());
  if (!browserPtr.get())
    return;

  CefRefPtr<CefFrameImpl> framePtr = browserPtr->GetWebFrameImpl(frame);

  v8::HandleScope handle_scope;
  v8::Context::Scope scope(context);

  CefRefPtr<CefV8Context> contextPtr(new CefV8ContextImpl(context));

  handler->OnContextReleased(browserPtr.get(), framePtr.get(), contextPtr);
}

unsigned long long CefContentRendererClient::VisitedLinkHash(
    const char* canonical_url, size_t length) {
  return 0LL;
}

bool CefContentRendererClient::IsLinkVisited(unsigned long long link_hash) {
  return false;
}

void CefContentRendererClient::PrefetchHostName(
    const char* hostname, size_t length) {
}

bool CefContentRendererClient::ShouldOverridePageVisibilityState(
    const content::RenderView* render_view,
    WebKit::WebPageVisibilityState* override_state) const {
  return false;
}

bool CefContentRendererClient::HandleGetCookieRequest(
    content::RenderView* sender,
    const GURL& url,
    const GURL& first_party_for_cookies,
    std::string* cookies) {
  return false;
}

bool CefContentRendererClient::HandleSetCookieRequest(
    content::RenderView* sender,
    const GURL& url,
    const GURL& first_party_for_cookies,
    const std::string& value) {
  return false;
}

void CefContentRendererClient::RegisterPPAPIInterfaceFactories(
    webkit::ppapi::PpapiInterfaceFactoryManager* factory_manager) {
}
