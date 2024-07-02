// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_BROWSER_GUEST_UTIL_H_
#define CEF_LIBCEF_BROWSER_BROWSER_GUEST_UTIL_H_

namespace content {
class WebContents;
}  // namespace content

// Returns the WebContents that owns the specified |guest|, if any.
content::WebContents* GetOwnerForGuestContents(
    const content::WebContents* guest);

// Test for different types of guest contents.
bool IsBrowserPluginGuest(const content::WebContents* web_contents);
bool IsPrintPreviewDialog(const content::WebContents* web_contents);

#endif  // CEF_LIBCEF_BROWSER_BROWSER_GUEST_UTIL_H_
