// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/views/layout_util.h"

#include <utility>

#include "libcef/browser/views/layout_adapter.h"

#include "ui/views/view.h"

namespace layout_util {

namespace {

// Manages the association between views::View and CefLayout instances.
class UserData : public base::SupportsUserData::Data {
 public:
  static CefRefPtr<CefLayout> GetFor(const views::View* view) {
    UserData* data = static_cast<UserData*>(view->GetUserData(UserDataKey()));
    if (data)
      return data->layout_;
    return nullptr;
  }

  // Assign ownership of the underlying views::LayoutManager to |owner_view|.
  // The views::View that owns the views::LayoutManager will gain a ref-counted
  // reference to the CefLayout and the CefLayout will keep an unowned reference
  // to the views::LayoutManager. Destruction of the views::View will release
  // the reference to the CefLayout.
  static void Assign(CefRefPtr<CefLayout> cef_layout,
                     views::View* owner_view) {
    DCHECK(owner_view);
    DCHECK(cef_layout->IsValid());

    views::LayoutManager* layout = CefLayoutAdapter::GetFor(cef_layout)->Get();
    DCHECK(layout);

    // The CefLayout previously associated with |owner_view|, if any, will be
    // destroyed by this call.
    owner_view->SetUserData(UserDataKey(), new UserData(cef_layout));
  }

 private:
  explicit UserData(CefRefPtr<CefLayout> cef_layout)
      : layout_(cef_layout) {
    DCHECK(layout_);
  }

  ~UserData() override {
    CefLayoutAdapter::GetFor(layout_)->Detach();
  }

  static void* UserDataKey() {
    // We just need a unique constant. Use the address of a static that
    // COMDAT folding won't touch in an optimizing linker.
    static int data_key = 0;
    return reinterpret_cast<void*>(&data_key);
  }

  CefRefPtr<CefLayout> layout_;
};

}  // namespace

CefRefPtr<CefLayout> GetFor(const views::View* view) {
  return UserData::GetFor(view);
}

void Assign(CefRefPtr<CefLayout> layout, views::View* owner_view) {
  return UserData::Assign(layout, owner_view);
}

}  // namespace layout_util
