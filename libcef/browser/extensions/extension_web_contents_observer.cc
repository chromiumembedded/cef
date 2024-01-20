// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/extensions/extension_web_contents_observer.h"

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/url_constants.h"
#include "third_party/blink/public/common/chrome_debug_urls.h"

namespace extensions {

CefExtensionWebContentsObserver::CefExtensionWebContentsObserver(
    content::WebContents* web_contents)
    : ExtensionWebContentsObserver(web_contents),
      content::WebContentsUserData<CefExtensionWebContentsObserver>(
          *web_contents),
      script_executor_(new ScriptExecutor(web_contents)) {}

CefExtensionWebContentsObserver::~CefExtensionWebContentsObserver() = default;

// static
void CefExtensionWebContentsObserver::CreateForWebContents(
    content::WebContents* web_contents) {
  content::WebContentsUserData<
      CefExtensionWebContentsObserver>::CreateForWebContents(web_contents);

  // Initialize this instance if necessary.
  FromWebContents(web_contents)->Initialize();
}

void CefExtensionWebContentsObserver::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  ExtensionWebContentsObserver::RenderFrameCreated(render_frame_host);

  const Extension* extension = GetExtensionFromFrame(render_frame_host, false);
  if (!extension) {
    return;
  }

  int process_id = render_frame_host->GetProcess()->GetID();
  auto policy = content::ChildProcessSecurityPolicy::GetInstance();

  // Components of chrome that are implemented as extensions or platform apps
  // are allowed to use chrome://resources/ and chrome://theme/ URLs.
  if ((extension->is_extension() || extension->is_platform_app()) &&
      Manifest::IsComponentLocation(extension->location())) {
    policy->GrantRequestOrigin(
        process_id, url::Origin::Create(GURL(blink::kChromeUIResourcesURL)));
    policy->GrantRequestOrigin(
        process_id, url::Origin::Create(GURL(chrome::kChromeUIThemeURL)));
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(CefExtensionWebContentsObserver);

}  // namespace extensions
