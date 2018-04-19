// Copyright 2017 the Chromium Embedded Framework Authors. Portions copyright
// 2013 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#include "libcef/browser/extensions/extension_view_host.h"

#include "libcef/browser/browser_platform_delegate.h"
#include "libcef/browser/extensions/extension_host_delegate.h"

#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/runtime_data.h"
#include "third_party/blink/public/platform/web_gesture_event.h"

using content::NativeWebKeyboardEvent;
using content::OpenURLParams;
using content::WebContents;
using content::WebContentsObserver;

namespace extensions {

CefExtensionViewHost::CefExtensionViewHost(
    CefBrowserHostImpl* browser,
    const Extension* extension,
    content::BrowserContext* browser_context,
    content::WebContents* host_contents,
    const GURL& url,
    ViewType host_type)
    : ExtensionHost(new CefExtensionHostDelegate(browser),
                    extension,
                    browser_context,
                    host_contents,
                    url,
                    host_type) {
  // Only used for dialogs and popups.
  DCHECK(host_type == VIEW_TYPE_EXTENSION_DIALOG ||
         host_type == VIEW_TYPE_EXTENSION_POPUP);
}

CefExtensionViewHost::~CefExtensionViewHost() {}

void CefExtensionViewHost::OnDidStopFirstLoad() {
  // Nothing to do here, but don't call the base class method.
}

void CefExtensionViewHost::LoadInitialURL() {
  if (!ExtensionSystem::Get(browser_context())
           ->runtime_data()
           ->IsBackgroundPageReady(extension())) {
    // Make sure the background page loads before any others.
    registrar_.Add(this,
                   extensions::NOTIFICATION_EXTENSION_BACKGROUND_PAGE_READY,
                   content::Source<Extension>(extension()));
    return;
  }

  ExtensionHost::LoadInitialURL();
}

bool CefExtensionViewHost::IsBackgroundPage() const {
  return false;
}

bool CefExtensionViewHost::ShouldTransferNavigation(
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
  if (extension_host_type() == VIEW_TYPE_EXTENSION_POPUP)
    return host_contents();
  return nullptr;
}

void CefExtensionViewHost::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(type, extensions::NOTIFICATION_EXTENSION_BACKGROUND_PAGE_READY);
  DCHECK(ExtensionSystem::Get(browser_context())
             ->runtime_data()
             ->IsBackgroundPageReady(extension()));
  LoadInitialURL();
}

}  // namespace extensions
