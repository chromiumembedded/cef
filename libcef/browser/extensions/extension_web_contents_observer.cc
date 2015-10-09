// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/extensions/extension_web_contents_observer.h"

#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/url_constants.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(extensions::CefExtensionWebContentsObserver);

namespace extensions {

CefExtensionWebContentsObserver::CefExtensionWebContentsObserver(
    content::WebContents* web_contents)
    : ExtensionWebContentsObserver(web_contents) {
}

CefExtensionWebContentsObserver::~CefExtensionWebContentsObserver() {
}

void CefExtensionWebContentsObserver::RenderViewCreated(
    content::RenderViewHost* render_view_host) {
  ExtensionWebContentsObserver::RenderViewCreated(render_view_host);

  const Extension* extension = GetExtension(render_view_host);
  if (!extension)
    return;

  int process_id = render_view_host->GetProcess()->GetID();
  auto policy = content::ChildProcessSecurityPolicy::GetInstance();

  // Components of chrome that are implemented as extensions or platform apps
  // are allowed to use chrome://resources/ URLs.
  if ((extension->is_extension() || extension->is_platform_app()) &&
      Manifest::IsComponentLocation(extension->location())) {
    policy->GrantOrigin(process_id,
                        url::Origin(GURL(content::kChromeUIResourcesURL)));
  }
}

}  // namespace extensions
