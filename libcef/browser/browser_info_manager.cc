// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "cef/libcef/browser/browser_info_manager.h"

#include <utility>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "cef/libcef/browser/browser_guest_util.h"
#include "cef/libcef/browser/browser_host_base.h"
#include "cef/libcef/browser/browser_platform_delegate.h"
#include "cef/libcef/browser/thread_util.h"
#include "cef/libcef/common/cef_switches.h"
#include "cef/libcef/common/frame_util.h"
#include "cef/libcef/common/values_impl.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"

namespace {

// Timeout delay for new browser info responses.
const int64_t kNewBrowserInfoResponseTimeoutMs = 2000;

void TranslatePopupFeatures(const blink::mojom::WindowFeatures& webKitFeatures,
                            CefPopupFeatures& features) {
  features.x = static_cast<int>(webKitFeatures.bounds.x());
  features.xSet = webKitFeatures.has_x;
  features.y = static_cast<int>(webKitFeatures.bounds.y());
  features.ySet = webKitFeatures.has_y;
  features.width = static_cast<int>(webKitFeatures.bounds.width());
  features.widthSet = webKitFeatures.has_width;
  features.height = static_cast<int>(webKitFeatures.bounds.height());
  features.heightSet = webKitFeatures.has_height;

  features.isPopup = webKitFeatures.is_popup;
}

CefBrowserInfoManager* g_info_manager = nullptr;

}  // namespace

CefBrowserInfoManager::PendingPopup::~PendingPopup() {
  CEF_REQUIRE_UIT();
  if (step != CREATION_COMPLETE && !aborted_callback.is_null()) {
    // Notify of pending popup abort.
    std::move(aborted_callback).Run();
  }
}

CefBrowserInfoManager::CefBrowserInfoManager() {
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
    bool print_preview_enabled,
    CefRefPtr<CefDictionaryValue> extra_info) {
  base::AutoLock lock_scope(browser_info_lock_);

  scoped_refptr<CefBrowserInfo> browser_info =
      new CefBrowserInfo(++next_browser_id_, is_popup, is_windowless,
                         print_preview_enabled, extra_info);
  browser_info_list_.push_back(browser_info);

  return browser_info;
}

