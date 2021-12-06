// Copyright (c) 2021 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_SIMPLE_MENU_MODEL_IMPL_H_
#define CEF_LIBCEF_BROWSER_SIMPLE_MENU_MODEL_IMPL_H_
#pragma once

#include <vector>

#include "include/cef_menu_model.h"

#include "base/threading/platform_thread.h"
#include "ui/base/models/simple_menu_model.h"

// Implementation of CefMenuModel that wraps an existing ui::SimpleMenuModel.
class CefSimpleMenuModelImpl : public CefMenuModel {
 public:
  // Interface for setting state using CefMenuModel methods that will later be
  // retrieved via the ui::SimpleMenuModel::Delegate implementation.
  class StateDelegate {
   public:
    virtual void SetChecked(int command_id, bool checked) = 0;
    virtual void SetAccelerator(int command_id,
                                absl::optional<ui::Accelerator> accel) = 0;

   protected:
    virtual ~StateDelegate() {}
  };

  // |delegate| should be the same that was used to create |model|.
  // If |is_owned| is true then |model| will be deleted on Detach().
  CefSimpleMenuModelImpl(ui::SimpleMenuModel* model,
                         ui::SimpleMenuModel::Delegate* delegate,
                         StateDelegate* state_delegate,
                         bool is_owned,
                         bool is_submenu);

  CefSimpleMenuModelImpl(const CefSimpleMenuModelImpl&) = delete;
  CefSimpleMenuModelImpl& operator=(const CefSimpleMenuModelImpl&) = delete;

  ~CefSimpleMenuModelImpl() override;

  // Must be called before the object is deleted.
  void Detach();

  // CefMenuModel methods.
  bool IsSubMenu() override;
  bool Clear() override;
  int GetCount() override;
  bool AddSeparator() override;
  bool AddItem(int command_id, const CefString& label) override;
  bool AddCheckItem(int command_id, const CefString& label) override;
  bool AddRadioItem(int command_id,
                    const CefString& label,
                    int group_id) override;
  CefRefPtr<CefMenuModel> AddSubMenu(int command_id,
                                     const CefString& label) override;
  bool InsertSeparatorAt(int index) override;
  bool InsertItemAt(int index, int command_id, const CefString& label) override;
  bool InsertCheckItemAt(int index,
                         int command_id,
                         const CefString& label) override;
  bool InsertRadioItemAt(int index,
                         int command_id,
                         const CefString& label,
                         int group_id) override;
  CefRefPtr<CefMenuModel> InsertSubMenuAt(int index,
                                          int command_id,
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
  bool SetAccelerator(int command_id,
                      int key_code,
                      bool shift_pressed,
                      bool ctrl_pressed,
                      bool alt_pressed) override;
  bool SetAcceleratorAt(int index,
                        int key_code,
                        bool shift_pressed,
                        bool ctrl_pressed,
                        bool alt_pressed) override;
  bool RemoveAccelerator(int command_id) override;
  bool RemoveAcceleratorAt(int index) override;
  bool GetAccelerator(int command_id,
                      int& key_code,
                      bool& shift_pressed,
                      bool& ctrl_pressed,
                      bool& alt_pressed) override;
  bool GetAcceleratorAt(int index,
                        int& key_code,
                        bool& shift_pressed,
                        bool& ctrl_pressed,
                        bool& alt_pressed) override;
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

  ui::SimpleMenuModel* model() const { return model_; }

 private:
  // Verify that the object is attached and being accessed from the correct
  // thread.
  bool VerifyContext();

  // Returns true if |index| is valid.
  bool ValidIndex(int index);

  CefRefPtr<CefSimpleMenuModelImpl> CreateNewSubMenu(
      ui::SimpleMenuModel* model);

  base::PlatformThreadId supported_thread_id_;

  ui::SimpleMenuModel* model_;
  ui::SimpleMenuModel::Delegate* const delegate_;
  StateDelegate* const state_delegate_;
  const bool is_owned_;
  const bool is_submenu_;

  // Keep the submenus alive until they're removed, or we're destroyed.
  using SubMenuMap =
      std::map<ui::SimpleMenuModel*, CefRefPtr<CefSimpleMenuModelImpl>>;
  SubMenuMap submenumap_;

  IMPLEMENT_REFCOUNTING(CefSimpleMenuModelImpl);
};

#endif  // CEF_LIBCEF_BROWSER_SIMPLE_MENU_MODEL_IMPL_H_
