// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/menu_model_impl.h"

#include <vector>

#include "libcef/browser/thread_util.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "content/public/common/menu_item.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/gfx/geometry/point.h"

namespace {

const int kSeparatorId = -1;
const int kInvalidGroupId = -1;
const int kInvalidCommandId = -1;
const int kDefaultIndex = -1;
const int kInvalidIndex = -2;

// A simple MenuModel implementation that delegates to CefMenuModelImpl.
class CefSimpleMenuModel : public ui::MenuModel {
 public:
  // The Delegate can be NULL, though if it is items can't be checked or
  // disabled.
  explicit CefSimpleMenuModel(CefMenuModelImpl* impl)
      : impl_(impl),
        menu_model_delegate_(NULL) {
  }

  // MenuModel methods.
  bool HasIcons() const override {
    return false;
  }

  int GetItemCount() const override {
    return impl_->GetCount();
  }

  ItemType GetTypeAt(int index) const override {
    switch (impl_->GetTypeAt(index)) {
    case MENUITEMTYPE_COMMAND:
      return TYPE_COMMAND;
    case MENUITEMTYPE_CHECK:
      return TYPE_CHECK;
    case MENUITEMTYPE_RADIO:
      return TYPE_RADIO;
    case MENUITEMTYPE_SEPARATOR:
      return TYPE_SEPARATOR;
    case MENUITEMTYPE_SUBMENU:
      return TYPE_SUBMENU;
    default:
      NOTREACHED();
      return TYPE_COMMAND;
    }
  }

  ui::MenuSeparatorType GetSeparatorTypeAt(int index) const override {
    return ui::NORMAL_SEPARATOR;
  }

  int GetCommandIdAt(int index) const override {
    return impl_->GetCommandIdAt(index);
  }

  base::string16 GetLabelAt(int index) const override {
    return impl_->GetFormattedLabelAt(index);
  }

  bool IsItemDynamicAt(int index) const override {
    return false;
  }

  const gfx::FontList* GetLabelFontListAt(int index) const override {
    return impl_->GetLabelFontListAt(index);
  }

  bool GetAcceleratorAt(int index,
                        ui::Accelerator* accelerator) const override {
    int key_code = 0;
    bool shift_pressed = false;
    bool ctrl_pressed = false;
    bool alt_pressed = false;
    if (impl_->GetAcceleratorAt(index, key_code, shift_pressed, ctrl_pressed,
                                alt_pressed)) {
      int modifiers = 0;
      if (shift_pressed)
        modifiers |= ui::EF_SHIFT_DOWN;
      if (ctrl_pressed)
        modifiers |= ui::EF_CONTROL_DOWN;
      if (alt_pressed)
        modifiers |= ui::EF_ALT_DOWN;

      *accelerator = ui::Accelerator(static_cast<ui::KeyboardCode>(key_code),
                                     modifiers);
      return true;
    }
    return false;
  }

  bool IsItemCheckedAt(int index) const override {
    return impl_->IsCheckedAt(index);
  }

  int GetGroupIdAt(int index) const override {
    return impl_->GetGroupIdAt(index);
  }

  bool GetIconAt(int index, gfx::Image* icon) override {
    return false;
  }

  ui::ButtonMenuItemModel* GetButtonMenuItemAt(
      int index) const override {
    return NULL;
  }

  bool IsEnabledAt(int index) const override {
    return impl_->IsEnabledAt(index);
  }

  bool IsVisibleAt(int index) const override {
    return impl_->IsVisibleAt(index);
  }

  void HighlightChangedTo(int index) override {
  }

  void ActivatedAt(int index) override {
    ActivatedAt(index, 0);
  }

  void ActivatedAt(int index, int event_flags) override {
    impl_->ActivatedAt(index, static_cast<cef_event_flags_t>(event_flags));
  }

  MenuModel* GetSubmenuModelAt(int index) const override {
    CefRefPtr<CefMenuModel> submenu = impl_->GetSubMenuAt(index);
    if (submenu.get())
      return static_cast<CefMenuModelImpl*>(submenu.get())->model();
    return NULL;
  }

