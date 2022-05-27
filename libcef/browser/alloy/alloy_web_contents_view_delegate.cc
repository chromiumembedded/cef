// Copyright 2022 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/alloy/alloy_web_contents_view_delegate.h"

#include "content/public/browser/web_contents.h"

#include "libcef/browser/alloy/alloy_browser_host_impl.h"

AlloyWebContentsViewDelegate::AlloyWebContentsViewDelegate(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

void AlloyWebContentsViewDelegate::ShowContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  if (auto browser =
          AlloyBrowserHostImpl::GetBrowserForContents(web_contents_)) {
    browser->ShowContextMenu(params);
  }
}
