// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/browser_info.h"

#include <memory>

#include "libcef/browser/browser_host_base.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/frame_util.h"
#include "libcef/common/values_impl.h"

#include "base/logging.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/render_process_host.h"
#include "ipc/ipc_message.h"

CefBrowserInfo::FrameInfo::~FrameInfo() {
#if DCHECK_IS_ON()
  if (frame_ && !IsCurrentMainFrame()) {
    // Should already be Detached.
    DCHECK(frame_->IsDetached());
  }
#endif
}

CefBrowserInfo::CefBrowserInfo(int browser_id,
                               bool is_popup,
                               bool is_windowless,
                               CefRefPtr<CefDictionaryValue> extra_info)
    : browser_id_(browser_id),
      is_popup_(is_popup),
      is_windowless_(is_windowless),
      extra_info_(extra_info) {
  DCHECK_GT(browser_id, 0);

  if (extra_info_ && !extra_info_->IsReadOnly()) {
    // |extra_info_| should always be read-only to avoid accidental future
    // modification. Take a copy instead of modifying the passed-in object for
    // backwards compatibility.
    extra_info_ = extra_info_->Copy(/*exclude_empty_children=*/false);
    auto extra_info_impl =
        static_cast<CefDictionaryValueImpl*>(extra_info_.get());
    extra_info_impl->MarkReadOnly();
  }
}

CefBrowserInfo::~CefBrowserInfo() {
  DCHECK(frame_info_set_.empty());
}

CefRefPtr<CefBrowserHostBase> CefBrowserInfo::browser() const {
  base::AutoLock lock_scope(lock_);
  if (!is_closing_) {
    return browser_;
  }
  return nullptr;
}

void CefBrowserInfo::SetBrowser(CefRefPtr<CefBrowserHostBase> browser) {
  NotificationStateLock lock_scope(this);

  if (browser) {
    DCHECK(!browser_);

    // Cache the associated frame handler.
    if (auto client = browser->GetClient()) {
      frame_handler_ = client->GetFrameHandler();
    }
  } else {
    DCHECK(browser_);
  }

  auto old_browser = browser_;
  browser_ = browser;

  if (!browser_) {
    RemoveAllFrames(old_browser);

    // Any future calls to MaybeExecuteFrameNotification will now fail.
    // NotificationStateLock already took a reference for the delivery of any
    // notifications that are currently queued due to RemoveAllFrames.
    frame_handler_ = nullptr;
  }
}

void CefBrowserInfo::SetClosing() {
  base::AutoLock lock_scope(lock_);
  DCHECK(!is_closing_);
  is_closing_ = true;
}