scoped_refptr<CefBrowserInfo> CefBrowserInfoManager::CreatePopupBrowserInfo(
    content::WebContents* new_contents,
    bool is_windowless,
    bool print_preview_enabled,
    CefRefPtr<CefDictionaryValue> extra_info) {
  CEF_REQUIRE_UIT();

  auto frame_host = new_contents->GetPrimaryMainFrame();

  scoped_refptr<CefBrowserInfo> browser_info;
  {
    base::AutoLock lock_scope(browser_info_lock_);
    browser_info = new CefBrowserInfo(++next_browser_id_, true, is_windowless,
                                      print_preview_enabled, extra_info);
    browser_info_list_.push_back(browser_info);
  }

  // Continue any pending NewBrowserInfo request.
  ContinueNewBrowserInfo(frame_host->GetGlobalFrameToken(), browser_info,
                         /*is_excluded=*/false);

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

  content::OpenURLParams params(target_url, referrer, disposition,
                                ui::PAGE_TRANSITION_LINK,
                                /*is_renderer_initiated=*/true);
  params.user_gesture = user_gesture;

  CefRefPtr<CefBrowserHostBase> browser;
  if (!MaybeAllowNavigation(opener, params, browser) || !browser) {
    // Cancel the popup.
    return false;
  }

  CefRefPtr<CefClient> client = browser->GetClient();
  bool allow = true;
  bool handled = false;

  CefWindowInfo window_info;
  CefBrowserCreateParams::InitWindowInfo(&window_info, browser.get());

  auto pending_popup = std::make_unique<CefBrowserInfoManager::PendingPopup>();
  pending_popup->step = PendingPopup::CAN_CREATE_WINDOW;
  pending_popup->opener_global_id = opener->GetGlobalId();
  pending_popup->target_url = target_url;
  pending_popup->target_frame_name = frame_name;

  // Start with the current browser's settings.
  pending_popup->client = client;
  pending_popup->settings = browser->settings();

  // With Chrome style, we want to use default popup Browser creation
  // for document picture-in-picture.
  pending_popup->use_default_browser_creation =
      disposition == WindowOpenDisposition::NEW_PICTURE_IN_PICTURE;

  if (client.get()) {
    CefRefPtr<CefLifeSpanHandler> handler = client->GetLifeSpanHandler();
    if (handler.get()) {
      CefRefPtr<CefFrame> opener_frame = browser->GetFrameForHost(opener);
      DCHECK(opener_frame);

      CefPopupFeatures cef_features;
      TranslatePopupFeatures(features, cef_features);

      // Default to the size from the popup features.
      if (cef_features.xSet) {
        window_info.bounds.x = cef_features.x;
      }
      if (cef_features.ySet) {
        window_info.bounds.y = cef_features.y;
      }
      if (cef_features.widthSet) {
        window_info.bounds.width = cef_features.width;
      }
      if (cef_features.heightSet) {
        window_info.bounds.height = cef_features.height;
      }

      const int popup_id = browser->GetNextPopupId();

      allow = !handler->OnBeforePopup(
          browser.get(), opener_frame, popup_id,
          pending_popup->target_url.spec(), pending_popup->target_frame_name,
          static_cast<cef_window_open_disposition_t>(disposition), user_gesture,
          cef_features, window_info, pending_popup->client,
          pending_popup->settings, pending_popup->extra_info,
          no_javascript_access);
      handled = true;

      if (allow) {
        // The parent browser may be destroyed during popup creation, so don't
        // bind a direct reference.
        pending_popup->aborted_callback = base::BindOnce(
            [](int browser_id, int popup_id) {
              LOG(WARNING) << "Pending popup " << popup_id
                           << " aborted for browser " << browser_id;
              if (auto browser =
                      CefBrowserHostBase::GetBrowserForBrowserId(browser_id)) {
                if (auto client = browser->GetClient()) {
                  if (auto handler = client->GetLifeSpanHandler()) {
                    handler->OnBeforePopupAborted(browser.get(), popup_id);
                  }
                }
              }
            },
            browser->GetIdentifier(), popup_id);
      }
    }
  }

  if (allow) {
    CefBrowserCreateParams create_params;
    create_params.MaybeSetWindowInfo(window_info, /*allow_alloy_style=*/true,
                                     /*allow_chrome_style=*/true);

    if (!handled) {
      // Use default Browser creation with Chrome style if OnBeforePopup was
      // unhandled.
      // TODO(chrome): Expose a mechanism for the client to choose default
      // creation.
      pending_popup->use_default_browser_creation = true;
    }

    create_params.popup_with_views_hosted_opener = ShouldCreateViewsHostedPopup(
        browser, pending_popup->use_default_browser_creation);

    // Potentially use Alloy style.
    create_params.popup_with_alloy_style_opener = browser->IsAlloyStyle();

    create_params.settings = pending_popup->settings;
    create_params.client = pending_popup->client;
    create_params.extra_info = pending_popup->extra_info;

    pending_popup->platform_delegate =
        CefBrowserPlatformDelegate::Create(create_params);
    CHECK(pending_popup->platform_delegate.get());

    // Expect runtime style to match.
    pending_popup->alloy_style = !create_params.IsChromeStyle();
    CHECK_EQ(pending_popup->alloy_style,
             pending_popup->platform_delegate->IsAlloyStyle());

    // Between the calls to CanCreateWindow and GetCustomWebContentsView
    // RenderViewHostImpl::CreateNewWindow() will call
    // RenderProcessHostImpl::FilterURL() which, in the case of "javascript:"
    // URIs, rewrites the URL to "about:blank". We need to apply the same filter
    // otherwise GetCustomWebContentsView will fail to retrieve the PopupInfo.
    opener->GetProcess()->FilterURL(false, &pending_popup->target_url);

    pending_create_popup_ = pending_popup.get();

    // Need to Push here because WebContentsCreated may be called before
    // CreateWindowResult.
    PushPendingPopup(std::move(pending_popup));
  }

  return allow;
}

