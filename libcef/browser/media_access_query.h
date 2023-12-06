// Copyright 2022 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_MEDIA_ACCESS_QUERY_H_
#define CEF_LIBCEF_BROWSER_MEDIA_ACCESS_QUERY_H_
#pragma once

#include "content/public/browser/media_stream_request.h"

namespace content {
class RenderFrameHost;
}

namespace url {
class Origin;
}

class CefBrowserHostBase;

namespace media_access_query {

// Called from WebContentsDelegate::CheckMediaAccessPermission.
bool CheckMediaAccessPermission(CefBrowserHostBase* browser,
                                content::RenderFrameHost* render_frame_host,
                                const url::Origin& security_origin,
                                blink::mojom::MediaStreamType type);

// Called from WebContentsDelegate::RequestMediaAccessPermission.
// |callback| will be returned if the request is unhandled and
// |default_disallow| is false.
[[nodiscard]] content::MediaResponseCallback RequestMediaAccessPermission(
    CefBrowserHostBase* browser,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback,
    bool default_disallow);

}  // namespace media_access_query

#endif  // CEF_LIBCEF_BROWSER_MEDIA_ACCESS_QUERY_H_
