// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/browser_info_manager.h"

#include <utility>

#include "libcef/browser/browser_platform_delegate.h"
#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/cef_messages.h"
#include "libcef/common/extensions/extensions_util.h"
#include "libcef/browser/extensions/browser_extensions_util.h"

#include "base/logging.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/common/view_messages.h"

namespace {

void TranslatePopupFeatures(const blink::WebWindowFeatures& webKitFeatures,
                            CefPopupFeatures& features) {
  features.x = static_cast<int>(webKitFeatures.x);
  features.xSet = webKitFeatures.xSet;
  features.y = static_cast<int>(webKitFeatures.y);
  features.ySet = webKitFeatures.ySet;
  features.width = static_cast<int>(webKitFeatures.width);
  features.widthSet = webKitFeatures.widthSet;
  features.height = static_cast<int>(webKitFeatures.height);
  features.heightSet =  webKitFeatures.heightSet;

  features.menuBarVisible =  webKitFeatures.menuBarVisible;
  features.statusBarVisible =  webKitFeatures.statusBarVisible;
  features.toolBarVisible =  webKitFeatures.toolBarVisible;
  features.locationBarVisible =  webKitFeatures.locationBarVisible;
  features.scrollbarsVisible =  webKitFeatures.scrollbarsVisible;
  features.resizable =  webKitFeatures.resizable;

  features.fullscreen =  webKitFeatures.fullscreen;
  features.dialog =  webKitFeatures.dialog;
  features.additionalFeatures = NULL;
  if (webKitFeatures.additionalFeatures.size() > 0)
     features.additionalFeatures = cef_string_list_alloc();

  CefString str;
  for (unsigned int i = 0; i < webKitFeatures.additionalFeatures.size(); ++i) {
    str = base::string16(webKitFeatures.additionalFeatures[i]);
    cef_string_list_append(features.additionalFeatures, str.GetStruct());
  }
}

CefBrowserInfoManager* g_info_manager = nullptr;

}  // namespace

CefBrowserInfoManager::CefBrowserInfoManager()
    : next_browser_id_(0) {
  DCHECK(!g_info_manager);
  g_info_manager = this;
}

CefBrowserInfoManager::~CefBrowserInfoManager() {
  DCHECK(browser_info_list_.empty());
  g_info_manager = nullptr;
}

// static
CefBrowserInfoManager* CefBrowserInfoManager::GetInstance() {
  return g_info_manager;
}

scoped_refptr<CefBrowserInfo> CefBrowserInfoManager::CreateBrowserInfo(
    bool is_popup,
    bool is_windowless) {
  base::AutoLock lock_scope(browser_info_lock_);

  scoped_refptr<CefBrowserInfo> browser_info =
      new CefBrowserInfo(++next_browser_id_, is_popup);
  browser_info_list_.push_back(browser_info);

  if (is_windowless)
    browser_info->set_windowless(true);

  return browser_info;
}

scoped_refptr<CefBrowserInfo> CefBrowserInfoManager::CreatePopupBrowserInfo(
    content::WebContents* new_contents,
    bool is_windowless) {
  base::AutoLock lock_scope(browser_info_lock_);

  content::RenderViewHost* view_host = new_contents->GetRenderViewHost();
  content::RenderFrameHost* main_frame_host = new_contents->GetMainFrame();

  content::RenderProcessHost* host = view_host->GetProcess();

  // The host processes may be different in the future with OOP iframes. When
  // that happens re-visit the implementation of this class.
  DCHECK_EQ(host, main_frame_host->GetProcess());

  const int render_process_routing_id = host->GetID();
  const int render_view_routing_id = view_host->GetRoutingID();
  const int render_frame_routing_id = main_frame_host->GetRoutingID();

  scoped_refptr<CefBrowserInfo> browser_info =
      new CefBrowserInfo(++next_browser_id_, true);
  browser_info->render_id_manager()->add_render_view_id(
      render_process_routing_id, render_view_routing_id);
  browser_info->render_id_manager()->add_render_frame_id(
      render_process_routing_id, render_frame_routing_id);
  browser_info_list_.push_back(browser_info);

  if (is_windowless)
    browser_info->set_windowless(true);

  // Continue any pending NewBrowserInfo requests.
  PendingNewBrowserInfoList::iterator it =
      pending_new_browser_info_list_.begin();
  for (; it != pending_new_browser_info_list_.end(); ++it) {
    PendingNewBrowserInfo* info = *it;
    if (info->host == host &&
        info->render_view_routing_id == render_view_routing_id &&
        info->render_frame_routing_id == render_frame_routing_id) {
      SendNewBrowserInfoResponse(host, browser_info.get(), false,
                                 info->reply_msg);

      pending_new_browser_info_list_.erase(it);
      break;
    }
  }

  return browser_info;
}