void CefBrowserInfoManager::CreateWindowResult(content::RenderFrameHost* opener,
                                               bool success) {
  // This method is called during RenderFrameHostImpl::CreateNewWindow execution
  // (if CanCreateWindow returns true) with three possible states:
  // 1. Before WebContentsCreated with |success=false|. This is the normal
  //    failure case where the pending popup will be canceled. For example, if a
  //    file select dialog is active.
  // 2. After WebContentsCreated/AddWebContents with |success=true|. This is the
  //    normal success case where OnAfterCreated has already been called.
  // 3. After WebContentsCreated/AddWebContents with |success=false|. This is
  //    the failure case where a WebContents won't have an opener from the
  //    renderer's perspective (for example, with JavaScript access disabled or
  //    no-referrer links). The WebContents is still valid, will navigate
  //    normally, and OnAfterCreated has already been called.
  if (!success && pending_create_popup_) {
    const auto* popup = pending_create_popup_.get();
    pending_create_popup_ = nullptr;

    // Cancel the pending popup.
    std::erase_if(pending_popup_list_, [popup](const auto& popup_ptr) {
      return popup_ptr.get() == popup;
    });
  }
}

void CefBrowserInfoManager::GetCustomWebContentsView(
    const GURL& target_url,
    const content::GlobalRenderFrameHostId& opener_global_id,
    raw_ptr<content::WebContentsView>* view,
    raw_ptr<content::RenderViewHostDelegateView>* delegate_view) {
  CEF_REQUIRE_UIT();

  auto pending_popup = PopPendingPopup(PendingPopup::CAN_CREATE_WINDOW,
                                       PendingPopup::CAN_CREATE_WINDOW,
                                       opener_global_id, target_url);
  DCHECK(pending_popup.get());
  DCHECK(pending_popup->platform_delegate.get());

  if (pending_popup->platform_delegate->IsWindowless()) {
    pending_popup->platform_delegate->CreateViewForWebContents(view,
                                                               delegate_view);
  }

  pending_popup->step = PendingPopup::GET_CUSTOM_WEB_CONTENTS_VIEW;
  PushPendingPopup(std::move(pending_popup));
}

void CefBrowserInfoManager::WebContentsCreated(
    const GURL& target_url,
    const content::GlobalRenderFrameHostId& opener_global_id,
    CefBrowserSettings& settings,
    CefRefPtr<CefClient>& client,
    std::unique_ptr<CefBrowserPlatformDelegate>& platform_delegate,
    CefRefPtr<CefDictionaryValue>& extra_info,
    content::WebContents* new_contents) {
  CEF_REQUIRE_UIT();

  pending_create_popup_ = nullptr;

  // GET_CUSTOM_WEB_CONTENTS_VIEW is only used with Alloy style.
  auto pending_popup = PopPendingPopup(
      PendingPopup::GET_CUSTOM_WEB_CONTENTS_VIEW,
      PendingPopup::CAN_CREATE_WINDOW, opener_global_id, target_url);
  DCHECK(pending_popup.get());
  DCHECK(pending_popup->platform_delegate.get());

  settings = pending_popup->settings;
  client = pending_popup->client;
  platform_delegate = std::move(pending_popup->platform_delegate);
  extra_info = pending_popup->extra_info;

  // AddWebContents (the next step) is only used with Chrome style.
  if (!pending_popup->alloy_style) {
    pending_popup->step = PendingPopup::WEB_CONTENTS_CREATED;
    pending_popup->new_contents = new_contents;
    PushPendingPopup(std::move(pending_popup));
  } else {
    pending_popup->step = PendingPopup::CREATION_COMPLETE;
  }
}

bool CefBrowserInfoManager::AddWebContents(content::WebContents* new_contents) {
  CEF_REQUIRE_UIT();

  // Pending popup information may be missing in cases where
  // chrome::AddWebContents is called directly from the Chrome UI (profile
  // settings, etc).
  auto pending_popup =
      PopPendingPopup(PendingPopup::WEB_CONTENTS_CREATED,
                      PendingPopup::WEB_CONTENTS_CREATED, new_contents);
  if (pending_popup) {
    DCHECK(!pending_popup->alloy_style);
    pending_popup->step = PendingPopup::CREATION_COMPLETE;
    return !pending_popup->use_default_browser_creation;
  }

  // Proceed with default handling.
  return false;
}

