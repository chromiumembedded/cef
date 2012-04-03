// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_RENDERER_CONTENT_RENDERER_CLIENT_H_
#define CEF_LIBCEF_RENDERER_CONTENT_RENDERER_CLIENT_H_
#pragma once

#include <map>
#include <set>
#include <string>

#include "libcef/renderer/browser_impl.h"

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop_proxy.h"
#include "content/public/renderer/content_renderer_client.h"

class CefRenderProcessObserver;


class CefContentRendererClient : public content::ContentRendererClient {
 public:
  CefContentRendererClient();
  virtual ~CefContentRendererClient();

  // Returns the singleton CefContentRendererClient instance.
  static CefContentRendererClient* Get();

  // Returns the browser associated with the specified RenderView.
  CefRefPtr<CefBrowserImpl> GetBrowserForView(content::RenderView* view);

  // Returns the browser associated with the specified main WebFrame.
  CefRefPtr<CefBrowserImpl> GetBrowserForMainFrame(WebKit::WebFrame* frame);

  // Called from CefBrowserImpl::OnDestruct().
  void OnBrowserDestroyed(CefBrowserImpl* browser);

  // Render thread message loop proxy.
  base::MessageLoopProxy* render_loop() const { return render_loop_.get(); }

 private:
  // ContentRendererClient implementation.
  virtual void RenderThreadStarted() OVERRIDE;
  virtual void RenderViewCreated(content::RenderView* render_view) OVERRIDE;
  virtual void SetNumberOfViews(int number_of_views) OVERRIDE;
  virtual SkBitmap* GetSadPluginBitmap() OVERRIDE;
  virtual std::string GetDefaultEncoding() OVERRIDE;
  virtual bool OverrideCreatePlugin(
      content::RenderView* render_view,
      WebKit::WebFrame* frame,
      const WebKit::WebPluginParams& params,
      WebKit::WebPlugin** plugin) OVERRIDE;
  virtual bool HasErrorPage(int http_status_code,
                            std::string* error_domain) OVERRIDE;
  virtual void GetNavigationErrorStrings(
      const WebKit::WebURLRequest& failed_request,
      const WebKit::WebURLError& error,
      std::string* error_html,
      string16* error_description) OVERRIDE;
  virtual webkit_media::WebMediaPlayerImpl* OverrideCreateWebMediaPlayer(
      content::RenderView* render_view,
      WebKit::WebFrame* frame,
      WebKit::WebMediaPlayerClient* client,
      base::WeakPtr<webkit_media::WebMediaPlayerDelegate> delegate,
      media::FilterCollection* collection,
      WebKit::WebAudioSourceProvider* audio_source_provider,
      media::MessageLoopFactory* message_loop_factory,
      webkit_media::MediaStreamClient* media_stream_client,
      media::MediaLog* media_log) OVERRIDE;
  virtual bool RunIdleHandlerWhenWidgetsHidden() OVERRIDE;
  virtual bool AllowPopup(const GURL& creator) OVERRIDE;
  virtual bool ShouldFork(WebKit::WebFrame* frame,
                          const GURL& url,
                         bool is_initial_navigation,
                          bool* send_referrer) OVERRIDE;
  virtual bool WillSendRequest(WebKit::WebFrame* frame,
                               const GURL& url,
                               GURL* new_url) OVERRIDE;
  virtual bool ShouldPumpEventsDuringCookieMessage() OVERRIDE;
  virtual void DidCreateScriptContext(WebKit::WebFrame* frame,
                                      v8::Handle<v8::Context> context,
                                      int extension_group,
                                      int world_id) OVERRIDE;
  virtual void WillReleaseScriptContext(WebKit::WebFrame* frame,
                                        v8::Handle<v8::Context> context,
                                        int world_id) OVERRIDE;
  virtual unsigned long long VisitedLinkHash(const char* canonical_url,
                                             size_t length) OVERRIDE;
  virtual bool IsLinkVisited(unsigned long long link_hash) OVERRIDE;
  virtual void PrefetchHostName(const char* hostname, size_t length) OVERRIDE;
  virtual bool ShouldOverridePageVisibilityState(
      const content::RenderView* render_view,
      WebKit::WebPageVisibilityState* override_state) const OVERRIDE;
  virtual bool HandleGetCookieRequest(content::RenderView* sender,
                                      const GURL& url,
                                      const GURL& first_party_for_cookies,
                                      std::string* cookies) OVERRIDE;
  virtual bool HandleSetCookieRequest(content::RenderView* sender,
                                      const GURL& url,
                                      const GURL& first_party_for_cookies,
                                      const std::string& value) OVERRIDE;
  virtual void RegisterPPAPIInterfaceFactories(
      webkit::ppapi::PpapiInterfaceFactoryManager* factory_manager) OVERRIDE;

  scoped_refptr<base::MessageLoopProxy> render_loop_;
  scoped_ptr<CefRenderProcessObserver> observer_;

  // Map of RenderView pointers to CefBrowserImpl references.
  typedef std::map<content::RenderView*, CefRefPtr<CefBrowserImpl> > BrowserMap;
  BrowserMap browsers_;
};

#endif  // CEF_LIBCEF_RENDERER_CONTENT_RENDERER_CLIENT_H_
