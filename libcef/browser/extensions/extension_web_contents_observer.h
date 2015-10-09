// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_EXTENSIONS_EXTENSION_WEB_CONTENTS_OBSERVER_H_
#define CEF_LIBCEF_BROWSER_EXTENSIONS_EXTENSION_WEB_CONTENTS_OBSERVER_H_

#include "content/public/browser/web_contents_user_data.h"
#include "extensions/browser/extension_web_contents_observer.h"

namespace extensions {

// The CEF version of ExtensionWebContentsObserver.
class CefExtensionWebContentsObserver
    : public ExtensionWebContentsObserver,
      public content::WebContentsUserData<CefExtensionWebContentsObserver> {
 private:
  friend class content::WebContentsUserData<CefExtensionWebContentsObserver>;

  explicit CefExtensionWebContentsObserver(
      content::WebContents* web_contents);
  ~CefExtensionWebContentsObserver() override;

  // content::WebContentsObserver overrides.
  void RenderViewCreated(content::RenderViewHost* render_view_host) override;

  DISALLOW_COPY_AND_ASSIGN(CefExtensionWebContentsObserver);
};

}  // namespace extensions

#endif  // CEF_LIBCEF_BROWSER_EXTENSIONS_EXTENSION_WEB_CONTENTS_OBSERVER_H_
