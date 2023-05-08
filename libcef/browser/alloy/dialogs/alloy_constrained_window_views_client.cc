// Copyright 2022 The Chromium Embedded Framework Authors.
// Portions copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/alloy/dialogs/alloy_constrained_window_views_client.h"

#include "libcef/browser/browser_host_base.h"

#include "base/notreached.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"

namespace {

class AlloyConstrainedWindowViewsClient
    : public constrained_window::ConstrainedWindowViewsClient {
 public:
  AlloyConstrainedWindowViewsClient() = default;

  AlloyConstrainedWindowViewsClient(const AlloyConstrainedWindowViewsClient&) =
      delete;
  AlloyConstrainedWindowViewsClient& operator=(
      const AlloyConstrainedWindowViewsClient&) = delete;

 private:
  // ConstrainedWindowViewsClient methods:
  web_modal::ModalDialogHost* GetModalDialogHost(
      gfx::NativeWindow parent) override {
    if (auto browser = GetPreferredBrowser(parent)) {
      return browser->platform_delegate()->GetWebContentsModalDialogHost();
    }
    DCHECK(false);
    return nullptr;
  }

  gfx::NativeView GetDialogHostView(gfx::NativeWindow parent) override {
    if (auto dialog_host = GetModalDialogHost(parent)) {
      return dialog_host->GetHostView();
    }
    return gfx::NativeView();
  }

  static CefRefPtr<CefBrowserHostBase> GetPreferredBrowser(
      gfx::NativeWindow parent) {
    CefRefPtr<CefBrowserHostBase> browser;

    // 1. Browser associated with the top-level native window (owning_window).
    // This should be reliable with windowed browsers. However, |parent| will
    // always be nullptr with windowless browsers.
    if (parent) {
      browser = CefBrowserHostBase::GetBrowserForTopLevelNativeWindow(parent);
      if (!browser) {
        LOG(WARNING) << "No browser associated with top-level native window";
      }
    }

    // 2. Browser most likely to be focused. This may be somewhat iffy with
    // windowless browsers as there is no guarantee that the client has only
    // one browser focused at a time.
    if (!browser) {
      browser = CefBrowserHostBase::GetLikelyFocusedBrowser();
      if (!browser) {
        LOG(WARNING) << "No likely focused browser";
      }
    }

    return browser;
  }
};

}  // namespace

std::unique_ptr<constrained_window::ConstrainedWindowViewsClient>
CreateAlloyConstrainedWindowViewsClient() {
  return std::make_unique<AlloyConstrainedWindowViewsClient>();
}
