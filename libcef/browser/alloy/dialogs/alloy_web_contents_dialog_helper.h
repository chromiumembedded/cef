// Copyright (c) 2019 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_ALLOY_DIALOGS_ALLOY_WEB_CONTENTS_DIALOG_HELPER_H_
#define CEF_LIBCEF_BROWSER_ALLOY_DIALOGS_ALLOY_WEB_CONTENTS_DIALOG_HELPER_H_
#pragma once

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/web_modal/modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"

class CefBrowserPlatformDelegate;

class AlloyWebContentsDialogHelper
    : public web_modal::WebContentsModalDialogManagerDelegate,
      public web_modal::WebContentsModalDialogHost {
 public:
  AlloyWebContentsDialogHelper(content::WebContents* web_contents,
                               CefBrowserPlatformDelegate* browser_delegate);

  base::RepeatingClosure GetBoundsChangedCallback();

  // web_modal::WebContentsModalDialogManagerDelegate methods:
  bool IsWebContentsVisible(content::WebContents* web_contents) override;
  web_modal::WebContentsModalDialogHost* GetWebContentsModalDialogHost()
      override;

  // web_modal::WebContentsModalDialogHost methods:
  gfx::NativeView GetHostView() const override;
  gfx::AcceleratedWidget GetAcceleratedWidget() const override;
  gfx::Point GetDialogPosition(const gfx::Size& size) override;
  gfx::Size GetMaximumDialogSize() override;
  void AddObserver(web_modal::ModalDialogHostObserver* observer) override;
  void RemoveObserver(web_modal::ModalDialogHostObserver* observer) override;

 private:
  void OnBoundsChanged();

  CefBrowserPlatformDelegate* const browser_delegate_;

  // Used to notify WebContentsModalDialog.
  base::ObserverList<web_modal::ModalDialogHostObserver>::Unchecked
      observer_list_;

  base::WeakPtrFactory<AlloyWebContentsDialogHelper> weak_factory_;
};

#endif  // CEF_LIBCEF_BROWSER_ALLOY_DIALOGS_ALLOY_WEB_CONTENTS_DIALOG_HELPER_H_