void CefBrowserInfoManager::OnGetNewBrowserInfo(
    const content::GlobalRenderFrameHostToken& global_token,
    cef::mojom::BrowserManager::GetNewBrowserInfoCallback callback) {
  DCHECK(frame_util::IsValidGlobalToken(global_token));
  DCHECK(callback);

  auto callback_runner = base::SequencedTaskRunner::GetCurrentDefault();

  base::AutoLock lock_scope(browser_info_lock_);

  scoped_refptr<CefBrowserInfo> browser_info =
      GetBrowserInfoInternal(global_token);

  if (browser_info) {
    // Send the response immediately.
    SendNewBrowserInfoResponse(browser_info, /*is_excluded=*/false,
                               std::move(callback), callback_runner);
    return;
  }

  // Verify that no request for the same route is currently queued.
  DCHECK(pending_new_browser_info_map_.find(global_token) ==
         pending_new_browser_info_map_.end());

  const int timeout_id = ++next_timeout_id_;

  // Queue the request.
  std::unique_ptr<PendingNewBrowserInfo> pending(new PendingNewBrowserInfo());
  pending->global_token = global_token;
  pending->timeout_id = timeout_id;
  pending->callback = std::move(callback);
  pending->callback_runner = callback_runner;
  pending_new_browser_info_map_.insert(
      std::make_pair(global_token, std::move(pending)));

  // Register a timeout for the pending response so that the renderer process
  // doesn't hang forever. With Chrome style, timeouts may occur in cases
  // where chrome::AddWebContents or WebContents::Create are called directly
  // from the Chrome UI (profile settings, etc).
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableNewBrowserInfoTimeout)) {
    CEF_POST_DELAYED_TASK(
        CEF_UIT,
        base::BindOnce(&CefBrowserInfoManager::TimeoutNewBrowserInfoResponse,
                       global_token, timeout_id),
        kNewBrowserInfoResponseTimeoutMs);
  }

  // Check for excluded content (PDF viewer or print preview).
  CEF_POST_TASK(
      CEF_UIT,
      base::BindOnce(
          &CefBrowserInfoManager::CheckExcludedNewBrowserInfoOnUIThread,
          global_token));
}

// static
void CefBrowserInfoManager::CheckExcludedNewBrowserInfoOnUIThread(
    const content::GlobalRenderFrameHostToken& global_token) {
  CEF_REQUIRE_UIT();
  if (!g_info_manager) {
    return;
  }

  // May return nullptr for PDF renderer process.
  auto* rfh = content::RenderFrameHost::FromFrameToken(global_token);
  if (!rfh) {
    return;
  }

  // PDF viewer and print preview create multiple renderer processes. These
  // excluded processes are not tracked by CefBrowserInfo.
  CefBrowserInfo* browser_info;
  bool is_excluded;
  GetFrameHost(rfh, /*prefer_speculative=*/true, &browser_info, &is_excluded);
  if (browser_info && is_excluded) {
    g_info_manager->ContinueNewBrowserInfo(global_token, browser_info,
                                           /*is_excluded=*/true);
  }
}

