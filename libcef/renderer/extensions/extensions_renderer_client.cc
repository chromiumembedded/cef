// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/renderer/extensions/extensions_renderer_client.h"

#include "libcef/common/cef_messages.h"
#include "libcef/renderer/extensions/extensions_dispatcher_delegate.h"
#include "libcef/renderer/render_thread_observer.h"

#include "base/command_line.h"
#include "chrome/common/extensions/extension_process_policy.h"
#include "chrome/common/url_constants.h"
#include "chrome/renderer/extensions/resource_request_policy.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "extensions/common/constants.h"
#include "extensions/common/switches.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "extensions/renderer/extension_helper.h"
#include "extensions/renderer/guest_view/extensions_guest_view_container.h"
#include "extensions/renderer/guest_view/extensions_guest_view_container_dispatcher.h"
#include "extensions/renderer/guest_view/mime_handler_view/mime_handler_view_container.h"
#include "extensions/renderer/renderer_extension_registry.h"
#include "extensions/renderer/script_context.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebPluginParams.h"

namespace extensions {

namespace {

bool IsStandaloneExtensionProcess() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      extensions::switches::kExtensionProcess);
}

void IsGuestViewApiAvailableToScriptContext(
    bool* api_is_available,
    extensions::ScriptContext* context) {
  if (context->GetAvailability("guestViewInternal").is_available()) {
    *api_is_available = true;
  }
}

// Returns true if the frame is navigating to an URL either into or out of an
// extension app's extent.
bool CrossesExtensionExtents(blink::WebLocalFrame* frame,
                             const GURL& new_url,
                             bool is_extension_url,
                             bool is_initial_navigation) {
  DCHECK(!frame->Parent());
  GURL old_url(frame->GetDocument().Url());

  extensions::RendererExtensionRegistry* extension_registry =
      extensions::RendererExtensionRegistry::Get();

  // If old_url is still empty and this is an initial navigation, then this is
  // a window.open operation.  We should look at the opener URL.  Note that the
  // opener is a local frame in this case.
  if (is_initial_navigation && old_url.is_empty() && frame->Opener() &&
      frame->Opener()->IsWebLocalFrame()) {
    blink::WebLocalFrame* opener_frame = frame->Opener()->ToWebLocalFrame();

    // We usually want to compare against the URL that determines the type of
    // process.  In default Chrome, that's the URL of the opener's top frame and
    // not the opener frame itself.  In --site-per-process, we can use the
    // opener frame itself.
    // TODO(nick): Either wire this up to SiteIsolationPolicy, or to state on
    // |opener_frame|/its ancestors.
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            ::switches::kSitePerProcess) ||
        extensions::IsIsolateExtensionsEnabled())
      old_url = opener_frame->GetDocument().Url();
    else
      old_url = opener_frame->Top()->GetDocument().Url();

    // If we're about to open a normal web page from a same-origin opener stuck
    // in an extension process (other than the Chrome Web Store), we want to
    // keep it in process to allow the opener to script it.
    blink::WebDocument opener_document = opener_frame->GetDocument();
    blink::WebSecurityOrigin opener_origin =
        opener_document.GetSecurityOrigin();
    bool opener_is_extension_url = !opener_origin.IsUnique() &&
                                   extension_registry->GetExtensionOrAppByURL(
                                       opener_document.Url()) != nullptr;
    const extensions::Extension* opener_top_extension =
        extension_registry->GetExtensionOrAppByURL(old_url);
    bool opener_is_web_store =
        opener_top_extension &&
        opener_top_extension->id() == extensions::kWebStoreAppId;
    if (!is_extension_url && !opener_is_extension_url && !opener_is_web_store &&
        IsStandaloneExtensionProcess() &&
        opener_origin.CanRequest(blink::WebURL(new_url)))
      return false;
  }

  // Only consider keeping non-app URLs in an app process if this window
  // has an opener (in which case it might be an OAuth popup that tries to
  // script an iframe within the app).
  bool should_consider_workaround = !!frame->Opener();

  return extensions::CrossesExtensionProcessBoundary(
      *extension_registry->GetMainThreadExtensionSet(), old_url, new_url,
      should_consider_workaround);
}

}  // namespace

CefExtensionsRendererClient::CefExtensionsRendererClient() {
}

CefExtensionsRendererClient::~CefExtensionsRendererClient() {
}

bool CefExtensionsRendererClient::IsIncognitoProcess() const {
  return CefRenderThreadObserver::is_incognito_process();
}

int CefExtensionsRendererClient::GetLowestIsolatedWorldId() const {
  // CEF doesn't need to reserve world IDs for anything other than extensions,
  // so we always return 1. Note that 0 is reserved for the global world.
  return 1;
}

