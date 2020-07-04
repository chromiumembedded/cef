// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/browser_platform_delegate.h"

#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/extensions/browser_extensions_util.h"
#include "libcef/browser/extensions/extension_background_host.h"
#include "libcef/browser/extensions/extension_system.h"
#include "libcef/browser/extensions/extension_view_host.h"
#include "libcef/browser/extensions/extension_web_contents_observer.h"
#include "libcef/browser/printing/print_view_manager.h"
#include "libcef/browser/web_contents_dialog_helper.h"
#include "libcef/common/extensions/extensions_util.h"
#include "libcef/features/runtime_checks.h"

#include "base/logging.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "components/zoom/zoom_controller.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "extensions/browser/process_manager.h"
#include "third_party/blink/public/mojom/frame/find_in_page.mojom.h"

CefBrowserPlatformDelegate::CefBrowserPlatformDelegate()
    : weak_ptr_factory_(this) {}

CefBrowserPlatformDelegate::~CefBrowserPlatformDelegate() {
  DCHECK(!browser_);
}

content::WebContents* CefBrowserPlatformDelegate::CreateWebContents(
    CefBrowserHostImpl::CreateParams& create_params,
    bool& own_web_contents) {
  // TODO(chrome-runtime): Add a path to create a Browser.
  REQUIRE_ALLOY_RUNTIME();

  // Get or create the request context and browser context.
  CefRefPtr<CefRequestContextImpl> request_context_impl =
      CefRequestContextImpl::GetOrCreateForRequestContext(
          create_params.request_context);
  CHECK(request_context_impl);
  auto cef_browser_context = request_context_impl->GetBrowserContext();
  CHECK(cef_browser_context);
  auto browser_context = cef_browser_context->AsBrowserContext();

  if (!create_params.request_context) {
    // Using the global request context.
    create_params.request_context = request_context_impl.get();
  }

  scoped_refptr<content::SiteInstance> site_instance;
  if (extensions::ExtensionsEnabled() && !create_params.url.is_empty()) {
    if (!create_params.extension) {
      // We might be loading an extension app view where the extension URL is
      // provided by the client.
      create_params.extension =
          extensions::GetExtensionForUrl(browser_context, create_params.url);
    }
    if (create_params.extension) {
      if (create_params.extension_host_type == extensions::VIEW_TYPE_INVALID) {
        // Default to dialog behavior.
        create_params.extension_host_type =
            extensions::VIEW_TYPE_EXTENSION_DIALOG;
      }

      // Extension resources will fail to load if we don't use a SiteInstance
      // associated with the extension.
      // (AlloyContentBrowserClient::SiteInstanceGotProcess won't find the
      // extension to register with InfoMap, and AllowExtensionResourceLoad in
      // ExtensionProtocolHandler::MaybeCreateJob will return false resulting in
      // ERR_BLOCKED_BY_CLIENT).
      site_instance = extensions::ProcessManager::Get(browser_context)
                          ->GetSiteInstanceForURL(create_params.url);
      DCHECK(site_instance);
    }
  }

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

void CefBrowserPlatformDelegate::CreateViewForWebContents(
    content::WebContentsView** view,
    content::RenderViewHostDelegateView** delegate_view) {
  NOTREACHED();
}

void CefBrowserPlatformDelegate::WebContentsCreated(
    content::WebContents* web_contents,
    bool owned) {
  // We should not have a browser at this point.
  DCHECK(!browser_);

  DCHECK(!web_contents_);
  web_contents_ = web_contents;
  if (owned) {
    SetOwnedWebContents(web_contents);
  }
}

void CefBrowserPlatformDelegate::AddNewContents(
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_rect,
    bool user_gesture,
    bool* was_blocked) {
  REQUIRE_ALLOY_RUNTIME();

  CefRefPtr<CefBrowserHostImpl> owner =
      CefBrowserHostImpl::GetBrowserForContents(new_contents.get());
  if (owner) {
    // Taking ownership of |new_contents|.
    owner->platform_delegate_->SetOwnedWebContents(new_contents.release());
    return;
  }

  if (extension_host_) {
    extension_host_->AddNewContents(source, std::move(new_contents), target_url,
                                    disposition, initial_rect, user_gesture,
                                    was_blocked);
  }
}

void CefBrowserPlatformDelegate::WebContentsDestroyed(
    content::WebContents* web_contents) {
  DCHECK(web_contents_ && web_contents_ == web_contents);
  web_contents_ = nullptr;
}

bool CefBrowserPlatformDelegate::ShouldTransferNavigation(
    bool is_main_frame_navigation) {
  if (extension_host_) {
    return extension_host_->ShouldTransferNavigation(is_main_frame_navigation);
  }
  return true;
}

void CefBrowserPlatformDelegate::RenderViewCreated(
    content::RenderViewHost* render_view_host) {
  // Indicate that the view has an external parent (namely us). This changes the
  // default view behavior in some cases (e.g. focus handling on Linux).
  if (!IsViewsHosted() && render_view_host->GetWidget()->GetView())
    render_view_host->GetWidget()->GetView()->SetHasExternalParent(true);
}

void CefBrowserPlatformDelegate::BrowserCreated(CefBrowserHostImpl* browser) {
  // We should have an associated WebContents at this point.
  DCHECK(web_contents_);

  DCHECK(!browser_);
  DCHECK(browser);
  browser_ = browser;

  web_contents_->SetDelegate(browser);

  PrefsTabHelper::CreateForWebContents(web_contents_);
  printing::CefPrintViewManager::CreateForWebContents(web_contents_);

  if (extensions::ExtensionsEnabled()) {
    extensions::CefExtensionWebContentsObserver::CreateForWebContents(
        web_contents_);

    // Used by the tabs extension API.
    zoom::ZoomController::CreateForWebContents(web_contents_);
  }

  if (IsPrintPreviewSupported()) {
    web_contents_dialog_helper_.reset(
        new CefWebContentsDialogHelper(web_contents_, this));
  }
}

void CefBrowserPlatformDelegate::CreateExtensionHost(
    const extensions::Extension* extension,
    const GURL& url,
    extensions::ViewType host_type) {
  // Should get WebContentsCreated and BrowserCreated calls first.
  DCHECK(web_contents_);
  DCHECK(browser_);
  DCHECK(!extension_host_);

  if (host_type == extensions::VIEW_TYPE_EXTENSION_DIALOG ||
      host_type == extensions::VIEW_TYPE_EXTENSION_POPUP) {
    // Create an extension host that we own.
    extension_host_ = new extensions::CefExtensionViewHost(
        browser_, extension, web_contents_, url, host_type);
    // Trigger load of the extension URL.
    extension_host_->CreateRenderViewSoon();
  } else if (host_type == extensions::VIEW_TYPE_EXTENSION_BACKGROUND_PAGE) {
    is_background_host_ = true;
    browser_->is_background_host_ = true;
    // Create an extension host that will be owned by ProcessManager.
    extension_host_ = new extensions::CefExtensionBackgroundHost(
        browser_,
        base::BindOnce(&CefBrowserPlatformDelegate::OnExtensionHostDeleted,
                       weak_ptr_factory_.GetWeakPtr()),
        extension, web_contents_, url, host_type);
    // Load will be triggered by ProcessManager::CreateBackgroundHost.
  } else {
    NOTREACHED() << " Unsupported extension host type: " << host_type;
  }
}

void CefBrowserPlatformDelegate::NotifyBrowserCreated() {}

void CefBrowserPlatformDelegate::NotifyBrowserDestroyed() {}

void CefBrowserPlatformDelegate::BrowserDestroyed(CefBrowserHostImpl* browser) {
  // WebContentsDestroyed should already be called.
  DCHECK(!web_contents_);

  DestroyExtensionHost();
  owned_web_contents_.reset();

  DCHECK(browser_ && browser_ == browser);
  browser_ = nullptr;
}

bool CefBrowserPlatformDelegate::CreateHostWindow() {
  NOTREACHED();
  return true;
}

void CefBrowserPlatformDelegate::CloseHostWindow() {
  NOTREACHED();
}

#if defined(USE_AURA)
views::Widget* CefBrowserPlatformDelegate::GetWindowWidget() const {
  NOTREACHED();
  return nullptr;
}

CefRefPtr<CefBrowserView> CefBrowserPlatformDelegate::GetBrowserView() const {
  NOTREACHED();
  return nullptr;
}
#endif

void CefBrowserPlatformDelegate::PopupWebContentsCreated(
    const CefBrowserSettings& settings,
    CefRefPtr<CefClient> client,
    content::WebContents* new_web_contents,
    CefBrowserPlatformDelegate* new_platform_delegate,
    bool is_devtools) {}

void CefBrowserPlatformDelegate::PopupBrowserCreated(
    CefBrowserHostImpl* new_browser,
    bool is_devtools) {}

void CefBrowserPlatformDelegate::SendCaptureLostEvent() {
  content::RenderWidgetHostImpl* widget = content::RenderWidgetHostImpl::From(
      browser_->web_contents()->GetRenderViewHost()->GetWidget());
  if (widget)
    widget->LostCapture();
}

#if defined(OS_WIN) || (defined(OS_POSIX) && !defined(OS_MACOSX))
void CefBrowserPlatformDelegate::NotifyMoveOrResizeStarted() {
  // Dismiss any existing popups.
  content::RenderViewHost* host = browser_->web_contents()->GetRenderViewHost();
  if (host)
    host->NotifyMoveOrResizeStarted();
}

void CefBrowserPlatformDelegate::SizeTo(int width, int height) {}
#endif

bool CefBrowserPlatformDelegate::PreHandleGestureEvent(
    content::WebContents* source,
    const blink::WebGestureEvent& event) {
  if (extension_host_)
    return extension_host_->PreHandleGestureEvent(source, event);
  return false;
}

bool CefBrowserPlatformDelegate::IsNeverComposited(
    content::WebContents* web_contents) {
  if (extension_host_)
    return extension_host_->IsNeverComposited(web_contents);
  return false;
}

std::unique_ptr<CefFileDialogRunner>
CefBrowserPlatformDelegate::CreateFileDialogRunner() {
  return nullptr;
}

std::unique_ptr<CefJavaScriptDialogRunner>
CefBrowserPlatformDelegate::CreateJavaScriptDialogRunner() {
  return nullptr;
}

void CefBrowserPlatformDelegate::WasHidden(bool hidden) {
  NOTREACHED();
}

void CefBrowserPlatformDelegate::NotifyScreenInfoChanged() {
  NOTREACHED();
}

void CefBrowserPlatformDelegate::Invalidate(cef_paint_element_type_t type) {
  NOTREACHED();
}

void CefBrowserPlatformDelegate::SendExternalBeginFrame() {
  NOTREACHED();
}

void CefBrowserPlatformDelegate::SetWindowlessFrameRate(int frame_rate) {
  NOTREACHED();
}

void CefBrowserPlatformDelegate::ImeSetComposition(
    const CefString& text,
    const std::vector<CefCompositionUnderline>& underlines,
    const CefRange& replacement_range,
    const CefRange& selection_range) {
  NOTREACHED();
}

void CefBrowserPlatformDelegate::ImeCommitText(
    const CefString& text,
    const CefRange& replacement_range,
    int relative_cursor_pos) {
  NOTREACHED();
}

void CefBrowserPlatformDelegate::ImeFinishComposingText(bool keep_selection) {
  NOTREACHED();
}

void CefBrowserPlatformDelegate::ImeCancelComposition() {
  NOTREACHED();
}

void CefBrowserPlatformDelegate::DragTargetDragEnter(
    CefRefPtr<CefDragData> drag_data,
    const CefMouseEvent& event,
    cef_drag_operations_mask_t allowed_ops) {
  NOTREACHED();
}

void CefBrowserPlatformDelegate::DragTargetDragOver(
    const CefMouseEvent& event,
    cef_drag_operations_mask_t allowed_ops) {
  NOTREACHED();
}

void CefBrowserPlatformDelegate::DragTargetDragLeave() {
  NOTREACHED();
}

void CefBrowserPlatformDelegate::DragTargetDrop(const CefMouseEvent& event) {
  NOTREACHED();
}

void CefBrowserPlatformDelegate::StartDragging(
    const content::DropData& drop_data,
    blink::WebDragOperationsMask allowed_ops,
    const gfx::ImageSkia& image,
    const gfx::Vector2d& image_offset,
    const content::DragEventSourceInfo& event_info,
    content::RenderWidgetHostImpl* source_rwh) {
  NOTREACHED();
}

void CefBrowserPlatformDelegate::UpdateDragCursor(
    blink::WebDragOperation operation) {
  NOTREACHED();
}

void CefBrowserPlatformDelegate::DragSourceEndedAt(
    int x,
    int y,
    cef_drag_operations_mask_t op) {
  NOTREACHED();
}

void CefBrowserPlatformDelegate::DragSourceSystemDragEnded() {
  NOTREACHED();
}

void CefBrowserPlatformDelegate::AccessibilityEventReceived(
    const content::AXEventNotificationDetails& eventData) {
  NOTREACHED();
}

void CefBrowserPlatformDelegate::AccessibilityLocationChangesReceived(
    const std::vector<content::AXLocationChangeNotificationDetails>& locData) {
  NOTREACHED();
}

gfx::Point CefBrowserPlatformDelegate::GetDialogPosition(
    const gfx::Size& size) {
  NOTREACHED();
  return gfx::Point();
}

gfx::Size CefBrowserPlatformDelegate::GetMaximumDialogSize() {
  NOTREACHED();
  return gfx::Size();
}

void CefBrowserPlatformDelegate::SetAutoResizeEnabled(bool enabled,
                                                      const CefSize& min_size,
                                                      const CefSize& max_size) {
  if (enabled == auto_resize_enabled_)
    return;

  auto_resize_enabled_ = enabled;
  if (enabled) {
    auto_resize_min_ = gfx::Size(min_size.width, min_size.height);
    auto_resize_max_ = gfx::Size(max_size.width, max_size.height);
  } else {
    auto_resize_min_ = auto_resize_max_ = gfx::Size();
  }
  ConfigureAutoResize();
}

void CefBrowserPlatformDelegate::ConfigureAutoResize() {
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

void CefBrowserPlatformDelegate::SetAccessibilityState(
    cef_state_t accessibility_state) {
  // Do nothing if state is set to default. It'll be disabled by default and
  // controlled by the commmand-line flags "force-renderer-accessibility" and
  // "disable-renderer-accessibility".
  if (accessibility_state == STATE_DEFAULT)
    return;

  content::WebContentsImpl* web_contents_impl =
      static_cast<content::WebContentsImpl*>(web_contents_);

  if (!web_contents_impl)
    return;

  ui::AXMode accMode;
  // In windowless mode set accessibility to TreeOnly mode. Else native
  // accessibility APIs, specific to each platform, are also created.
  if (accessibility_state == STATE_ENABLED) {
    accMode = IsWindowless() ? ui::kAXModeWebContentsOnly : ui::kAXModeComplete;
  }
  web_contents_impl->SetAccessibilityMode(accMode);
}

bool CefBrowserPlatformDelegate::IsPrintPreviewSupported() const {
  CEF_REQUIRE_UIT();
  auto actionable_contents = GetActionableWebContents();
  if (!actionable_contents)
    return false;

  auto cef_browser_context = CefBrowserContext::FromBrowserContext(
      actionable_contents->GetBrowserContext());
  if (!cef_browser_context->IsPrintPreviewSupported()) {
    return false;
  }

  // Print preview is not currently supported with OSR.
  return !IsWindowless();
}

void CefBrowserPlatformDelegate::Print() {
  auto actionable_contents = GetActionableWebContents();
  if (!actionable_contents)
    return;

  auto rfh = actionable_contents->GetMainFrame();

  if (IsPrintPreviewSupported()) {
    printing::CefPrintViewManager::FromWebContents(actionable_contents)
        ->PrintPreviewNow(rfh, false);
  } else {
    printing::PrintViewManager::FromWebContents(actionable_contents)
        ->PrintNow(rfh);
  }
}

void CefBrowserPlatformDelegate::PrintToPDF(
    const CefString& path,
    const CefPdfPrintSettings& settings,
    CefRefPtr<CefPdfPrintCallback> callback) {
  content::WebContents* actionable_contents = GetActionableWebContents();
  if (!actionable_contents)
    return;
  printing::CefPrintViewManager::PdfPrintCallback pdf_callback;
  if (callback.get()) {
    pdf_callback = base::Bind(&CefPdfPrintCallback::OnPdfPrintFinished,
                              callback.get(), path);
  }
  printing::CefPrintViewManager::FromWebContents(actionable_contents)
      ->PrintToPDF(actionable_contents->GetMainFrame(), base::FilePath(path),
                   settings, pdf_callback);
}

void CefBrowserPlatformDelegate::Find(int identifier,
                                      const CefString& searchText,
                                      bool forward,
                                      bool matchCase,
                                      bool findNext) {
  if (!web_contents_)
    return;

  // Every find request must have a unique ID and these IDs must strictly
  // increase so that newer requests always have greater IDs than older
  // requests.
  if (identifier <= find_request_id_counter_)
    identifier = ++find_request_id_counter_;
  else
    find_request_id_counter_ = identifier;

  auto options = blink::mojom::FindOptions::New();
  options->forward = forward;
  options->match_case = matchCase;
  options->find_next = findNext;
  web_contents_->Find(identifier, searchText, std::move(options));
}

void CefBrowserPlatformDelegate::StopFinding(bool clearSelection) {
  if (!web_contents_)
    return;

  content::StopFindAction action =
      clearSelection ? content::STOP_FIND_ACTION_CLEAR_SELECTION
                     : content::STOP_FIND_ACTION_KEEP_SELECTION;
  web_contents_->StopFinding(action);
}

base::RepeatingClosure CefBrowserPlatformDelegate::GetBoundsChangedCallback() {
  if (web_contents_dialog_helper_) {
    return web_contents_dialog_helper_->GetBoundsChangedCallback();
  }

  return base::RepeatingClosure();
}

// static
int CefBrowserPlatformDelegate::TranslateWebEventModifiers(
    uint32 cef_modifiers) {
  int result = 0;
  // Set modifiers based on key state.
  if (cef_modifiers & EVENTFLAG_SHIFT_DOWN)
    result |= blink::WebInputEvent::kShiftKey;
  if (cef_modifiers & EVENTFLAG_CONTROL_DOWN)
    result |= blink::WebInputEvent::kControlKey;
  if (cef_modifiers & EVENTFLAG_ALT_DOWN)
    result |= blink::WebInputEvent::kAltKey;
  if (cef_modifiers & EVENTFLAG_COMMAND_DOWN)
    result |= blink::WebInputEvent::kMetaKey;
  if (cef_modifiers & EVENTFLAG_LEFT_MOUSE_BUTTON)
    result |= blink::WebInputEvent::kLeftButtonDown;
  if (cef_modifiers & EVENTFLAG_MIDDLE_MOUSE_BUTTON)
    result |= blink::WebInputEvent::kMiddleButtonDown;
  if (cef_modifiers & EVENTFLAG_RIGHT_MOUSE_BUTTON)
    result |= blink::WebInputEvent::kRightButtonDown;
  if (cef_modifiers & EVENTFLAG_CAPS_LOCK_ON)
    result |= blink::WebInputEvent::kCapsLockOn;
  if (cef_modifiers & EVENTFLAG_NUM_LOCK_ON)
    result |= blink::WebInputEvent::kNumLockOn;
  if (cef_modifiers & EVENTFLAG_IS_LEFT)
    result |= blink::WebInputEvent::kIsLeft;
  if (cef_modifiers & EVENTFLAG_IS_RIGHT)
    result |= blink::WebInputEvent::kIsRight;
  if (cef_modifiers & EVENTFLAG_IS_KEY_PAD)
    result |= blink::WebInputEvent::kIsKeyPad;
  return result;
}

content::WebContents* CefBrowserPlatformDelegate::GetActionableWebContents()
    const {
  if (web_contents_ && extensions::ExtensionsEnabled()) {
    content::WebContents* guest_contents =
        extensions::GetFullPageGuestForOwnerContents(web_contents_);
    if (guest_contents)
      return guest_contents;
  }
  return web_contents_;
}

void CefBrowserPlatformDelegate::SetOwnedWebContents(
    content::WebContents* owned_contents) {
  REQUIRE_ALLOY_RUNTIME();

  // Should not currently own a WebContents.
  CHECK(!owned_web_contents_);
  owned_web_contents_.reset(owned_contents);
}

void CefBrowserPlatformDelegate::DestroyExtensionHost() {
  if (!extension_host_)
    return;
  if (extension_host_->extension_host_type() ==
      extensions::VIEW_TYPE_EXTENSION_BACKGROUND_PAGE) {
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

void CefBrowserPlatformDelegate::OnExtensionHostDeleted() {
  DCHECK(is_background_host_);
  DCHECK(extension_host_);
  extension_host_ = nullptr;
}
