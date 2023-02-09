// Copyright 2020 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_CHROME_BROWSER_DELEGATE_H_
#define CEF_LIBCEF_BROWSER_CHROME_BROWSER_DELEGATE_H_
#pragma once

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/base/window_open_disposition.h"

class Browser;

namespace cef {

// Delegate for the chrome Browser object. Lifespan is controlled by the Browser
// object. See the ChromeBrowserDelegate documentation for additional details.
// Only accessed on the UI thread.
class BrowserDelegate : public content::WebContentsDelegate {
 public:
  // Opaque ref-counted base class for CEF-specific parameters passed via
  // Browser::CreateParams::cef_params and possibly shared by multiple Browser
  // instances.
  class CreateParams : public base::RefCounted<CreateParams> {
   public:
    virtual ~CreateParams() {}
  };

  // Called from the Browser constructor to create a new delegate.
  static std::unique_ptr<BrowserDelegate> Create(
      Browser* browser,
      scoped_refptr<CreateParams> cef_params);

  ~BrowserDelegate() override {}

  // Optionally override chrome::AddWebContents behavior. This is most often
  // called via Browser::AddNewContents for new popup browsers and provides an
  // opportunity for CEF to create a new Browser instead of proceeding with
  // default Browser or tab creation.
  virtual std::unique_ptr<content::WebContents> AddWebContents(
      std::unique_ptr<content::WebContents> new_contents) = 0;

  // Called immediately after |new_contents| is created via chrome::Navigate.
  // This is most often called for navigations targeting a new tab without a
  // pre-existing WebContents.
  virtual void OnWebContentsCreated(content::WebContents* new_contents) = 0;

  // Add or remove ownership of the WebContents.
  virtual void SetAsDelegate(content::WebContents* web_contents,
                             bool set_delegate) = 0;

  // Return true to show the status bubble. This should consistently return the
  // same value for the lifespan of a Browser.
  virtual bool ShowStatusBubble(bool show_by_default) {
    return show_by_default;
  }

  // Return true to handle (or disable) a command. ID values come from
  // chrome/app/chrome_command_ids.h.
  virtual bool HandleCommand(int command_id,
                             WindowOpenDisposition disposition) {
    return false;
  }

  // Same as RequestMediaAccessPermission but returning |callback| if the
  // request is unhandled.
  [[nodiscard]] virtual content::MediaResponseCallback
  RequestMediaAccessPermissionEx(content::WebContents* web_contents,
                                 const content::MediaStreamRequest& request,
                                 content::MediaResponseCallback callback) {
    return callback;
  }
};

}  // namespace cef

#endif  // CEF_LIBCEF_BROWSER_CHROME_BROWSER_DELEGATE_H_