  void MouseOutsideMenu(const gfx::Point& screen_point) override {
    impl_->MouseOutsideMenu(screen_point);
  }

  void UnhandledOpenSubmenu(bool is_rtl) override {
    impl_->UnhandledOpenSubmenu(is_rtl);
  }

  void UnhandledCloseSubmenu(bool is_rtl) override {
    impl_->UnhandledCloseSubmenu(is_rtl);
  }

  bool GetTextColor(int index,
                    bool is_minor,
                    bool is_hovered,
                    SkColor* override_color) const override {
    return impl_->GetTextColor(index, is_minor, is_hovered, override_color);
  }

  bool GetBackgroundColor(int index,
                          bool is_hovered,
                          SkColor* override_color) const override {
    return impl_->GetBackgroundColor(index, is_hovered, override_color);
  }

  void MenuWillShow() override {
    impl_->MenuWillShow();
  }

  void MenuWillClose() override {
    impl_->MenuWillClose();
  }

  void SetMenuModelDelegate(
      ui::MenuModelDelegate* menu_model_delegate) override {
    menu_model_delegate_ = menu_model_delegate;
  }

  ui::MenuModelDelegate* GetMenuModelDelegate() const override {
    return menu_model_delegate_;
  }

 private:
  CefMenuModelImpl* impl_;
  ui::MenuModelDelegate* menu_model_delegate_;

  DISALLOW_COPY_AND_ASSIGN(CefSimpleMenuModel);
};

cef_menu_color_type_t GetMenuColorType(bool is_text,
                                       bool is_accelerator,
                                       bool is_hovered) {
  if (is_text) {
    if (is_accelerator) {
      return is_hovered ? CEF_MENU_COLOR_TEXT_ACCELERATOR_HOVERED :
                          CEF_MENU_COLOR_TEXT_ACCELERATOR;
    }
    return is_hovered ? CEF_MENU_COLOR_TEXT_HOVERED : CEF_MENU_COLOR_TEXT;
  }
  
  DCHECK(!is_accelerator);
  return is_hovered ? CEF_MENU_COLOR_BACKGROUND_HOVERED :
                      CEF_MENU_COLOR_BACKGROUND;
}

}  // namespace

// static
CefRefPtr<CefMenuModel> CefMenuModel::CreateMenuModel(
    CefRefPtr<CefMenuModelDelegate> delegate) {
  CEF_REQUIRE_UIT_RETURN(nullptr);
  DCHECK(delegate);
  if (!delegate)
    return nullptr;

  CefRefPtr<CefMenuModelImpl> menu_model =
      new CefMenuModelImpl(nullptr, delegate, false);
  return menu_model;
}

struct CefMenuModelImpl::Item {
  Item(cef_menu_item_type_t type,
       int command_id,
       const CefString& label,
       int group_id)
      : type_(type),
        command_id_(command_id),
        label_(label),
        group_id_(group_id) {
  }

  // Basic information.
  cef_menu_item_type_t type_;
  int command_id_;
  CefString label_;
  int group_id_;
  CefRefPtr<CefMenuModelImpl> submenu_;

  // State information.
  bool enabled_ = true;
  bool visible_ = true;
  bool checked_ = false;

  // Accelerator information.
  bool has_accelerator_ = false;
  int key_code_ = 0;
  bool shift_pressed_ = false;
  bool ctrl_pressed_ = false;
  bool alt_pressed_ = false;

  cef_color_t colors_[CEF_MENU_COLOR_COUNT] = {0};
  gfx::FontList font_list_;
  bool has_font_list_ = false;
};


CefMenuModelImpl::CefMenuModelImpl(
    Delegate* delegate,
    CefRefPtr<CefMenuModelDelegate> menu_model_delegate,
    bool is_submenu)
    : supported_thread_id_(base::PlatformThread::CurrentId()),
      delegate_(delegate),
      menu_model_delegate_(menu_model_delegate),
      is_submenu_(is_submenu) {
  DCHECK(delegate_ || menu_model_delegate_);
  model_.reset(new CefSimpleMenuModel(this));
}

