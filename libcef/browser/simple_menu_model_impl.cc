// Copyright (c) 2021 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/simple_menu_model_impl.h"

#include <vector>

#include "libcef/browser/thread_util.h"
#include "libcef/common/task_runner_impl.h"

namespace {

// Value based on the documentation on SimpleMenuModel::GetIndexOfCommandId().
const int kInvalidIndex = -1;
const int kInvalidCommandId = -1;
const int kInvalidGroupId = -1;

cef_menu_item_type_t GetCefItemType(ui::MenuModel::ItemType type) {
  switch (type) {
    case ui::MenuModel::TYPE_COMMAND:
      return MENUITEMTYPE_COMMAND;
    case ui::MenuModel::TYPE_CHECK:
      return MENUITEMTYPE_CHECK;
    case ui::MenuModel::TYPE_RADIO:
      return MENUITEMTYPE_RADIO;
    case ui::MenuModel::TYPE_SEPARATOR:
      return MENUITEMTYPE_SEPARATOR;
    case ui::MenuModel::TYPE_SUBMENU:
      return MENUITEMTYPE_SUBMENU;
    default:
      return MENUITEMTYPE_NONE;
  }
}

}  // namespace

CefSimpleMenuModelImpl::CefSimpleMenuModelImpl(
    ui::SimpleMenuModel* model,
    ui::SimpleMenuModel::Delegate* delegate,
    StateDelegate* state_delegate,
    bool is_owned,
    bool is_submenu)
    : supported_thread_id_(base::PlatformThread::CurrentId()),
      model_(model),
      delegate_(delegate),
      state_delegate_(state_delegate),
      is_owned_(is_owned),
      is_submenu_(is_submenu) {
  DCHECK(model_);
  DCHECK(delegate_);
  DCHECK(state_delegate_);
}

CefSimpleMenuModelImpl::~CefSimpleMenuModelImpl() {
  // Detach() must be called before object destruction.
  DCHECK(!model_);
  DCHECK(submenumap_.empty());
}

void CefSimpleMenuModelImpl::Detach() {
  DCHECK(VerifyContext());

  if (!submenumap_.empty()) {
    auto it = submenumap_.begin();
    for (; it != submenumap_.end(); ++it) {
      it->second->Detach();
    }
    submenumap_.clear();
  }

  if (is_owned_) {
    delete model_;
  }
  model_ = nullptr;
}

bool CefSimpleMenuModelImpl::IsSubMenu() {
  if (!VerifyContext()) {
    return false;
  }
  return is_submenu_;
}

bool CefSimpleMenuModelImpl::Clear() {
  if (!VerifyContext()) {
    return false;
  }

  model_->Clear();
  return true;
}

size_t CefSimpleMenuModelImpl::GetCount() {
  if (!VerifyContext()) {
    return 0;
  }

  return model_->GetItemCount();
}

bool CefSimpleMenuModelImpl::AddSeparator() {
  if (!VerifyContext()) {
    return false;
  }

  model_->AddSeparator(ui::NORMAL_SEPARATOR);
  return true;
}

bool CefSimpleMenuModelImpl::AddItem(int command_id, const CefString& label) {
  if (!VerifyContext()) {
    return false;
  }

  model_->AddItem(command_id, label);
  return true;
}

bool CefSimpleMenuModelImpl::AddCheckItem(int command_id,
                                          const CefString& label) {
  if (!VerifyContext()) {
    return false;
  }

  model_->AddCheckItem(command_id, label);
  return true;
}

bool CefSimpleMenuModelImpl::AddRadioItem(int command_id,
                                          const CefString& label,
                                          int group_id) {
  if (!VerifyContext()) {
    return false;
  }

  model_->AddRadioItem(command_id, label, group_id);
  return true;
}

CefRefPtr<CefMenuModel> CefSimpleMenuModelImpl::AddSubMenu(
    int command_id,
    const CefString& label) {
  if (!VerifyContext()) {
    return nullptr;
  }

  auto new_menu = CreateNewSubMenu(nullptr);
  model_->AddSubMenu(command_id, label, new_menu->model());
  return new_menu;
}

bool CefSimpleMenuModelImpl::InsertSeparatorAt(size_t index) {
  if (!VerifyContext()) {
    return false;
  }

  model_->InsertSeparatorAt(index, ui::NORMAL_SEPARATOR);
  return true;
}

