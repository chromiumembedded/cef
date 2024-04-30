// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/alloy/browser_platform_delegate_alloy.h"

#include <memory>

#include "libcef/browser/alloy/alloy_browser_host_impl.h"
#include "libcef/browser/extensions/extension_background_host.h"
#include "libcef/browser/extensions/extension_system.h"
#include "libcef/browser/extensions/extension_view_host.h"
#include "libcef/browser/extensions/extension_web_contents_observer.h"
#include "libcef/common/extensions/extensions_util.h"
#include "libcef/common/net/url_util.h"

#include "base/logging.h"
#include "cef/libcef/features/features.h"
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

#if BUILDFLAG(ENABLE_ALLOY_BOOTSTRAP)
#include "libcef/browser/alloy/dialogs/alloy_javascript_dialog_manager_delegate.h"
#include "libcef/features/runtime_checks.h"

#include "chrome/browser/printing/printing_init.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "components/javascript_dialogs/tab_modal_dialog_manager.h"
#include "components/permissions/permission_request_manager.h"
#include "components/zoom/zoom_controller.h"
#include "extensions/browser/extension_registry.h"
#endif  // BUILDFLAG(ENABLE_ALLOY_BOOTSTRAP)

namespace {

const char kAttachedHelpersUserDataKey[] = "CefAttachedHelpers";

#if BUILDFLAG(ENABLE_ALLOY_BOOTSTRAP)
const extensions::Extension* GetExtensionForUrl(
    content::BrowserContext* browser_context,
    const GURL& url) {
  auto* registry = extensions::ExtensionRegistry::Get(browser_context);
  if (!registry) {
    return nullptr;
  }
  std::string extension_id = url.host();
  return registry->enabled_extensions().GetByID(extension_id);
}
#endif  // BUILDFLAG(ENABLE_ALLOY_BOOTSTRAP)

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

  scoped_refptr<content::SiteInstance> site_instance;
#if BUILDFLAG(ENABLE_ALLOY_BOOTSTRAP)
  if (extensions::ExtensionsEnabled() && !create_params.url.empty()) {
    GURL gurl = url_util::MakeGURL(create_params.url, /*fixup=*/true);
    if (!create_params.extension) {
      // We might be loading an extension app view where the extension URL is
      // provided by the client.
      create_params.extension = GetExtensionForUrl(browser_context, gurl);
    }
    if (create_params.extension) {
      if (create_params.extension_host_type ==
          extensions::mojom::ViewType::kInvalid) {
        // Default to popup behavior.
        create_params.extension_host_type =
            extensions::mojom::ViewType::kExtensionPopup;
      }

      // Extension resources will fail to load if we don't use a SiteInstance
      // associated with the extension.
      // (AlloyContentBrowserClient::SiteInstanceGotProcessAndSite won't find
      // the extension to register with InfoMap, and AllowExtensionResourceLoad
      // in ExtensionProtocolHandler::MaybeCreateJob will return false resulting
      // in ERR_BLOCKED_BY_CLIENT).
      site_instance = extensions::ProcessManager::Get(browser_context)
                          ->GetSiteInstanceForURL(gurl);
      DCHECK(site_instance);
    }
  }
#endif  // BUILDFLAG(ENABLE_ALLOY_BOOTSTRAP)

  content::WebContents::CreateParams wc_create_params(browser_context,
                                                      site_instance);

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

#if BUILDFLAG(ENABLE_ALLOY_BOOTSTRAP)
  if (extension_host_) {
    extension_host_->AddNewContents(source, std::move(new_contents), target_url,
                                    disposition, window_features, user_gesture,
                                    was_blocked);
  }
#endif
}

#if BUILDFLAG(ENABLE_ALLOY_BOOTSTRAP)
bool CefBrowserPlatformDelegateAlloy::
    ShouldAllowRendererInitiatedCrossProcessNavigation(
        bool is_main_frame_navigation) {
  if (extension_host_) {
    return extension_host_->ShouldAllowRendererInitiatedCrossProcessNavigation(
        is_main_frame_navigation);
  }
  return true;
}
#endif

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

