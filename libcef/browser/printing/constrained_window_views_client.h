// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_PRINTING_CONSTRAINED_WINDOW_VIEWS_CLIENT_H_
#define CEF_LIBCEF_BROWSER_PRINTING_CONSTRAINED_WINDOW_VIEWS_CLIENT_H_

#include <memory>

#include "components/constrained_window/constrained_window_views_client.h"

// Creates a ConstrainedWindowViewsClient for the Chrome environment.
std::unique_ptr<constrained_window::ConstrainedWindowViewsClient>
CreateCefConstrainedWindowViewsClient();

#endif  // CEF_LIBCEF_BROWSER_PRINTING_CONSTRAINED_WINDOW_VIEWS_CLIENT_H_