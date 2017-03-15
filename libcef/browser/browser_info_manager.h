// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_BROWSER_INFO_MANAGER_H_
#define CEF_LIBCEF_BROWSER_BROWSER_INFO_MANAGER_H_
#pragma once

#include "include/cef_client.h"

#include <list>

#include "libcef/browser/browser_info.h"

#include "base/memory/scoped_vector.h"
#include "base/synchronization/lock.h"
#include "content/public/browser/render_process_host_observer.h"
#include "third_party/WebKit/public/web/window_features.mojom.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace blink {
struct WebWindowFeatures;
}

namespace content {
struct Referrer;
class RenderViewHostDelegateView;
class WebContents;
class WebContentsView;
}

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
  scoped_refptr<CefBrowserInfo> CreateBrowserInfo(bool is_popup,
                                                  bool is_windowless);

  // Called from CefBrowserHostImpl::WebContentsCreated when a new browser is
  // being created for a traditional popup (e.g. window.open() or targeted
  // link). If any OnGetNewBrowserInfo requests are pending for the popup the
  // response will be sent when this method is called.
  scoped_refptr<CefBrowserInfo> CreatePopupBrowserInfo(
      content::WebContents* new_contents,
      bool is_windowless);

  // Called from CefContentBrowserClient::CanCreateWindow. See comments on
  // PendingPopup for more information.
  bool CanCreateWindow(
      const GURL& target_url,
      const content::Referrer& referrer,
      const std::string& frame_name,
      WindowOpenDisposition disposition,
      const blink::mojom::WindowFeatures& features,
      bool user_gesture,
      bool opener_suppressed,
      int opener_render_process_id,
      int opener_render_frame_id,
      bool* no_javascript_access);

  // Called from CefBrowserHostImpl::GetCustomWebContentsView. See comments on
  // PendingPopup for more information.
  void GetCustomWebContentsView(
    const GURL& target_url,
    int opener_render_process_id,
    int opener_render_frame_id,
    content::WebContentsView** view,
    content::RenderViewHostDelegateView** delegate_view);

  // Called from CefBrowserHostImpl::WebContentsCreated. See comments on
  // PendingPopup for more information.
  void WebContentsCreated(
      const GURL& target_url,
      int opener_render_process_id,
      int opener_render_frame_id,
      CefBrowserSettings& settings,
      CefRefPtr<CefClient>& client,
      std::unique_ptr<CefBrowserPlatformDelegate>& platform_delegate);

  // Called from CefBrowserMessageFilter::OnGetNewBrowserInfo for delivering
  // browser info to the renderer process. If the browser info already exists
  // the response will be sent immediately. Otherwise, the response will be sent
  // when CreatePopupBrowserInfo creates the browser info. The info will already
  // exist for explicitly created browsers and guest views. It may sometimes
  // already exist for traditional popup browsers depending on timing. See
  // comments on PendingPopup for more information.
  void OnGetNewBrowserInfo(
      int render_process_id,
      int render_view_routing_id,
      int render_frame_routing_id,
      IPC::Message* reply_msg);

  // Called from CefBrowserHostImpl::DestroyBrowser() when a browser is
  // destroyed.
  void RemoveBrowserInfo(scoped_refptr<CefBrowserInfo> browser_info);

  // Called from CefContext::FinishShutdownOnUIThread() to destroy all browsers.
  void DestroyAllBrowsers();

  // Retrieves the CefBrowserInfo matching the specified IDs or an empty
  // pointer if no match is found. It is allowed to add new callers of this
  // method but consider using CefBrowserHostImpl::GetBrowserFor[View|Frame]()
  // or extensions::GetOwnerBrowserForFrame() instead.
  // |is_guest_view| will be set to true if the IDs match a guest view
  // associated with the returned browser info instead of the browser itself.
  scoped_refptr<CefBrowserInfo> GetBrowserInfoForView(int render_process_id,
                                                      int render_routing_id,
                                                      bool* is_guest_view);
  scoped_refptr<CefBrowserInfo> GetBrowserInfoForFrame(int render_process_id,
                                                       int render_routing_id,
                                                       bool* is_guest_view);

  // Retrieves all existing CefBrowserInfo objects.
  typedef std::list<scoped_refptr<CefBrowserInfo> > BrowserInfoList;
  void GetBrowserInfoList(BrowserInfoList& list);

 private:
  // RenderProcessHostObserver methods:
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

  // Store state information about pending popups. Call order is:
  // - CefContentBrowserClient::CanCreateWindow (IOT)
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
    int opener_process_id;
    int opener_frame_id;
    GURL target_url;
    std::string target_frame_name;

    // Values specified by OnBeforePopup.
    CefBrowserSettings settings;
    CefRefPtr<CefClient> client;

    // Platform delegate specific to the new popup.
    std::unique_ptr<CefBrowserPlatformDelegate> platform_delegate;
  };

  // Between the calls to CanCreateWindow and GetCustomWebContentsView
  // RenderViewHostImpl::CreateNewWindow() will call
  // RenderProcessHostImpl::FilterURL() which, in the case of "javascript:"
  // URIs, rewrites the URL to "about:blank". We need to apply the same filter
  // otherwise GetCustomWebContentsView will fail to retrieve the PopupInfo.
  static void FilterPendingPopupURL(
    int opener_process_id,
    std::unique_ptr<PendingPopup> pending_popup);

  // Manage pending popups.
  void PushPendingPopup(std::unique_ptr<PendingPopup> popup);
  std::unique_ptr<PendingPopup> PopPendingPopup(
      PendingPopup::Step step,
      int opener_process_id,
      int opener_frame_id,
      const GURL& target_url);

  // Retrieves the BrowserInfo matching the specified IDs. If both sets are
  // valid then this method makes sure both sets have been registered.
  scoped_refptr<CefBrowserInfo> GetBrowserInfo(
      int render_view_process_id,
      int render_view_routing_id,
      int render_frame_process_id,
      int render_frame_routing_id,
      bool* is_guest_view);

  // Send the response for a pending OnGetNewBrowserInfo request.
  static void SendNewBrowserInfoResponse(
      int render_process_id,
      CefBrowserInfo* browser_info,
      bool is_guest_view,
      IPC::Message* reply_msg);

  // Pending request for OnGetNewBrowserInfo.
  struct PendingNewBrowserInfo {
    int render_process_id;
    int render_view_routing_id;
    int render_frame_routing_id;
    IPC::Message* reply_msg;
  };

  mutable base::Lock browser_info_lock_;

  // Access to the below members must be protected by |browser_info_lock_|.

  BrowserInfoList browser_info_list_;
  int next_browser_id_;

  typedef ScopedVector<PendingNewBrowserInfo> PendingNewBrowserInfoList;
  PendingNewBrowserInfoList pending_new_browser_info_list_;

  base::Lock pending_popup_lock_;

  // Access to the below members must be protected by |pending_popup_lock_|.

  typedef ScopedVector<PendingPopup> PendingPopupList;
  PendingPopupList pending_popup_list_;

  DISALLOW_COPY_AND_ASSIGN(CefBrowserInfoManager);
};

#endif  // CEF_LIBCEF_BROWSER_BROWSER_INFO_H_