CefMenuModelImpl::~CefMenuModelImpl() {
}

bool CefMenuModelImpl::IsSubMenu() {
  if (!VerifyContext())
    return false;
  return is_submenu_;
}

bool CefMenuModelImpl::Clear() {
  if (!VerifyContext())
    return false;

  items_.clear();
  return true;
}

int CefMenuModelImpl::GetCount() {
  if (!VerifyContext())
    return 0;

  return static_cast<int>(items_.size());
}

bool CefMenuModelImpl::AddSeparator() {
  if (!VerifyContext())
    return false;

  AppendItem(Item(MENUITEMTYPE_SEPARATOR, kSeparatorId, CefString(),
                  kInvalidGroupId));
  return true;
}

bool CefMenuModelImpl::AddItem(int command_id, const CefString& label) {
  if (!VerifyContext())
    return false;

  AppendItem(Item(MENUITEMTYPE_COMMAND, command_id, label,
                  kInvalidGroupId));
  return true;
}

bool CefMenuModelImpl::AddCheckItem(int command_id, const CefString& label) {
  if (!VerifyContext())
    return false;

  AppendItem(Item(MENUITEMTYPE_CHECK, command_id, label, kInvalidGroupId));
  return true;
}

bool CefMenuModelImpl::AddRadioItem(int command_id, const CefString& label,
                                    int group_id) {
  if (!VerifyContext())
    return false;

  AppendItem(Item(MENUITEMTYPE_RADIO, command_id, label, group_id));
  return true;
}

CefRefPtr<CefMenuModel> CefMenuModelImpl::AddSubMenu(int command_id,
                                                     const CefString& label) {
  if (!VerifyContext())
    return NULL;

  Item item(MENUITEMTYPE_SUBMENU, command_id, label, kInvalidGroupId);
  item.submenu_ = new CefMenuModelImpl(delegate_, menu_model_delegate_, true);
  AppendItem(item);
  return item.submenu_.get();
}

bool CefMenuModelImpl::InsertSeparatorAt(int index) {
  if (!VerifyContext())
    return false;

  InsertItemAt(Item(MENUITEMTYPE_SEPARATOR, kSeparatorId, CefString(),
                    kInvalidGroupId),
               index);
  return true;
}

bool CefMenuModelImpl::InsertItemAt(int index, int command_id,
                                    const CefString& label) {
  if (!VerifyContext())
    return false;

  InsertItemAt(Item(MENUITEMTYPE_COMMAND, command_id, label, kInvalidGroupId),
               index);
  return true;
}

bool CefMenuModelImpl::InsertCheckItemAt(int index, int command_id,
    const CefString& label) {
  if (!VerifyContext())
    return false;

  InsertItemAt(Item(MENUITEMTYPE_CHECK, command_id, label, kInvalidGroupId),
               index);
  return true;
}

bool CefMenuModelImpl::InsertRadioItemAt(int index, int command_id,
                                         const CefString& label, int group_id) {
  if (!VerifyContext())
    return false;

  InsertItemAt(Item(MENUITEMTYPE_RADIO, command_id, label, group_id), index);
  return true;
}

CefRefPtr<CefMenuModel> CefMenuModelImpl::InsertSubMenuAt(
    int index, int command_id, const CefString& label) {
  if (!VerifyContext())
    return NULL;

  Item item(MENUITEMTYPE_SUBMENU, command_id, label, kInvalidGroupId);
  item.submenu_ = new CefMenuModelImpl(delegate_, menu_model_delegate_, true);
  InsertItemAt(item, index);
  return item.submenu_.get();
}

bool CefMenuModelImpl::Remove(int command_id) {
  return RemoveAt(GetIndexOf(command_id));
}

