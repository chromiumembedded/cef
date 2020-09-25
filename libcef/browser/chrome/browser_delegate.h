// Copyright 2020 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_CHROME_BROWSER_DELEGATE_H_
#define CEF_LIBCEF_BROWSER_CHROME_BROWSER_DELEGATE_H_
#pragma once

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "content/public/browser/web_contents_delegate.h"

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

  // Called immediately after |new_contents| is created.
  virtual void OnWebContentsCreated(content::WebContents* new_contents) = 0;

  // Add or remove ownership of the WebContents.
  virtual void SetAsDelegate(content::WebContents* web_contents,
                             bool set_delegate) = 0;
};

}  // namespace cef

#endif  // CEF_LIBCEF_BROWSER_CHROME_BROWSER_DELEGATE_H_
