// Copyright (c) 2008-2009 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NAVIGATION_CONTROLLER_H_
#define CEF_LIBCEF_BROWSER_NAVIGATION_CONTROLLER_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDataSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebHTTPBody.h"

#include "include/cef.h"

class GURL;
class CefBrowserImpl;

// Associated with browser-initated navigations to hold tracking data.
class BrowserExtraData : public WebKit::WebDataSource::ExtraData {
 public:
  explicit BrowserExtraData(int32 pending_page_id)
      : pending_page_id(pending_page_id),
        request_committed(false) {
  }

  // Contains the page_id for this navigation or -1 if there is none yet.
  int32 pending_page_id;

  // True if we have already processed the "DidCommitLoad" event for this
  // request.  Used by session history.
  bool request_committed;
};

// Stores one back/forward navigation state for the test shell.
class BrowserNavigationEntry {
 public:
  BrowserNavigationEntry();
  BrowserNavigationEntry(int page_id,
                      const GURL& url,
                      const CefString& title,
                      const CefString& target_frame,
                      const CefString& method,
                      const WebKit::WebHTTPBody& upload,
                      const CefRequest::HeaderMap& headers);
  ~BrowserNavigationEntry();

  // Set / Get the URI
  void SetURL(const GURL& url) { url_ = url; }
  const GURL& GetURL() const { return url_; }

  // Set / Get the title
  void SetTitle(const CefString& a_title) { title_ = a_title; }
  const CefString& GetTitle() const { return title_; }

  // Set / Get opaque state.
  // WARNING: This state is saved to the database and used to restore previous
  // states. If you use write a custom TabContents and provide your own
  // state make sure you have the ability to modify the format in the future
  // while being able to deal with older versions.
  void SetContentState(const std::string& state);
  const std::string& GetContentState() const { return state_; }

  // Get the page id corresponding to the tab's state.
  void SetPageID(int page_id) { page_id_ = page_id; }
  int32 GetPageID() const { return page_id_; }

  const CefString& GetTargetFrame() const { return target_frame_; }

  const CefString& GetMethod() const { return method_; }
  const WebKit::WebHTTPBody& GetUploadData() const { return upload_; }
  const CefRequest::HeaderMap& GetHeaders() const { return headers_; }

 private:
  // Describes the current page that the tab represents. This is not relevant
  // for all tab contents types.
  int32 page_id_;

  GURL url_;
  CefString title_;
  std::string state_;
  CefString method_;
  WebKit::WebHTTPBody upload_;
  CefRequest::HeaderMap headers_;

  CefString target_frame_;

  DISALLOW_COPY_AND_ASSIGN(BrowserNavigationEntry);
};

// Browser's NavigationController.  The goal is to be as close to the Chrome
// version as possible.
class BrowserNavigationController {
 public:
  explicit BrowserNavigationController(CefBrowserImpl* browser);
  ~BrowserNavigationController();

  void Reset();

  // Causes the controller to reload the current (or pending) entry.
  void Reload(bool ignoreCache);

  // Causes the controller to go to the specified offset from current.  Does
  // nothing if out of bounds.
  void GoToOffset(int offset);

  // Causes the controller to go to the specified index.
  void GoToIndex(int index);

  // Causes the controller to load the specified entry.  The controller
  // assumes ownership of the entry.
  // NOTE: Do not pass an entry that the controller already owns!
  void LoadEntry(BrowserNavigationEntry* entry);

  // Returns the last committed entry, which may be null if there are no
  // committed entries.
  BrowserNavigationEntry* GetLastCommittedEntry() const;

  // Returns the number of entries in the NavigationControllerBase, excluding
  // the pending entry if there is one.
  int GetEntryCount() const {
    return static_cast<int>(entries_.size());
  }

  // Returns the active entry, which is the pending entry if a navigation is in
  // progress or the last committed entry otherwise.  NOTE: This can be NULL!!
  //
  // If you are trying to get the current state of the NavigationControllerBase,
  // this is the method you will typically want to call.
  BrowserNavigationEntry* GetActiveEntry() const;

  // Returns the index from which we would go back/forward or reload.  This is
  // the last_committed_entry_index_ if pending_entry_index_ is -1.  Otherwise,
  // it is the pending_entry_index_.
  int GetCurrentEntryIndex() const;

  // Returns the entry at the specified index.  Returns NULL if out of
  // bounds.
  BrowserNavigationEntry* GetEntryAtIndex(int index) const;

  // Return the entry with the corresponding type and page_id, or NULL if
  // not found.
  BrowserNavigationEntry* GetEntryWithPageID(int32 page_id) const;

  // Returns the index of the last committed entry.
  int GetLastCommittedEntryIndex() const {
    return last_committed_entry_index_;
  }

  // Returns true if there are no entries before the last committed entry.
  bool IsAtStart() const {
    return (GetLastCommittedEntryIndex() == 0);
  }

  // Returns true if there are no entries after the last committed entry.
  bool IsAtEnd() const {
    return (GetLastCommittedEntryIndex() == GetEntryCount()-1);
  }

  // Used to inform us of a navigation being committed for a tab. We will take
  // ownership of the entry. Any entry located forward to the current entry will
  // be deleted. The new entry becomes the current entry.
  void DidNavigateToEntry(BrowserNavigationEntry* entry);

  // Used to inform us to discard its pending entry.
  void DiscardPendingEntry();

 private:
  // Inserts an entry after the current position, removing all entries after it.
  // The new entry will become the active one.
  void InsertEntry(BrowserNavigationEntry* entry);

  int GetMaxPageID() const { return max_page_id_; }
  void NavigateToPendingEntry(bool reload, bool ignoreCache);

  // Return the index of the entry with the corresponding type and page_id,
  // or -1 if not found.
  int GetEntryIndexWithPageID(int32 page_id) const;

  // Updates the max page ID with that of the given entry, if is larger.
  void UpdateMaxPageID();

  // List of NavigationEntry for this tab
  typedef ScopedVector<BrowserNavigationEntry> NavigationEntryList;
  NavigationEntryList entries_;

  // An entry we haven't gotten a response for yet.  This will be discarded
  // when we navigate again.  It's used only so we know what the currently
  // displayed tab is.
  BrowserNavigationEntry* pending_entry_;

  // currently visible entry
  int last_committed_entry_index_;

  // index of pending entry if it is in entries_, or -1 if pending_entry_ is a
  // new entry (created by LoadURL).
  int pending_entry_index_;

  CefBrowserImpl* browser_;
  int max_page_id_;

  DISALLOW_EVIL_CONSTRUCTORS(BrowserNavigationController);
};

#endif  // CEF_LIBCEF_BROWSER_NAVIGATION_CONTROLLER_H_