bool CefMenuModelImpl::RemoveAt(int index) {
  if (!VerifyContext())
    return false;

  if (index >= 0 && index < static_cast<int>(items_.size())) {
    items_.erase(items_.begin()+index);
    return true;
  }
  return false;
}

int CefMenuModelImpl::GetIndexOf(int command_id) {
  if (!VerifyContext())
    return kInvalidIndex;

  for (ItemVector::iterator i = items_.begin(); i != items_.end(); ++i) {
    if ((*i).command_id_ == command_id) {
      return static_cast<int>(std::distance(items_.begin(), i));
    }
  }
  return kInvalidIndex;
}

int CefMenuModelImpl::GetCommandIdAt(int index) {
  if (!VerifyContext())
    return kInvalidCommandId;

  if (index >= 0 && index < static_cast<int>(items_.size()))
    return items_[index].command_id_;
  return kInvalidCommandId;
}

bool CefMenuModelImpl::SetCommandIdAt(int index, int command_id) {
  if (!VerifyContext())
    return false;

  if (index >= 0 && index < static_cast<int>(items_.size())) {
    items_[index].command_id_ = command_id;
    return true;
  }
  return false;
}

CefString CefMenuModelImpl::GetLabel(int command_id) {
  return GetLabelAt(GetIndexOf(command_id));
}

CefString CefMenuModelImpl::GetLabelAt(int index) {
  if (!VerifyContext())
    return CefString();

  if (index >= 0 && index < static_cast<int>(items_.size()))
    return items_[index].label_;
  return CefString();
}

bool CefMenuModelImpl::SetLabel(int command_id, const CefString& label) {
  return SetLabelAt(GetIndexOf(command_id), label);
}

bool CefMenuModelImpl::SetLabelAt(int index, const CefString& label) {
  if (!VerifyContext())
    return false;

  if (index >= 0 && index < static_cast<int>(items_.size())) {
    items_[index].label_ = label;
    return true;
  }
  return false;
}

CefMenuModelImpl::MenuItemType CefMenuModelImpl::GetType(int command_id) {
  return GetTypeAt(GetIndexOf(command_id));
}

CefMenuModelImpl::MenuItemType CefMenuModelImpl::GetTypeAt(int index) {
  if (!VerifyContext())
    return MENUITEMTYPE_NONE;

  if (index >= 0 && index < static_cast<int>(items_.size()))
    return items_[index].type_;
  return MENUITEMTYPE_NONE;
}

int CefMenuModelImpl::GetGroupId(int command_id) {
  return GetGroupIdAt(GetIndexOf(command_id));
}

int CefMenuModelImpl::GetGroupIdAt(int index) {
  if (!VerifyContext())
    return kInvalidGroupId;

  if (index >= 0 && index < static_cast<int>(items_.size()))
    return items_[index].group_id_;
  return kInvalidGroupId;
}

bool CefMenuModelImpl::SetGroupId(int command_id, int group_id) {
  return SetGroupIdAt(GetIndexOf(command_id), group_id);
}

bool CefMenuModelImpl::SetGroupIdAt(int index, int group_id) {
  if (!VerifyContext())
    return false;

  if (index >= 0 && index < static_cast<int>(items_.size())) {
    items_[index].group_id_ = group_id;
    return true;
  }
  return false;
}

CefRefPtr<CefMenuModel> CefMenuModelImpl::GetSubMenu(int command_id) {
  return GetSubMenuAt(GetIndexOf(command_id));
}

CefRefPtr<CefMenuModel> CefMenuModelImpl::GetSubMenuAt(int index) {
  if (!VerifyContext())
    return NULL;

  if (index >= 0 && index < static_cast<int>(items_.size()))
    return items_[index].submenu_.get();
  return NULL;
}

bool CefMenuModelImpl::IsVisible(int command_id) {
  return IsVisibleAt(GetIndexOf(command_id));
}