void CefBrowserInfo::MaybeCreateFrame(content::RenderFrameHost* host,
                                      bool is_guest_view) {
  CEF_REQUIRE_UIT();

  const auto global_id = host->GetGlobalId();
  const bool is_main_frame = (host->GetParent() == nullptr);

  // A speculative RFH will be created in response to a browser-initiated
  // cross-origin navigation (e.g. via LoadURL) and eventually either discarded
  // or swapped in based on whether the navigation is committed. We'll create a
  // frame object for the speculative RFH so that it can be found by
  // frame/routing ID. However, we won't replace the main frame with a
  // speculative RFH until after it's swapped in, and we'll generally prefer to
  // return a non-speculative RFH for the same node ID if one exists.
  const bool is_speculative = (static_cast<content::RenderFrameHostImpl*>(host)
                                   ->frame_tree_node()
                                   ->render_manager()
                                   ->current_frame_host() != host);

  NotificationStateLock lock_scope(this);
  DCHECK(browser_);

  const auto it = frame_id_map_.find(global_id);
  if (it != frame_id_map_.end()) {
    auto info = it->second;

#if DCHECK_IS_ON()
    // Check that the frame info hasn't changed unexpectedly.
    DCHECK_EQ(info->global_id_, global_id);
    DCHECK_EQ(info->is_guest_view_, is_guest_view);
    DCHECK_EQ(info->is_main_frame_, is_main_frame);
#endif

    if (!info->is_guest_view_ && info->is_speculative_ && !is_speculative) {
      // Upgrade the frame info from speculative to non-speculative.
      if (info->is_main_frame_) {
        // Set the main frame object.
        SetMainFrame(browser_, info->frame_);
      }
      info->is_speculative_ = false;
    }
    return;
  }

  auto frame_info = new FrameInfo;
  frame_info->host_ = host;
  frame_info->global_id_ = global_id;
  frame_info->is_guest_view_ = is_guest_view;
  frame_info->is_main_frame_ = is_main_frame;
  frame_info->is_speculative_ = is_speculative;

  // Guest views don't get their own CefBrowser or CefFrame objects.
  if (!is_guest_view) {
    // Create a new frame object.
    frame_info->frame_ = new CefFrameHostImpl(this, host);
    MaybeNotifyFrameCreated(frame_info->frame_);
    if (is_main_frame && !is_speculative) {
      SetMainFrame(browser_, frame_info->frame_);
    }

#if DCHECK_IS_ON()
    // Check that the frame info hasn't changed unexpectedly.
    DCHECK(host->GetGlobalFrameToken() == *frame_info->frame_->frame_token());
    DCHECK_EQ(frame_info->is_main_frame_, frame_info->frame_->IsMain());
#endif
  }

  browser_->request_context()->OnRenderFrameCreated(global_id, is_main_frame,
                                                    is_guest_view);

  // Populate the lookup maps.
  frame_id_map_.insert(std::make_pair(global_id, frame_info));
  frame_token_to_id_map_.insert(
      std::make_pair(host->GetGlobalFrameToken(), global_id));

  // And finally set the ownership.
  frame_info_set_.insert(base::WrapUnique(frame_info));
}

void CefBrowserInfo::FrameHostStateChanged(
    content::RenderFrameHost* host,
    content::RenderFrameHost::LifecycleState old_state,
    content::RenderFrameHost::LifecycleState new_state) {
  CEF_REQUIRE_UIT();

  if ((old_state == content::RenderFrameHost::LifecycleState::kPrerendering ||
       old_state ==
           content::RenderFrameHost::LifecycleState::kInBackForwardCache) &&
      new_state == content::RenderFrameHost::LifecycleState::kActive) {
    if (auto frame = GetFrameForHost(host)) {
      // Update the associated RFH, which may have changed.
      frame->MaybeReAttach(this, host);

      if (frame->IsMain()) {
        // Update the main frame object.
        NotificationStateLock lock_scope(this);
        SetMainFrame(browser_, frame);
      }

      // Update draggable regions.
      frame->MaybeSendDidStopLoading();
    }
  }

  // Update BackForwardCache state.
  bool added_to_bfcache =
      new_state ==
      content::RenderFrameHost::LifecycleState::kInBackForwardCache;
  bool removed_from_bfcache =
      old_state ==
      content::RenderFrameHost::LifecycleState::kInBackForwardCache;
  if (!added_to_bfcache && !removed_from_bfcache) {
    return;
  }

  base::AutoLock lock_scope(lock_);

  auto it = frame_id_map_.find(host->GetGlobalId());
  DCHECK(it != frame_id_map_.end());
  DCHECK((!it->second->is_in_bfcache_ && added_to_bfcache) ||
         (it->second->is_in_bfcache_ && removed_from_bfcache));
  it->second->is_in_bfcache_ = added_to_bfcache;
}

void CefBrowserInfo::RemoveFrame(content::RenderFrameHost* host) {
  CEF_REQUIRE_UIT();

  NotificationStateLock lock_scope(this);

  const auto global_id = host->GetGlobalId();
  auto it = frame_id_map_.find(global_id);
  DCHECK(it != frame_id_map_.end());

  auto frame_info = it->second;

  browser_->request_context()->OnRenderFrameDeleted(
      global_id, frame_info->is_main_frame_, frame_info->is_guest_view_);

  // Remove from the lookup maps.
  frame_id_map_.erase(it);

  {
    auto it2 = frame_token_to_id_map_.find(host->GetGlobalFrameToken());
    DCHECK(it2 != frame_token_to_id_map_.end());
    frame_token_to_id_map_.erase(it2);
  }

  // And finally delete the frame info.
  {
    auto it2 = frame_info_set_.find(frame_info);

    // Explicitly Detach everything but the current main frame.
    const auto& other_frame_info = *it2;
    if (other_frame_info->frame_ && !other_frame_info->IsCurrentMainFrame()) {
      if (other_frame_info->frame_->Detach(
              CefFrameHostImpl::DetachReason::RENDER_FRAME_DELETED)) {
        MaybeNotifyFrameDetached(browser_, other_frame_info->frame_);
      }
    }

    frame_info_set_.erase(it2);
  }
}

