// Copyright 2024 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_CHROME_CHROME_WEB_CONTENTS_VIEW_DELEGATE_CEF_H_
#define CEF_LIBCEF_BROWSER_CHROME_CHROME_WEB_CONTENTS_VIEW_DELEGATE_CEF_H_
#pragma once

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/views/tab_contents/chrome_web_contents_view_delegate_views_mac.h"
using ChromeWebContentsViewDelegateBase = ChromeWebContentsViewDelegateViewsMac;
#else
#include "chrome/browser/ui/views/tab_contents/chrome_web_contents_view_delegate_views.h"
using ChromeWebContentsViewDelegateBase = ChromeWebContentsViewDelegateViews;
#endif

class ChromeWebContentsViewDelegateCef
    : public ChromeWebContentsViewDelegateBase {
 public:
  explicit ChromeWebContentsViewDelegateCef(content::WebContents* web_contents);

  ChromeWebContentsViewDelegateCef(const ChromeWebContentsViewDelegateCef&) =
      delete;
  ChromeWebContentsViewDelegateCef& operator=(
      const ChromeWebContentsViewDelegateCef&) = delete;

  // WebContentsViewDelegate methods:
  void ShowContextMenu(content::RenderFrameHost& render_frame_host,
                       const content::ContextMenuParams& params) override;

 private:
  const raw_ptr<content::WebContents> web_contents_;
};

#endif  // CEF_LIBCEF_BROWSER_CHROME_CHROME_WEB_CONTENTS_VIEW_DELEGATE_CEF_H_
