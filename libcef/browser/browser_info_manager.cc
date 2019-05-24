// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/browser_info_manager.h"

#include <utility>

#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/browser_platform_delegate.h"
#include "libcef/browser/extensions/browser_extensions_util.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/cef_messages.h"
#include "libcef/common/extensions/extensions_util.h"
#include "libcef/common/values_impl.h"

#include "base/logging.h"
#include "content/common/view_messages.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/child_process_host.h"

namespace {

void TranslatePopupFeatures(const blink::mojom::WindowFeatures& webKitFeatures,
                            CefPopupFeatures& features) {
  features.x = static_cast<int>(webKitFeatures.x);
  features.xSet = webKitFeatures.has_x;
  features.y = static_cast<int>(webKitFeatures.y);
  features.ySet = webKitFeatures.has_y;
  features.width = static_cast<int>(webKitFeatures.width);
  features.widthSet = webKitFeatures.has_width;
  features.height = static_cast<int>(webKitFeatures.height);
  features.heightSet = webKitFeatures.has_height;

  features.menuBarVisible = webKitFeatures.menu_bar_visible;
  features.statusBarVisible = webKitFeatures.status_bar_visible;
  features.toolBarVisible = webKitFeatures.tool_bar_visible;
  features.scrollbarsVisible = webKitFeatures.scrollbars_visible;
}

CefBrowserInfoManager* g_info_manager = nullptr;

}  // namespace

CefBrowserInfoManager::CefBrowserInfoManager() : next_browser_id_(0) {
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
    bool is_windowless,
    CefRefPtr<CefDictionaryValue> extra_info) {
  base::AutoLock lock_scope(browser_info_lock_);

  scoped_refptr<CefBrowserInfo> browser_info = new CefBrowserInfo(
      ++next_browser_id_, is_popup, is_windowless, extra_info);
  browser_info_list_.push_back(browser_info);

  return browser_info;
}

scoped_refptr<CefBrowserInfo> CefBrowserInfoManager::CreatePopupBrowserInfo(
    content::WebContents* new_contents,
    bool is_windowless,
    CefRefPtr<CefDictionaryValue> extra_info) {
  base::AutoLock lock_scope(browser_info_lock_);

  auto frame_host = new_contents->GetMainFrame();
  const int render_process_id = frame_host->GetProcess()->GetID();
  const auto frame_id = CefFrameHostImpl::MakeFrameId(frame_host);

  scoped_refptr<CefBrowserInfo> browser_info =
      new CefBrowserInfo(++next_browser_id_, true, is_windowless, extra_info);
  browser_info_list_.push_back(browser_info);

  // Continue any pending NewBrowserInfo requests.
  auto it = pending_new_browser_info_map_.find(frame_id);
  if (it != pending_new_browser_info_map_.end()) {
    SendNewBrowserInfoResponse(render_process_id, browser_info, false,
                               it->second->reply_msg);
    pending_new_browser_info_map_.erase(it);
  }

  return browser_info;
}