CefRefPtr<CefFrameHostImpl> CefBrowserInfo::GetMainFrame() {
  base::AutoLock lock_scope(lock_);
  // Early exit if called post-destruction.
  if (!browser_ || is_closing_) {
    return nullptr;
  }

  CHECK(main_frame_);
  return main_frame_;
}

CefRefPtr<CefFrameHostImpl> CefBrowserInfo::CreateTempSubFrame(
    const content::GlobalRenderFrameHostId& parent_global_id) {
  CefRefPtr<CefFrameHostImpl> parent = GetFrameForGlobalId(parent_global_id);
  if (!parent) {
    parent = GetMainFrame();
  }
  // Intentionally not notifying for temporary frames.
  return new CefFrameHostImpl(this, parent->frame_token());
}

CefRefPtr<CefFrameHostImpl> CefBrowserInfo::GetFrameForHost(
    const content::RenderFrameHost* host,
    bool* is_guest_view,
    bool prefer_speculative) const {
  if (is_guest_view) {
    *is_guest_view = false;
  }

  if (!host) {
    return nullptr;
  }

  return GetFrameForGlobalId(
      const_cast<content::RenderFrameHost*>(host)->GetGlobalId(), is_guest_view,
      prefer_speculative);
}

CefRefPtr<CefFrameHostImpl> CefBrowserInfo::GetFrameForGlobalId(
    const content::GlobalRenderFrameHostId& global_id,
    bool* is_guest_view,
    bool prefer_speculative) const {
  if (is_guest_view) {
    *is_guest_view = false;
  }

  if (!frame_util::IsValidGlobalId(global_id)) {
    return nullptr;
  }

  base::AutoLock lock_scope(lock_);

  const auto it = frame_id_map_.find(global_id);
  if (it != frame_id_map_.end()) {
    const auto info = it->second;

    if (info->is_guest_view_) {
      if (is_guest_view) {
        *is_guest_view = true;
      }
      return nullptr;
    }

    if (info->is_speculative_ && !prefer_speculative) {
      if (info->is_main_frame_ && main_frame_) {
        // Always prefer the non-speculative main frame.
        return main_frame_;
      }

      LOG(WARNING) << "Returning a speculative frame for "
                   << frame_util::GetFrameDebugString(global_id);
    }

    DCHECK(info->frame_);
    return info->frame_;
  }

  return nullptr;
}

CefRefPtr<CefFrameHostImpl> CefBrowserInfo::GetFrameForGlobalToken(
    const content::GlobalRenderFrameHostToken& global_token,
    bool* is_guest_view,
    bool prefer_speculative) const {
  if (is_guest_view) {
    *is_guest_view = false;
  }

  if (!frame_util::IsValidGlobalToken(global_token)) {
    return nullptr;
  }

  content::GlobalRenderFrameHostId global_id;

  {
    base::AutoLock lock_scope(lock_);
    const auto it = frame_token_to_id_map_.find(global_token);
    if (it == frame_token_to_id_map_.end()) {
      return nullptr;
    }
    global_id = it->second;
  }

  return GetFrameForGlobalId(global_id, is_guest_view, prefer_speculative);
}

CefBrowserInfo::FrameHostList CefBrowserInfo::GetAllFrames() const {
  base::AutoLock lock_scope(lock_);
  FrameHostList frames;
  for (const auto& info : frame_info_set_) {
    if (info->frame_ && !info->is_speculative_ && !info->is_in_bfcache_) {
      frames.insert(info->frame_);
    }
  }
  return frames;
}