void CefBrowserInfoManager::ContinueNewBrowserInfo(
    const content::GlobalRenderFrameHostToken& global_token,
    scoped_refptr<CefBrowserInfo> browser_info,
    bool is_excluded) {
  base::AutoLock lock_scope(browser_info_lock_);

  // Continue any pending NewBrowserInfo request.
  auto it = pending_new_browser_info_map_.find(global_token);
  if (it != pending_new_browser_info_map_.end()) {
    SendNewBrowserInfoResponse(browser_info, is_excluded,
                               std::move(it->second->callback),
                               it->second->callback_runner);
    pending_new_browser_info_map_.erase(it);
  }
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

  DCHECK(false);
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
      CefRefPtr<CefBrowserHostBase> browser = (*it)->browser();
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

scoped_refptr<CefBrowserInfo> CefBrowserInfoManager::GetBrowserInfo(
    const content::GlobalRenderFrameHostId& global_id) {
  base::AutoLock lock_scope(browser_info_lock_);
  return GetBrowserInfoInternal(global_id);
}

scoped_refptr<CefBrowserInfo> CefBrowserInfoManager::GetBrowserInfo(
    const content::GlobalRenderFrameHostToken& global_token) {
  base::AutoLock lock_scope(browser_info_lock_);
  return GetBrowserInfoInternal(global_token);
}

bool CefBrowserInfoManager::MaybeAllowNavigation(
    content::RenderFrameHost* opener,
    const content::OpenURLParams& params,
    CefRefPtr<CefBrowserHostBase>& browser_out) const {
  CEF_REQUIRE_UIT();

  auto browser = CefBrowserHostBase::GetBrowserForHost(opener);
  if (!browser) {
    // Print preview uses a modal dialog where we don't own the WebContents.
    // Allow that navigation to proceed.
    return true;
  }

  if (!browser->MaybeAllowNavigation(opener, params)) {
    return false;
  }

  browser_out = browser;
  return true;
}

// static
bool CefBrowserInfoManager::ShouldCreateViewsHostedPopup(
    CefRefPtr<CefBrowserHostBase> opener,
    bool use_default_browser_creation) {
  // In most cases, Views-hosted browsers should create Views-hosted popups
  // and native browsers should use default popup handling. With Chrome
  // style, we should additionally use default handling (a) when using an
  // external parent and (b) when using default Browser creation.
  return opener->HasView() &&
         !opener->platform_delegate()->HasExternalParent() &&
         !use_default_browser_creation;
}

// static
CefRefPtr<CefFrameHostImpl> CefBrowserInfoManager::GetFrameHost(
    content::RenderFrameHost* rfh,
    bool prefer_speculative,
    CefBrowserInfo** browser_info,
    bool* is_excluded) {
  CEF_REQUIRE_UIT();
  DCHECK(rfh);

  const bool is_pdf_process = rfh->GetProcess()->IsPdf();
  auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
  const bool is_browser_process_guest = IsBrowserPluginGuest(web_contents);
  const bool is_print_preview_dialog = IsPrintPreviewDialog(web_contents);

  bool excluded =
      is_pdf_process || is_browser_process_guest || is_print_preview_dialog;

  CefRefPtr<CefFrameHostImpl> frame;

  // A BrowserHost may match an excluded RFH type. Some associations are
  // registered directly via CefBrowserInfo::MaybeCreateFrame and some are
  // discovered indirectly via GetOwnerForGuestContents.
  auto browser = CefBrowserHostBase::GetBrowserForHost(rfh);
  if (browser && !excluded) {
    frame = browser->browser_info()->GetFrameForHost(rfh, prefer_speculative);
  }

  if (browser_info) {
    *browser_info = browser ? browser->browser_info().get() : nullptr;
  }

  if (is_excluded) {
    *is_excluded = excluded;
  }

  if (VLOG_IS_ON(1)) {
    const std::string& debug_string =
        frame_util::GetFrameDebugString(rfh->GetGlobalFrameToken());
    const bool is_main = rfh->GetParent() == nullptr;

    VLOG(1) << "frame " << debug_string << ", pdf_process=" << is_pdf_process
            << ", browser_process_guest=" << is_browser_process_guest
            << ", print_preview_dialog=" << is_print_preview_dialog
            << ", main=" << is_main << (browser ? "" : ", has no BrowserHost")
            << (frame ? "" : ", has no FrameHost");
  }

  return frame;
}

// static
bool CefBrowserInfoManager::IsExcludedFrameHost(content::RenderFrameHost* rfh) {
  CEF_REQUIRE_UIT();
  DCHECK(rfh);

  const bool is_pdf_process = rfh->GetProcess()->IsPdf();
  if (is_pdf_process) {
    return true;
  }

  auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
  const bool is_browser_process_guest = IsBrowserPluginGuest(web_contents);
  if (is_browser_process_guest) {
    return true;
  }
  const bool is_print_preview_dialog = IsPrintPreviewDialog(web_contents);
  if (is_print_preview_dialog) {
    return true;
  }

  return false;
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

  host->RemoveObserver(this);

  // TODO: Change to content::ChildProcessId usage once supported by
  // GlobalRenderFrameHostToken. See https://crbug.com/379869738.
  const int render_process_id = host->GetDeprecatedID();
  DCHECK_GT(render_process_id, 0);

  // Remove all pending requests that reference the destroyed host.
  {
    base::AutoLock lock_scope(browser_info_lock_);

    PendingNewBrowserInfoMap::iterator it =
        pending_new_browser_info_map_.begin();
    while (it != pending_new_browser_info_map_.end()) {
      const auto& info = it->second;
      if (info->global_token.child_id == render_process_id) {
        CancelNewBrowserInfoResponse(info.get());
        it = pending_new_browser_info_map_.erase(it);
      } else {
        ++it;
      }
    }
  }

  // Remove all pending popups that reference the destroyed host as the opener.
  std::erase_if(
      pending_popup_list_, [render_process_id](const auto& popup_ptr) {
        return popup_ptr->opener_global_id.child_id == render_process_id;
      });
}

void CefBrowserInfoManager::PushPendingPopup(
    std::unique_ptr<PendingPopup> popup) {
  CEF_REQUIRE_UIT();
  pending_popup_list_.push_back(std::move(popup));
}

std::unique_ptr<CefBrowserInfoManager::PendingPopup>
CefBrowserInfoManager::PopPendingPopup(
    PendingPopup::Step previous_step_alloy,
    PendingPopup::Step previous_step_chrome,
    const content::GlobalRenderFrameHostId& opener_global_id,
    const GURL& target_url) {
  CEF_REQUIRE_UIT();
  DCHECK(frame_util::IsValidGlobalId(opener_global_id));
  DCHECK_LE(previous_step_alloy, PendingPopup::GET_CUSTOM_WEB_CONTENTS_VIEW);
  DCHECK_LE(previous_step_chrome, PendingPopup::GET_CUSTOM_WEB_CONTENTS_VIEW);

  PendingPopupList::iterator it = pending_popup_list_.begin();
  for (; it != pending_popup_list_.end(); ++it) {
    PendingPopup* popup = it->get();
    const auto previous_step =
        popup->alloy_style ? previous_step_alloy : previous_step_chrome;

    if (popup->step == previous_step &&
        popup->opener_global_id == opener_global_id &&
        popup->target_url == target_url) {
      // Transfer ownership of the pointer.
      it->release();
      pending_popup_list_.erase(it);
      return base::WrapUnique(popup);
    }
  }

  return nullptr;
}

std::unique_ptr<CefBrowserInfoManager::PendingPopup>
CefBrowserInfoManager::PopPendingPopup(PendingPopup::Step previous_step_alloy,
                                       PendingPopup::Step previous_step_chrome,
                                       content::WebContents* new_contents) {
  CEF_REQUIRE_UIT();
  DCHECK_GE(previous_step_alloy, PendingPopup::WEB_CONTENTS_CREATED);
  DCHECK_GE(previous_step_chrome, PendingPopup::WEB_CONTENTS_CREATED);

  PendingPopupList::iterator it = pending_popup_list_.begin();
  for (; it != pending_popup_list_.end(); ++it) {
    PendingPopup* popup = it->get();
    const auto previous_step =
        popup->alloy_style ? previous_step_alloy : previous_step_chrome;

    if (popup->step == previous_step && popup->new_contents == new_contents) {
      // Transfer ownership of the pointer.
      it->release();
      pending_popup_list_.erase(it);
      return base::WrapUnique(popup);
    }
  }

  return nullptr;
}

scoped_refptr<CefBrowserInfo> CefBrowserInfoManager::GetBrowserInfoInternal(
    const content::GlobalRenderFrameHostId& global_id) {
  browser_info_lock_.AssertAcquired();

  if (!frame_util::IsValidGlobalId(global_id)) {
    return nullptr;
  }

  for (const auto& browser_info : browser_info_list_) {
    auto frame = browser_info->GetFrameForGlobalId(global_id);
    if (frame) {
      return browser_info;
    }
  }

  return nullptr;
}

scoped_refptr<CefBrowserInfo> CefBrowserInfoManager::GetBrowserInfoInternal(
    const content::GlobalRenderFrameHostToken& global_token) {
  browser_info_lock_.AssertAcquired();

  if (!frame_util::IsValidGlobalToken(global_token)) {
    return nullptr;
  }

  for (const auto& browser_info : browser_info_list_) {
    auto frame = browser_info->GetFrameForGlobalToken(global_token);
    if (frame) {
      return browser_info;
    }
  }

  return nullptr;
}

// static
void CefBrowserInfoManager::SendNewBrowserInfoResponse(
    scoped_refptr<CefBrowserInfo> browser_info,
    bool is_excluded,
    cef::mojom::BrowserManager::GetNewBrowserInfoCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_runner) {
  if (!callback_runner->RunsTasksInCurrentSequence()) {
    callback_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&CefBrowserInfoManager::SendNewBrowserInfoResponse,
                       browser_info, is_excluded, std::move(callback),
                       callback_runner));
    return;
  }

  auto params = cef::mojom::NewBrowserInfo::New();
  params->is_excluded = is_excluded;

  if (browser_info) {
    params->browser_id = browser_info->browser_id();
    params->is_windowless = browser_info->is_windowless();
    params->is_popup = browser_info->is_popup();
    params->print_preview_enabled = browser_info->print_preview_enabled();

    auto extra_info = browser_info->extra_info();
    if (extra_info) {
      auto extra_info_impl =
          static_cast<CefDictionaryValueImpl*>(extra_info.get());
      auto extra_info_value = extra_info_impl->CopyValue();
      params->extra_info = std::move(extra_info_value.GetDict());
    }
  } else {
    // The new browser info response has timed out.
    params->browser_id = -1;
  }

  std::move(callback).Run(std::move(params));
}