void CefExtensionsRendererClient::RenderThreadStarted() {
  content::RenderThread* thread = content::RenderThread::Get();

  extension_dispatcher_delegate_.reset(
      new extensions::CefExtensionsDispatcherDelegate());
  extension_dispatcher_.reset(
      new extensions::Dispatcher(extension_dispatcher_delegate_.get()));
  resource_request_policy_.reset(
    new extensions::ResourceRequestPolicy(extension_dispatcher_.get()));
  guest_view_container_dispatcher_.reset(
      new extensions::ExtensionsGuestViewContainerDispatcher());

  thread->AddObserver(extension_dispatcher_.get());
  thread->AddObserver(guest_view_container_dispatcher_.get());
}

void CefExtensionsRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame) {
  new extensions::ExtensionFrameHelper(render_frame,
                                       extension_dispatcher_.get());
  extension_dispatcher_->OnRenderFrameCreated(render_frame);
}

void CefExtensionsRendererClient::RenderViewCreated(
    content::RenderView* render_view) {
  new extensions::ExtensionHelper(render_view, extension_dispatcher_.get());
}

bool CefExtensionsRendererClient::OverrideCreatePlugin(
    content::RenderFrame* render_frame,
    const blink::WebPluginParams& params) {
  if (params.mime_type.Utf8() != content::kBrowserPluginMimeType)
    return true;

  bool guest_view_api_available = false;
  extension_dispatcher_->script_context_set().ForEach(
      render_frame, base::Bind(&IsGuestViewApiAvailableToScriptContext,
                               &guest_view_api_available));
  return !guest_view_api_available;
}

bool CefExtensionsRendererClient::WillSendRequest(
    blink::WebLocalFrame* frame,
    ui::PageTransition transition_type,
    const blink::WebURL& url,
    GURL* new_url) {
  // Check whether the request should be allowed. If not allowed, we reset the
  // URL to something invalid to prevent the request and cause an error.
  if (url.ProtocolIs(extensions::kExtensionScheme) &&
      !resource_request_policy_->CanRequestResource(GURL(url), frame,
                                                    transition_type)) {
    *new_url = GURL(chrome::kExtensionInvalidRequestURL);
    return true;
  }

  return false;
}

void CefExtensionsRendererClient::RunScriptsAtDocumentStart(
    content::RenderFrame* render_frame) {
  extension_dispatcher_->RunScriptsAtDocumentStart(render_frame);
}

void CefExtensionsRendererClient::RunScriptsAtDocumentEnd(
    content::RenderFrame* render_frame) {
  extension_dispatcher_->RunScriptsAtDocumentEnd(render_frame);
}

void CefExtensionsRendererClient::RunScriptsAtDocumentIdle(
    content::RenderFrame* render_frame) {
  extension_dispatcher_->RunScriptsAtDocumentIdle(render_frame);
}

// static
bool CefExtensionsRendererClient::ShouldFork(blink::WebLocalFrame* frame,
                                             const GURL& url,
                                             bool is_initial_navigation,
                                             bool is_server_redirect,
                                             bool* send_referrer) {
  const extensions::RendererExtensionRegistry* extension_registry =
      extensions::RendererExtensionRegistry::Get();

  // Determine if the new URL is an extension (excluding bookmark apps).
  const extensions::Extension* new_url_extension =
      extensions::GetNonBookmarkAppExtension(
          *extension_registry->GetMainThreadExtensionSet(), url);
  bool is_extension_url = !!new_url_extension;

  // If the navigation would cross an app extent boundary, we also need
  // to defer to the browser to ensure process isolation.  This is not
  // necessary for server redirects, which will be transferred to a new
  // process by the browser process when they are ready to commit.  It is
  // necessary for client redirects, which won't be transferred in the same
  // way.
  if (!is_server_redirect &&
      CrossesExtensionExtents(frame, url, is_extension_url,
                              is_initial_navigation)) {
    // Include the referrer in this case since we're going from a hosted web
    // page. (the packaged case is handled previously by the extension
    // navigation test)
    *send_referrer = true;
    return true;
  }

  // If this is a reload, check whether it has the wrong process type.  We
  // should send it to the browser if it's an extension URL (e.g., hosted app)
  // in a normal process, or if it's a process for an extension that has been
  // uninstalled.  Without --site-per-process mode, we never fork processes
  // for subframes, so this check only makes sense for top-level frames.
  // TODO(alexmos,nasko): Figure out how this check should work when reloading
  // subframes in --site-per-process mode.
  if (!frame->Parent() &&  GURL(frame->GetDocument().Url()) == url) {
    if (is_extension_url != IsStandaloneExtensionProcess())
      return true;
  }

  return false;
}

// static
content::BrowserPluginDelegate*
CefExtensionsRendererClient::CreateBrowserPluginDelegate(
    content::RenderFrame* render_frame,
    const std::string& mime_type,
    const GURL& original_url) {
  if (mime_type == content::kBrowserPluginMimeType)
    return new extensions::ExtensionsGuestViewContainer(render_frame);
  return new extensions::MimeHandlerViewContainer(render_frame, mime_type,
                                                  original_url);
}

}  // namespace extensions