#if BUILDFLAG(ENABLE_ALLOY_BOOTSTRAP)
void CefBrowserPlatformDelegateAlloy::CreateExtensionHost(
    const extensions::Extension* extension,
    const GURL& url,
    extensions::mojom::ViewType host_type) {
  REQUIRE_ALLOY_RUNTIME();
  DCHECK(primary_);

  // Should get WebContentsCreated and BrowserCreated calls first.
  DCHECK(web_contents_);
  DCHECK(browser_);
  DCHECK(!extension_host_);

  auto alloy_browser = AlloyBrowserHostImpl::FromBaseChecked(browser_);

  if (host_type == extensions::mojom::ViewType::kExtensionPopup) {
    // Create an extension host that we own.
    extension_host_ = new extensions::CefExtensionViewHost(
        alloy_browser.get(), extension, web_contents_, url, host_type);
    // Trigger load of the extension URL.
    extension_host_->CreateRendererSoon();
  } else if (host_type ==
             extensions::mojom::ViewType::kExtensionBackgroundPage) {
    is_background_host_ = true;
    alloy_browser->is_background_host_ = true;
    // Create an extension host that will be owned by ProcessManager.
    extension_host_ = new extensions::CefExtensionBackgroundHost(
        alloy_browser.get(),
        base::BindOnce(&CefBrowserPlatformDelegateAlloy::OnExtensionHostDeleted,
                       weak_ptr_factory_.GetWeakPtr()),
        extension, web_contents_, url, host_type);
    // Load will be triggered by ProcessManager::CreateBackgroundHost.
  } else {
    DCHECK(false) << " Unsupported extension host type: " << host_type;
  }
}

extensions::ExtensionHost* CefBrowserPlatformDelegateAlloy::GetExtensionHost()
    const {
  return extension_host_;
}
#endif  // BUILDFLAG(ENABLE_ALLOY_BOOTSTRAP)

void CefBrowserPlatformDelegateAlloy::BrowserDestroyed(
    CefBrowserHostBase* browser) {
  if (primary_) {
#if BUILDFLAG(ENABLE_ALLOY_BOOTSTRAP)
    DestroyExtensionHost();
#endif
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

#if BUILDFLAG(ENABLE_ALLOY_BOOTSTRAP)
bool CefBrowserPlatformDelegateAlloy::PreHandleGestureEvent(
    content::WebContents* source,
    const blink::WebGestureEvent& event) {
  if (extension_host_) {
    return extension_host_->PreHandleGestureEvent(source, event);
  }
  return false;
}

bool CefBrowserPlatformDelegateAlloy::IsNeverComposited(
    content::WebContents* web_contents) {
  if (extension_host_) {
    return extension_host_->IsNeverComposited(web_contents);
  }
  return false;
}
#endif  // BUILDFLAG(ENABLE_ALLOY_BOOTSTRAP)

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

#if BUILDFLAG(ENABLE_ALLOY_BOOTSTRAP)
void CefBrowserPlatformDelegateAlloy::DestroyExtensionHost() {
  if (!extension_host_) {
    return;
  }
  if (extension_host_->extension_host_type() ==
      extensions::mojom::ViewType::kExtensionBackgroundPage) {
    DCHECK(is_background_host_);
    // Close notification for background pages arrives via CloseContents.
    // The extension host will be deleted by
    // ProcessManager::CloseBackgroundHost and OnExtensionHostDeleted will be
    // called to notify us.
    extension_host_->Close();
  } else {
    DCHECK(!is_background_host_);
    // We own the extension host and must delete it.
    delete extension_host_;
    extension_host_ = nullptr;
  }
}

void CefBrowserPlatformDelegateAlloy::OnExtensionHostDeleted() {
  DCHECK(is_background_host_);
  DCHECK(extension_host_);
  extension_host_ = nullptr;
}
#endif  // BUILDFLAG(ENABLE_ALLOY_BOOTSTRAP)

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

#if BUILDFLAG(ENABLE_ALLOY_BOOTSTRAP)
  // Create all the helpers.
  if (cef::IsAlloyRuntimeEnabled()) {
    find_in_page::FindTabHelper::CreateForWebContents(web_contents);
    permissions::PermissionRequestManager::CreateForWebContents(web_contents);
    PrefsTabHelper::CreateForWebContents(web_contents);
    printing::InitializePrintingForWebContents(web_contents);
    zoom::ZoomController::CreateForWebContents(web_contents);

    javascript_dialogs::TabModalDialogManager::CreateForWebContents(
        web_contents, CreateAlloyJavaScriptTabModalDialogManagerDelegateDesktop(
                          web_contents));
  } else
#endif  // BUILDFLAG(ENABLE_ALLOY_BOOTSTRAP)
  {
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
}
