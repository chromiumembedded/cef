// Copyright 2022 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_CHROME_VIEWS_CHROME_CHILD_WINDOW_H_
#define CEF_LIBCEF_BROWSER_CHROME_VIEWS_CHROME_CHILD_WINDOW_H_
#pragma once

#include "libcef/browser/browser_host_base.h"

#include "ui/gfx/native_widget_types.h"

namespace chrome_child_window {

bool HasParentHandle(const CefWindowInfo& window_info);
gfx::AcceleratedWidget GetParentHandle(const CefWindowInfo& window_info);

// Called from CefBrowserHostBase::Create.
CefRefPtr<CefBrowserHostBase> MaybeCreateChildBrowser(
    const CefBrowserCreateParams& create_params);

}  // namespace chrome_child_window

#endif  // CEF_LIBCEF_BROWSER_CHROME_VIEWS_CHROME_CHILD_WINDOW_H_
