// Copyright (c) 2017 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFCLIENT_BROWSER_VIEWS_MENU_BAR_H_
#define CEF_TESTS_CEFCLIENT_BROWSER_VIEWS_MENU_BAR_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "include/cef_menu_model.h"
#include "include/cef_menu_model_delegate.h"
#include "include/views/cef_menu_button.h"
#include "include/views/cef_menu_button_delegate.h"
#include "include/views/cef_panel.h"

namespace client {

// Implements a menu bar which is composed of CefMenuButtons positioned in a
// row with automatic switching between them via mouse/keyboard. All methods
// must be called on the browser process UI thread.
class ViewsMenuBar : public CefMenuButtonDelegate,
                     public CefMenuModelDelegate,
                     public CefPanelDelegate {
 public:
  // Delegate methods will be called on the browser process UI thread.
  class Delegate {
   public:
    // Called when a menu command is selected.
    virtual void MenuBarExecuteCommand(CefRefPtr<CefMenuModel> menu_model,
                                       int command_id,
                                       cef_event_flags_t event_flags) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  // |delegate| must outlive this object.
  // |menu_id_start| is the ID for the first CefMenuButton in the bar. An ID
  // range starting with |menu_id_start| and extending for a reasonable distance
  // should be reserved in the client for MenuBar usage.
  ViewsMenuBar(Delegate* delegate, int menu_id_start, bool use_bottom_controls);

  // Returns true if |menu_id| exists in the menu bar.
  bool HasMenuId(int menu_id) const;

  // Returns the CefPanel that represents the menu bar.
  CefRefPtr<CefPanel> GetMenuPanel();

  // Create a new menu with the specified |label|. If |menu_id| is non-nullptr
  // it will be populated with the new menu ID.
  CefRefPtr<CefMenuModel> CreateMenuModel(const CefString& label, int* menu_id);

  // Returns the menu with the specified |menu_id|, or nullptr if no such menu
  // exists.
  CefRefPtr<CefMenuModel> GetMenuModel(int menu_id) const;

  // Assign or remove focus from the menu bar.
  // Focus is assigned to the menu bar by ViewsWindow::OnKeyEvent when the ALT
  // key is pressed. Focus is removed from the menu bar by ViewsWindow::OnFocus
  // when a control not in the menu bar gains focus.
  void SetMenuFocusable(bool focusable);

  // Key events forwarded from ViewsWindow::OnKeyEvent when the menu bar has
  // focus.
  bool OnKeyEvent(const CefKeyEvent& event);

  // Reset menu bar state.
  void Reset();

 protected:
  // CefButtonDelegate methods:
  void OnButtonPressed(CefRefPtr<CefButton> button) override {}

  // CefMenuButtonDelegate methods:
  void OnMenuButtonPressed(
      CefRefPtr<CefMenuButton> menu_button,
      const CefPoint& screen_point,
      CefRefPtr<CefMenuButtonPressedLock> button_pressed_lock) override;

  // CefMenuModelDelegate methods:
  void ExecuteCommand(CefRefPtr<CefMenuModel> menu_model,
                      int command_id,
                      cef_event_flags_t event_flags) override;
  void MouseOutsideMenu(CefRefPtr<CefMenuModel> menu_model,
                        const CefPoint& screen_point) override;
  void UnhandledOpenSubmenu(CefRefPtr<CefMenuModel> menu_model,
                            bool is_rtl) override;
  void UnhandledCloseSubmenu(CefRefPtr<CefMenuModel> menu_model,
                             bool is_rtl) override;
  void MenuClosed(CefRefPtr<CefMenuModel> menu_model) override;

  // CefViewDelegate methods:
  void OnThemeChanged(CefRefPtr<CefView> view) override;

 private:
  // Creates the menu panel if it doesn't already exist.
  void EnsureMenuPanel();

  // Returns the ID for the currently active menu, or -1 if no menu is currently
  // active.
  int GetActiveMenuId();

  // Triggers the menu at the specified |offset| from the currently active menu.
  void TriggerNextMenu(int offset);

  // Triggers the specified MenuButton |button|.
  void TriggerMenuButton(CefRefPtr<CefView> button);

  Delegate* delegate_;  // Not owned by this object.
  const int id_start_;
  const bool use_bottom_controls_;
  int id_next_;
  CefRefPtr<CefPanel> panel_;
  std::vector<CefRefPtr<CefMenuModel>> models_;
  bool last_nav_with_keyboard_ = false;

  // Map of mnemonic to MenuButton ID.
  typedef std::map<char16_t, int> MnemonicMap;
  MnemonicMap mnemonics_;

  IMPLEMENT_REFCOUNTING(ViewsMenuBar);
  DISALLOW_COPY_AND_ASSIGN(ViewsMenuBar);
};

}  // namespace client

#endif  // CEF_TESTS_CEFCLIENT_BROWSER_VIEWS_MENU_BAR_H_