CefBrowserInfo::NavigationLock::NavigationLock() : weak_ptr_factory_(this) {}

CefBrowserInfo::NavigationLock::~NavigationLock() {
  CEF_REQUIRE_UIT();
  if (pending_action_) {
    CEF_POST_TASK(CEF_UIT, std::move(pending_action_));
  }
}

scoped_refptr<CefBrowserInfo::NavigationLock>
CefBrowserInfo::CreateNavigationLock() {
  CEF_REQUIRE_UIT();
  scoped_refptr<NavigationLock> lock;
  if (!navigation_lock_) {
    lock = new NavigationLock();
    navigation_lock_ = lock->weak_ptr_factory_.GetWeakPtr();
  } else {
    lock = navigation_lock_.get();
  }
  return lock;
}

bool CefBrowserInfo::IsNavigationLocked(base::OnceClosure pending_action) {
  CEF_REQUIRE_UIT();
  if (navigation_lock_) {
    navigation_lock_->pending_action_ = std::move(pending_action);
    return true;
  }
  return false;
}

void CefBrowserInfo::MaybeExecuteFrameNotification(
    FrameNotifyOnceAction pending_action) {
  CefRefPtr<CefFrameHandler> frame_handler;

  {
    base::AutoLock lock_scope_(notification_lock_);
    if (!frame_handler_) {
      // No notifications will be executed.
      return;
    }

    if (notification_state_lock_) {
      // Queue the notification until the lock is released.
      notification_state_lock_->queue_.push(std::move(pending_action));
      return;
    }

    frame_handler = frame_handler_;
  }

  // Execute immediately if not locked.
  std::move(pending_action).Run(frame_handler);
}

void CefBrowserInfo::MaybeNotifyDraggableRegionsChanged(
    CefRefPtr<CefBrowserHostBase> browser,
    CefRefPtr<CefFrameHostImpl> frame,
    std::vector<CefDraggableRegion> draggable_regions) {
  CEF_REQUIRE_UIT();
  DCHECK(frame->IsMain());

  if (draggable_regions == draggable_regions_) {
    return;
  }

  draggable_regions_ = std::move(draggable_regions);

  if (auto client = browser->GetClient()) {
    if (auto handler = client->GetDragHandler()) {
      handler->OnDraggableRegionsChanged(browser.get(), frame,
                                         draggable_regions_);
    }
  }
}

// Passing in |browser| here because |browser_| may already be cleared.
void CefBrowserInfo::SetMainFrame(CefRefPtr<CefBrowserHostBase> browser,
                                  CefRefPtr<CefFrameHostImpl> frame) {
  lock_.AssertAcquired();
  DCHECK(browser);
  DCHECK(!frame || frame->IsMain());

  if (frame && main_frame_ &&
      frame->GetIdentifier() == main_frame_->GetIdentifier()) {
    // Nothing to do.
    return;
  }

  CefRefPtr<CefFrameHostImpl> old_frame;
  if (main_frame_) {
    old_frame = main_frame_;
    if (old_frame->Detach(CefFrameHostImpl::DetachReason::NEW_MAIN_FRAME)) {
      MaybeNotifyFrameDetached(browser, old_frame);
    }
  }

  main_frame_ = frame;

  MaybeNotifyMainFrameChanged(browser, old_frame, main_frame_);
}

void CefBrowserInfo::MaybeNotifyFrameCreated(
    CefRefPtr<CefFrameHostImpl> frame) {
  CEF_REQUIRE_UIT();

  // Never notify for temporary objects.
  DCHECK(!frame->is_temporary());

  MaybeExecuteFrameNotification(base::BindOnce(
      [](scoped_refptr<CefBrowserInfo> self, CefRefPtr<CefFrameHostImpl> frame,
         CefRefPtr<CefFrameHandler> handler) {
        if (auto browser = self->browser()) {
          handler->OnFrameCreated(browser, frame);
        }
      },
      scoped_refptr<CefBrowserInfo>(this), frame));
}

