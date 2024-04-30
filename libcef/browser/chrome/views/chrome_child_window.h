// Copyright 2022 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_CHROME_VIEWS_CHROME_CHILD_WINDOW_H_
#define CEF_LIBCEF_BROWSER_CHROME_VIEWS_CHROME_CHILD_WINDOW_H_
#pragma once

#include "cef/include/views/cef_browser_view_delegate.h"
#include "cef/libcef/browser/browser_host_base.h"

namespace chrome_child_window {

bool HasParentHandle(const CefWindowInfo& window_info);

// Called from CefBrowserHostBase::Create.
CefRefPtr<CefBrowserHostBase> MaybeCreateChildBrowser(
    const CefBrowserCreateParams& create_params);

CefRefPtr<CefBrowserViewDelegate> GetDefaultBrowserViewDelegateForPopupOpener();

}  // namespace chrome_child_window

#endif  // CEF_LIBCEF_BROWSER_CHROME_VIEWS_CHROME_CHILD_WINDOW_H_
