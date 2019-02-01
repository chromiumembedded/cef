// Copyright 2017 the Chromium Embedded Framework Authors. Portions copyright
// 2014 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#ifndef LIBCEF_BROWSER_EXTENSIONS_EXTENSION_HOST_DELEGATE_H_
#define LIBCEF_BROWSER_EXTENSIONS_EXTENSION_HOST_DELEGATE_H_

#include "base/macros.h"
#include "extensions/browser/extension_host_delegate.h"

class CefBrowserHostImpl;

namespace extensions {

class CefExtensionHostDelegate : public ExtensionHostDelegate {
 public:
  explicit CefExtensionHostDelegate(CefBrowserHostImpl* browser);
  ~CefExtensionHostDelegate() override;

  // ExtensionHostDelegate implementation.
  void OnExtensionHostCreated(content::WebContents* web_contents) override;
  void OnRenderViewCreatedForBackgroundPage(ExtensionHost* host) override;
  content::JavaScriptDialogManager* GetJavaScriptDialogManager() override;
  void CreateTab(std::unique_ptr<content::WebContents> web_contents,
                 const std::string& extension_id,
                 WindowOpenDisposition disposition,
                 const gfx::Rect& initial_rect,
                 bool user_gesture) override;
  void ProcessMediaAccessRequest(content::WebContents* web_contents,
                                 const content::MediaStreamRequest& request,
                                 content::MediaResponseCallback callback,
                                 const Extension* extension) override;
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const GURL& security_origin,
                                  blink::MediaStreamType type,
                                  const Extension* extension) override;
  ExtensionHostQueue* GetExtensionHostQueue() const override;
  gfx::Size EnterPictureInPicture(content::WebContents* web_contents,
                                  const viz::SurfaceId& surface_id,
                                  const gfx::Size& natural_size) override;
  void ExitPictureInPicture() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CefExtensionHostDelegate);
};

}  // namespace extensions

#endif  // LIBCEF_BROWSER_EXTENSIONS_EXTENSION_HOST_DELEGATE_H_
