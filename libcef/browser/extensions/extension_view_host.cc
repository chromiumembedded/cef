// Copyright 2017 the Chromium Embedded Framework Authors. Portions copyright
// 2013 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#include "libcef/browser/extensions/extension_view_host.h"

#include "libcef/browser/browser_platform_delegate.h"
#include "libcef/browser/extensions/extension_host_delegate.h"

#include "content/public/browser/web_contents.h"
#include "extensions/browser/process_util.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"

using content::NativeWebKeyboardEvent;
using content::OpenURLParams;
using content::WebContents;
using content::WebContentsObserver;

namespace extensions {

CefExtensionViewHost::CefExtensionViewHost(AlloyBrowserHostImpl* browser,
                                           const Extension* extension,
                                           content::WebContents* host_contents,
                                           const GURL& url,
                                           mojom::ViewType host_type)
    : ExtensionHost(new CefExtensionHostDelegate(browser),
                    extension,
                    host_contents->GetBrowserContext(),
                    host_contents,
                    url,
                    host_type) {
  // Only used for popups.
  DCHECK(host_type == mojom::ViewType::kExtensionPopup);
}

CefExtensionViewHost::~CefExtensionViewHost() = default;

void CefExtensionViewHost::OnDidStopFirstLoad() {
  // Nothing to do here, but don't call the base class method.
}

void CefExtensionViewHost::LoadInitialURL() {
  if (process_util::GetPersistentBackgroundPageState(*extension(),
                                                     browser_context()) ==
      process_util::PersistentBackgroundPageState::kNotReady) {
    // Make sure the background page loads before any others.
    host_registry_observation_.Observe(
        ExtensionHostRegistry::Get(browser_context()));
    return;
  }

  ExtensionHost::LoadInitialURL();
}

bool CefExtensionViewHost::IsBackgroundPage() const {
  return false;
}

bool CefExtensionViewHost::ShouldAllowRendererInitiatedCrossProcessNavigation(
    bool is_main_frame_navigation) {
  // Block navigations that cause the main frame to navigate to non-extension
  // content (i.e. to web content).
  return !is_main_frame_navigation;
}

bool CefExtensionViewHost::PreHandleGestureEvent(
    content::WebContents* source,
    const blink::WebGestureEvent& event) {
  // Disable pinch zooming.
  return blink::WebInputEvent::IsPinchGestureEventType(event.GetType());
}

WebContents* CefExtensionViewHost::GetVisibleWebContents() const {
  if (extension_host_type() == mojom::ViewType::kExtensionPopup) {
    return host_contents();
  }
  return nullptr;
}

void CefExtensionViewHost::OnExtensionHostDocumentElementAvailable(
    content::BrowserContext* host_browser_context,
    ExtensionHost* extension_host) {
  DCHECK(extension_host->extension());
  if (host_browser_context != browser_context() ||
      extension_host->extension() != extension() ||
      extension_host->extension_host_type() !=
          mojom::ViewType::kExtensionBackgroundPage) {
    return;
  }

  DCHECK_EQ(process_util::PersistentBackgroundPageState::kReady,
            process_util::GetPersistentBackgroundPageState(*extension(),
                                                           browser_context()));
  // We only needed to wait for the background page to load, so stop observing.
  host_registry_observation_.Reset();
  LoadInitialURL();
}

}  // namespace extensions
