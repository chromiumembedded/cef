// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/renderer/extensions/extensions_dispatcher_delegate.h"

#include "content/public/common/url_constants.h"
#include "extensions/common/extension.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebSecurityPolicy.h"

namespace extensions {

CefExtensionsDispatcherDelegate::CefExtensionsDispatcherDelegate() {
}

CefExtensionsDispatcherDelegate::~CefExtensionsDispatcherDelegate() {
}

void CefExtensionsDispatcherDelegate::InitOriginPermissions(
    const Extension* extension,
    bool is_extension_active) {
  if (is_extension_active) {
    // The chrome: scheme is marked as display isolated in
    // RenderThreadImpl::RegisterSchemes() so an exception must be added for
    // accessing chrome://resources from the extension origin.
    blink::WebSecurityPolicy::addOriginAccessWhitelistEntry(
        extension->url(),
        blink::WebString::fromUTF8(content::kChromeUIScheme),
        blink::WebString::fromUTF8("resources"),
        false);
  }
}

}  // namespace extensions
