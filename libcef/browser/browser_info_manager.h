// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_BROWSER_INFO_MANAGER_H_
#define CEF_LIBCEF_BROWSER_BROWSER_INFO_MANAGER_H_
#pragma once

#include "include/cef_client.h"

#include <map>
#include <memory>
#include <vector>

#include "libcef/browser/browser_info.h"

#include "base/synchronization/lock.h"
#include "content/public/browser/render_process_host_observer.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace blink {
struct WebWindowFeatures;
}

namespace content {
struct Referrer;
class RenderFrameHost;
class RenderViewHostDelegateView;
class WebContents;
class WebContentsView;
}  // namespace content

namespace IPC {
class Message;
}

class CefBrowserPlatformDelegate;

// Singleton object for managing BrowserInfo instances.
class CefBrowserInfoManager : public content::RenderProcessHostObserver {
 public:
  CefBrowserInfoManager();
  ~CefBrowserInfoManager() override;

  // Returns this singleton instance of this class.
  static CefBrowserInfoManager* GetInstance();

  // Called from CefBrowserHostImpl::Create when a new browser is being created
  // directly. In this case |is_popup| will be true only for DevTools browsers.
  scoped_refptr<CefBrowserInfo> CreateBrowserInfo(
      bool is_popup,
      bool is_windowless,
      CefRefPtr<CefDictionaryValue> extra_info);

  // Called from CefBrowserHostImpl::WebContentsCreated when a new browser is
  // being created for a traditional popup (e.g. window.open() or targeted
  // link). If any OnGetNewBrowserInfo requests are pending for the popup the
  // response will be sent when this method is called.
  scoped_refptr<CefBrowserInfo> CreatePopupBrowserInfo(
      content::WebContents* new_contents,
      bool is_windowless,
      CefRefPtr<CefDictionaryValue> extra_info);

  // Called from CefContentBrowserClient::CanCreateWindow. See comments on
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

  // Called from CefBrowserHostImpl::GetCustomWebContentsView. See comments on
  // PendingPopup for more information.
  void GetCustomWebContentsView(
      const GURL& target_url,
      int opener_render_process_id,
      int opener_render_routing_id,
      content::WebContentsView** view,
      content::RenderViewHostDelegateView** delegate_view);

  // Called from CefBrowserHostImpl::WebContentsCreated. See comments on
  // PendingPopup for more information.
  void WebContentsCreated(
      const GURL& target_url,
      int opener_render_process_id,
      int opener_render_routing_id,
      CefBrowserSettings& settings,
      CefRefPtr<CefClient>& client,
      std::unique_ptr<CefBrowserPlatformDelegate>& platform_delegate,
      CefRefPtr<CefDictionaryValue>& extra_info);

  // Called from CefBrowserMessageFilter::OnGetNewBrowserInfo for delivering
  // browser info to the renderer process. If the browser info already exists
  // the response will be sent immediately. Otherwise, the response will be sent
  // when CreatePopupBrowserInfo creates the browser info. The info will already
  // exist for explicitly created browsers and guest views. It may sometimes
  // already exist for traditional popup browsers depending on timing. See
  // comments on PendingPopup for more information.
  void OnGetNewBrowserInfo(int render_process_id,
                           int render_routing_id,
                           IPC::Message* reply_msg);

  // Called from CefBrowserHostImpl::DestroyBrowser() when a browser is
  // destroyed.
  void RemoveBrowserInfo(scoped_refptr<CefBrowserInfo> browser_info);

  // Called from CefContext::FinishShutdownOnUIThread() to destroy all browsers.
  void DestroyAllBrowsers();

  // Returns the CefBrowserInfo matching the specified IDs or nullptr if no
  // match is found. It is allowed to add new callers of this method but
  // consider using CefBrowserHostImpl::GetBrowserForFrameRoute() or
  // extensions::GetOwnerBrowserForFrameRoute() instead. If |is_guest_view| is
  // non-nullptr it will be set to true if the IDs match a guest view associated
  // with the returned browser info instead of the browser itself.
  scoped_refptr<CefBrowserInfo> GetBrowserInfoForFrameRoute(
      int render_process_id,
      int render_routing_id,
      bool* is_guest_view = nullptr);

  // Returns the CefBrowserInfo matching the specified ID or nullptr if no match
  // is found. It is allowed to add new callers of this method but consider
  // using CefBrowserHostImpl::GetBrowserForFrameTreeNode() instead. If
  // |is_guest_view| is non-nullptr it will be set to true if the IDs match a
  // guest view associated with the returned browser info instead of the browser
  // itself.
  scoped_refptr<CefBrowserInfo> GetBrowserInfoForFrameTreeNode(
      int frame_tree_node_id,
      bool* is_guest_view = nullptr);

  // Returns all existing CefBrowserInfo objects.
  typedef std::list<scoped_refptr<CefBrowserInfo>> BrowserInfoList;
  BrowserInfoList GetBrowserInfoList();

 private:
  // RenderProcessHostObserver methods:
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

  // Store state information about pending popups. Call order is:
  // - CefContentBrowserClient::CanCreateWindow (UIT)
  //   Provides an opportunity to cancel the popup (calls OnBeforePopup) and
  //   creates the new platform delegate for the popup. If the popup owner is
  //   an extension guest view then the popup is canceled and
  //   CefBrowserHostImpl::OpenURLFromTab is called.
  // And then the following calls may occur at the same time:
  // - CefBrowserHostImpl::GetCustomWebContentsView (UIT)
  //   Creates the OSR views for windowless popups.
  // - CefBrowserHostImpl::WebContentsCreated (UIT)
  //   Creates the CefBrowserHostImpl representation for the popup.
  // - CefBrowserMessageFilter::OnGetNewBrowserInfo (IOT)
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
    int opener_render_process_id;
    int opener_render_routing_id;
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
  std::unique_ptr<PendingPopup> PopPendingPopup(PendingPopup::Step step,
                                                int opener_process_id,
                                                int opener_routing_id,
                                                const GURL& target_url);

  // Retrieves the BrowserInfo matching the specified IDs. If both sets are
  // valid then this method makes sure both sets have been registered.
  scoped_refptr<CefBrowserInfo> GetBrowserInfo(int render_process_id,
                                               int render_routing_id,
                                               bool* is_guest_view);

  // Send the response for a pending OnGetNewBrowserInfo request.
  static void SendNewBrowserInfoResponse(
      int render_process_id,
      scoped_refptr<CefBrowserInfo> browser_info,
      bool is_guest_view,
      IPC::Message* reply_msg);

  // Pending request for OnGetNewBrowserInfo.
  struct PendingNewBrowserInfo {
    int render_process_id;
    int render_routing_id;
    IPC::Message* reply_msg;
  };

  mutable base::Lock browser_info_lock_;

  // Access to the below members must be protected by |browser_info_lock_|.

  BrowserInfoList browser_info_list_;
  int next_browser_id_;

  // Map of frame ID to info.
  using PendingNewBrowserInfoMap =
      std::map<int64_t, std::unique_ptr<PendingNewBrowserInfo>>;
  PendingNewBrowserInfoMap pending_new_browser_info_map_;

  // Only accessed on the UI thread.
  using PendingPopupList = std::vector<std::unique_ptr<PendingPopup>>;
  PendingPopupList pending_popup_list_;

  DISALLOW_COPY_AND_ASSIGN(CefBrowserInfoManager);
};

#endif  // CEF_LIBCEF_BROWSER_BROWSER_INFO_H_
