// Copyright 2020 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/net/throttle_handler.h"

#include "libcef/browser/browser_host_base.h"
#include "libcef/browser/browser_info_manager.h"
#include "libcef/browser/frame_host_impl.h"
#include "libcef/common/frame_util.h"
#include "libcef/common/request_impl.h"

#include "components/navigation_interception/intercept_navigation_throttle.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/page_navigator.h"

namespace throttle {

namespace {

bool NavigationOnUIThread(content::NavigationHandle* navigation_handle) {
  CEF_REQUIRE_UIT();

  const bool is_main_frame = navigation_handle->IsInMainFrame();
  const auto global_id = frame_util::GetGlobalId(navigation_handle);

  // Identify the RenderFrameHost that originated the navigation.
  const auto parent_global_id =
      !is_main_frame ? navigation_handle->GetParentFrame()->GetGlobalId()
                     : frame_util::InvalidGlobalId();

  const content::Referrer referrer(navigation_handle->GetReferrer().url,
                                   navigation_handle->GetReferrer().policy);

  content::OpenURLParams open_params(navigation_handle->GetURL(), referrer,
                                     WindowOpenDisposition::CURRENT_TAB,
                                     navigation_handle->GetPageTransition(),
                                     navigation_handle->IsRendererInitiated());
  open_params.user_gesture = navigation_handle->HasUserGesture();
  open_params.initiator_origin = navigation_handle->GetInitiatorOrigin();
  open_params.is_pdf = navigation_handle->IsPdf();

  CefRefPtr<CefBrowserHostBase> browser;
  if (!CefBrowserInfoManager::GetInstance()->MaybeAllowNavigation(
          navigation_handle->GetWebContents()->GetPrimaryMainFrame(),
          open_params, browser)) {
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
        } else {
          frame = browser->GetFrameForGlobalId(global_id);
        }
        if (!frame) {
          // Create a temporary frame object for navigation of sub-frames that
          // don't yet exist.
          frame = browser->browser_info()->CreateTempSubFrame(parent_global_id);
        }

        CefRefPtr<CefRequestImpl> request = new CefRequestImpl();
        request->Set(navigation_handle);
        request->SetReadOnly(true);

        // Initiating a new navigation in OnBeforeBrowse will delete the
        // InterceptNavigationThrottle that currently owns this callback,
        // resulting in a crash. Use the lock to prevent that.
        auto navigation_lock = browser->browser_info()->CreateNavigationLock();
        ignore_navigation =
            handler->OnBeforeBrowse(browser.get(), frame, request.get(),
                                    navigation_handle->HasUserGesture(),
                                    navigation_handle->WasServerRedirect());
      }
    }
  }

  return ignore_navigation;
}

}  // namespace

void CreateThrottlesForNavigation(content::NavigationHandle* navigation_handle,
                                  NavigationThrottleList& throttles) {
  CEF_REQUIRE_UIT();

  // Must use SynchronyMode::kSync to ensure that OnBeforeBrowse is always
  // called before OnBeforeResourceLoad.
  std::unique_ptr<content::NavigationThrottle> throttle =
      std::make_unique<navigation_interception::InterceptNavigationThrottle>(
          navigation_handle, base::BindRepeating(&NavigationOnUIThread),
          navigation_interception::SynchronyMode::kSync);
  throttles.push_back(std::move(throttle));
}

}  // namespace throttle
