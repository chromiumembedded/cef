// Copyright 2021 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_CHROME_VIEWS_CHROME_VIEWS_H_
#define CEF_LIBCEF_BROWSER_CHROME_VIEWS_CHROME_VIEWS_H_
#pragma once

namespace views {
class View;
}

namespace cef {

// Returns true if |view| is a CefView.
bool IsCefView(views::View* view);

}  // namespace cef

#endif  // CEF_LIBCEF_BROWSER_CHROME_VIEWS_CHROME_VIEWS_H_
