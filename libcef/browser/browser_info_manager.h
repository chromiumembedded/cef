// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_BROWSER_INFO_MANAGER_H_
#define CEF_LIBCEF_BROWSER_BROWSER_INFO_MANAGER_H_
#pragma once

#include <list>
#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "cef/include/cef_client.h"
#include "cef/libcef/browser/browser_info.h"
#include "cef/libcef/browser/browser_info_request_state.h"
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
  // directly.
  scoped_refptr<CefBrowserInfo> CreateBrowserInfo(
      bool is_devtools_popup,
      const cef::BrowserConfig& config,
      CefRefPtr<CefDictionaryValue> extra_info);

  // Called from WebContentsDelegate::WebContentsCreated when a new browser is
  // being created for a traditional popup (e.g. window.open() or targeted
  // link). If any OnGetNewBrowserInfo requests are pending for the popup the
  // response will be sent when this method is called.
  scoped_refptr<CefBrowserInfo> CreatePopupBrowserInfo(
      content::WebContents* new_contents,
      const cef::BrowserConfig& config,
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

  // Called from ContentBrowserClient::CreateWindowResult if CanCreateWindow
  // returns true. See comments on PendingPopup for more information.
  void CreateWindowResult(content::RenderFrameHost* opener, bool success);

  // Called from WebContentsDelegate::GetCustomWebContentsView (Alloy style
  // only). See comments on PendingPopup for more information.
  void GetCustomWebContentsView(
      const GURL& target_url,
      const content::GlobalRenderFrameHostId& opener_global_id,
      raw_ptr<content::WebContentsView>* view,
      raw_ptr<content::RenderViewHostDelegateView>* delegate_view);

  // Called from WebContentsDelegate::WebContentsCreated. See comments on
  // PendingPopup for more information.
  void WebContentsCreated(
      const GURL& target_url,
      const content::GlobalRenderFrameHostId& opener_global_id,
      CefBrowserSettings& settings,
      CefRefPtr<CefClient>& client,
      std::unique_ptr<CefBrowserPlatformDelegate>& platform_delegate,
      CefRefPtr<CefDictionaryValue>& extra_info,
      content::WebContents* new_contents);

  // Called from ChromeBrowserDelegate::AddWebContents. See comments on
  // PendingPopup for more information. Returns true for custom handling.
  bool AddWebContents(content::WebContents* source_contents);

  // Called from CefBrowserManager::GetNewBrowserInfo to deliver browser info
  // to the renderer process. Responds immediately if the frame is registered;
  // otherwise queues the request until CreatePopupBrowserInfo or OnFrameCreated
  // supplies the info, the UI-thread check identifies excluded content or a
  // missing RFH, the frame or renderer process is deleted, or the request times
  // out (unless the timeout is disabled). Responses run on the calling
  // sequence. See PendingPopup for popup creation ordering.
  void OnGetNewBrowserInfo(
      const content::GlobalRenderFrameHostToken& global_token,
      cef::mojom::BrowserManager::GetNewBrowserInfoCallback callback);

  // Called from CefBrowserHostBase::DestroyBrowser() when a browser is
  // destroyed.
  void RemoveBrowserInfo(scoped_refptr<CefBrowserInfo> browser_info);

  // Called from CefContext::FinishShutdownOnUIThread() to destroy all browsers.
  void DestroyAllBrowsers();

  // Called by CefBrowserInfo::MaybeCreateFrame after registering a main frame
  // or subframe and releasing its notification lock. Completes any pending
  // lookup for that frame.
  void OnFrameCreated(const content::GlobalRenderFrameHostToken& global_token,
                      scoped_refptr<CefBrowserInfo> browser_info);

  // Cancels a pending lookup after RenderFrameDeleted, even if CEF never
  // registered the frame, or when the UI-thread check finds no RFH.
  void OnFrameDeleted(const content::GlobalRenderFrameHostToken& global_token);

  // Returns the CefBrowserInfo matching the specified ID/token or nullptr if no
  // match is found. It is allowed to add new callers of this method but
  // consider using CefBrowserHostBase::GetBrowserForGlobalId/Token() instead.
  scoped_refptr<CefBrowserInfo> GetBrowserInfo(
      const content::GlobalRenderFrameHostId& global_id);
  scoped_refptr<CefBrowserInfo> GetBrowserInfo(
      const content::GlobalRenderFrameHostToken& global_token);

  // Returns all existing CefBrowserInfo objects.
  using BrowserInfoList = std::list<scoped_refptr<CefBrowserInfo>>;
  BrowserInfoList GetBrowserInfoList();

  // Returns true if the navigation should be allowed to proceed, or false if
  // the navigation will instead be sent via OpenURLFromTab. If allowed,
  // |browser| will be set to the target browser if any.
  bool MaybeAllowNavigation(content::RenderFrameHost* opener,
                            const content::OpenURLParams& params,
                            CefRefPtr<CefBrowserHostBase>& browser) const;

  static bool ShouldCreateViewsHostedPopup(CefRefPtr<CefBrowserHostBase> opener,
                                           bool use_default_browser_creation);

  // Returns the FrameHost associated with |rfh|, if any. |browser_info| and
  // |is_excluded| will be populated if non-nullptr. An excluded type will not
  // have a FrameHost but |browser_info| may still be populated if the
  // association is known.
  static CefRefPtr<CefFrameHostImpl> GetFrameHost(content::RenderFrameHost* rfh,
                                                  bool prefer_speculative,
                                                  CefBrowserInfo** browser_info,
                                                  bool* is_excluded);

  // Returns true if |rfh| should be excluded (no FrameHost created).
  static bool IsExcludedFrameHost(content::RenderFrameHost* rfh);

 private:
  // RenderProcessHostObserver methods:
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

  // Store state information about pending popups. The UIT callbacks occur
  // synchronously during RenderFrameHostImpl::CreateNewWindow execution. The
  // result of CreateNewWindow execution will be passed to CreateWindowResult
  // (may call OnBeforePopupAborted; see documentation in that method). Call
  // order for successful popup creation is is:
  // - CanCreateWindow (UIT):
  //   Provides an opportunity to cancel the popup (calls OnBeforePopup) and
  //   creates the new platform delegate for the popup. If the popup owner is
  //   an extension guest view (PDF viewer) then the popup is canceled and
  //   WebContentsDelegate::OpenURLFromTab is called via the
  //   CefBrowserHostBase::MaybeAllowNavigation implementation.
  // And then the following UIT and IOT calls may occur at the same time:
  // - GetCustomWebContentsView (UIT) (Alloy style only):
  //   Creates the OSR views for windowless popups.
  // - WebContentsCreated (UIT):
  //   Creates the CefBrowserHost representation for the popup (calls
  //   OnAfterCreated).
  // - AddWebContents (UIT) (Chrome style only):
  //   Creates the Browser or tab representation for the popup.
  // - CefBrowserManager::GetNewBrowserInfo (IOT)
  //   Passes information about the popup to the renderer process.
  struct PendingPopup {
    ~PendingPopup();

    // Used to notify if popup creation is aborted.
    base::OnceClosure aborted_callback;

    // Track the last method that modified this PendingPopup instance. There may
    // be multiple pending popups with the same identifiers and this allows us
    // to differentiate between them at different processing steps.
    enum Step {
      CAN_CREATE_WINDOW,
      GET_CUSTOM_WEB_CONTENTS_VIEW,
      WEB_CONTENTS_CREATED,
      CREATION_COMPLETE,
    } step;

    // True if this popup is Alloy style, otherwise Chrome style.
    bool alloy_style;

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

    // True if default Browser or tab creation should proceed from
    // AddWebContents (Chrome style only).
    bool use_default_browser_creation = false;

    // The newly created WebContents (set in WebContentsCreated).
    raw_ptr<content::WebContents> new_contents = nullptr;
  };

  // Manage pending popups. Only called on the UI thread.
  void PushPendingPopup(std::unique_ptr<PendingPopup> popup);

  // Used after CanCreateWindow is called.
  std::unique_ptr<PendingPopup> PopPendingPopup(
      PendingPopup::Step previous_step_alloy,
      PendingPopup::Step previous_step_chrome,
      const content::GlobalRenderFrameHostId& opener_global_id,
      const GURL& target_url);

  // Used after WebContentsCreated is called.
  std::unique_ptr<PendingPopup> PopPendingPopup(
      PendingPopup::Step previous_step_alloy,
      PendingPopup::Step previous_step_chrome,
      content::WebContents* new_contents);

  // Retrieves the BrowserInfo matching the specified ID/token.
  scoped_refptr<CefBrowserInfo> GetBrowserInfoInternal(
      const content::GlobalRenderFrameHostId& global_id);
  scoped_refptr<CefBrowserInfo> GetBrowserInfoInternal(
      const content::GlobalRenderFrameHostToken& global_token);

  // On the UI thread, cancels lookups with no RFH and completes lookups for
  // excluded content. Other lookups remain pending.
  static void CheckNewBrowserInfoOnUIThread(
      const content::GlobalRenderFrameHostToken& global_token);

  // Completes a pending lookup, delivering the response on its calling
  // sequence.
  void ContinueNewBrowserInfo(
      const content::GlobalRenderFrameHostToken& global_token,
      scoped_refptr<CefBrowserInfo> browser_info,
      bool is_excluded);

  // Builds and sends a response on the current sequence. Pending requests reach
  // this method on their calling sequence via BrowserInfoRequestState.
  static void SendNewBrowserInfoResponse(
      scoped_refptr<CefBrowserInfo> browser_info,
      bool is_excluded,
      cef::mojom::BrowserManager::GetNewBrowserInfoCallback callback);

  using BrowserInfoRequestState =
      CefBrowserInfoRequestState<content::GlobalRenderFrameHostToken,
                                 scoped_refptr<CefBrowserInfo>>;

  // Cancels and logs a lookup only if it is still pending with this timeout ID.
  static void TimeoutNewBrowserInfoResponse(
      const content::GlobalRenderFrameHostToken& global_token,
      BrowserInfoRequestState::TimeoutId timeout_id);

  mutable base::Lock browser_info_lock_;

  // Access to the below members must be protected by |browser_info_lock_|.

  BrowserInfoList browser_info_list_;
  int next_browser_id_ = 0;

  // Tokens uniquely identify a RFH for its complete lifespan.
  BrowserInfoRequestState pending_new_browser_info_;

  // Only accessed on the UI thread.
  using PendingPopupList = std::vector<std::unique_ptr<PendingPopup>>;
  PendingPopupList pending_popup_list_;

  // Current popup pending creation during RenderFrameHostImpl::CreateNewWindow
  // execution (valid from CanCreateWindow returning true to WebContentsCreated
  // or CreateWindowResult being called). Only accessed on the UI thread.
  raw_ptr<PendingPopup> pending_create_popup_ = nullptr;
};

#endif  // CEF_LIBCEF_BROWSER_BROWSER_INFO_H_
