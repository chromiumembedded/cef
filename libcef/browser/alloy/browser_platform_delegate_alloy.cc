// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/browser/alloy/browser_platform_delegate_alloy.h"

#include <memory>

#include "base/logging.h"
#include "cef/libcef/browser/alloy/alloy_browser_host_impl.h"
#include "cef/libcef/common/net/url_util.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/ui/tab_helpers.h"
#include "components/find_in_page/find_tab_helper.h"
#include "components/find_in_page/find_types.h"
#include "components/performance_manager/embedder/performance_manager_registry.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/render_view_host.h"
#include "extensions/browser/process_manager.h"
#include "pdf/pdf_features.h"
#include "third_party/blink/public/mojom/frame/find_in_page.mojom.h"

namespace {

const char kAttachedHelpersUserDataKey[] = "CefAttachedHelpers";

}  // namespace

CefBrowserPlatformDelegateAlloy::CefBrowserPlatformDelegateAlloy()
    : weak_ptr_factory_(this) {}

content::WebContents* CefBrowserPlatformDelegateAlloy::CreateWebContents(
    CefBrowserCreateParams& create_params,
    bool& own_web_contents) {
  DCHECK(primary_);

  if (!create_params.request_context) {
    // Using the global request context.
    create_params.request_context = CefRequestContext::GetGlobalContext();
  }

  auto* browser_context =
      CefRequestContextImpl::GetBrowserContext(create_params.request_context);
  CHECK(browser_context);

  content::WebContents::CreateParams wc_create_params(browser_context, nullptr);

  if (IsWindowless()) {
    // Create the OSR view for the WebContents.
    CreateViewForWebContents(&wc_create_params.view,
                             &wc_create_params.delegate_view);
  }

  auto web_contents = content::WebContents::Create(wc_create_params);
  CHECK(web_contents);

  own_web_contents = true;
  return web_contents.release();
}

void CefBrowserPlatformDelegateAlloy::WebContentsCreated(
    content::WebContents* web_contents,
    bool owned) {
  CefBrowserPlatformDelegate::WebContentsCreated(web_contents, owned);

  if (primary_) {
    AttachHelpers(web_contents);

    if (owned) {
      SetOwnedWebContents(web_contents);
    }
  } else {
    DCHECK(!owned);
  }
}

void CefBrowserPlatformDelegateAlloy::AddNewContents(
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture,
    bool* was_blocked) {
  DCHECK(primary_);

  CefRefPtr<AlloyBrowserHostImpl> owner =
      AlloyBrowserHostImpl::GetBrowserForContents(new_contents.get());
  if (owner) {
    // Taking ownership of |new_contents|.
    static_cast<CefBrowserPlatformDelegateAlloy*>(
        owner->platform_delegate_.get())
        ->SetOwnedWebContents(new_contents.release());
    return;
  }
}

void CefBrowserPlatformDelegateAlloy::RenderViewReady() {
  ConfigureAutoResize();
}

void CefBrowserPlatformDelegateAlloy::BrowserCreated(
    CefBrowserHostBase* browser) {
  CefBrowserPlatformDelegate::BrowserCreated(browser);

  // Only register WebContents delegate/observers if we're the primary delegate.
  if (!primary_) {
    return;
  }

  DCHECK(!web_contents_->GetDelegate());
  web_contents_->SetDelegate(
      AlloyBrowserHostImpl::FromBaseChecked(browser).get());

  AttachHelpers(web_contents_);

  // Used for print preview and JavaScript dialogs.
  web_contents_dialog_helper_ =
      std::make_unique<AlloyWebContentsDialogHelper>(web_contents_, this);
}

void CefBrowserPlatformDelegateAlloy::BrowserDestroyed(
    CefBrowserHostBase* browser) {
  if (primary_) {
    owned_web_contents_.reset();
  }

  CefBrowserPlatformDelegate::BrowserDestroyed(browser);
}

web_modal::WebContentsModalDialogHost*
CefBrowserPlatformDelegateAlloy::GetWebContentsModalDialogHost() const {
  return web_contents_dialog_helper_.get();
}

void CefBrowserPlatformDelegateAlloy::SendCaptureLostEvent() {
  if (!web_contents_) {
    return;
  }
  content::RenderViewHost* host = web_contents_->GetRenderViewHost();
  if (!host) {
    return;
  }

  content::RenderWidgetHostImpl* widget =
      content::RenderWidgetHostImpl::From(host->GetWidget());
  if (widget) {
    widget->LostCapture();
  }
}