void CefBrowserInfoManager::OnCreateWindow(
    content::RenderProcessHost* host,
    const ViewHostMsg_CreateWindow_Params& params) {
  DCHECK_GT(params.opener_id, 0);
  DCHECK_GT(params.opener_render_frame_id, 0);

  scoped_ptr<CefBrowserInfoManager::PendingPopup> pending_popup(
      new CefBrowserInfoManager::PendingPopup);
  pending_popup->step = CefBrowserInfoManager::PendingPopup::ON_CREATE_WINDOW;
  pending_popup->opener_process_id = host->GetID();
  pending_popup->opener_view_id = params.opener_id;
  pending_popup->opener_frame_id = params.opener_render_frame_id;
  pending_popup->target_url = params.target_url;
  pending_popup->target_frame_name = params.frame_name;
  PushPendingPopup(std::move(pending_popup));
}

bool CefBrowserInfoManager::CanCreateWindow(
    const GURL& target_url,
    const content::Referrer& referrer,
    WindowOpenDisposition disposition,
    const blink::WebWindowFeatures& features,
    bool user_gesture,
    bool opener_suppressed,
    int render_process_id,
    int opener_render_view_id,
    int opener_render_frame_id,
    bool* no_javascript_access) {
  scoped_ptr<CefBrowserInfoManager::PendingPopup> pending_popup =
      PopPendingPopup(CefBrowserInfoManager::PendingPopup::ON_CREATE_WINDOW,
                      render_process_id, opener_render_view_id, target_url);
  DCHECK(pending_popup.get());
  DCHECK(!pending_popup->platform_delegate.get());

  bool is_guest_view = false;
  CefRefPtr<CefBrowserHostImpl> browser =
      extensions::GetOwnerBrowserForView(
          pending_popup->opener_process_id,
          pending_popup->opener_view_id,
          &is_guest_view);
  DCHECK(browser.get());
  if (!browser.get()) {
    // Cancel the popup.
    return false;
  }

  if (is_guest_view) {
    content::OpenURLParams params(target_url,
                                  referrer,
                                  disposition,
                                  ui::PAGE_TRANSITION_LINK,
                                  true);
    params.user_gesture = user_gesture;

    // Pass navigation to the owner browser.
    CEF_POST_TASK(CEF_UIT,
        base::Bind(base::IgnoreResult(&CefBrowserHostImpl::OpenURLFromTab),
                   browser.get(), nullptr, params));

    // Cancel the popup.
    return false;
  }

  CefRefPtr<CefClient> client = browser->GetClient();
  bool allow = true;

#if defined(OS_WIN)
  pending_popup->window_info.SetAsPopup(NULL, CefString());
#endif

  // Start with the current browser's settings.
  pending_popup->client = client;
  pending_popup->settings = browser->settings();

  if (client.get()) {
    CefRefPtr<CefLifeSpanHandler> handler = client->GetLifeSpanHandler();
    if (handler.get()) {
      CefRefPtr<CefFrame> frame =
          browser->GetFrame(pending_popup->opener_frame_id);

      CefPopupFeatures cef_features;
      TranslatePopupFeatures(features, cef_features);

#if (defined(OS_WIN) || defined(OS_MACOSX))
      // Default to the size from the popup features.
      if (cef_features.xSet)
        pending_popup->window_info.x = cef_features.x;
      if (cef_features.ySet)
        pending_popup->window_info.y = cef_features.y;
      if (cef_features.widthSet)
        pending_popup->window_info.width = cef_features.width;
      if (cef_features.heightSet)
        pending_popup->window_info.height = cef_features.height;
#endif

      allow = !handler->OnBeforePopup(browser.get(),
          frame,
          pending_popup->target_url.spec(),
          pending_popup->target_frame_name,
          static_cast<cef_window_open_disposition_t>(disposition),
          user_gesture,
          cef_features,
          pending_popup->window_info,
          pending_popup->client,
          pending_popup->settings,
          no_javascript_access);
    }
  }

  if (allow) {
    pending_popup->platform_delegate =
        CefBrowserPlatformDelegate::Create(pending_popup->window_info,
                                           pending_popup->settings,
                                           pending_popup->client);
    CHECK(pending_popup->platform_delegate.get());

    pending_popup->step =
        CefBrowserInfoManager::PendingPopup::CAN_CREATE_WINDOW;

    // Filtering needs to be done on the UI thread.
    CEF_POST_TASK(CEF_UIT,
        base::Bind(FilterPendingPopupURL, render_process_id,
                   base::Passed(&pending_popup)));
  }

  return allow;
}

