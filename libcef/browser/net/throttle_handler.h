// Copyright 2020 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NET_THROTTLE_HANDLER_H_
#define CEF_LIBCEF_BROWSER_NET_THROTTLE_HANDLER_H_
#pragma once

namespace content {
class NavigationThrottleRegistry;
}  // namespace content

namespace throttle {

void CreateThrottlesForNavigation(
    content::NavigationThrottleRegistry& registry);

}  // namespace throttle

#endif  // CEF_LIBCEF_BROWSER_NET_THROTTLE_HANDLER_H_
