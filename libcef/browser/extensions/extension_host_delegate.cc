// Copyright 2017 the Chromium Embedded Framework Authors. Portions copyright
// 2014 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#include "libcef/browser/extensions/extension_host_delegate.h"

#include "libcef/browser/extensions/extensions_browser_client.h"

#include "base/logging.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"

namespace extensions {

CefExtensionHostDelegate::CefExtensionHostDelegate(
    AlloyBrowserHostImpl* browser) {}

CefExtensionHostDelegate::~CefExtensionHostDelegate() {}

void CefExtensionHostDelegate::OnExtensionHostCreated(
    content::WebContents* web_contents) {}

void CefExtensionHostDelegate::OnMainFrameCreatedForBackgroundPage(
    ExtensionHost* host) {}

content::JavaScriptDialogManager*
CefExtensionHostDelegate::GetJavaScriptDialogManager() {
  // Never routed here from AlloyBrowserHostImpl.
  NOTREACHED();
  return nullptr;
}

void CefExtensionHostDelegate::CreateTab(
    std::unique_ptr<content::WebContents> web_contents,
    const std::string& extension_id,
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_rect,
    bool user_gesture) {
  // TODO(cef): Add support for extensions opening popup windows.
  NOTIMPLEMENTED();
}

void CefExtensionHostDelegate::ProcessMediaAccessRequest(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback,
    const Extension* extension) {
  // Never routed here from AlloyBrowserHostImpl.
  NOTREACHED();
}

bool CefExtensionHostDelegate::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const GURL& security_origin,
    blink::mojom::MediaStreamType type,
    const Extension* extension) {
  // Never routed here from AlloyBrowserHostImpl.
  NOTREACHED();
  return false;
}

content::PictureInPictureResult CefExtensionHostDelegate::EnterPictureInPicture(
    content::WebContents* web_contents,
    const viz::SurfaceId& surface_id,
    const gfx::Size& natural_size) {
  NOTREACHED();
  return content::PictureInPictureResult::kNotSupported;
}

void CefExtensionHostDelegate::ExitPictureInPicture() {
  NOTREACHED();
}

}  // namespace extensions