void CefBrowserInfoManager::ShouldCreateWebContents(
    content::WebContents* web_contents,
    const GURL& target_url,
    content::WebContentsView** view,
    content::RenderViewHostDelegateView** delegate_view) {
  scoped_ptr<CefBrowserInfoManager::PendingPopup> pending_popup =
      PopPendingPopup(CefBrowserInfoManager::PendingPopup::CAN_CREATE_WINDOW,
                      web_contents->GetRenderViewHost()->GetProcess()->GetID(),
                      web_contents->GetRenderViewHost()->GetRoutingID(),
                      target_url);
  DCHECK(pending_popup.get());
  DCHECK(pending_popup->platform_delegate.get());

  if (pending_popup->platform_delegate->IsWindowless()) {
    pending_popup->platform_delegate->CreateViewForWebContents(view,
                                                               delegate_view);
  }

  pending_popup->step =
      CefBrowserInfoManager::PendingPopup::SHOULD_CREATE_WEB_CONTENTS;
  PushPendingPopup(std::move(pending_popup));
}

void CefBrowserInfoManager::WebContentsCreated(
    content::WebContents* source_contents,
    const GURL& target_url,
    content::WebContents* new_contents,
    CefBrowserSettings& settings,
    CefRefPtr<CefClient>& client,
    scoped_ptr<CefBrowserPlatformDelegate>& platform_delegate) {
  DCHECK(source_contents);
  DCHECK(new_contents);

  scoped_ptr<CefBrowserInfoManager::PendingPopup> pending_popup =
      PopPendingPopup(
          CefBrowserInfoManager::PendingPopup::SHOULD_CREATE_WEB_CONTENTS,
          source_contents->GetRenderViewHost()->GetProcess()->GetID(),
          source_contents->GetRenderViewHost()->GetRoutingID(),
          target_url);
  DCHECK(pending_popup.get());
  DCHECK(pending_popup->platform_delegate.get());

  settings = pending_popup->settings;
  client = pending_popup->client;
  platform_delegate = std::move(pending_popup->platform_delegate);
}

