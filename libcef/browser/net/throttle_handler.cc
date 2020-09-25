// Copyright 2020 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/net/throttle_handler.h"

#include "libcef/browser/browser_host_base.h"
#include "libcef/browser/browser_info_manager.h"
#include "libcef/browser/frame_host_impl.h"
#include "libcef/common/request_impl.h"

#include "components/navigation_interception/intercept_navigation_throttle.h"
#include "components/navigation_interception/navigation_params.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/page_navigator.h"

namespace throttle {

namespace {

// TODO(cef): We can't currently trust NavigationParams::is_main_frame() because
// it's always set to true in
// InterceptNavigationThrottle::CheckIfShouldIgnoreNavigation. Remove the
// |is_main_frame| argument once this problem is fixed.
bool NavigationOnUIThread(
    bool is_main_frame,
    int64_t frame_id,
    int64_t parent_frame_id,
    int frame_tree_node_id,
    content::WebContents* source,
    const navigation_interception::NavigationParams& params) {
  CEF_REQUIRE_UIT();

  content::OpenURLParams open_params(
      params.url(), params.referrer(), WindowOpenDisposition::CURRENT_TAB,
      params.transition_type(), params.is_renderer_initiated());
  open_params.user_gesture = params.has_user_gesture();
  open_params.initiator_origin = params.initiator_origin();

  CefRefPtr<CefBrowserHostBase> browser;
  if (!CefBrowserInfoManager::GetInstance()->MaybeAllowNavigation(
          source->GetMainFrame(), open_params, browser)) {
    // Cancel the navigation.
    return true;
  }

  bool ignore_navigation = false;

  if (browser) {
    if (auto client = browser->GetClient()) {
      if (auto handler = client->GetRequestHandler()) {
        CefRefPtr<CefFrame> frame;
        if (is_main_frame) {
          frame = browser->GetMainFrame();
        } else if (frame_id >= 0) {
          frame = browser->GetFrame(frame_id);
        }
        if (!frame && frame_tree_node_id >= 0) {
          frame = browser->GetFrameForFrameTreeNode(frame_tree_node_id);
        }
        if (!frame) {
          // Create a temporary frame object for navigation of sub-frames that
          // don't yet exist.
          frame = browser->browser_info()->CreateTempSubFrame(parent_frame_id);
        }

        CefRefPtr<CefRequestImpl> request = new CefRequestImpl();
        request->Set(params, is_main_frame);
        request->SetReadOnly(true);

        // Initiating a new navigation in OnBeforeBrowse will delete the
        // InterceptNavigationThrottle that currently owns this callback,
        // resulting in a crash. Use the lock to prevent that.
        auto navigation_lock = browser->browser_info()->CreateNavigationLock();
        ignore_navigation = handler->OnBeforeBrowse(
            browser.get(), frame, request.get(), params.has_user_gesture(),
            params.is_redirect());
      }
    }
  }

  return ignore_navigation;
}

}  // namespace

void CreateThrottlesForNavigation(content::NavigationHandle* navigation_handle,
                                  NavigationThrottleList& throttles) {
  CEF_REQUIRE_UIT();

  const bool is_main_frame = navigation_handle->IsInMainFrame();

  // Identify the RenderFrameHost that originated the navigation.
  const int64_t parent_frame_id =
      !is_main_frame
          ? CefFrameHostImpl::MakeFrameId(navigation_handle->GetParentFrame())
          : CefFrameHostImpl::kInvalidFrameId;

  const int64_t frame_id = !is_main_frame && navigation_handle->HasCommitted()
                               ? CefFrameHostImpl::MakeFrameId(
                                     navigation_handle->GetRenderFrameHost())
                               : CefFrameHostImpl::kInvalidFrameId;

  // Must use SynchronyMode::kSync to ensure that OnBeforeBrowse is always
  // called before OnBeforeResourceLoad.
  std::unique_ptr<content::NavigationThrottle> throttle =
      std::make_unique<navigation_interception::InterceptNavigationThrottle>(
          navigation_handle,
          base::Bind(&NavigationOnUIThread, is_main_frame, frame_id,
                     parent_frame_id, navigation_handle->GetFrameTreeNodeId()),
          navigation_interception::SynchronyMode::kSync);
  throttles.push_back(std::move(throttle));
}

}  // namespace throttle
