// Copyright (c) 2019 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/alloy/dialogs/alloy_web_contents_dialog_helper.h"

#include "libcef/browser/browser_platform_delegate.h"

#include "base/notreached.h"
#include "chrome/browser/platform_util.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "ui/views/widget/widget.h"

AlloyWebContentsDialogHelper::AlloyWebContentsDialogHelper(
    content::WebContents* web_contents,
    CefBrowserPlatformDelegate* browser_delegate)
    : browser_delegate_(browser_delegate), weak_factory_(this) {
  web_modal::WebContentsModalDialogManager::CreateForWebContents(web_contents);
  web_modal::WebContentsModalDialogManager::FromWebContents(web_contents)
      ->SetDelegate(this);
}

base::RepeatingClosure
AlloyWebContentsDialogHelper::GetBoundsChangedCallback() {
  return base::BindRepeating(&AlloyWebContentsDialogHelper::OnBoundsChanged,
                             weak_factory_.GetWeakPtr());
}

bool AlloyWebContentsDialogHelper::IsWebContentsVisible(
    content::WebContents* web_contents) {
  if (browser_delegate_->IsWindowless()) {
    return !browser_delegate_->IsHidden();
  } else if (auto native_view = web_contents->GetNativeView()) {
    return platform_util::IsVisible(native_view);
  }
  DCHECK(false);
  return false;
}

web_modal::WebContentsModalDialogHost*
AlloyWebContentsDialogHelper::GetWebContentsModalDialogHost() {
  return this;
}

gfx::NativeView AlloyWebContentsDialogHelper::GetHostView() const {
  // Windowless rendering uses GetAcceleratedWidget() instead.
  if (browser_delegate_->IsWindowless()) {
    return gfx::NativeView();
  }

  if (auto widget = browser_delegate_->GetWindowWidget()) {
    return widget->GetNativeView();
  }
  DCHECK(false);
  return gfx::NativeView();
}

gfx::AcceleratedWidget AlloyWebContentsDialogHelper::GetAcceleratedWidget()
    const {
#if defined(USE_AURA)
  // Windowed rendering uses GetHostView() instead.
  if (!browser_delegate_->IsWindowless()) {
    return gfx::kNullAcceleratedWidget;
  }

  if (auto parent_widget = browser_delegate_->GetHostWindowHandle()) {
    return parent_widget;
  }
#endif  // defined(USE_AURA)
  DCHECK(false);
  return gfx::kNullAcceleratedWidget;
}

gfx::Point AlloyWebContentsDialogHelper::GetDialogPosition(
    const gfx::Size& size) {
  return browser_delegate_->GetDialogPosition(size);
}

gfx::Size AlloyWebContentsDialogHelper::GetMaximumDialogSize() {
  return browser_delegate_->GetMaximumDialogSize();
}

void AlloyWebContentsDialogHelper::AddObserver(
    web_modal::ModalDialogHostObserver* observer) {
  if (observer && !observer_list_.HasObserver(observer)) {
    observer_list_.AddObserver(observer);
  }
}

void AlloyWebContentsDialogHelper::RemoveObserver(
    web_modal::ModalDialogHostObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void AlloyWebContentsDialogHelper::OnBoundsChanged() {
  for (auto& observer : observer_list_) {
    observer.OnPositionRequiresUpdate();
  }
}