void CefBrowserInfoManager::OnGetNewBrowserInfo(
    content::RenderProcessHost* host,
    int render_view_routing_id,
    int render_frame_routing_id,
    IPC::Message* reply_msg) {
  DCHECK(host);
  DCHECK_GT(render_view_routing_id, 0);
  DCHECK_GT(render_frame_routing_id, 0);
  DCHECK(reply_msg);

  base::AutoLock lock_scope(browser_info_lock_);

  const int render_process_routing_id = host->GetID();
  bool is_guest_view = false;

  scoped_refptr<CefBrowserInfo> browser_info = GetBrowserInfo(
      render_process_routing_id, render_view_routing_id,
      render_process_routing_id, render_frame_routing_id,
      &is_guest_view);

  if (browser_info.get()) {
    // Send the response immediately.
    SendNewBrowserInfoResponse(host, browser_info.get(), is_guest_view,
                               reply_msg);
    return;
  }

#ifndef NDEBUG
  // Verify that no request for the same route is currently queued.
  {
    PendingNewBrowserInfoList::const_iterator it =
        pending_new_browser_info_list_.begin();
    for (; it != pending_new_browser_info_list_.end(); ++it) {
      PendingNewBrowserInfo* info = *it;
      if (info->host == host &&
          info->render_view_routing_id == render_view_routing_id &&
          info->render_frame_routing_id == render_frame_routing_id) {
        NOTREACHED();
      }
    }
  }
#endif

  // Queue the request.
  scoped_ptr<PendingNewBrowserInfo> pending(new PendingNewBrowserInfo());
  pending->host = host;
  pending->render_view_routing_id = render_view_routing_id;
  pending->render_frame_routing_id = render_frame_routing_id;
  pending->reply_msg = reply_msg;
  pending_new_browser_info_list_.push_back(std::move(pending));
}

void CefBrowserInfoManager::RemoveBrowserInfo(
    scoped_refptr<CefBrowserInfo> browser_info) {
  base::AutoLock lock_scope(browser_info_lock_);

  BrowserInfoList::iterator it = browser_info_list_.begin();
  for (; it != browser_info_list_.end(); ++it) {
    if (*it == browser_info) {
      browser_info_list_.erase(it);
      return;
    }
  }

  NOTREACHED();
}

void CefBrowserInfoManager::DestroyAllBrowsers() {
  BrowserInfoList list;

  {
    base::AutoLock lock_scope(browser_info_lock_);
    list = browser_info_list_;
  }

  // Destroy any remaining browser windows.
  if (!list.empty()) {
    BrowserInfoList::iterator it = list.begin();
    for (; it != list.end(); ++it) {
      CefRefPtr<CefBrowserHostImpl> browser = (*it)->browser();
      DCHECK(browser.get());
      if (browser.get()) {
        // DestroyBrowser will call RemoveBrowserInfo.
        browser->DestroyBrowser();
      }
    }
  }

#ifndef NDEBUG
  {
    // Verify that all browser windows have been destroyed.
    base::AutoLock lock_scope(browser_info_lock_);
    DCHECK(browser_info_list_.empty());
  }
#endif
}

scoped_refptr<CefBrowserInfo> CefBrowserInfoManager::GetBrowserInfoForView(
    int render_process_id,
    int render_routing_id,
    bool* is_guest_view) {
  base::AutoLock lock_scope(browser_info_lock_);
  return GetBrowserInfo(render_process_id, render_routing_id, 0, 0,
                        is_guest_view);
}

scoped_refptr<CefBrowserInfo> CefBrowserInfoManager::GetBrowserInfoForFrame(
    int render_process_id,
    int render_routing_id,
    bool* is_guest_view) {
  base::AutoLock lock_scope(browser_info_lock_);
  return GetBrowserInfo(0, 0, render_process_id, render_routing_id,
                        is_guest_view);
}

void CefBrowserInfoManager::RenderProcessHostDestroyed(
    content::RenderProcessHost* host) {
  base::AutoLock lock_scope(browser_info_lock_);

  // Remove all pending requests that reference the destroyed host.
  PendingNewBrowserInfoList::iterator it =
      pending_new_browser_info_list_.begin();
  while (it != pending_new_browser_info_list_.end()) {
    PendingNewBrowserInfo* info = *it;
    if (info->host == host)
      it = pending_new_browser_info_list_.erase(it);
    else
      ++it;
  }
}

