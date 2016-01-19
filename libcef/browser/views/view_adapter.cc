// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/views/view_adapter.h"

#include "libcef/browser/views/basic_label_button_impl.h"
#include "libcef/browser/views/basic_panel_impl.h"
#include "libcef/browser/views/browser_view_impl.h"
#include "libcef/browser/views/menu_button_impl.h"
#include "libcef/browser/views/scroll_view_impl.h"
#include "libcef/browser/views/textfield_impl.h"
#include "libcef/browser/views/view_util.h"
#include "libcef/browser/views/window_impl.h"

// static
CefViewAdapter* CefViewAdapter::GetFor(CefRefPtr<CefView> view) {
  CefViewAdapter* adapter = nullptr;
  if (view->AsBrowserView()) {
    adapter = static_cast<CefBrowserViewImpl*>(view->AsBrowserView().get());
  } else if (view->AsButton()) {
    CefRefPtr<CefButton> button = view->AsButton();
    if (button->AsLabelButton()) {
      CefRefPtr<CefLabelButton> label_button = button->AsLabelButton();
      if (label_button->AsMenuButton()) {
        adapter = static_cast<CefMenuButtonImpl*>(
            label_button->AsMenuButton().get());
      } else {
        adapter = static_cast<CefBasicLabelButtonImpl*>(label_button.get());
      }
    }
  } else if (view->AsPanel()) {
    CefRefPtr<CefPanel> panel = view->AsPanel();
    if (panel->AsWindow()) {
      adapter = static_cast<CefWindowImpl*>(panel->AsWindow().get());
    } else {
      adapter = static_cast<CefBasicPanelImpl*>(panel.get());
    }
  } else if (view->AsScrollView()) {
    adapter = static_cast<CefScrollViewImpl*>(view->AsScrollView().get());
  } else if (view->AsTextfield()) {
    adapter = static_cast<CefTextfieldImpl*>(view->AsTextfield().get());
  }

  DCHECK(adapter);
  return adapter;
}

// static
CefViewAdapter* CefViewAdapter::GetFor(views::View* view) {
  CefRefPtr<CefView> cef_view = view_util::GetFor(view, false);
  if (cef_view)
    return GetFor(cef_view);
  return nullptr;
}
