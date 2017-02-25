// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_MENU_MODEL_IMPL_H_
#define CEF_LIBCEF_BROWSER_MENU_MODEL_IMPL_H_
#pragma once

#include <vector>

#include "include/cef_menu_model.h"
#include "include/cef_menu_model_delegate.h"

#include "base/threading/platform_thread.h"
#include "ui/base/models/menu_model.h"
#include "ui/gfx/font_list.h"

namespace content {
struct MenuItem;
}

class CefMenuModelImpl : public CefMenuModel {
 public:
  class Delegate {
   public:
    // Perform the action associated with the specified |command_id| and
    // optional |event_flags|.
    virtual void ExecuteCommand(CefRefPtr<CefMenuModelImpl> source,
                                int command_id,
                                cef_event_flags_t event_flags) =0;

    // Called when the user moves the mouse outside the menu and over the owning
    // window.
    virtual void MouseOutsideMenu(CefRefPtr<CefMenuModelImpl> source,
                                  const gfx::Point& screen_point) {}

    // Called on unhandled open/close submenu keyboard commands. |is_rtl| will
    // be true if the menu is displaying a right-to-left language.
    virtual void UnhandledOpenSubmenu(CefRefPtr<CefMenuModelImpl> source,
                                      bool is_rtl) {}
    virtual void UnhandledCloseSubmenu(CefRefPtr<CefMenuModelImpl> source,
                                       bool is_rtl) {}

    // Called when the menu is about to show.
    virtual void MenuWillShow(CefRefPtr<CefMenuModelImpl> source) =0;

    // Called when the menu has closed.
    virtual void MenuClosed(CefRefPtr<CefMenuModelImpl> source) =0;

    // Allows the delegate to modify a menu item label before it's displayed.
    virtual bool FormatLabel(CefRefPtr<CefMenuModelImpl> source,
                             base::string16& label) =0;

   protected:
    virtual ~Delegate() {}
  };

  // Either |delegate| or |menu_model_delegate| must be non-nullptr.
  // If |delegate| is non-nullptr it must outlive this class.
  CefMenuModelImpl(Delegate* delegate,
                   CefRefPtr<CefMenuModelDelegate> menu_model_delegate,
                   bool is_submenu);
  ~CefMenuModelImpl() override;

  // CefMenuModel methods.
  bool IsSubMenu() override;
  bool Clear() override;
  int GetCount() override;
  bool AddSeparator() override;
  bool AddItem(int command_id, const CefString& label) override;
  bool AddCheckItem(int command_id, const CefString& label) override;
  bool AddRadioItem(int command_id, const CefString& label,
      int group_id) override;
  CefRefPtr<CefMenuModel> AddSubMenu(int command_id,
      const CefString& label) override;
  bool InsertSeparatorAt(int index) override;
  bool InsertItemAt(int index, int command_id,
      const CefString& label) override;
  bool InsertCheckItemAt(int index, int command_id,
      const CefString& label) override;
  bool InsertRadioItemAt(int index, int command_id,
      const CefString& label, int group_id) override;
  CefRefPtr<CefMenuModel> InsertSubMenuAt(int index, int command_id,
      const CefString& label) override;
  bool Remove(int command_id) override;
  bool RemoveAt(int index) override;
  int GetIndexOf(int command_id) override;
  int GetCommandIdAt(int index) override;
  bool SetCommandIdAt(int index, int command_id) override;
  CefString GetLabel(int command_id) override;
  CefString GetLabelAt(int index) override;
  bool SetLabel(int command_id, const CefString& label) override;
  bool SetLabelAt(int index, const CefString& label) override;
  MenuItemType GetType(int command_id) override;
  MenuItemType GetTypeAt(int index) override;
  int GetGroupId(int command_id) override;
  int GetGroupIdAt(int index) override;
  bool SetGroupId(int command_id, int group_id) override;
  bool SetGroupIdAt(int index, int group_id) override;
  CefRefPtr<CefMenuModel> GetSubMenu(int command_id) override;
  CefRefPtr<CefMenuModel> GetSubMenuAt(int index) override;
  bool IsVisible(int command_id) override;
  bool IsVisibleAt(int index) override;
  bool SetVisible(int command_id, bool visible) override;
  bool SetVisibleAt(int index, bool visible) override;
  bool IsEnabled(int command_id) override;
  bool IsEnabledAt(int index) override;
  bool SetEnabled(int command_id, bool enabled) override;
  bool SetEnabledAt(int index, bool enabled) override;
  bool IsChecked(int command_id) override;
  bool IsCheckedAt(int index) override;
  bool SetChecked(int command_id, bool checked) override;
  bool SetCheckedAt(int index, bool checked) override;
  bool HasAccelerator(int command_id) override;
  bool HasAcceleratorAt(int index) override;
  bool SetAccelerator(int command_id, int key_code, bool shift_pressed,
      bool ctrl_pressed, bool alt_pressed) override;
  bool SetAcceleratorAt(int index, int key_code, bool shift_pressed,
      bool ctrl_pressed, bool alt_pressed) override;
  bool RemoveAccelerator(int command_id) override;
  bool RemoveAcceleratorAt(int index) override;
  bool GetAccelerator(int command_id, int& key_code,
      bool& shift_pressed, bool& ctrl_pressed, bool& alt_pressed) override;
  bool GetAcceleratorAt(int index, int& key_code, bool& shift_pressed,
      bool& ctrl_pressed, bool& alt_pressed) override;
  bool SetColor(int command_id,
                cef_menu_color_type_t color_type,
                cef_color_t color) override;
  bool SetColorAt(int index,
                  cef_menu_color_type_t color_type,
                  cef_color_t color) override;
  bool GetColor(int command_id,
                cef_menu_color_type_t color_type,
                cef_color_t& color) override;
  bool GetColorAt(int index,
                  cef_menu_color_type_t color_type,
                  cef_color_t& color) override;
  bool SetFontList(int command_id, const CefString& font_list) override;
  bool SetFontListAt(int index, const CefString& font_list) override;

