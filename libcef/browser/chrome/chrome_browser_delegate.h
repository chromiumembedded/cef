// Copyright 2020 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_CHROME_CHROME_BROWSER_DELEGATE_H_
#define CEF_LIBCEF_BROWSER_CHROME_CHROME_BROWSER_DELEGATE_H_
#pragma once

#include <memory>

#include "libcef/browser/browser_host_base.h"
#include "libcef/browser/browser_info.h"
#include "libcef/browser/chrome/browser_delegate.h"

#include "base/optional.h"

class CefBrowserContentsDelegate;
class CefRequestContextImpl;
class ChromeBrowserHostImpl;

// Implementation of the cef::BrowserDelegate interface. Lifespan is controlled
// by the Browser object. Only accessed on the UI thread.
//
// The Browser object represents the top-level Chrome browser window. One or
// more tabs (WebContents) are then owned by the Browser object via
// TabStripModel. A new Browser object can be created programmatically using
// "new Browser" or Browser::Create, or as a result of user action such as
// dragging a tab out of an existing window. New or existing tabs can also be
// added to an already existing Browser object.
//
// The Browser object acts as the WebContentsDelegate for all attached tabs. CEF
// integration requires WebContentsDelegate callbacks and notification of tab
// attach/detach. To support this integration a cef::BrowserDelegate
// (ChromeBrowserDelegate) member is created in the Browser constructor and
// receives delegation for the Browser callbacks. ChromeBrowserDelegate creates
// a new ChromeBrowserHostImpl when a tab is added to a Browser for the first
// time, and that ChromeBrowserHostImpl continues to exist until the tab's
// WebContents is destroyed. The associated WebContents object does not change,
// but the Browser object will change when the tab is dragged between windows.
class ChromeBrowserDelegate : public cef::BrowserDelegate {
 public:
  ChromeBrowserDelegate(Browser* browser,
                        const CefBrowserHostBase::CreateParams& create_params);
  ~ChromeBrowserDelegate() override;

  // cef::BrowserDelegate methods:
  void SetAsDelegate(content::WebContents* web_contents,
                     bool set_delegate) override;

  // WebContentsDelegate methods:
  void LoadingStateChanged(content::WebContents* source,
                           bool to_different_document) override;
  void UpdateTargetURL(content::WebContents* source, const GURL& url) override;
  bool DidAddMessageToConsole(content::WebContents* source,
                              blink::mojom::ConsoleMessageLevel log_level,
                              const base::string16& message,
                              int32_t line_no,
                              const base::string16& source_id) override;
  void DidNavigateMainFramePostCommit(
      content::WebContents* web_contents) override;
  void EnterFullscreenModeForTab(
      content::RenderFrameHost* requesting_frame,
      const blink::mojom::FullscreenOptions& options) override;
  void ExitFullscreenModeForTab(content::WebContents* web_contents) override;

  Browser* browser() const { return browser_; }

 private:
  CefBrowserContentsDelegate* GetDelegateForWebContents(
      content::WebContents* web_contents);

  Browser* const browser_;

  // Used when creating a new browser host.
  const CefBrowserHostBase::CreateParams create_params_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserDelegate);
};

#endif  // CEF_LIBCEF_BROWSER_CHROME_CHROME_BROWSER_DELEGATE_H_
