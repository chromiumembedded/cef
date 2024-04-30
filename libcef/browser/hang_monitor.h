// Copyright 2024 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_HANG_MONITOR_H_
#define CEF_LIBCEF_BROWSER_HANG_MONITOR_H_
#pragma once

#include "base/functional/callback.h"
#include "cef/include/cef_unresponsive_process_callback.h"

namespace content {
class RenderWidgetHost;
}

class CefBrowserHostBase;

namespace hang_monitor {

// Called from WebContentsDelegate::RendererUnresponsive.
// Returns false for default handling.
bool RendererUnresponsive(CefBrowserHostBase* browser,
                          content::RenderWidgetHost* render_widget_host,
                          base::RepeatingClosure hang_monitor_restarter);

// Called from WebContentsDelegate::RendererResponsive.
// Returns false for default handling.
bool RendererResponsive(CefBrowserHostBase* browser,
                        content::RenderWidgetHost* render_widget_host);

// Detach an existing callback object.
void Detach(CefRefPtr<CefUnresponsiveProcessCallback> callback);

}  // namespace hang_monitor

#endif  // CEF_LIBCEF_BROWSER_HANG_MONITOR_H_
