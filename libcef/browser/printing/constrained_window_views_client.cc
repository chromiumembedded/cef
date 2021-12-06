// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/printing/constrained_window_views_client.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser_finder.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"

namespace {

class CefConstrainedWindowViewsClient
    : public constrained_window::ConstrainedWindowViewsClient {
 public:
  CefConstrainedWindowViewsClient() {}

  CefConstrainedWindowViewsClient(const CefConstrainedWindowViewsClient&) =
      delete;
  CefConstrainedWindowViewsClient& operator=(
      const CefConstrainedWindowViewsClient&) = delete;

  ~CefConstrainedWindowViewsClient() override {}

 private:
  // ConstrainedWindowViewsClient:
  web_modal::ModalDialogHost* GetModalDialogHost(
      gfx::NativeWindow parent) override {
    NOTREACHED();
    return nullptr;
  }
  gfx::NativeView GetDialogHostView(gfx::NativeWindow parent) override {
    NOTREACHED();
    return gfx::NativeView();
  }
};

}  // namespace

std::unique_ptr<constrained_window::ConstrainedWindowViewsClient>
CreateCefConstrainedWindowViewsClient() {
  return base::WrapUnique(new CefConstrainedWindowViewsClient);
}