bool CefMenuModelImpl::IsVisibleAt(int index) {
  if (!VerifyContext())
    return false;

  if (index >= 0 && index < static_cast<int>(items_.size()))
    return items_[index].visible_;
  return false;
}

bool CefMenuModelImpl::SetVisible(int command_id, bool visible) {
  return SetVisibleAt(GetIndexOf(command_id), visible);
}

bool CefMenuModelImpl::SetVisibleAt(int index, bool visible) {
  if (!VerifyContext())
    return false;

  if (index >= 0 && index < static_cast<int>(items_.size())) {
    items_[index].visible_ = visible;
    return true;
  }
  return false;
}

bool CefMenuModelImpl::IsEnabled(int command_id) {
  return IsEnabledAt(GetIndexOf(command_id));
}

bool CefMenuModelImpl::IsEnabledAt(int index) {
  if (!VerifyContext())
    return false;

  if (index >= 0 && index < static_cast<int>(items_.size()))
    return items_[index].enabled_;
  return false;
}

bool CefMenuModelImpl::SetEnabled(int command_id, bool enabled) {
  return SetEnabledAt(GetIndexOf(command_id), enabled);
}

bool CefMenuModelImpl::SetEnabledAt(int index, bool enabled) {
  if (!VerifyContext())
    return false;

  if (index >= 0 && index < static_cast<int>(items_.size())) {
    items_[index].enabled_ = enabled;
    return true;
  }
  return false;
}

bool CefMenuModelImpl::IsChecked(int command_id) {
  return IsCheckedAt(GetIndexOf(command_id));
}

bool CefMenuModelImpl::IsCheckedAt(int index) {
  if (!VerifyContext())
    return false;

  if (index >= 0 && index < static_cast<int>(items_.size()))
    return items_[index].checked_;
  return false;
}

bool CefMenuModelImpl::SetChecked(int command_id, bool checked) {
  return SetCheckedAt(GetIndexOf(command_id), checked);
}

bool CefMenuModelImpl::SetCheckedAt(int index, bool checked) {
  if (!VerifyContext())
    return false;

  if (index >= 0 && index < static_cast<int>(items_.size())) {
    items_[index].checked_ = checked;
    return true;
  }
  return false;
}

bool CefMenuModelImpl::HasAccelerator(int command_id) {
  return HasAcceleratorAt(GetIndexOf(command_id));
}

bool CefMenuModelImpl::HasAcceleratorAt(int index) {
  if (!VerifyContext())
    return false;

  if (index >= 0 && index < static_cast<int>(items_.size()))
    return items_[index].has_accelerator_;
  return false;
}

bool CefMenuModelImpl::SetAccelerator(int command_id, int key_code,
                                      bool shift_pressed, bool ctrl_pressed,
                                      bool alt_pressed) {
  return SetAcceleratorAt(GetIndexOf(command_id), key_code, shift_pressed,
                          ctrl_pressed, alt_pressed);
}

bool CefMenuModelImpl::SetAcceleratorAt(int index, int key_code,
                                        bool shift_pressed, bool ctrl_pressed,
                                        bool alt_pressed) {
  if (!VerifyContext())
    return false;

  if (index >= 0 && index < static_cast<int>(items_.size())) {
    Item& item = items_[index];
    item.has_accelerator_ = true;
    item.key_code_ = key_code;
    item.shift_pressed_ = shift_pressed;
    item.ctrl_pressed_ = ctrl_pressed;
    item.alt_pressed_ = alt_pressed;
    return true;
  }
  return false;
}

bool CefMenuModelImpl::RemoveAccelerator(int command_id) {
  return RemoveAcceleratorAt(GetIndexOf(command_id));
}

bool CefMenuModelImpl::RemoveAcceleratorAt(int index) {
  if (!VerifyContext())
    return false;

  if (index >= 0 && index < static_cast<int>(items_.size())) {
    Item& item = items_[index];
    if (item.has_accelerator_) {
      item.has_accelerator_ = false;
      item.key_code_ = 0;
      item.shift_pressed_ = false;
      item.ctrl_pressed_ = false;
      item.alt_pressed_ = false;
    }
    return true;
  }
  return false;
}