#if BUILDFLAG(IS_WIN) || (BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC))
void CefBrowserPlatformDelegateAlloy::NotifyMoveOrResizeStarted() {
  if (!browser_) {
    return;
  }

  // Dismiss any existing popups.
  auto frame = browser_->GetMainFrame();
  if (frame && frame->IsValid()) {
    static_cast<CefFrameHostImpl*>(frame.get())->NotifyMoveOrResizeStarted();
  }
}
#endif

void CefBrowserPlatformDelegateAlloy::SetAutoResizeEnabled(
    bool enabled,
    const CefSize& min_size,
    const CefSize& max_size) {
  if (enabled == auto_resize_enabled_) {
    return;
  }

  auto_resize_enabled_ = enabled;
  if (enabled) {
    auto_resize_min_ = gfx::Size(min_size.width, min_size.height);
    auto_resize_max_ = gfx::Size(max_size.width, max_size.height);
  } else {
    auto_resize_min_ = auto_resize_max_ = gfx::Size();
  }
  ConfigureAutoResize();
}

void CefBrowserPlatformDelegateAlloy::ConfigureAutoResize() {
  if (!web_contents_ || !web_contents_->GetRenderWidgetHostView()) {
    return;
  }

  if (auto_resize_enabled_) {
    web_contents_->GetRenderWidgetHostView()->EnableAutoResize(
        auto_resize_min_, auto_resize_max_);
  } else {
    web_contents_->GetRenderWidgetHostView()->DisableAutoResize(gfx::Size());
  }
}

void CefBrowserPlatformDelegateAlloy::Find(const CefString& searchText,
                                           bool forward,
                                           bool matchCase,
                                           bool findNext) {
  if (!web_contents_) {
    return;
  }

  find_in_page::FindTabHelper::FromWebContents(web_contents_)
      ->StartFinding(searchText.ToString16(), forward, matchCase, findNext,
                     /*run_synchronously_for_testing=*/false);
}

void CefBrowserPlatformDelegateAlloy::StopFinding(bool clearSelection) {
  if (!web_contents_) {
    return;
  }

  last_search_result_ = find_in_page::FindNotificationDetails();
  find_in_page::FindTabHelper::FromWebContents(web_contents_)
      ->StopFinding(clearSelection ? find_in_page::SelectionAction::kClear
                                   : find_in_page::SelectionAction::kKeep);
}

bool CefBrowserPlatformDelegateAlloy::HandleFindReply(
    int request_id,
    int number_of_matches,
    const gfx::Rect& selection_rect,
    int active_match_ordinal,
    bool final_update) {
  if (!web_contents_) {
    return false;
  }

  auto find_in_page =
      find_in_page::FindTabHelper::FromWebContents(web_contents_);

  find_in_page->HandleFindReply(request_id, number_of_matches, selection_rect,
                                active_match_ordinal, final_update);
  if (!(find_in_page->find_result() == last_search_result_)) {
    last_search_result_ = find_in_page->find_result();
    return true;
  }
  return false;
}

base::RepeatingClosure
CefBrowserPlatformDelegateAlloy::GetBoundsChangedCallback() {
  if (web_contents_dialog_helper_) {
    return web_contents_dialog_helper_->GetBoundsChangedCallback();
  }

  return base::RepeatingClosure();
}

void CefBrowserPlatformDelegateAlloy::SetOwnedWebContents(
    content::WebContents* owned_contents) {
  DCHECK(primary_);

  // Should not currently own a WebContents.
  CHECK(!owned_web_contents_);
  owned_web_contents_.reset(owned_contents);
}

void CefBrowserPlatformDelegateAlloy::AttachHelpers(
    content::WebContents* web_contents) {
  // If already attached, nothing to be done.
  base::SupportsUserData::Data* attached_tag =
      web_contents->GetUserData(&kAttachedHelpersUserDataKey);
  if (attached_tag) {
    return;
  }

  // Mark as attached.
  web_contents->SetUserData(&kAttachedHelpersUserDataKey,
                            std::make_unique<base::SupportsUserData::Data>());

  if (IsWindowless()) {
    // Logic from ChromeContentBrowserClientCef::GetWebContentsViewDelegate
    // which is not called for windowless browsers. Needs to be done before
    // calling AttachTabHelpers.
    if (auto* registry =
            performance_manager::PerformanceManagerRegistry::GetInstance()) {
      registry->MaybeCreatePageNodeForWebContents(web_contents);
    }
  }

  // Adopt the WebContents now, so all observers are in place, as the network
  // requests for its initial navigation will start immediately
  TabHelpers::AttachTabHelpers(web_contents);

  // Make the tab show up in the task manager.
  task_manager::WebContentsTags::CreateForTabContents(web_contents);
}