bool CefSimpleMenuModelImpl::InsertItemAt(size_t index,
                                          int command_id,
                                          const CefString& label) {
  if (!VerifyContext() || !ValidIndex(index)) {
    return false;
  }

  model_->InsertItemAt(index, command_id, label);
  return true;
}

bool CefSimpleMenuModelImpl::InsertCheckItemAt(size_t index,
                                               int command_id,
                                               const CefString& label) {
  if (!VerifyContext() || !ValidIndex(index)) {
    return false;
  }

  model_->InsertCheckItemAt(index, command_id, label);
  return true;
}

bool CefSimpleMenuModelImpl::InsertRadioItemAt(size_t index,
                                               int command_id,
                                               const CefString& label,
                                               int group_id) {
  if (!VerifyContext() || !ValidIndex(index)) {
    return false;
  }

  model_->InsertRadioItemAt(index, command_id, label, group_id);
  return true;
}

CefRefPtr<CefMenuModel> CefSimpleMenuModelImpl::InsertSubMenuAt(
    size_t index,
    int command_id,
    const CefString& label) {
  if (!VerifyContext() || !ValidIndex(index)) {
    return nullptr;
  }

  auto new_menu = CreateNewSubMenu(nullptr);
  model_->InsertSubMenuAt(index, command_id, label, new_menu->model());
  return new_menu;
}

bool CefSimpleMenuModelImpl::Remove(int command_id) {
  return RemoveAt(GetIndexOf(command_id));
}

bool CefSimpleMenuModelImpl::RemoveAt(size_t index) {
  if (!VerifyContext() || !ValidIndex(index)) {
    return false;
  }

  auto* sub_menu =
      static_cast<ui::SimpleMenuModel*>(model_->GetSubmenuModelAt(index));
  if (sub_menu) {
    auto it = submenumap_.find(sub_menu);
    if (it != submenumap_.end()) {
      it->second->Detach();
      submenumap_.erase(it);
    }
  }

  model_->RemoveItemAt(index);
  return true;
}

int CefSimpleMenuModelImpl::GetIndexOf(int command_id) {
  if (!VerifyContext()) {
    return kInvalidIndex;
  }

  auto index = model_->GetIndexOfCommandId(command_id);
  if (index.has_value()) {
    return static_cast<int>(*index);
  }
  return -1;
}

int CefSimpleMenuModelImpl::GetCommandIdAt(size_t index) {
  if (!VerifyContext() || !ValidIndex(index)) {
    return kInvalidCommandId;
  }

  return model_->GetCommandIdAt(index);
}

bool CefSimpleMenuModelImpl::SetCommandIdAt(size_t index, int command_id) {
  NOTIMPLEMENTED();
  return false;
}

CefString CefSimpleMenuModelImpl::GetLabel(int command_id) {
  return GetLabelAt(GetIndexOf(command_id));
}

CefString CefSimpleMenuModelImpl::GetLabelAt(size_t index) {
  if (!VerifyContext() || !ValidIndex(index)) {
    return CefString();
  }

  return model_->GetLabelAt(index);
}

bool CefSimpleMenuModelImpl::SetLabel(int command_id, const CefString& label) {
  return SetLabelAt(GetIndexOf(command_id), label);
}

bool CefSimpleMenuModelImpl::SetLabelAt(size_t index, const CefString& label) {
  if (!VerifyContext() || !ValidIndex(index)) {
    return false;
  }

  model_->SetLabel(index, label);
  return true;
}

CefSimpleMenuModelImpl::MenuItemType CefSimpleMenuModelImpl::GetType(
    int command_id) {
  return GetTypeAt(GetIndexOf(command_id));
}

CefSimpleMenuModelImpl::MenuItemType CefSimpleMenuModelImpl::GetTypeAt(
    size_t index) {
  if (!VerifyContext() || !ValidIndex(index)) {
    return MENUITEMTYPE_NONE;
  }

  return GetCefItemType(model_->GetTypeAt(index));
}

int CefSimpleMenuModelImpl::GetGroupId(int command_id) {
  return GetGroupIdAt(GetIndexOf(command_id));
}

int CefSimpleMenuModelImpl::GetGroupIdAt(size_t index) {
  if (!VerifyContext() || !ValidIndex(index)) {
    return kInvalidGroupId;
  }

  return model_->GetGroupIdAt(index);
}