bool CefMenuModelImpl::GetAccelerator(int command_id, int& key_code,
                                      bool& shift_pressed, bool& ctrl_pressed,
                                      bool& alt_pressed) {
  return GetAcceleratorAt(GetIndexOf(command_id), key_code, shift_pressed,
                          ctrl_pressed, alt_pressed);
}

bool CefMenuModelImpl::GetAcceleratorAt(int index, int& key_code,
                                        bool& shift_pressed, bool& ctrl_pressed,
                                        bool& alt_pressed) {
  if (!VerifyContext())
    return false;

  if (index >= 0 && index < static_cast<int>(items_.size())) {
    const Item& item = items_[index];
    if (item.has_accelerator_) {
      key_code = item.key_code_;
      shift_pressed = item.shift_pressed_;
      ctrl_pressed = item.ctrl_pressed_;
      alt_pressed = item.alt_pressed_;
      return true;
    }
  }
  return false;
}

bool CefMenuModelImpl::SetColor(int command_id,
                                cef_menu_color_type_t color_type,
                                cef_color_t color) {
  return SetColorAt(GetIndexOf(command_id), color_type, color);
}

bool CefMenuModelImpl::SetColorAt(int index,
                                  cef_menu_color_type_t color_type,
                                  cef_color_t color) {
  if (!VerifyContext())
    return false;

  if (color_type < 0 || color_type >= CEF_MENU_COLOR_COUNT)
    return false;

  if (index == kDefaultIndex) {
    default_colors_[color_type] = color;
    return true;
  }

  if (index >= 0 && index < static_cast<int>(items_.size())) {
    Item& item = items_[index];
    item.colors_[color_type] = color;
    return true;
  }

  return false;
}

bool CefMenuModelImpl::GetColor(int command_id,
                                cef_menu_color_type_t color_type,
                                cef_color_t& color) {
  return GetColorAt(GetIndexOf(command_id), color_type, color);
}

bool CefMenuModelImpl::GetColorAt(int index,
                                  cef_menu_color_type_t color_type,
                                  cef_color_t& color) {
  if (!VerifyContext())
    return false;

  if (color_type < 0 || color_type >= CEF_MENU_COLOR_COUNT)
    return false;

  if (index == kDefaultIndex) {
    color = default_colors_[color_type];
    return true;
  }

  if (index >= 0 && index < static_cast<int>(items_.size())) {
    Item& item = items_[index];
    color = item.colors_[color_type];
    return true;
  }

  return false;
}

bool CefMenuModelImpl::SetFontList(int command_id, const CefString& font_list) {
  return SetFontListAt(GetIndexOf(command_id), font_list);
}

bool CefMenuModelImpl::SetFontListAt(int index, const CefString& font_list) {
  if (!VerifyContext())
    return false;

  if (index == kDefaultIndex) {
    if (font_list.empty()) {
      has_default_font_list_ = false;
    } else {
      default_font_list_ = gfx::FontList(font_list);
      has_default_font_list_ = true;
    }
    return true;
  }

  if (index >= 0 && index < static_cast<int>(items_.size())) {
    Item& item = items_[index];
    if (font_list.empty()) {
      item.has_font_list_ = false;
    } else {
      item.font_list_ = gfx::FontList(font_list);
      item.has_font_list_ = true;
    }
    return true;
  }
  return false;
}

void CefMenuModelImpl::ActivatedAt(int index, cef_event_flags_t event_flags) {
  if (!VerifyContext())
    return;
  
  const int command_id = GetCommandIdAt(index);
  if (delegate_)
    delegate_->ExecuteCommand(this, command_id, event_flags);
  if (menu_model_delegate_)
    menu_model_delegate_->ExecuteCommand(this, command_id, event_flags);
}