bool CefBrowserInfoManager::CanCreateWindow(
    content::RenderFrameHost* opener,
    const GURL& target_url,
    const content::Referrer& referrer,
    const std::string& frame_name,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& features,
    bool user_gesture,
    bool opener_suppressed,
    bool* no_javascript_access) {
  CEF_REQUIRE_UIT();

  bool is_guest_view = false;
  CefRefPtr<CefBrowserHostImpl> browser =
      extensions::GetOwnerBrowserForHost(opener, &is_guest_view);
  DCHECK(browser.get());
  if (!browser.get()) {
    // Cancel the popup.
    return false;
  }

  if (is_guest_view) {
    content::OpenURLParams params(target_url, referrer, disposition,
                                  ui::PAGE_TRANSITION_LINK, true);
    params.user_gesture = user_gesture;

    // Pass navigation to the owner browser.
    CEF_POST_TASK(
        CEF_UIT,
        base::Bind(base::IgnoreResult(&CefBrowserHostImpl::OpenURLFromTab),
                   browser.get(), nullptr, params));

    // Cancel the popup.
    return false;
  }

  CefRefPtr<CefClient> client = browser->GetClient();
  bool allow = true;

  std::unique_ptr<CefWindowInfo> window_info(new CefWindowInfo);

#if defined(OS_WIN)
  window_info->SetAsPopup(NULL, CefString());
#endif

  auto pending_popup = std::make_unique<CefBrowserInfoManager::PendingPopup>();
  pending_popup->step = CefBrowserInfoManager::PendingPopup::CAN_CREATE_WINDOW;
  pending_popup->opener_render_process_id = opener->GetProcess()->GetID();
  pending_popup->opener_render_routing_id = opener->GetRoutingID();
  pending_popup->target_url = target_url;
  pending_popup->target_frame_name = frame_name;

  // Start with the current browser's settings.
  pending_popup->client = client;
  pending_popup->settings = browser->settings();

  if (client.get()) {
    CefRefPtr<CefLifeSpanHandler> handler = client->GetLifeSpanHandler();
    if (handler.get()) {
      CefRefPtr<CefFrame> opener_frame = browser->GetFrameForHost(opener);
      DCHECK(opener_frame);

      CefPopupFeatures cef_features;
      TranslatePopupFeatures(features, cef_features);

#if (defined(OS_WIN) || defined(OS_MACOSX))
      // Default to the size from the popup features.
      if (cef_features.xSet)
        window_info->x = cef_features.x;
      if (cef_features.ySet)
        window_info->y = cef_features.y;
      if (cef_features.widthSet)
        window_info->width = cef_features.width;
      if (cef_features.heightSet)
        window_info->height = cef_features.height;
#endif

      allow = !handler->OnBeforePopup(
          browser.get(), opener_frame, pending_popup->target_url.spec(),
          pending_popup->target_frame_name,
          static_cast<cef_window_open_disposition_t>(disposition), user_gesture,
          cef_features, *window_info, pending_popup->client,
          pending_popup->settings, pending_popup->extra_info,
          no_javascript_access);
    }
  }

  if (allow) {
    CefBrowserHostImpl::CreateParams create_params;

    if (!browser->IsViewsHosted())
      create_params.window_info = std::move(window_info);

    create_params.settings = pending_popup->settings;
    create_params.client = pending_popup->client;
    create_params.extra_info = pending_popup->extra_info;

    pending_popup->platform_delegate =
        CefBrowserPlatformDelegate::Create(create_params);
    CHECK(pending_popup->platform_delegate.get());

    // Between the calls to CanCreateWindow and GetCustomWebContentsView
    // RenderViewHostImpl::CreateNewWindow() will call
    // RenderProcessHostImpl::FilterURL() which, in the case of "javascript:"
    // URIs, rewrites the URL to "about:blank". We need to apply the same filter
    // otherwise GetCustomWebContentsView will fail to retrieve the PopupInfo.
    opener->GetProcess()->FilterURL(false, &pending_popup->target_url);

    PushPendingPopup(std::move(pending_popup));
  }

  return allow;
}

void CefBrowserInfoManager::GetCustomWebContentsView(
    const GURL& target_url,
    int opener_render_process_id,
    int opener_render_routing_id,
    content::WebContentsView** view,
    content::RenderViewHostDelegateView** delegate_view) {
  CEF_REQUIRE_UIT();

  std::unique_ptr<CefBrowserInfoManager::PendingPopup> pending_popup =
      PopPendingPopup(CefBrowserInfoManager::PendingPopup::CAN_CREATE_WINDOW,
                      opener_render_process_id, opener_render_routing_id,
                      target_url);
  DCHECK(pending_popup.get());
  DCHECK(pending_popup->platform_delegate.get());

  if (pending_popup->platform_delegate->IsWindowless()) {
    pending_popup->platform_delegate->CreateViewForWebContents(view,
                                                               delegate_view);
  }

  pending_popup->step =
      CefBrowserInfoManager::PendingPopup::GET_CUSTOM_WEB_CONTENTS_VIEW;
  PushPendingPopup(std::move(pending_popup));
}

