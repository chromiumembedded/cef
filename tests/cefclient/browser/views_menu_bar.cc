// Copyright (c) 2017 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/views_menu_bar.h"

#include "include/views/cef_box_layout.h"
#include "include/views/cef_window.h"
#include "tests/cefclient/browser/views_style.h"

namespace client {

namespace {

const int kMenuBarGroupId = 100;

// Convert |c| to lowercase using the current ICU locale.
// TODO(jshin): What about Turkish locale? See http://crbug.com/81719.
// If the mnemonic is capital I and the UI language is Turkish, lowercasing it
// results in 'small dotless i', which is different from a 'dotted i'. Similar
// issues may exist for az and lt locales.
char16 ToLower(char16 c) {
  CefStringUTF16 str16;
  cef_string_utf16_to_lower(&c, 1, str16.GetWritableStruct());
  return str16.length() > 0 ? str16.c_str()[0] : 0;
}

// Extract the mnemonic character from |title|. For example, if |title| is
// "&Test" then the mnemonic character is 'T'.
char16 GetMnemonic(const std::u16string& title) {
  size_t index = 0;
  do {
    index = title.find('&', index);
    if (index != std::u16string::npos) {
      if (index + 1 != title.size() && title[index + 1] != '&')
        return ToLower(title[index + 1]);
      index++;
    }
  } while (index != std::u16string::npos);
  return 0;
}

}  // namespace

ViewsMenuBar::ViewsMenuBar(Delegate* delegate, int menu_id_start)
    : delegate_(delegate),
      id_start_(menu_id_start),
      id_next_(menu_id_start),
      last_nav_with_keyboard_(false) {
  DCHECK(delegate_);
  DCHECK_GT(id_start_, 0);
}

bool ViewsMenuBar::HasMenuId(int menu_id) const {
  return menu_id >= id_start_ && menu_id < id_next_;
}

CefRefPtr<CefPanel> ViewsMenuBar::GetMenuPanel() {
  EnsureMenuPanel();
  return panel_;
}

CefRefPtr<CefMenuModel> ViewsMenuBar::CreateMenuModel(const CefString& label,
                                                      int* menu_id) {
  EnsureMenuPanel();

  // Assign the new menu ID.
  const int new_menu_id = id_next_++;
  if (menu_id)
    *menu_id = new_menu_id;

  // Create the new MenuModel.
  CefRefPtr<CefMenuModel> model = CefMenuModel::CreateMenuModel(this);
  views_style::ApplyTo(model);
  models_.push_back(model);

  // Create the new MenuButton.
  CefRefPtr<CefMenuButton> button =
      CefMenuButton::CreateMenuButton(this, label);
  button->SetID(new_menu_id);
  views_style::ApplyTo(button.get());
  button->SetInkDropEnabled(true);

  // Assign a group ID to allow focus traversal between MenuButtons using the
  // arrow keys when the menu is not displayed.
  button->SetGroupID(kMenuBarGroupId);

  // Add the new MenuButton to the Planel.
  panel_->AddChildView(button);

  // Extract the mnemonic that triggers the menu, if any.
  char16 mnemonic = GetMnemonic(label);
  if (mnemonic != 0)
    mnemonics_.insert(std::make_pair(mnemonic, new_menu_id));

  return model;
}

CefRefPtr<CefMenuModel> ViewsMenuBar::GetMenuModel(int menu_id) const {
  if (HasMenuId(menu_id))
    return models_[menu_id - id_start_];
  return nullptr;
}

void ViewsMenuBar::SetMenuFocusable(bool focusable) {
  if (!panel_)
    return;

  for (int id = id_start_; id < id_next_; ++id)
    panel_->GetViewForID(id)->SetFocusable(focusable);

  if (focusable) {
    // Give focus to the first MenuButton.
    panel_->GetViewForID(id_start_)->RequestFocus();
  }
}

bool ViewsMenuBar::OnKeyEvent(const CefKeyEvent& event) {
  if (!panel_)
    return false;

  if (event.type != KEYEVENT_RAWKEYDOWN)
    return false;

  // Do not check mnemonics if the Alt or Ctrl modifiers are pressed. For
  // example Ctrl+<T> is an accelerator, but <T> only is a mnemonic.
  if (event.modifiers & (EVENTFLAG_ALT_DOWN | EVENTFLAG_CONTROL_DOWN))
    return false;

  MnemonicMap::const_iterator it = mnemonics_.find(ToLower(event.character));
  if (it == mnemonics_.end())
    return false;

  // Set status indicating that we navigated using the keyboard.
  last_nav_with_keyboard_ = true;

  // Show the selected menu.
  TriggerMenuButton(panel_->GetViewForID(it->second));

  return true;
}

void ViewsMenuBar::Reset() {
  panel_ = nullptr;
  models_.clear();
  mnemonics_.clear();
  id_next_ = id_start_;
}

void ViewsMenuBar::OnMenuButtonPressed(
    CefRefPtr<CefMenuButton> menu_button,
    const CefPoint& screen_point,
    CefRefPtr<CefMenuButtonPressedLock> button_pressed_lock) {
  CefRefPtr<CefMenuModel> menu_model = GetMenuModel(menu_button->GetID());

  // Adjust menu position left by button width.
  CefPoint point = screen_point;
  point.x -= menu_button->GetBounds().width - 4;

  // Keep track of the current |last_nav_with_keyboard_| status and restore it
  // after displaying the new menu.
  bool cur_last_nav_with_keyboard = last_nav_with_keyboard_;

  // May result in the previous menu being closed, in which case MenuClosed will
  // be called before the new menu is displayed.
  menu_button->ShowMenu(menu_model, point, CEF_MENU_ANCHOR_TOPLEFT);

  last_nav_with_keyboard_ = cur_last_nav_with_keyboard;
}

void ViewsMenuBar::ExecuteCommand(CefRefPtr<CefMenuModel> menu_model,
                                  int command_id,
                                  cef_event_flags_t event_flags) {
  delegate_->MenuBarExecuteCommand(menu_model, command_id, event_flags);
}

void ViewsMenuBar::MouseOutsideMenu(CefRefPtr<CefMenuModel> menu_model,
                                    const CefPoint& screen_point) {
  DCHECK(panel_);

  // Retrieve the Window hosting the Panel.
  CefRefPtr<CefWindow> window = panel_->GetWindow();
  DCHECK(window);

  // Convert the point from screen to window coordinates.
  CefPoint window_point = screen_point;
  if (!window->ConvertPointFromScreen(window_point))
    return;

  CefRect panel_bounds = panel_->GetBounds();

  if (last_nav_with_keyboard_) {
    // The user navigated last using the keyboard. Don't change menus using
    // mouse movements until the mouse exits and re-enters the Panel.
    if (panel_bounds.Contains(window_point))
      return;
    last_nav_with_keyboard_ = false;
  }

  // Check that the point is inside the Panel.
  if (!panel_bounds.Contains(window_point))
    return;

  const int active_menu_id = GetActiveMenuId();

  // Determine which MenuButton is under the specified point.
  for (int id = id_start_; id < id_next_; ++id) {
    // Skip the currently active MenuButton.
    if (id == active_menu_id)
      continue;

    CefRefPtr<CefView> button = panel_->GetViewForID(id);
    CefRect button_bounds = button->GetBounds();
    if (button_bounds.Contains(window_point)) {
      // Trigger the hovered MenuButton.
      TriggerMenuButton(button);
      break;
    }
  }
}

void ViewsMenuBar::UnhandledOpenSubmenu(CefRefPtr<CefMenuModel> menu_model,
                                        bool is_rtl) {
  TriggerNextMenu(is_rtl ? 1 : -1);
}

void ViewsMenuBar::UnhandledCloseSubmenu(CefRefPtr<CefMenuModel> menu_model,
                                         bool is_rtl) {
  TriggerNextMenu(is_rtl ? -1 : 1);
}

void ViewsMenuBar::MenuClosed(CefRefPtr<CefMenuModel> menu_model) {
  // Reset |last_nav_with_keyboard_| status whenever the main menu closes.
  if (!menu_model->IsSubMenu() && last_nav_with_keyboard_)
    last_nav_with_keyboard_ = false;
}

void ViewsMenuBar::EnsureMenuPanel() {
  if (panel_)
    return;

  panel_ = CefPanel::CreatePanel(nullptr);
  views_style::ApplyTo(panel_);

  // Use a horizontal box layout.
  CefBoxLayoutSettings top_panel_layout_settings;
  top_panel_layout_settings.horizontal = true;
  panel_->SetToBoxLayout(top_panel_layout_settings);
}

int ViewsMenuBar::GetActiveMenuId() {
  DCHECK(panel_);

  for (int id = id_start_; id < id_next_; ++id) {
    CefRefPtr<CefButton> button = panel_->GetViewForID(id)->AsButton();
    if (button->GetState() == CEF_BUTTON_STATE_PRESSED)
      return id;
  }

  return -1;
}

void ViewsMenuBar::TriggerNextMenu(int offset) {
  DCHECK(panel_);

  const int active_menu_id = GetActiveMenuId();
  const int menu_count = id_next_ - id_start_;
  const int active_menu_index = active_menu_id - id_start_;

  // Compute the modulus to avoid negative values.
  int next_menu_index = (active_menu_index + offset) % menu_count;
  if (next_menu_index < 0)
    next_menu_index += menu_count;

  // Cancel the existing menu. MenuClosed may be called.
  panel_->GetWindow()->CancelMenu();

  // Set status indicating that we navigated using the keyboard.
  last_nav_with_keyboard_ = true;

  // Show the new menu.
  TriggerMenuButton(panel_->GetViewForID(id_start_ + next_menu_index));
}

void ViewsMenuBar::TriggerMenuButton(CefRefPtr<CefView> button) {
  CefRefPtr<CefMenuButton> menu_button =
      button->AsButton()->AsLabelButton()->AsMenuButton();
  if (menu_button->IsFocusable())
    menu_button->RequestFocus();
  menu_button->TriggerMenu();
}

}  // namespace client