  // Callbacks from the ui::MenuModel implementation.
  void ActivatedAt(int index, cef_event_flags_t event_flags);
  void MouseOutsideMenu(const gfx::Point& screen_point);
  void UnhandledOpenSubmenu(bool is_rtl);
  void UnhandledCloseSubmenu(bool is_rtl);
  bool GetTextColor(int index,
                    bool is_accelerator,
                    bool is_hovered,
                    SkColor* override_color) const;
  bool GetBackgroundColor(int index,
                          bool is_hovered,
                          SkColor* override_color) const;
  void MenuWillShow();
  void MenuWillClose();
  base::string16 GetFormattedLabelAt(int index);
  const gfx::FontList* GetLabelFontListAt(int index) const;

  // Verify that only a single reference exists to all CefMenuModelImpl objects.
  bool VerifyRefCount();

  // Helper for adding custom menu items originating from the renderer process.
  void AddMenuItem(const content::MenuItem& menu_item);

  ui::MenuModel* model() const { return model_.get(); }

  // Used when created via CefMenuManager.
  Delegate* delegate() const { return delegate_; }
  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

  // Used for menus run via CefWindowImpl::ShowMenu to provide more accurate
  // menu close notification.
  void set_auto_notify_menu_closed(bool val) { auto_notify_menu_closed_ = val; }
  void NotifyMenuClosed();

 private:
  struct Item;

  typedef std::vector<Item> ItemVector;

  // Functions for inserting items into |items_|.
  void AppendItem(const Item& item);
  void InsertItemAt(const Item& item, int index);
  void ValidateItem(const Item& item);

  // Notify the delegate asynchronously.
  void OnMouseOutsideMenu(const gfx::Point& screen_point);
  void OnUnhandledOpenSubmenu(bool is_rtl);
  void OnUnhandledCloseSubmenu(bool is_rtl);
  void OnMenuClosed();

  // Verify that the object is being accessed from the correct thread.
  bool VerifyContext();

  base::PlatformThreadId supported_thread_id_;

  // Used when created via CefMenuManager.
  Delegate* delegate_;

  // Used when created via CefMenuModel::CreateMenuModel().
  CefRefPtr<CefMenuModelDelegate> menu_model_delegate_;

  const bool is_submenu_;

  ItemVector items_;
  std::unique_ptr<ui::MenuModel> model_;

  // Style information.
  cef_color_t default_colors_[CEF_MENU_COLOR_COUNT] = {0};
  gfx::FontList default_font_list_;
  bool has_default_font_list_ = false;

  bool auto_notify_menu_closed_ = true;

  IMPLEMENT_REFCOUNTING(CefMenuModelImpl);
  DISALLOW_COPY_AND_ASSIGN(CefMenuModelImpl);
};

#endif  // CEF_LIBCEF_BROWSER_MENU_MODEL_IMPL_H_