bool CefSimpleMenuModelImpl::SetGroupId(int command_id, int group_id) {
  return SetGroupIdAt(GetIndexOf(command_id), group_id);
}

bool CefSimpleMenuModelImpl::SetGroupIdAt(size_t index, int group_id) {
  NOTIMPLEMENTED();
  return false;
}

CefRefPtr<CefMenuModel> CefSimpleMenuModelImpl::GetSubMenu(int command_id) {
  return GetSubMenuAt(GetIndexOf(command_id));
}

CefRefPtr<CefMenuModel> CefSimpleMenuModelImpl::GetSubMenuAt(size_t index) {
  if (!VerifyContext() || !ValidIndex(index)) {
    return nullptr;
  }

  auto* sub_model =
      static_cast<ui::SimpleMenuModel*>(model_->GetSubmenuModelAt(index));
  auto it = submenumap_.find(sub_model);
  if (it != submenumap_.end()) {
    return it->second;
  }
  return CreateNewSubMenu(sub_model);
}

bool CefSimpleMenuModelImpl::IsVisible(int command_id) {
  return IsVisibleAt(GetIndexOf(command_id));
}

bool CefSimpleMenuModelImpl::IsVisibleAt(size_t index) {
  if (!VerifyContext() || !ValidIndex(index)) {
    return false;
  }

  return model_->IsVisibleAt(index);
}

bool CefSimpleMenuModelImpl::SetVisible(int command_id, bool visible) {
  return SetVisibleAt(GetIndexOf(command_id), visible);
}

bool CefSimpleMenuModelImpl::SetVisibleAt(size_t index, bool visible) {
  if (!VerifyContext() || !ValidIndex(index)) {
    return false;
  }

  model_->SetVisibleAt(index, visible);
  return true;
}

bool CefSimpleMenuModelImpl::IsEnabled(int command_id) {
  return IsEnabledAt(GetIndexOf(command_id));
}

bool CefSimpleMenuModelImpl::IsEnabledAt(size_t index) {
  if (!VerifyContext() || !ValidIndex(index)) {
    return false;
  }

  return model_->IsEnabledAt(index);
}

bool CefSimpleMenuModelImpl::SetEnabled(int command_id, bool enabled) {
  return SetEnabledAt(GetIndexOf(command_id), enabled);
}

bool CefSimpleMenuModelImpl::SetEnabledAt(size_t index, bool enabled) {
  if (!VerifyContext() || !ValidIndex(index)) {
    return false;
  }

  model_->SetEnabledAt(index, enabled);
  return true;
}

bool CefSimpleMenuModelImpl::IsChecked(int command_id) {
  return IsCheckedAt(GetIndexOf(command_id));
}

bool CefSimpleMenuModelImpl::IsCheckedAt(size_t index) {
  if (!VerifyContext() || !ValidIndex(index)) {
    return false;
  }

  return model_->IsItemCheckedAt(index);
}

bool CefSimpleMenuModelImpl::SetChecked(int command_id, bool checked) {
  if (!VerifyContext() || command_id == kInvalidIndex) {
    return false;
  }

  state_delegate_->SetChecked(command_id, checked);
  return true;
}

bool CefSimpleMenuModelImpl::SetCheckedAt(size_t index, bool checked) {
  return SetChecked(GetCommandIdAt(index), checked);
}

bool CefSimpleMenuModelImpl::HasAccelerator(int command_id) {
  return HasAcceleratorAt(GetIndexOf(command_id));
}

bool CefSimpleMenuModelImpl::HasAcceleratorAt(size_t index) {
  if (!VerifyContext() || !ValidIndex(index)) {
    return false;
  }

  ui::Accelerator accelerator;
  return model_->GetAcceleratorAt(index, &accelerator);
}

bool CefSimpleMenuModelImpl::SetAccelerator(int command_id,
                                            int key_code,
                                            bool shift_pressed,
                                            bool ctrl_pressed,
                                            bool alt_pressed) {
  if (!VerifyContext() || command_id == kInvalidIndex) {
    return false;
  }

  int modifiers = 0;
  if (shift_pressed) {
    modifiers |= ui::EF_SHIFT_DOWN;
  }
  if (ctrl_pressed) {
    modifiers |= ui::EF_CONTROL_DOWN;
  }
  if (alt_pressed) {
    modifiers |= ui::EF_ALT_DOWN;
  }

  state_delegate_->SetAccelerator(
      command_id,
      ui::Accelerator(static_cast<ui::KeyboardCode>(key_code), modifiers));
  return true;
}