void CefMenuModelImpl::MouseOutsideMenu(const gfx::Point& screen_point) {
  if (!VerifyContext())
    return;

  // Allow the callstack to unwind before notifying the delegate since it may
  // result in the menu being destroyed.
  base::MessageLoop::current()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&CefMenuModelImpl::OnMouseOutsideMenu, this, screen_point));
}

void CefMenuModelImpl::UnhandledOpenSubmenu(bool is_rtl) {
  if (!VerifyContext())
    return;

  // Allow the callstack to unwind before notifying the delegate since it may
  // result in the menu being destroyed.
  base::MessageLoop::current()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&CefMenuModelImpl::OnUnhandledOpenSubmenu, this, is_rtl));
}

void CefMenuModelImpl::UnhandledCloseSubmenu(bool is_rtl) {
  if (!VerifyContext())
    return;

  // Allow the callstack to unwind before notifying the delegate since it may
  // result in the menu being destroyed.
  base::MessageLoop::current()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&CefMenuModelImpl::OnUnhandledCloseSubmenu, this, is_rtl));
}

bool CefMenuModelImpl::GetTextColor(int index,
                                    bool is_accelerator,
                                    bool is_hovered,
                                    SkColor* override_color) const {
  if (index >= 0 && index < static_cast<int>(items_.size())) {
    const Item& item = items_[index];
    if (!item.enabled_) {
      // Use accelerator color for disabled item text.
      is_accelerator = true;
    }

    const cef_menu_color_type_t color_type =
        GetMenuColorType(true, is_accelerator, is_hovered);
    if (item.colors_[color_type] != 0) {
      *override_color = item.colors_[color_type];
      return true;
    }
  }

  const cef_menu_color_type_t color_type =
      GetMenuColorType(true, is_accelerator, is_hovered);
  if (default_colors_[color_type] != 0) {
    *override_color = default_colors_[color_type];
    return true;
  }

  return false;
}

bool CefMenuModelImpl::GetBackgroundColor(int index,
                                          bool is_hovered,
                                          SkColor* override_color) const {
  const cef_menu_color_type_t color_type =
      GetMenuColorType(false, false, is_hovered);

  if (index >= 0 && index < static_cast<int>(items_.size())) {
    const Item& item = items_[index];
    if (item.colors_[color_type] != 0) {
      *override_color = item.colors_[color_type];
      return true;
    }
  }

  if (default_colors_[color_type] != 0) {
    *override_color = default_colors_[color_type];
    return true;
  }

  return false;
}

void CefMenuModelImpl::MenuWillShow() {
  if (!VerifyContext())
    return;
  
  if (delegate_)
    delegate_->MenuWillShow(this);
  if (menu_model_delegate_)
    menu_model_delegate_->MenuWillShow(this);
}

void CefMenuModelImpl::MenuWillClose() {
  if (!VerifyContext())
    return;

  if (!auto_notify_menu_closed_)
    return;

  // Due to how menus work on the different platforms, ActivatedAt will be
  // called after this.  It's more convenient for the delegate to be called
  // afterwards, though, so post a task.
  base::MessageLoop::current()->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&CefMenuModelImpl::OnMenuClosed, this));
}

base::string16 CefMenuModelImpl::GetFormattedLabelAt(int index) {
  base::string16 label = GetLabelAt(index).ToString16();
  if (delegate_)
    delegate_->FormatLabel(this, label);
  if (menu_model_delegate_) {
    CefString new_label = label;
    if (menu_model_delegate_->FormatLabel(this, new_label))
      label = new_label;
  }
  return label;
}

const gfx::FontList* CefMenuModelImpl::GetLabelFontListAt(int index) const {
  if (index >= 0 && index < static_cast<int>(items_.size())) {
    const Item& item = items_[index];
    if (item.has_font_list_)
      return &item.font_list_;
  }

  if (has_default_font_list_)
    return &default_font_list_;
  return nullptr;
}

