// Copyright (c) 2019 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/web_contents_dialog_helper.h"

#include "libcef/browser/browser_platform_delegate.h"

#include "chrome/browser/platform_util.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"

#if defined(TOOLKIT_VIEWS)
#include "ui/views/widget/widget.h"
#endif

CefWebContentsDialogHelper::CefWebContentsDialogHelper(
    content::WebContents* web_contents,
    CefBrowserPlatformDelegate* browser_delegate)
    : browser_delegate_(browser_delegate), weak_factory_(this) {
  web_modal::WebContentsModalDialogManager::CreateForWebContents(web_contents);
  web_modal::WebContentsModalDialogManager::FromWebContents(web_contents)
      ->SetDelegate(this);
}

base::RepeatingClosure CefWebContentsDialogHelper::GetBoundsChangedCallback() {
  return base::BindRepeating(&CefWebContentsDialogHelper::OnBoundsChanged,
                             weak_factory_.GetWeakPtr());
}

bool CefWebContentsDialogHelper::IsWebContentsVisible(
    content::WebContents* web_contents) {
  return platform_util::IsVisible(web_contents->GetNativeView());
}

web_modal::WebContentsModalDialogHost*
CefWebContentsDialogHelper::GetWebContentsModalDialogHost() {
  return this;
}

gfx::NativeView CefWebContentsDialogHelper::GetHostView() const {
#if defined(TOOLKIT_VIEWS)
  return browser_delegate_->GetWindowWidget()->GetNativeView();
#else
  NOTIMPLEMENTED();
  return gfx::NativeView();
#endif
}

gfx::Point CefWebContentsDialogHelper::GetDialogPosition(
    const gfx::Size& size) {
  return browser_delegate_->GetDialogPosition(size);
}

gfx::Size CefWebContentsDialogHelper::GetMaximumDialogSize() {
  return browser_delegate_->GetMaximumDialogSize();
}

void CefWebContentsDialogHelper::AddObserver(
    web_modal::ModalDialogHostObserver* observer) {
  if (observer && !observer_list_.HasObserver(observer))
    observer_list_.AddObserver(observer);
}

void CefWebContentsDialogHelper::RemoveObserver(
    web_modal::ModalDialogHostObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void CefWebContentsDialogHelper::OnBoundsChanged() {
  for (auto& observer : observer_list_)
    observer.OnPositionRequiresUpdate();
}
