// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_BROWSER_INFO_MANAGER_H_
#define CEF_LIBCEF_BROWSER_BROWSER_INFO_MANAGER_H_
#pragma once

#include "include/cef_client.h"

#include <list>
#include <map>
#include <memory>
#include <vector>

#include "libcef/browser/browser_info.h"

#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "cef/libcef/common/mojom/cef.mojom.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_process_host_observer.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace blink {
struct WebWindowFeatures;
}

namespace content {
struct OpenURLParams;
struct Referrer;
class RenderFrameHost;
class RenderViewHostDelegateView;
class WebContents;
class WebContentsView;
}  // namespace content

class CefBrowserHostBase;
class CefBrowserPlatformDelegate;

// Singleton object for managing BrowserInfo instances.
class CefBrowserInfoManager : public content::RenderProcessHostObserver {
 public:
  CefBrowserInfoManager();

  CefBrowserInfoManager(const CefBrowserInfoManager&) = delete;
  CefBrowserInfoManager& operator=(const CefBrowserInfoManager&) = delete;

  ~CefBrowserInfoManager() override;

  // Returns this singleton instance of this class.
  static CefBrowserInfoManager* GetInstance();

  // Called immediately before a new CefBrowserHost implementation is created
  // directly. In this case |is_popup| will be true only for DevTools browsers.
  scoped_refptr<CefBrowserInfo> CreateBrowserInfo(
      bool is_popup,
      bool is_windowless,
      CefRefPtr<CefDictionaryValue> extra_info);

  // Called from WebContentsDelegate::WebContentsCreated when a new browser is
  // being created for a traditional popup (e.g. window.open() or targeted
  // link). If any OnGetNewBrowserInfo requests are pending for the popup the
  // response will be sent when this method is called.
  scoped_refptr<CefBrowserInfo> CreatePopupBrowserInfo(
      content::WebContents* new_contents,
      bool is_windowless,
      CefRefPtr<CefDictionaryValue> extra_info);

  // Called from ContentBrowserClient::CanCreateWindow. See comments on
  // PendingPopup for more information.
  bool CanCreateWindow(content::RenderFrameHost* opener,
                       const GURL& target_url,
                       const content::Referrer& referrer,
                       const std::string& frame_name,
                       WindowOpenDisposition disposition,
                       const blink::mojom::WindowFeatures& features,
                       bool user_gesture,
                       bool opener_suppressed,
                       bool* no_javascript_access);

  // Called from WebContentsDelegate::GetCustomWebContentsView (alloy runtime
  // only). See comments on PendingPopup for more information.
  void GetCustomWebContentsView(
      const GURL& target_url,
      const content::GlobalRenderFrameHostId& opener_global_id,
      content::WebContentsView** view,
      content::RenderViewHostDelegateView** delegate_view);

  // Called from WebContentsDelegate::WebContentsCreated. See comments on
  // PendingPopup for more information.
  void WebContentsCreated(
      const GURL& target_url,
      const content::GlobalRenderFrameHostId& opener_global_id,
      CefBrowserSettings& settings,
      CefRefPtr<CefClient>& client,
      std::unique_ptr<CefBrowserPlatformDelegate>& platform_delegate,
      CefRefPtr<CefDictionaryValue>& extra_info);

  // Called from CefBrowserManager::GetNewBrowserInfo for delivering
  // browser info to the renderer process. If the browser info already exists
  // the response will be sent immediately. Otherwise, the response will be sent
  // when CreatePopupBrowserInfo creates the browser info. The info will already
  // exist for explicitly created browsers and guest views. It may sometimes
  // already exist for traditional popup browsers depending on timing. See
  // comments on PendingPopup for more information.
  void OnGetNewBrowserInfo(
      const content::GlobalRenderFrameHostId& global_id,
      cef::mojom::BrowserManager::GetNewBrowserInfoCallback callback);

  // Called from CefBrowserHostBase::DestroyBrowser() when a browser is
  // destroyed.
  void RemoveBrowserInfo(scoped_refptr<CefBrowserInfo> browser_info);

  // Called from CefContext::FinishShutdownOnUIThread() to destroy all browsers.
  void DestroyAllBrowsers();

  // Returns the CefBrowserInfo matching the specified ID or nullptr if no
  // match is found. It is allowed to add new callers of this method but
  // consider using CefBrowserHostBase::GetBrowserForGlobalId() or
  // extensions::GetOwnerBrowserForGlobalId() instead. If |is_guest_view| is
  // non-nullptr it will be set to true if the ID matches a guest view
  // associated with the returned browser info instead of the browser itself.
  scoped_refptr<CefBrowserInfo> GetBrowserInfo(
      const content::GlobalRenderFrameHostId& global_id,
      bool* is_guest_view = nullptr);

