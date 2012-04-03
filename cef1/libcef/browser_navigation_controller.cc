// Copyright (c) 2008 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser_navigation_controller.h"
#include "libcef/browser_impl.h"

#include "base/logging.h"
#include "net/base/upload_data.h"

// ----------------------------------------------------------------------------
// BrowserNavigationEntry

BrowserNavigationEntry::BrowserNavigationEntry()
    : page_id_(-1) {
}

BrowserNavigationEntry::BrowserNavigationEntry(int page_id,
                                         const GURL& url,
                                         const CefString& title,
                                         const CefString& target_frame,
                                         const CefString& method,
                                         const WebKit::WebHTTPBody& upload,
                                         const CefRequest::HeaderMap& headers)
    : page_id_(page_id),
      url_(url),
      title_(title),
      method_(method),
      headers_(headers),
      target_frame_(target_frame) {
  if (!upload.isNull())
    upload_ = upload;
}

BrowserNavigationEntry::~BrowserNavigationEntry() {
}

void BrowserNavigationEntry::SetContentState(const std::string& state) {
  state_ = state;
}

// ----------------------------------------------------------------------------
// BrowserNavigationController

BrowserNavigationController::BrowserNavigationController(
    CefBrowserImpl* browser)
    : pending_entry_(NULL),
      last_committed_entry_index_(-1),
      pending_entry_index_(-1),
      browser_(browser),
      max_page_id_(-1) {
}

BrowserNavigationController::~BrowserNavigationController() {
  DiscardPendingEntry();
}

void BrowserNavigationController::Reset() {
  entries_.clear();
  DiscardPendingEntry();

  last_committed_entry_index_ = -1;
  UpdateMaxPageID();
}

void BrowserNavigationController::Reload(bool ignoreCache) {
  // Base the navigation on where we are now...
  int current_index = GetCurrentEntryIndex();

  // If we are no where, then we can't reload.  TODO(darin): We should add a
  // CanReload method.
  if (current_index == -1)
    return;

  DiscardPendingEntry();

  pending_entry_index_ = current_index;
  NavigateToPendingEntry(true, ignoreCache);
}

void BrowserNavigationController::GoToOffset(int offset) {
  int index = last_committed_entry_index_ + offset;
  if (index < 0 || index >= GetEntryCount())
    return;

  GoToIndex(index);
}

void BrowserNavigationController::GoToIndex(int index) {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, static_cast<int>(entries_.size()));

  DiscardPendingEntry();

  pending_entry_index_ = index;
  NavigateToPendingEntry(false, false);
}

void BrowserNavigationController::LoadEntry(BrowserNavigationEntry* entry) {
  // When navigating to a new page, we don't know for sure if we will actually
  // end up leaving the current page.  The new page load could for example
  // result in a download or a 'no content' response (e.g., a mailto: URL).
  DiscardPendingEntry();
  pending_entry_ = entry;
  NavigateToPendingEntry(false, false);
}


BrowserNavigationEntry* BrowserNavigationController::GetLastCommittedEntry()
    const {
  if (last_committed_entry_index_ == -1)
    return NULL;
  return entries_[last_committed_entry_index_].get();
}

BrowserNavigationEntry* BrowserNavigationController::GetActiveEntry() const {
  BrowserNavigationEntry* entry = pending_entry_;
  if (!entry)
    entry = GetLastCommittedEntry();
  return entry;
}

int BrowserNavigationController::GetCurrentEntryIndex() const {
  if (pending_entry_index_ != -1)
    return pending_entry_index_;
  return last_committed_entry_index_;
}


BrowserNavigationEntry* BrowserNavigationController::GetEntryAtIndex(
    int index) const {
  if (index < 0 || index >= GetEntryCount())
    return NULL;

  return entries_[index].get();
}

BrowserNavigationEntry* BrowserNavigationController::GetEntryWithPageID(
    int32 page_id) const {
  int index = GetEntryIndexWithPageID(page_id);
  return (index != -1) ? entries_[index].get() : NULL;
}

