// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_ALLOY_ALLOY_TAB_INTERFACE_H_
#define CEF_LIBCEF_BROWSER_ALLOY_ALLOY_TAB_INTERFACE_H_

#include "base/callback_list.h"
#include "components/split_tabs/split_tab_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

// WebContents-owned adapter for Chromium's shared tab features. Alloy has one
// permanently selected tab per browser, without a Chrome browser window, tab
// strip, contents discarding, or Chrome tab-modal UI. Callbacks for those
// unsupported transitions never fire. Window visibility is independent of tab
// selection/visibility, as it is for TabInterface in Chrome.
class AlloyTabInterface
    : public tabs::TabInterface,
      public content::WebContentsObserver,
      public content::WebContentsUserData<AlloyTabInterface> {
 public:
  ~AlloyTabInterface() override;

  // tabs::TabInterface:
  void DeleteSelf() override;
  base::WeakPtr<tabs::TabInterface> GetWeakPtr() override;
  content::WebContents* GetContents() const override;
  void LoadIfNeeded() override;
  std::u16string GetTitle() const override;
  GURL GetURL() const override;
  base::Time GetLastActiveTime() const override;
  Profile* GetProfile() const override;
  void Close() override;
  base::CallbackListSubscription RegisterWillDiscardContents(
      WillDiscardContentsCallback callback) override;
  bool IsActivated() const override { return true; }
  base::CallbackListSubscription RegisterDidActivate(
      DidActivateCallback callback) override;
  base::CallbackListSubscription RegisterWillDeactivate(
      WillDeactivateCallback callback) override;
  bool IsVisible() const override { return true; }
  bool IsSelected() const override { return true; }
  base::CallbackListSubscription RegisterDidBecomeVisible(
      DidBecomeVisibleCallback callback) override;
  base::CallbackListSubscription RegisterWillBecomeHidden(
      WillBecomeHiddenCallback callback) override;
  base::CallbackListSubscription RegisterWillDetach(
      WillDetach callback) override;
  base::CallbackListSubscription RegisterDidInsert(
      DidInsertCallback callback) override;
  base::CallbackListSubscription RegisterPinnedStateChanged(
      PinnedStateChangedCallback callback) override;
  base::CallbackListSubscription RegisterGroupChanged(
      GroupChangedCallback callback) override;
  base::CallbackListSubscription RegisterBlockedStateChanged(
      BlockedStateChangedCallback callback) override;
  bool CanShowModalUI() const override { return false; }
  std::unique_ptr<tabs::ScopedTabModalUI> ShowModalUI() override;
  base::CallbackListSubscription RegisterModalUIChanged(
      TabInterfaceCallback callback) override;
  bool IsInNormalWindow() const override { return false; }
  BrowserWindowInterface* GetBrowserWindowInterface() override {
    return nullptr;
  }
  const BrowserWindowInterface* GetBrowserWindowInterface() const override {
    return nullptr;
  }
  tabs::TabFeatures* GetTabFeatures() override { return tab_features_.get(); }
  const tabs::TabFeatures* GetTabFeatures() const override {
    return tab_features_.get();
  }
  bool IsPinned() const override { return false; }
  bool IsBlocked() const override { return false; }
  bool IsSplit() const override { return false; }
  std::optional<tab_groups::TabGroupId> GetGroup() const override;
  std::optional<split_tabs::SplitTabId> GetSplit() const override;
  tabs::TabCollection* GetParentCollection(
      base::PassKey<tabs::TabCollection>) const override;
  const tabs::TabCollection* GetParentCollection() const override;
  void OnReparented(tabs::TabCollection* parent,
                    base::PassKey<tabs::TabCollection>) override;
  void OnAncestorChanged(base::PassKey<tabs::TabCollection>) override;
  ui::UnownedUserDataHost& GetUnownedUserDataHost() override;
  const ui::UnownedUserDataHost& GetUnownedUserDataHost() const override;

 private:
  friend content::WebContentsUserData<AlloyTabInterface>;
  explicit AlloyTabInterface(content::WebContents* contents);

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;

  // Features must be destroyed before the host and while WebContents is valid.
  ui::UnownedUserDataHost unowned_user_data_host_;
  base::RepeatingCallbackList<void(tabs::TabInterface*, DetachReason)>
      will_detach_callbacks_;
  std::unique_ptr<tabs::TabFeatures> tab_features_;
  base::WeakPtrFactory<AlloyTabInterface> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CEF_LIBCEF_BROWSER_ALLOY_ALLOY_TAB_INTERFACE_H_