// Passing in |browser| here because |browser_| may already be cleared.
void CefBrowserInfo::MaybeNotifyFrameDetached(
    CefRefPtr<CefBrowserHostBase> browser,
    CefRefPtr<CefFrameHostImpl> frame) {
  CEF_REQUIRE_UIT();

  // Never notify for temporary objects.
  DCHECK(!frame->is_temporary());

  MaybeExecuteFrameNotification(base::BindOnce(
      [](CefRefPtr<CefBrowserHostBase> browser,
         CefRefPtr<CefFrameHostImpl> frame,
         CefRefPtr<CefFrameHandler> handler) {
        handler->OnFrameDetached(browser, frame);
      },
      browser, frame));
}

// Passing in |browser| here because |browser_| may already be cleared.
void CefBrowserInfo::MaybeNotifyMainFrameChanged(
    CefRefPtr<CefBrowserHostBase> browser,
    CefRefPtr<CefFrameHostImpl> old_frame,
    CefRefPtr<CefFrameHostImpl> new_frame) {
  CEF_REQUIRE_UIT();

  // Never notify for temporary objects.
  DCHECK(!old_frame || !old_frame->is_temporary());
  DCHECK(!new_frame || !new_frame->is_temporary());

  MaybeExecuteFrameNotification(base::BindOnce(
      [](CefRefPtr<CefBrowserHostBase> browser,
         CefRefPtr<CefFrameHostImpl> old_frame,
         CefRefPtr<CefFrameHostImpl> new_frame,
         CefRefPtr<CefFrameHandler> handler) {
        handler->OnMainFrameChanged(browser, old_frame, new_frame);
      },
      browser, old_frame, new_frame));
}

void CefBrowserInfo::RemoveAllFrames(
    CefRefPtr<CefBrowserHostBase> old_browser) {
  lock_.AssertAcquired();

  // Make sure any callbacks will see the correct state (e.g. like
  // CefBrowser::GetMainFrame returning nullptr and CefBrowser::IsValid
  // returning false).
  DCHECK(!browser_);
  DCHECK(old_browser);

  // Clear the lookup maps.
  frame_id_map_.clear();
  frame_token_to_id_map_.clear();

  // Explicitly Detach everything but the current main frame.
  for (auto& info : frame_info_set_) {
    if (info->frame_ && !info->IsCurrentMainFrame()) {
      if (info->frame_->Detach(
              CefFrameHostImpl::DetachReason::BROWSER_DESTROYED)) {
        MaybeNotifyFrameDetached(old_browser, info->frame_);
      }
    }
  }

  if (main_frame_) {
    SetMainFrame(old_browser, nullptr);
  }

  // And finally delete the frame info.
  frame_info_set_.clear();
}

CefBrowserInfo::NotificationStateLock::NotificationStateLock(
    CefBrowserInfo* browser_info)
    : browser_info_(browser_info) {
  CEF_REQUIRE_UIT();

  // Take the navigation state lock.
  {
    base::AutoLock lock_scope_(browser_info_->notification_lock_);
    CHECK(!browser_info_->notification_state_lock_);
    browser_info_->notification_state_lock_ = this;
    // We may need this on destruction, and the original might be cleared.
    frame_handler_ = browser_info_->frame_handler_;
  }

  // Take the browser info state lock.
  browser_info_lock_scope_ =
      std::make_unique<base::AutoLock>(browser_info_->lock_);
}

CefBrowserInfo::NotificationStateLock::~NotificationStateLock() {
  CEF_REQUIRE_UIT();

  // Unlock in reverse order.
  browser_info_lock_scope_.reset();

  {
    base::AutoLock lock_scope_(browser_info_->notification_lock_);
    CHECK_EQ(this, browser_info_->notification_state_lock_);
    browser_info_->notification_state_lock_ = nullptr;
  }

  if (!queue_.empty()) {
    DCHECK(frame_handler_);

    // Don't navigate while inside callbacks.
    auto nav_lock = browser_info_->CreateNavigationLock();

    // Empty the queue of pending actions. Any of these actions might result in
    // the acquisition of a new NotificationStateLock.
    while (!queue_.empty()) {
      std::move(queue_.front()).Run(frame_handler_);
      queue_.pop();
    }
  }
}
