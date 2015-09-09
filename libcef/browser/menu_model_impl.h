// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_MENU_MODEL_IMPL_H_
#define CEF_LIBCEF_BROWSER_MENU_MODEL_IMPL_H_
#pragma once

#include <vector>

#include "include/cef_menu_model.h"

#include "base/memory/scoped_ptr.h"
#include "base/threading/platform_thread.h"
#include "ui/base/models/menu_model.h"

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

    // Notifies the delegate that the menu is about to show.
    virtual void MenuWillShow(CefRefPtr<CefMenuModelImpl> source) =0;

    // Notifies the delegate that the menu has closed.
    virtual void MenuClosed(CefRefPtr<CefMenuModelImpl> source) =0;

    // Allows the delegate to modify a menu item label before it's displayed.
    virtual bool FormatLabel(base::string16& label) =0;

   protected:
    virtual ~Delegate() {}
  };

  // The delegate must outlive this class.
  explicit CefMenuModelImpl(Delegate* delegate);
  ~CefMenuModelImpl() override;

  // CefMenuModel methods.
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

  // Callbacks from the ui::MenuModel implementation.
  void ActivatedAt(int index, cef_event_flags_t event_flags);
  void MenuWillShow();
  void MenuClosed();
  base::string16 GetFormattedLabelAt(int index);

  // Verify that only a single reference exists to all CefMenuModelImpl objects.
  bool VerifyRefCount();

  // Helper for adding custom menu items originating from the renderer process.
  void AddMenuItem(const content::MenuItem& menu_item);

  ui::MenuModel* model() { return model_.get(); }
  Delegate* delegate() { return delegate_; }
  void set_delegate(Delegate* delegate) { delegate_ = NULL; }

 private:
  struct Item;

  typedef std::vector<Item> ItemVector;

  // Functions for inserting items into |items_|.
  void AppendItem(const Item& item);
  void InsertItemAt(const Item& item, int index);
  void ValidateItem(const Item& item);

  // Notify the delegate that the menu is closed.
  void OnMenuClosed();

  // Verify that the object is being accessed from the correct thread.
  bool VerifyContext();

  base::PlatformThreadId supported_thread_id_;
  Delegate* delegate_;
  ItemVector items_;
  scoped_ptr<ui::MenuModel> model_;

  IMPLEMENT_REFCOUNTING(CefMenuModelImpl);
  DISALLOW_COPY_AND_ASSIGN(CefMenuModelImpl);
};

#endif  // CEF_LIBCEF_BROWSER_MENU_MODEL_IMPL_H_