void CefBrowserInfoManager::WebContentsCreated(
    const GURL& target_url,
    int opener_render_process_id,
    int opener_render_routing_id,
    CefBrowserSettings& settings,
    CefRefPtr<CefClient>& client,
    std::unique_ptr<CefBrowserPlatformDelegate>& platform_delegate,
    CefRefPtr<CefDictionaryValue>& extra_info) {
  CEF_REQUIRE_UIT();

  std::unique_ptr<CefBrowserInfoManager::PendingPopup> pending_popup =
      PopPendingPopup(
          CefBrowserInfoManager::PendingPopup::GET_CUSTOM_WEB_CONTENTS_VIEW,
          opener_render_process_id, opener_render_routing_id, target_url);
  DCHECK(pending_popup.get());
  DCHECK(pending_popup->platform_delegate.get());

  settings = pending_popup->settings;
  client = pending_popup->client;
  platform_delegate = std::move(pending_popup->platform_delegate);
  extra_info = pending_popup->extra_info;
}

void CefBrowserInfoManager::OnGetNewBrowserInfo(int render_process_id,
                                                int render_routing_id,
                                                IPC::Message* reply_msg) {
  DCHECK_NE(render_process_id, content::ChildProcessHost::kInvalidUniqueID);
  DCHECK_GT(render_routing_id, 0);
  DCHECK(reply_msg);

  base::AutoLock lock_scope(browser_info_lock_);

  bool is_guest_view = false;

  scoped_refptr<CefBrowserInfo> browser_info =
      GetBrowserInfo(render_process_id, render_routing_id, &is_guest_view);

  if (browser_info.get()) {
    // Send the response immediately.
    SendNewBrowserInfoResponse(render_process_id, browser_info, is_guest_view,
                               reply_msg);
    return;
  }

  const auto frame_id =
      CefFrameHostImpl::MakeFrameId(render_process_id, render_routing_id);

  // Verify that no request for the same route is currently queued.
  DCHECK(pending_new_browser_info_map_.find(frame_id) ==
         pending_new_browser_info_map_.end());

  // Queue the request.
  std::unique_ptr<PendingNewBrowserInfo> pending(new PendingNewBrowserInfo());
  pending->render_process_id = render_process_id;
  pending->render_routing_id = render_routing_id;
  pending->reply_msg = reply_msg;
  pending_new_browser_info_map_.insert(
      std::make_pair(frame_id, std::move(pending)));
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

#if DCHECK_IS_ON()
  {
    // Verify that all browser windows have been destroyed.
    base::AutoLock lock_scope(browser_info_lock_);
    DCHECK(browser_info_list_.empty());
  }
#endif
}

scoped_refptr<CefBrowserInfo>
CefBrowserInfoManager::GetBrowserInfoForFrameRoute(int render_process_id,
                                                   int render_routing_id,
                                                   bool* is_guest_view) {
  base::AutoLock lock_scope(browser_info_lock_);
  return GetBrowserInfo(render_process_id, render_routing_id, is_guest_view);
}

scoped_refptr<CefBrowserInfo>
CefBrowserInfoManager::GetBrowserInfoForFrameTreeNode(int frame_tree_node_id,
                                                      bool* is_guest_view) {
  if (is_guest_view)
    *is_guest_view = false;

  if (frame_tree_node_id < 0)
    return nullptr;

  base::AutoLock lock_scope(browser_info_lock_);

  for (const auto& browser_info : browser_info_list_) {
    bool is_guest_view_tmp;
    auto frame = browser_info->GetFrameForFrameTreeNode(frame_tree_node_id,
                                                        &is_guest_view_tmp);
    if (frame || is_guest_view_tmp) {
      if (is_guest_view)
        *is_guest_view = is_guest_view_tmp;
      return browser_info;
    }
  }

  return nullptr;
}

CefBrowserInfoManager::BrowserInfoList
CefBrowserInfoManager::GetBrowserInfoList() {
  base::AutoLock lock_scope(browser_info_lock_);
  BrowserInfoList copy;
  copy.assign(browser_info_list_.begin(), browser_info_list_.end());
  return copy;
}

