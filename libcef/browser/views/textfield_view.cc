// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "cef/libcef/browser/views/textfield_view.h"

#include "cef/libcef/browser/browser_event_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"

CefTextfieldView::CefTextfieldView(CefTextfieldDelegate* cef_delegate)
    : ParentClass(cef_delegate) {
  set_controller(this);
}

void CefTextfieldView::Initialize() {
  ParentClass::Initialize();

  // Use our defaults instead of the Views framework defaults.
  SetFontList(gfx::FontList(view_util::kDefaultFontList));
}

bool CefTextfieldView::HandleKeyEvent(views::Textfield* sender,
                                      const ui::KeyEvent& key_event) {
  DCHECK_EQ(sender, this);

  if (!cef_delegate()) {
    return false;
  }

  CefKeyEvent cef_key_event;
  if (!GetCefKeyEvent(key_event, cef_key_event)) {
    return false;
  }

  return cef_delegate()->OnKeyEvent(GetCefTextfield(), cef_key_event);
}

void CefTextfieldView::OnAfterUserAction(views::Textfield* sender) {
  DCHECK_EQ(sender, this);
  if (cef_delegate()) {
    cef_delegate()->OnAfterUserAction(GetCefTextfield());
  }
}

BEGIN_METADATA(CefTextfieldView)
END_METADATA
