/// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/renderer/render_process_observer.h"
#include "libcef/common/cef_messages.h"
#include "libcef/common/cef_switches.h"
#include "libcef/common/command_line_impl.h"
#include "libcef/common/content_client.h"
#include "libcef/renderer/content_renderer_client.h"
#include "libcef/renderer/v8_impl.h"

#include "base/bind.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "googleurl/src/gurl.h"
#include "googleurl/src/url_util.h"
#include "media/base/media.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRuntimeFeatures.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityPolicy.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURL.h"

CefRenderProcessObserver::CefRenderProcessObserver() {
  // Note that under Linux, the media library will normally already have
  // been initialized by the Zygote before this instance became a Renderer.
  FilePath media_path;
  PathService::Get(base::DIR_MODULE, &media_path);
  if (!media_path.empty())
    media::InitializeMediaLibrary(media_path);
}

CefRenderProcessObserver::~CefRenderProcessObserver() {
}

bool CefRenderProcessObserver::OnControlMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(CefRenderProcessObserver, message)
    IPC_MESSAGE_HANDLER(CefProcessMsg_ModifyCrossOriginWhitelistEntry,
                        OnModifyCrossOriginWhitelistEntry)
    IPC_MESSAGE_HANDLER(CefProcessMsg_ClearCrossOriginWhitelist,
                        OnClearCrossOriginWhitelist)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void CefRenderProcessObserver::WebKitInitialized() {
  WebKit::WebRuntimeFeatures::enableMediaPlayer(
      media::IsMediaLibraryInitialized());

  // TODO(cef): Enable these once the implementation supports it.
  WebKit::WebRuntimeFeatures::enableNotifications(false);

  // Register any custom schemes with WebKit.
  CefContentRendererClient::Get()->RegisterCustomSchemes();

  if (pending_cross_origin_whitelist_entries_.size() > 0) {
    // Add pending cross-origin white list entries.
    for (size_t i = 0;
         i < pending_cross_origin_whitelist_entries_.size(); ++i) {
      const Cef_CrossOriginWhiteListEntry_Params& entry =
          pending_cross_origin_whitelist_entries_[i];
      GURL gurl = GURL(entry.source_origin);
      WebKit::WebSecurityPolicy::addOriginAccessWhitelistEntry(
          gurl,
          WebKit::WebString::fromUTF8(entry.target_protocol),
          WebKit::WebString::fromUTF8(entry.target_domain),
          entry.allow_target_subdomains);
    }
    pending_cross_origin_whitelist_entries_.clear();
  }

  // The number of stack trace frames to capture for uncaught exceptions.
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kUncaughtExceptionStackSize)) {
    int uncaught_exception_stack_size = 0;
    base::StringToInt(
        command_line.GetSwitchValueASCII(switches::kUncaughtExceptionStackSize),
        &uncaught_exception_stack_size);

    if (uncaught_exception_stack_size > 0) {
      CefContentRendererClient::Get()->SetUncaughtExceptionStackSize(
          uncaught_exception_stack_size);
      v8::V8::AddMessageListener(&CefV8MessageHandler);
      v8::V8::SetCaptureStackTraceForUncaughtExceptions(true,
          uncaught_exception_stack_size, v8::StackTrace::kDetailed);
    }
  }

  // Notify the render process handler.
  CefRefPtr<CefApp> application = CefContentClient::Get()->application();
  if (application.get()) {
    CefRefPtr<CefRenderProcessHandler> handler =
        application->GetRenderProcessHandler();
    if (handler.get())
      handler->OnWebKitInitialized();
  }
}

void CefRenderProcessObserver::OnModifyCrossOriginWhitelistEntry(
    bool add,
    const Cef_CrossOriginWhiteListEntry_Params& params) {
  GURL gurl = GURL(params.source_origin);
  if (add) {
    WebKit::WebSecurityPolicy::addOriginAccessWhitelistEntry(
        gurl,
        WebKit::WebString::fromUTF8(params.target_protocol),
        WebKit::WebString::fromUTF8(params.target_domain),
        params.allow_target_subdomains);
  } else {
    WebKit::WebSecurityPolicy::removeOriginAccessWhitelistEntry(
        gurl,
        WebKit::WebString::fromUTF8(params.target_protocol),
        WebKit::WebString::fromUTF8(params.target_domain),
        params.allow_target_subdomains);
  }
}

void CefRenderProcessObserver::OnClearCrossOriginWhitelist() {
  WebKit::WebSecurityPolicy::resetOriginAccessWhitelists();
}
