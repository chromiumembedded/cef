// Copyright 2020 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NET_THROTTLE_HANDLER_H_
#define CEF_LIBCEF_BROWSER_NET_THROTTLE_HANDLER_H_
#pragma once

#include <memory>
#include <vector>

namespace content {
class NavigationHandle;
class NavigationThrottle;
}  // namespace content

namespace throttle {

using NavigationThrottleList =
    std::vector<std::unique_ptr<content::NavigationThrottle>>;

void CreateThrottlesForNavigation(content::NavigationHandle* navigation_handle,
                                  NavigationThrottleList& throttles);

}  // namespace throttle

#endif  // CEF_LIBCEF_BROWSER_NET_THROTTLE_HANDLER_H_