bool CefMenuModelImpl::VerifyRefCount() {
  if (!VerifyContext())
    return false;

  if (!HasOneRef())
    return false;

  for (ItemVector::iterator i = items_.begin(); i != items_.end(); ++i) {
    if ((*i).submenu_.get()) {
      if (!(*i).submenu_->VerifyRefCount())
        return false;
    }
  }

  return true;
}

void CefMenuModelImpl::AddMenuItem(const content::MenuItem& menu_item) {
  const int command_id = static_cast<int>(menu_item.action);

  switch (menu_item.type) {
    case content::MenuItem::OPTION:
      AddItem(command_id, menu_item.label);
      break;
    case content::MenuItem::CHECKABLE_OPTION:
      AddCheckItem(command_id, menu_item.label);
      break;
    case content::MenuItem::GROUP:
      AddRadioItem(command_id, menu_item.label, 0);
      break;
    case content::MenuItem::SEPARATOR:
      AddSeparator();
      break;
    case content::MenuItem::SUBMENU: {
      CefRefPtr<CefMenuModelImpl> sub_menu = static_cast<CefMenuModelImpl*>(
          AddSubMenu(command_id, menu_item.label).get());
      for (size_t i = 0; i < menu_item.submenu.size(); ++i)
        sub_menu->AddMenuItem(menu_item.submenu[i]);
      break;
    }
  }

  if (!menu_item.enabled && menu_item.type != content::MenuItem::SEPARATOR)
    SetEnabled(command_id, false);

  if (menu_item.checked &&
      (menu_item.type == content::MenuItem::CHECKABLE_OPTION ||
       menu_item.type == content::MenuItem::GROUP)) {
    SetChecked(command_id, true);
  }
}

void CefMenuModelImpl::NotifyMenuClosed() {
  DCHECK(!auto_notify_menu_closed_);
  OnMenuClosed();
}

void CefMenuModelImpl::AppendItem(const Item& item) {
  ValidateItem(item);
  items_.push_back(item);
}

void CefMenuModelImpl::InsertItemAt(const Item& item, int index) {
  // Sanitize the index.
  if (index < 0)
    index = 0;
  else if (index > static_cast<int>(items_.size()))
    index = items_.size();

  ValidateItem(item);
  items_.insert(items_.begin() + index, item);
}

void CefMenuModelImpl::ValidateItem(const Item& item) {
#if DCHECK_IS_ON()
  if (item.type_ == MENUITEMTYPE_SEPARATOR) {
    DCHECK_EQ(item.command_id_, kSeparatorId);
  } else {
    DCHECK_GE(item.command_id_, 0);
  }
#endif
}

void CefMenuModelImpl::OnMouseOutsideMenu(const gfx::Point& screen_point) {
  if (delegate_)
    delegate_->MouseOutsideMenu(this, screen_point);
  if (menu_model_delegate_) {
    menu_model_delegate_->MouseOutsideMenu(this,
        CefPoint(screen_point.x(), screen_point.y()));
  }
}

void CefMenuModelImpl::OnUnhandledOpenSubmenu(bool is_rtl) {
  if (delegate_)
    delegate_->UnhandledOpenSubmenu(this, is_rtl);
  if (menu_model_delegate_)
    menu_model_delegate_->UnhandledOpenSubmenu(this, is_rtl);
}

void CefMenuModelImpl::OnUnhandledCloseSubmenu(bool is_rtl) {
  if (delegate_)
    delegate_->UnhandledCloseSubmenu(this, is_rtl);
  if (menu_model_delegate_)
    menu_model_delegate_->UnhandledCloseSubmenu(this, is_rtl);
}

void CefMenuModelImpl::OnMenuClosed() {
  if (delegate_)
    delegate_->MenuClosed(this);
  if (menu_model_delegate_)
    menu_model_delegate_->MenuClosed(this);
}

bool CefMenuModelImpl::VerifyContext() {
  if (base::PlatformThread::CurrentId() != supported_thread_id_) {
    // This object should only be accessed from the thread that created it.
    NOTREACHED();
    return false;
  }

  return true;
}