void CefBrowserInfoManager::FilterPendingPopupURL(
    int render_process_id,
    scoped_ptr<CefBrowserInfoManager::PendingPopup> pending_popup) {
  content::RenderProcessHost* rph =
      content::RenderProcessHost::FromID(render_process_id);
  DCHECK(rph);
  rph->FilterURL(false, &pending_popup->target_url);

  GetInstance()->PushPendingPopup(std::move(pending_popup));
}

void CefBrowserInfoManager::PushPendingPopup(scoped_ptr<PendingPopup> popup) {
  base::AutoLock lock_scope(pending_popup_lock_);
  pending_popup_list_.push_back(std::move(popup));
}

scoped_ptr<CefBrowserInfoManager::PendingPopup>
    CefBrowserInfoManager::PopPendingPopup(
    PendingPopup::Step step,
    int opener_process_id,
    int opener_view_id,
    const GURL& target_url) {
  DCHECK_GT(opener_process_id, 0);
  DCHECK_GT(opener_view_id, 0);

  base::AutoLock lock_scope(pending_popup_lock_);

  PendingPopupList::iterator it = pending_popup_list_.begin();
  for (; it != pending_popup_list_.end(); ++it) {
    PendingPopup* popup = *it;
    if (popup->step == step &&
        popup->opener_process_id == opener_process_id &&
        popup->opener_view_id == opener_view_id &&
        popup->target_url == target_url) {
      pending_popup_list_.weak_erase(it);
      return make_scoped_ptr(popup);
    }
  }

  return nullptr;
}

scoped_refptr<CefBrowserInfo> CefBrowserInfoManager::GetBrowserInfo(
    int render_view_process_id,
    int render_view_routing_id,
    int render_frame_process_id,
    int render_frame_routing_id,
    bool* is_guest_view) {
  browser_info_lock_.AssertAcquired();

  if (is_guest_view)
    *is_guest_view = false;

  const bool valid_view_ids = render_view_process_id > 0 &&
                              render_view_routing_id > 0;
  const bool valid_frame_ids = render_frame_process_id > 0 &&
                               render_frame_routing_id > 0;

  BrowserInfoList::const_iterator it = browser_info_list_.begin();
  for (; it != browser_info_list_.end(); ++it) {
    const scoped_refptr<CefBrowserInfo>& browser_info = *it;
    if (valid_view_ids &&
        browser_info->render_id_manager()->is_render_view_id_match(
            render_view_process_id, render_view_routing_id)) {
      if (valid_frame_ids) {
        // Make sure the frame id is also registered.
        browser_info->render_id_manager()->add_render_frame_id(
            render_frame_process_id, render_frame_routing_id);
      }
      return browser_info;
    }
    if (valid_frame_ids &&
        browser_info->render_id_manager()->is_render_frame_id_match(
            render_frame_process_id, render_frame_routing_id)) {
      if (valid_view_ids) {
        // Make sure the view id is also registered.
        browser_info->render_id_manager()->add_render_view_id(
            render_view_process_id, render_view_routing_id);
      }
      return browser_info;
    }
    if (extensions::ExtensionsEnabled() &&
        ((valid_view_ids &&
          browser_info->guest_render_id_manager()->is_render_view_id_match(
              render_view_process_id, render_view_routing_id)) ||
         (valid_frame_ids &&
          browser_info->guest_render_id_manager()->is_render_frame_id_match(
              render_frame_process_id, render_frame_routing_id)))) {
      if (is_guest_view)
        *is_guest_view = true;
      return browser_info;
    }
  }

  return nullptr;
}

// static
void CefBrowserInfoManager::SendNewBrowserInfoResponse(
    content::RenderProcessHost* host,
    CefBrowserInfo* browser_info,
    bool is_guest_view,
    IPC::Message* reply_msg) {
  CefProcessHostMsg_GetNewBrowserInfo_Params params;
  params.browser_id = browser_info->browser_id();
  params.is_windowless = browser_info->is_windowless();
  params.is_popup = browser_info->is_popup();
  params.is_guest_view = is_guest_view;

  CefProcessHostMsg_GetNewBrowserInfo::WriteReplyParams(reply_msg, params);
  host->Send(reply_msg);
}
