// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/browser/alloy/alloy_tab_interface.h"

#include "base/notreached.h"
#include "cef/libcef/browser/alloy/alloy_browser_host_impl.h"
#include "chrome/browser/history/history_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper_factory.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"

AlloyTabInterface::AlloyTabInterface(content::WebContents* contents)
    : content::WebContentsObserver(contents),
      content::WebContentsUserData<AlloyTabInterface>(*contents),
      tab_features_(std::make_unique<tabs::TabFeatures>()) {
  CHECK(!tabs::TabInterface::MaybeGetFromContents(contents));
  tabs::TabLookupFromWebContents::CreateForWebContents(contents, this);
  CreateSessionServiceTabHelper(contents);
  SetSessionId(sessions::SessionTabHelper::IdForTab(contents).id());
  tab_features_->Init(*this, GetProfile());

  // Support visited links. The HistoryTabHelper object is created in
  // TabFeatures::Init.
  if (auto* history_tab_helper = HistoryTabHelper::FromWebContents(contents)) {
    // Bypass the Chrome-specific check in
    // HistoryTabHelper::DidFinishNavigation.
    history_tab_helper->SetForceEligibleTabForTesting(true);
  }
}

AlloyTabInterface::~AlloyTabInterface() = default;

void AlloyTabInterface::WebContentsDestroyed() {
  will_detach_callbacks_.Notify(this, DetachReason::kDelete);
  weak_ptr_factory_.InvalidateWeakPtrs();
  tab_features_.reset();
  GetContents()->RemoveUserData(tabs::TabLookupFromWebContents::UserDataKey());
  ClearSessionId();
}

void AlloyTabInterface::DeleteSelf() {
  // Owned by WebContents, never by a TabCollection.
}

base::WeakPtr<tabs::TabInterface> AlloyTabInterface::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

content::WebContents* AlloyTabInterface::GetContents() const {
  return web_contents();
}

void AlloyTabInterface::LoadIfNeeded() {
  if (GetContents()->GetController().NeedsReload()) {
    GetContents()->GetController().LoadIfNecessary();
  }
}

std::u16string AlloyTabInterface::GetTitle() const {
  return GetContents()->GetTitle();
}

GURL AlloyTabInterface::GetURL() const {
  return GetContents()->GetLastCommittedURL();
}

base::Time AlloyTabInterface::GetLastActiveTime() const {
  return GetContents()->GetLastActiveTime();
}

Profile* AlloyTabInterface::GetProfile() const {
  return Profile::FromBrowserContext(GetContents()->GetBrowserContext());
}

void AlloyTabInterface::Close() {
  if (auto browser =
          AlloyBrowserHostImpl::GetBrowserForContents(GetContents())) {
    browser->CloseBrowser(false);
  }
}

base::CallbackListSubscription AlloyTabInterface::RegisterWillDiscardContents(
    WillDiscardContentsCallback callback) {
  return {};
}

base::CallbackListSubscription AlloyTabInterface::RegisterDidActivate(
    DidActivateCallback callback) {
  return {};
}

base::CallbackListSubscription AlloyTabInterface::RegisterWillDeactivate(
    WillDeactivateCallback callback) {
  return {};
}

base::CallbackListSubscription AlloyTabInterface::RegisterDidBecomeVisible(
    DidBecomeVisibleCallback callback) {
  return {};
}

base::CallbackListSubscription AlloyTabInterface::RegisterWillBecomeHidden(
    WillBecomeHiddenCallback callback) {
  return {};
}

base::CallbackListSubscription AlloyTabInterface::RegisterWillDetach(
    WillDetach callback) {
  return will_detach_callbacks_.Add(std::move(callback));
}

base::CallbackListSubscription AlloyTabInterface::RegisterDidInsert(
    DidInsertCallback callback) {
  return {};
}

base::CallbackListSubscription AlloyTabInterface::RegisterPinnedStateChanged(
    PinnedStateChangedCallback callback) {
  return {};
}

base::CallbackListSubscription AlloyTabInterface::RegisterGroupChanged(
    GroupChangedCallback callback) {
  return {};
}

base::CallbackListSubscription AlloyTabInterface::RegisterBlockedStateChanged(
    BlockedStateChangedCallback callback) {
  return {};
}

bool AlloyTabInterface::CanShowModalUI() const {
  return !showing_modal_ui_;
}

std::unique_ptr<tabs::ScopedTabModalUI> AlloyTabInterface::ShowModalUI() {
  return std::make_unique<ScopedTabModalUIImpl>(this);
}

base::CallbackListSubscription AlloyTabInterface::RegisterModalUIChanged(
    TabInterfaceCallback callback) {
  return modal_ui_changed_callbacks_.Add(std::move(callback));
}

AlloyTabInterface::ScopedTabModalUIImpl::ScopedTabModalUIImpl(
    AlloyTabInterface* tab)
    : tab_(tab->weak_ptr_factory_.GetWeakPtr()) {
  CHECK(tab_->CanShowModalUI());
  tab_->showing_modal_ui_ = true;
  tab_->modal_ui_changed_callbacks_.Notify(tab_.get());
}

AlloyTabInterface::ScopedTabModalUIImpl::~ScopedTabModalUIImpl() {
  if (tab_) {
    tab_->showing_modal_ui_ = false;
    tab_->modal_ui_changed_callbacks_.Notify(tab_.get());
  }
}

std::optional<tab_groups::TabGroupId> AlloyTabInterface::GetGroup() const {
  return std::nullopt;
}

std::optional<split_tabs::SplitTabId> AlloyTabInterface::GetSplit() const {
  return std::nullopt;
}

tabs::TabCollection* AlloyTabInterface::GetParentCollection(
    base::PassKey<tabs::TabCollection>) const {
  return nullptr;
}

const tabs::TabCollection* AlloyTabInterface::GetParentCollection() const {
  return nullptr;
}

void AlloyTabInterface::OnReparented(tabs::TabCollection* parent,
                                     base::PassKey<tabs::TabCollection>) {
  NOTREACHED();
}

void AlloyTabInterface::OnAncestorChanged(base::PassKey<tabs::TabCollection>) {
  NOTREACHED();
}

ui::UnownedUserDataHost& AlloyTabInterface::GetUnownedUserDataHost() {
  return unowned_user_data_host_;
}

const ui::UnownedUserDataHost& AlloyTabInterface::GetUnownedUserDataHost()
    const {
  return unowned_user_data_host_;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(AlloyTabInterface);
