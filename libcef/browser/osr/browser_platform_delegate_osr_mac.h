// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_OSR_BROWSER_PLATFORM_DELEGATE_OSR_MAC_H_
#define CEF_LIBCEF_BROWSER_OSR_BROWSER_PLATFORM_DELEGATE_OSR_MAC_H_

#include "libcef/browser/osr/browser_platform_delegate_osr.h"

// Windowless browser implementation for Mac OS X.
class CefBrowserPlatformDelegateOsrMac : public CefBrowserPlatformDelegateOsr {
 public:
  explicit CefBrowserPlatformDelegateOsrMac(
      std::unique_ptr<CefBrowserPlatformDelegateNative> native_delegate);
    
  // CefBrowserPlatformDelegate methods:
  CefWindowHandle GetHostWindowHandle() const override;
};

#endif  // CEF_LIBCEF_BROWSER_NATIVE_BROWSER_PLATFORM_DELEGATE_OSR_MAC_H_
