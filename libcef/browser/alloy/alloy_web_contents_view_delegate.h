// Copyright 2022 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_ALLOY_ALLOY_WEB_CONTENTS_VIEW_DELEGATE_H_
#define CEF_LIBCEF_BROWSER_ALLOY_ALLOY_WEB_CONTENTS_VIEW_DELEGATE_H_
#pragma once

#include "include/internal/cef_ptr.h"

#include "content/public/browser/web_contents_view_delegate.h"

namespace content {
class WebContents;
}

class AlloyWebContentsViewDelegate : public content::WebContentsViewDelegate {
 public:
  explicit AlloyWebContentsViewDelegate(content::WebContents* web_contents);

  AlloyWebContentsViewDelegate(const AlloyWebContentsViewDelegate&) = delete;
  AlloyWebContentsViewDelegate& operator=(const AlloyWebContentsViewDelegate&) =
      delete;

  // WebContentsViewDelegate methods:
  void ShowContextMenu(content::RenderFrameHost& render_frame_host,
                       const content::ContextMenuParams& params) override;

 private:
  content::WebContents* const web_contents_;
};

#endif  // CEF_LIBCEF_BROWSER_ALLOY_ALLOY_WEB_CONTENTS_VIEW_DELEGATE_H_
