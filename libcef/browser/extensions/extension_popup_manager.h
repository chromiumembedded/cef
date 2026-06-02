// Copyright 2025 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_EXTENSIONS_EXTENSION_POPUP_MANAGER_H_
#define CEF_LIBCEF_BROWSER_EXTENSIONS_EXTENSION_POPUP_MANAGER_H_
#pragma once

#include <map>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "cef/include/cef_base.h"
#include "cef/include/internal/cef_types_geometry.h"

class CefBrowser;
class CefRequestContextImpl;

namespace content {
class BrowserContext;
}  // namespace content

namespace cef {

// One per CefRequestContextImpl. Owns the bubble Widgets for currently-open
// extension action popups (Chrome's ExtensionPopup for Chrome style browsers,
// CefAlloyExtensionPopup for Alloy style). At most one popup per extension id:
// re-showing replaces the previous one. All methods must run on the UI thread.
class CefExtensionPopupManager {
 public:
  explicit CefExtensionPopupManager(CefRequestContextImpl* request_context);

  CefExtensionPopupManager(const CefExtensionPopupManager&) = delete;
  CefExtensionPopupManager& operator=(const CefExtensionPopupManager&) = delete;

  ~CefExtensionPopupManager();

  // Opens (or replaces) the action popup for |extension_id| on behalf of
  // |source_browser|, anchored to |anchor_screen_rect| in screen DIPs. If the
  // extension's action declares no popup, dispatches the click instead.
  void ShowPopup(const CefString& extension_id,
                 CefRefPtr<CefBrowser> source_browser,
                 const CefRect& anchor_screen_rect);

  // Closes any popup currently open for |extension_id|.
  void ClosePopup(const CefString& extension_id);

 private:
  class PopupTracker;

  content::BrowserContext* GetBrowserContext() const;

  // Called by PopupTracker when its tracked Widget is destroyed.
  void RemovePopup(const std::string& extension_id);

  // Owns us; always outlives this object.
  raw_ptr<CefRequestContextImpl> request_context_;

  std::map<std::string, std::unique_ptr<PopupTracker>> popups_;
};

}  // namespace cef

#endif  // CEF_LIBCEF_BROWSER_EXTENSIONS_EXTENSION_POPUP_MANAGER_H_