  // Returns all existing CefBrowserInfo objects.
  using BrowserInfoList = std::list<scoped_refptr<CefBrowserInfo>>;
  BrowserInfoList GetBrowserInfoList();

  // Returns true if the navigation should be allowed to proceed, or false if
  // the navigation will instead be sent via OpenURLFromTab. If allowed,
  // |browser| will be set to the target browser if any.
  bool MaybeAllowNavigation(content::RenderFrameHost* opener,
                            const content::OpenURLParams& params,
                            CefRefPtr<CefBrowserHostBase>& browser) const;

 private:
  // RenderProcessHostObserver methods:
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

  // Store state information about pending popups. Call order is:
  // - CanCreateWindow (UIT):
  //   Provides an opportunity to cancel the popup (calls OnBeforePopup) and
  //   creates the new platform delegate for the popup. If the popup owner is
  //   an extension guest view then the popup is canceled and
  //   WebContentsDelegate::OpenURLFromTab is called via the
  //   CefBrowserHostBase::MaybeAllowNavigation implementation.
  // And then the following calls may occur at the same time:
  // - GetCustomWebContentsView (UIT) (alloy runtime only):
  //   Creates the OSR views for windowless popups.
  // - WebContentsCreated (UIT):
  //   Creates the CefBrowserHost representation for the popup.
  // - CefBrowserManager::GetNewBrowserInfo (IOT)
  //   Passes information about the popup to the renderer process.
  struct PendingPopup {
    // Track the last method that modified this PendingPopup instance. There may
    // be multiple pending popups with the same identifiers and this allows us
    // to differentiate between them at different processing steps.
    enum Step {
      CAN_CREATE_WINDOW,
      GET_CUSTOM_WEB_CONTENTS_VIEW,
    } step;

    // Initial state from ViewHostMsg_CreateWindow.
    // |target_url| will be empty if a popup is created via window.open() and
    // never navigated. For example: javascript:window.open();
    content::GlobalRenderFrameHostId opener_global_id;
    GURL target_url;
    std::string target_frame_name;

    // Values specified by OnBeforePopup.
    CefBrowserSettings settings;
    CefRefPtr<CefClient> client;
    CefRefPtr<CefDictionaryValue> extra_info;

    // Platform delegate specific to the new popup.
    std::unique_ptr<CefBrowserPlatformDelegate> platform_delegate;
  };

  // Manage pending popups. Only called on the UI thread.
  void PushPendingPopup(std::unique_ptr<PendingPopup> popup);
  std::unique_ptr<PendingPopup> PopPendingPopup(
      PendingPopup::Step step,
      const content::GlobalRenderFrameHostId& opener_global_id,
      const GURL& target_url);

  // Retrieves the BrowserInfo matching the specified ID.
  scoped_refptr<CefBrowserInfo> GetBrowserInfoInternal(
      const content::GlobalRenderFrameHostId& global_id,
      bool* is_guest_view);

  // Send the response for a pending OnGetNewBrowserInfo request.
  static void SendNewBrowserInfoResponse(
      scoped_refptr<CefBrowserInfo> browser_info,
      bool is_guest_view,
      cef::mojom::BrowserManager::GetNewBrowserInfoCallback callback,
      scoped_refptr<base::SequencedTaskRunner> callback_runner);

  // Pending request for OnGetNewBrowserInfo.
  struct PendingNewBrowserInfo {
    content::GlobalRenderFrameHostId global_id;
    int timeout_id;
    cef::mojom::BrowserManager::GetNewBrowserInfoCallback callback;
    scoped_refptr<base::SequencedTaskRunner> callback_runner;
  };

  // Cancel a response that is still pending.
  static void CancelNewBrowserInfoResponse(PendingNewBrowserInfo* pending_info);

  // Time out a response if it's still pending.
  static void TimeoutNewBrowserInfoResponse(
      const content::GlobalRenderFrameHostId& global_id,
      int timeout_id);

  mutable base::Lock browser_info_lock_;

  // Access to the below members must be protected by |browser_info_lock_|.

  BrowserInfoList browser_info_list_;
  int next_browser_id_ = 0;

  // Map of global ID to info. These IDs are guaranteed to uniquely
  // identify a RFH for its complete lifespan. See documentation on
  // RenderFrameHost::GetFrameTreeNodeId() for background.
  using PendingNewBrowserInfoMap =
      std::map<content::GlobalRenderFrameHostId,
               std::unique_ptr<PendingNewBrowserInfo>>;
  PendingNewBrowserInfoMap pending_new_browser_info_map_;

  // Only accessed on the UI thread.
  using PendingPopupList = std::vector<std::unique_ptr<PendingPopup>>;
  PendingPopupList pending_popup_list_;

  int next_timeout_id_ = 0;
};

#endif  // CEF_LIBCEF_BROWSER_BROWSER_INFO_H_
