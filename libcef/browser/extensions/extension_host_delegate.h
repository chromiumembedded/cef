// Copyright 2017 the Chromium Embedded Framework Authors. Portions copyright
// 2014 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#ifndef LIBCEF_BROWSER_EXTENSIONS_EXTENSION_HOST_DELEGATE_H_
#define LIBCEF_BROWSER_EXTENSIONS_EXTENSION_HOST_DELEGATE_H_

#include "extensions/browser/extension_host_delegate.h"

class AlloyBrowserHostImpl;

namespace extensions {

class CefExtensionHostDelegate : public ExtensionHostDelegate {
 public:
  explicit CefExtensionHostDelegate(AlloyBrowserHostImpl* browser);

  CefExtensionHostDelegate(const CefExtensionHostDelegate&) = delete;
  CefExtensionHostDelegate& operator=(const CefExtensionHostDelegate&) = delete;

  ~CefExtensionHostDelegate() override;

  // ExtensionHostDelegate implementation.
  void OnExtensionHostCreated(content::WebContents* web_contents) override;
  void OnMainFrameCreatedForBackgroundPage(ExtensionHost* host) override;
  content::JavaScriptDialogManager* GetJavaScriptDialogManager() override;
  void CreateTab(std::unique_ptr<content::WebContents> web_contents,
                 const std::string& extension_id,
                 WindowOpenDisposition disposition,
                 const blink::mojom::WindowFeatures& window_features,
                 bool user_gesture) override;
  void ProcessMediaAccessRequest(content::WebContents* web_contents,
                                 const content::MediaStreamRequest& request,
                                 content::MediaResponseCallback callback,
                                 const Extension* extension) override;
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const url::Origin& security_origin,
                                  blink::mojom::MediaStreamType type,
                                  const Extension* extension) override;
  content::PictureInPictureResult EnterPictureInPicture(
      content::WebContents* web_contents) override;
  void ExitPictureInPicture() override;
};

}  // namespace extensions

#endif  // LIBCEF_BROWSER_EXTENSIONS_EXTENSION_HOST_DELEGATE_H_