bool CefSimpleMenuModelImpl::SetAcceleratorAt(size_t index,
                                              int key_code,
                                              bool shift_pressed,
                                              bool ctrl_pressed,
                                              bool alt_pressed) {
  return SetAccelerator(GetCommandIdAt(index), key_code, shift_pressed,
                        ctrl_pressed, alt_pressed);
}

bool CefSimpleMenuModelImpl::RemoveAccelerator(int command_id) {
  if (!VerifyContext() || command_id == kInvalidIndex) {
    return false;
  }
  state_delegate_->SetAccelerator(command_id, absl::nullopt);
  return true;
}

bool CefSimpleMenuModelImpl::RemoveAcceleratorAt(size_t index) {
  return RemoveAccelerator(GetCommandIdAt(index));
}

bool CefSimpleMenuModelImpl::GetAccelerator(int command_id,
                                            int& key_code,
                                            bool& shift_pressed,
                                            bool& ctrl_pressed,
                                            bool& alt_pressed) {
  return GetAcceleratorAt(GetIndexOf(command_id), key_code, shift_pressed,
                          ctrl_pressed, alt_pressed);
}

bool CefSimpleMenuModelImpl::GetAcceleratorAt(size_t index,
                                              int& key_code,
                                              bool& shift_pressed,
                                              bool& ctrl_pressed,
                                              bool& alt_pressed) {
  if (!VerifyContext() || !ValidIndex(index)) {
    return false;
  }

  ui::Accelerator accel;
  if (model_->GetAcceleratorAt(index, &accel)) {
    key_code = accel.key_code();
    shift_pressed = accel.modifiers() & ui::EF_SHIFT_DOWN;
    ctrl_pressed = accel.modifiers() & ui::EF_CONTROL_DOWN;
    alt_pressed = accel.modifiers() & ui::EF_ALT_DOWN;
    return true;
  }
  return false;
}

bool CefSimpleMenuModelImpl::SetColor(int command_id,
                                      cef_menu_color_type_t color_type,
                                      cef_color_t color) {
  NOTIMPLEMENTED();
  return false;
}

bool CefSimpleMenuModelImpl::SetColorAt(int index,
                                        cef_menu_color_type_t color_type,
                                        cef_color_t color) {
  NOTIMPLEMENTED();
  return false;
}

bool CefSimpleMenuModelImpl::GetColor(int command_id,
                                      cef_menu_color_type_t color_type,
                                      cef_color_t& color) {
  NOTIMPLEMENTED();
  return false;
}

bool CefSimpleMenuModelImpl::GetColorAt(int index,
                                        cef_menu_color_type_t color_type,
                                        cef_color_t& color) {
  NOTIMPLEMENTED();
  return false;
}

bool CefSimpleMenuModelImpl::SetFontList(int command_id,
                                         const CefString& font_list) {
  NOTIMPLEMENTED();
  return false;
}

bool CefSimpleMenuModelImpl::SetFontListAt(int index,
                                           const CefString& font_list) {
  NOTIMPLEMENTED();
  return false;
}

bool CefSimpleMenuModelImpl::VerifyContext() {
  if (base::PlatformThread::CurrentId() != supported_thread_id_) {
    // This object should only be accessed from the thread that created it.
    DCHECK(false);
    return false;
  }

  if (!model_) {
    return false;
  }

  return true;
}

bool CefSimpleMenuModelImpl::ValidIndex(size_t index) {
  return index < model_->GetItemCount();
}

CefRefPtr<CefSimpleMenuModelImpl> CefSimpleMenuModelImpl::CreateNewSubMenu(
    ui::SimpleMenuModel* model) {
  bool is_owned = false;
  if (!model) {
    model = new ui::SimpleMenuModel(delegate_);
    is_owned = true;
  }

  CefRefPtr<CefSimpleMenuModelImpl> new_impl = new CefSimpleMenuModelImpl(
      model, delegate_, state_delegate_, is_owned, /*is_submenu=*/true);
  submenumap_.insert(std::make_pair(model, new_impl));
  return new_impl;
}