// static
void CefBrowserInfoManager::CancelNewBrowserInfoResponse(
    PendingNewBrowserInfo* pending_info) {
  SendNewBrowserInfoResponse(/*browser_info=*/nullptr, /*is_excluded=*/false,
                             std::move(pending_info->callback),
                             pending_info->callback_runner);
}

// static
void CefBrowserInfoManager::TimeoutNewBrowserInfoResponse(
    const content::GlobalRenderFrameHostToken& global_token,
    int timeout_id) {
  CEF_REQUIRE_UIT();
  if (!g_info_manager) {
    return;
  }

  base::AutoLock lock_scope(g_info_manager->browser_info_lock_);

  // Continue the NewBrowserInfo request if it's still pending.
  auto it = g_info_manager->pending_new_browser_info_map_.find(global_token);
  if (it != g_info_manager->pending_new_browser_info_map_.end()) {
    const auto& pending_info = it->second;
    // Don't accidentally timeout a new request for the same frame.
    if (pending_info->timeout_id != timeout_id) {
      return;
    }

    // Cases where we expect to time out are:
    // - With Chrome style when chrome::AddWebContents or WebContents::Create
    //   are called directly from the Chrome UI (profile settings, etc). A RFH
    //   will exist without a matching CefBrowserHost.
    // - When the PDF renderer is loaded in the print preview dialog. There will
    //   be no RFH in this case.
    // Any additional cases should be debugged and, if possible,
    // GetOwnerForGuestContents should be improved to find the associated
    // CefBrowserHost.
    const bool has_rfh =
        !!content::RenderFrameHost::FromFrameToken(global_token);

    LOG(ERROR) << "Timeout of new browser info response for frame "
               << frame_util::GetFrameDebugString(global_token)
               << " (has_rfh=" << has_rfh << ")";

    CancelNewBrowserInfoResponse(pending_info.get());
    g_info_manager->pending_new_browser_info_map_.erase(it);
  }
}