void CefBrowserInfoManager::RenderProcessHostDestroyed(
    content::RenderProcessHost* host) {
  CEF_REQUIRE_UIT();

  const int render_process_id = host->GetID();
  DCHECK_GT(render_process_id, 0);

  // Remove all pending requests that reference the destroyed host.
  {
    base::AutoLock lock_scope(browser_info_lock_);

    PendingNewBrowserInfoMap::iterator it =
        pending_new_browser_info_map_.begin();
    while (it != pending_new_browser_info_map_.end()) {
      auto info = it->second.get();
      if (info->render_process_id == render_process_id)
        it = pending_new_browser_info_map_.erase(it);
      else
        ++it;
    }
  }

  // Remove all pending popups that reference the destroyed host as the opener.
  {
    PendingPopupList::iterator it = pending_popup_list_.begin();
    while (it != pending_popup_list_.end()) {
      PendingPopup* popup = it->get();
      if (popup->opener_render_process_id == render_process_id) {
        it = pending_popup_list_.erase(it);
      } else {
        ++it;
      }
    }
  }
}

void CefBrowserInfoManager::PushPendingPopup(
    std::unique_ptr<PendingPopup> popup) {
  CEF_REQUIRE_UIT();
  pending_popup_list_.push_back(std::move(popup));
}

std::unique_ptr<CefBrowserInfoManager::PendingPopup>
CefBrowserInfoManager::PopPendingPopup(PendingPopup::Step step,
                                       int opener_render_process_id,
                                       int opener_render_routing_id,
                                       const GURL& target_url) {
  CEF_REQUIRE_UIT();
  DCHECK_GT(opener_render_process_id, 0);
  DCHECK_GT(opener_render_routing_id, 0);

  PendingPopupList::iterator it = pending_popup_list_.begin();
  for (; it != pending_popup_list_.end(); ++it) {
    PendingPopup* popup = it->get();
    if (popup->step == step &&
        popup->opener_render_process_id == opener_render_process_id &&
        popup->opener_render_routing_id == opener_render_routing_id &&
        popup->target_url == target_url) {
      // Transfer ownership of the pointer.
      it->release();
      pending_popup_list_.erase(it);
      return base::WrapUnique(popup);
    }
  }

  return nullptr;
}

scoped_refptr<CefBrowserInfo> CefBrowserInfoManager::GetBrowserInfo(
    int render_process_id,
    int render_routing_id,
    bool* is_guest_view) {
  browser_info_lock_.AssertAcquired();

  if (is_guest_view)
    *is_guest_view = false;

  if (render_process_id < 0 || render_routing_id < 0)
    return nullptr;

  for (const auto& browser_info : browser_info_list_) {
    bool is_guest_view_tmp;
    auto frame = browser_info->GetFrameForRoute(
        render_process_id, render_routing_id, &is_guest_view_tmp);
    if (frame || is_guest_view_tmp) {
      if (is_guest_view)
        *is_guest_view = is_guest_view_tmp;
      return browser_info;
    }
  }

  return nullptr;
}

// static
void CefBrowserInfoManager::SendNewBrowserInfoResponse(
    int render_process_id,
    scoped_refptr<CefBrowserInfo> browser_info,
    bool is_guest_view,
    IPC::Message* reply_msg) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::Bind(&CefBrowserInfoManager::SendNewBrowserInfoResponse,
                   render_process_id, browser_info, is_guest_view, reply_msg));
    return;
  }

  content::RenderProcessHost* host =
      content::RenderProcessHost::FromID(render_process_id);
  if (!host) {
    delete reply_msg;
    return;
  }

  CefProcessHostMsg_GetNewBrowserInfo_Params params;
  params.browser_id = browser_info->browser_id();
  params.is_windowless = browser_info->is_windowless();
  params.is_popup = browser_info->is_popup();
  params.is_guest_view = is_guest_view;

  auto extra_info = browser_info->extra_info();
  if (extra_info) {
    auto extra_info_impl =
        static_cast<CefDictionaryValueImpl*>(extra_info.get());
    auto extra_info_value = extra_info_impl->CopyValue();
    extra_info_value->Swap(&params.extra_info);
  }

  CefProcessHostMsg_GetNewBrowserInfo::WriteReplyParams(reply_msg, params);
  host->Send(reply_msg);
}