void BrowserNavigationController::DidNavigateToEntry(
    BrowserNavigationEntry* entry) {
  // If the entry is that of a page with PageID larger than any this Tab has
  // seen before, then consider it a new navigation.
  if (entry->GetPageID() > GetMaxPageID()) {
    InsertEntry(entry);
    return;
  }

  // Otherwise, we just need to update an existing entry with matching PageID.
  // If the existing entry corresponds to the entry which is pending, then we
  // must update the current entry index accordingly.  When navigating to the
  // same URL, a new PageID is not created.

  int existing_entry_index = GetEntryIndexWithPageID(entry->GetPageID());
  BrowserNavigationEntry* existing_entry = (existing_entry_index != -1) ?
      entries_[existing_entry_index].get() : NULL;
  if (!existing_entry) {
    // No existing entry, then simply ignore this navigation!
    DLOG(WARNING) << "ignoring navigation for page: " << entry->GetPageID();
  } else if (existing_entry == pending_entry_) {
    // The given entry might provide a new URL... e.g., navigating back to a
    // page in session history could have resulted in a new client redirect.
    existing_entry->SetURL(entry->GetURL());
    existing_entry->SetContentState(entry->GetContentState());
    last_committed_entry_index_ = pending_entry_index_;
    pending_entry_index_ = -1;
    pending_entry_ = NULL;
  } else if (pending_entry_ && pending_entry_->GetPageID() == -1 &&
             pending_entry_->GetURL() == existing_entry->GetURL()) {
    // Not a new navigation
    DiscardPendingEntry();
  } else {
    // The given entry might provide a new URL... e.g., navigating to a page
    // might result in a client redirect, which should override the URL of the
    // existing entry.
    existing_entry->SetURL(entry->GetURL());
    existing_entry->SetContentState(entry->GetContentState());

    // The navigation could have been issued by the renderer, so be sure that
    // we update our current index.
    last_committed_entry_index_ = existing_entry_index;
  }

  delete entry;
  UpdateMaxPageID();
}

void BrowserNavigationController::DiscardPendingEntry() {
  if (pending_entry_index_ == -1)
    delete pending_entry_;
  pending_entry_ = NULL;
  pending_entry_index_ = -1;
}

void BrowserNavigationController::InsertEntry(BrowserNavigationEntry* entry) {
  DiscardPendingEntry();

  const CefBrowserSettings& settings = browser_->settings();
  if (settings.history_disabled) {
    // History is disabled. Remove any existing entries.
    if (entries_.size() > 0)
      entries_.clear();
  } else {
    // Prune any entry which are in front of the current entry.
    int current_size = static_cast<int>(entries_.size());
    if (current_size > 0) {
      while (last_committed_entry_index_ < (current_size - 1)) {
        entries_.pop_back();
        current_size--;
      }
    }
  }

  entries_.push_back(linked_ptr<BrowserNavigationEntry>(entry));
  last_committed_entry_index_ = static_cast<int>(entries_.size()) - 1;
  UpdateMaxPageID();
}

int BrowserNavigationController::GetEntryIndexWithPageID(int32 page_id) const {
  for (int i = static_cast<int>(entries_.size())-1; i >= 0; --i) {
    if (entries_[i]->GetPageID() == page_id)
      return i;
  }
  return -1;
}

void BrowserNavigationController::NavigateToPendingEntry(bool reload,
                                                         bool ignoreCache) {
  // For session history navigations only the pending_entry_index_ is set.
  if (!pending_entry_) {
    DCHECK_NE(pending_entry_index_, -1);
    pending_entry_ = entries_[pending_entry_index_].get();
  }

  if (browser_->UIT_Navigate(*pending_entry_, reload, ignoreCache)) {
    // Note: this is redundant if navigation completed synchronously because
    // DidNavigateToEntry call this as well.
    UpdateMaxPageID();
  } else {
    DiscardPendingEntry();
  }
}

void BrowserNavigationController::UpdateMaxPageID() {
  BrowserNavigationEntry* entry = GetActiveEntry();
  if (entry)
    max_page_id_ = std::max(max_page_id_, entry->GetPageID());
}
