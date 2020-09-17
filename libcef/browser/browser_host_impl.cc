// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/browser_host_impl.h"

#include <string>
#include <utility>

#include "libcef/browser/alloy/alloy_browser_context.h"
#include "libcef/browser/audio_capturer.h"
#include "libcef/browser/browser_context.h"
#include "libcef/browser/browser_info.h"
#include "libcef/browser/browser_info_manager.h"
#include "libcef/browser/browser_platform_delegate.h"
#include "libcef/browser/browser_util.h"
#include "libcef/browser/context.h"
#include "libcef/browser/devtools/devtools_manager.h"
#include "libcef/browser/media_capture_devices_dispatcher.h"
#include "libcef/browser/osr/osr_util.h"
#include "libcef/browser/request_context_impl.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/cef_messages.h"
#include "libcef/common/cef_switches.h"
#include "libcef/common/drag_data_impl.h"
#include "libcef/common/request_impl.h"
#include "libcef/common/values_impl.h"
#include "libcef/features/runtime_checks.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/widget_messages.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_observer.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"
#include "net/base/net_errors.h"
#include "third_party/blink/public/mojom/page/widget.mojom-test-utils.h"
#include "ui/events/base_event_utils.h"

using content::KeyboardEventProcessingResult;

namespace {

class ShowDevToolsHelper {
 public:
  ShowDevToolsHelper(CefRefPtr<CefBrowserHostImpl> browser,
                     const CefWindowInfo& windowInfo,
                     CefRefPtr<CefClient> client,
                     const CefBrowserSettings& settings,
                     const CefPoint& inspect_element_at)
      : browser_(browser),
        window_info_(windowInfo),
        client_(client),
        settings_(settings),
        inspect_element_at_(inspect_element_at) {}

  CefRefPtr<CefBrowserHostImpl> browser_;
  CefWindowInfo window_info_;
  CefRefPtr<CefClient> client_;
  CefBrowserSettings settings_;
  CefPoint inspect_element_at_;
};

void ShowDevToolsWithHelper(ShowDevToolsHelper* helper) {
  helper->browser_->ShowDevTools(helper->window_info_, helper->client_,
                                 helper->settings_,
                                 helper->inspect_element_at_);
  delete helper;
}

class CefWidgetHostInterceptor
    : public blink::mojom::WidgetHostInterceptorForTesting,
      public content::RenderWidgetHostObserver {
 public:
  CefWidgetHostInterceptor(CefBrowserHostImpl* browser,
                           content::RenderViewHost* render_view_host)
      : browser_(browser),
        render_widget_host_(
            content::RenderWidgetHostImpl::From(render_view_host->GetWidget())),
        impl_(render_widget_host_->widget_host_receiver_for_testing()
                  .SwapImplForTesting(this)) {
    render_widget_host_->AddObserver(this);
  }

  blink::mojom::WidgetHost* GetForwardingInterface() override { return impl_; }

  // WidgetHostInterceptorForTesting method:
  void SetCursor(const ui::Cursor& cursor) override {
    if (browser_->IsMouseCursorChangeDisabled()) {
      // Don't change the cursor.
      return;
    }
    GetForwardingInterface()->SetCursor(cursor);
  }

  // RenderWidgetHostObserver method:
  void RenderWidgetHostDestroyed(
      content::RenderWidgetHost* widget_host) override {
    widget_host->RemoveObserver(this);
    delete this;
  }

 private:
  CefBrowserHostImpl* const browser_;
  content::RenderWidgetHostImpl* const render_widget_host_;
  blink::mojom::WidgetHost* const impl_;

  DISALLOW_COPY_AND_ASSIGN(CefWidgetHostInterceptor);
};

static constexpr base::TimeDelta kRecentlyAudibleTimeout =
    base::TimeDelta::FromSeconds(2);

}  // namespace

// CefBrowserHostImpl static methods.
// -----------------------------------------------------------------------------

// static
CefRefPtr<CefBrowserHostImpl> CefBrowserHostImpl::Create(
    CreateParams& create_params) {
  std::unique_ptr<CefBrowserPlatformDelegate> platform_delegate =
      CefBrowserPlatformDelegate::Create(create_params);
  CHECK(platform_delegate);

  const bool is_devtools_popup = !!create_params.devtools_opener;

  scoped_refptr<CefBrowserInfo> info =
      CefBrowserInfoManager::GetInstance()->CreateBrowserInfo(
          is_devtools_popup, platform_delegate->IsWindowless(),
          create_params.extra_info);

  bool own_web_contents = false;

  // This call may modify |create_params|.
  auto web_contents =
      platform_delegate->CreateWebContents(create_params, own_web_contents);

  auto request_context_impl =
      static_cast<CefRequestContextImpl*>(create_params.request_context.get());

  CefRefPtr<CefExtension> cef_extension;
  if (create_params.extension) {
    auto cef_browser_context = request_context_impl->GetBrowserContext();
    cef_extension =
        cef_browser_context->GetExtension(create_params.extension->id());
    CHECK(cef_extension);
  }

  auto platform_delegate_ptr = platform_delegate.get();

  CefRefPtr<CefBrowserHostImpl> browser = CreateInternal(
      create_params.settings, create_params.client, web_contents,
      own_web_contents, info,
      static_cast<CefBrowserHostImpl*>(create_params.devtools_opener.get()),
      is_devtools_popup, request_context_impl, std::move(platform_delegate),
      cef_extension);
  if (!browser)
    return nullptr;

  if (create_params.extension) {
    platform_delegate_ptr->CreateExtensionHost(
        create_params.extension, create_params.url,
        create_params.extension_host_type);
  } else if (!create_params.url.is_empty()) {
    browser->LoadMainFrameURL(create_params.url.spec(), content::Referrer(),
                              CefFrameHostImpl::kPageTransitionExplicit,
                              std::string());
  }

  return browser.get();
}

// static
CefRefPtr<CefBrowserHostImpl> CefBrowserHostImpl::CreateInternal(
    const CefBrowserSettings& settings,
    CefRefPtr<CefClient> client,
    content::WebContents* web_contents,
    bool own_web_contents,
    scoped_refptr<CefBrowserInfo> browser_info,
    CefRefPtr<CefBrowserHostImpl> opener,
    bool is_devtools_popup,
    CefRefPtr<CefRequestContextImpl> request_context,
    std::unique_ptr<CefBrowserPlatformDelegate> platform_delegate,
    CefRefPtr<CefExtension> extension) {
  CEF_REQUIRE_UIT();
  DCHECK(web_contents);
  DCHECK(browser_info);
  DCHECK(request_context);
  DCHECK(platform_delegate);

  // If |opener| is non-NULL it must be a popup window.
  DCHECK(!opener.get() || browser_info->is_popup());

  if (opener) {
    if (!opener->platform_delegate_) {
      // The opener window is being destroyed. Cancel the popup.
      if (own_web_contents)
        delete web_contents;
      return nullptr;
    }

    // Give the opener browser's platform delegate an opportunity to modify the
    // new browser's platform delegate.
    opener->platform_delegate_->PopupWebContentsCreated(
        settings, client, web_contents, platform_delegate.get(),
        is_devtools_popup);
  }

  // Take ownership of |web_contents| if |own_web_contents| is true.
  platform_delegate->WebContentsCreated(web_contents, own_web_contents);

  CefRefPtr<CefBrowserHostImpl> browser = new CefBrowserHostImpl(
      settings, client, web_contents, browser_info, opener, request_context,
      std::move(platform_delegate), extension);
  browser->InitializeBrowser();

  if (!browser->CreateHostWindow())
    return nullptr;

  // Notify that the browser has been created. These must be delivered in the
  // expected order.

  // 1. Notify the browser's LifeSpanHandler. This must always be the first
  // notification for the browser.
  browser->OnAfterCreated();

  // 2. Notify the platform delegate. With Views this will result in a call to
  // CefBrowserViewDelegate::OnBrowserCreated().
  browser->platform_delegate_->NotifyBrowserCreated();

  if (opener && opener->platform_delegate_) {
    // 3. Notify the opener browser's platform delegate. With Views this will
    // result in a call to CefBrowserViewDelegate::OnPopupBrowserViewCreated().
    opener->platform_delegate_->PopupBrowserCreated(browser.get(),
                                                    is_devtools_popup);
  }

  return browser;
}

// static
CefRefPtr<CefBrowserHostImpl> CefBrowserHostImpl::GetBrowserForHost(
    const content::RenderViewHost* host) {
  REQUIRE_ALLOY_RUNTIME();
  auto browser = CefBrowserHostBase::GetBrowserForHost(host);
  return static_cast<CefBrowserHostImpl*>(browser.get());
}

// static
CefRefPtr<CefBrowserHostImpl> CefBrowserHostImpl::GetBrowserForHost(
    const content::RenderFrameHost* host) {
  REQUIRE_ALLOY_RUNTIME();
  auto browser = CefBrowserHostBase::GetBrowserForHost(host);
  return static_cast<CefBrowserHostImpl*>(browser.get());
}

// static
CefRefPtr<CefBrowserHostImpl> CefBrowserHostImpl::GetBrowserForContents(
    const content::WebContents* contents) {
  REQUIRE_ALLOY_RUNTIME();
  auto browser = CefBrowserHostBase::GetBrowserForContents(contents);
  return static_cast<CefBrowserHostImpl*>(browser.get());
}

// static
CefRefPtr<CefBrowserHostImpl> CefBrowserHostImpl::GetBrowserForFrameTreeNode(
    int frame_tree_node_id) {
  REQUIRE_ALLOY_RUNTIME();
  auto browser =
      CefBrowserHostBase::GetBrowserForFrameTreeNode(frame_tree_node_id);
  return static_cast<CefBrowserHostImpl*>(browser.get());
}

// static
CefRefPtr<CefBrowserHostImpl> CefBrowserHostImpl::GetBrowserForFrameRoute(
    int render_process_id,
    int render_routing_id) {
  REQUIRE_ALLOY_RUNTIME();
  auto browser = CefBrowserHostBase::GetBrowserForFrameRoute(render_process_id,
                                                             render_routing_id);
  return static_cast<CefBrowserHostImpl*>(browser.get());
}

// CefBrowserHostImpl methods.
// -----------------------------------------------------------------------------

CefBrowserHostImpl::~CefBrowserHostImpl() {}

void CefBrowserHostImpl::CloseBrowser(bool force_close) {
  if (CEF_CURRENTLY_ON_UIT()) {
    // Exit early if a close attempt is already pending and this method is
    // called again from somewhere other than WindowDestroyed().
    if (destruction_state_ >= DESTRUCTION_STATE_PENDING &&
        (IsWindowless() || !window_destroyed_)) {
      if (force_close && destruction_state_ == DESTRUCTION_STATE_PENDING) {
        // Upgrade the destruction state.
        destruction_state_ = DESTRUCTION_STATE_ACCEPTED;
      }
      return;
    }

    if (destruction_state_ < DESTRUCTION_STATE_ACCEPTED) {
      destruction_state_ = (force_close ? DESTRUCTION_STATE_ACCEPTED
                                        : DESTRUCTION_STATE_PENDING);
    }

    content::WebContents* contents = web_contents();
    if (contents && contents->NeedToFireBeforeUnloadOrUnloadEvents()) {
      // Will result in a call to BeforeUnloadFired() and, if the close isn't
      // canceled, CloseContents().
      contents->DispatchBeforeUnload(false /* auto_cancel */);
    } else {
      CloseContents(contents);
    }
  } else {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefBrowserHostImpl::CloseBrowser,
                                          this, force_close));
  }
}

bool CefBrowserHostImpl::TryCloseBrowser() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    NOTREACHED() << "called on invalid thread";
    return false;
  }

  // Protect against multiple requests to close while the close is pending.
  if (destruction_state_ <= DESTRUCTION_STATE_PENDING) {
    if (destruction_state_ == DESTRUCTION_STATE_NONE) {
      // Request that the browser close.
      CloseBrowser(false);
    }

    // Cancel the close.
    return false;
  }

  // Allow the close.
  return true;
}

void CefBrowserHostImpl::SetFocus(bool focus) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHostImpl::SetFocus, this, focus));
    return;
  }

  if (focus)
    OnSetFocus(FOCUS_SOURCE_SYSTEM);
  else if (platform_delegate_)
    platform_delegate_->SendFocusEvent(false);
}

CefWindowHandle CefBrowserHostImpl::GetWindowHandle() {
  if (IsViewsHosted() && CEF_CURRENTLY_ON_UIT()) {
    // Always return the most up-to-date window handle for a views-hosted
    // browser since it may change if the view is re-parented.
    if (platform_delegate_)
      return platform_delegate_->GetHostWindowHandle();
  }
  return host_window_handle_;
}

CefWindowHandle CefBrowserHostImpl::GetOpenerWindowHandle() {
  return opener_;
}

bool CefBrowserHostImpl::HasView() {
  return IsViewsHosted();
}

double CefBrowserHostImpl::GetZoomLevel() {
  // Verify that this method is being called on the UI thread.
  if (!CEF_CURRENTLY_ON_UIT()) {
    NOTREACHED() << "called on invalid thread";
    return 0;
  }

  if (web_contents())
    return content::HostZoomMap::GetZoomLevel(web_contents());

  return 0;
}

void CefBrowserHostImpl::SetZoomLevel(double zoomLevel) {
  if (CEF_CURRENTLY_ON_UIT()) {
    if (web_contents())
      content::HostZoomMap::SetZoomLevel(web_contents(), zoomLevel);
  } else {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefBrowserHostImpl::SetZoomLevel,
                                          this, zoomLevel));
  }
}

void CefBrowserHostImpl::RunFileDialog(
    FileDialogMode mode,
    const CefString& title,
    const CefString& default_file_path,
    const std::vector<CefString>& accept_filters,
    int selected_accept_filter,
    CefRefPtr<CefRunFileDialogCallback> callback) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHostImpl::RunFileDialog, this, mode,
                                 title, default_file_path, accept_filters,
                                 selected_accept_filter, callback));
    return;
  }

  EnsureFileDialogManager();
  file_dialog_manager_->RunFileDialog(mode, title, default_file_path,
                                      accept_filters, selected_accept_filter,
                                      callback);
}

void CefBrowserHostImpl::Print() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefBrowserHostImpl::Print, this));
    return;
  }

  if (platform_delegate_)
    platform_delegate_->Print();
}

void CefBrowserHostImpl::PrintToPDF(const CefString& path,
                                    const CefPdfPrintSettings& settings,
                                    CefRefPtr<CefPdfPrintCallback> callback) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefBrowserHostImpl::PrintToPDF, this,
                                          path, settings, callback));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->PrintToPDF(path, settings, callback);
  }
}

void CefBrowserHostImpl::Find(int identifier,
                              const CefString& searchText,
                              bool forward,
                              bool matchCase,
                              bool findNext) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHostImpl::Find, this, identifier,
                                 searchText, forward, matchCase, findNext));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->Find(identifier, searchText, forward, matchCase,
                             findNext);
  }
}

void CefBrowserHostImpl::StopFinding(bool clearSelection) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefBrowserHostImpl::StopFinding,
                                          this, clearSelection));
    return;
  }

  if (platform_delegate_)
    platform_delegate_->StopFinding(clearSelection);
}

void CefBrowserHostImpl::ShowDevTools(const CefWindowInfo& windowInfo,
                                      CefRefPtr<CefClient> client,
                                      const CefBrowserSettings& settings,
                                      const CefPoint& inspect_element_at) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    ShowDevToolsHelper* helper = new ShowDevToolsHelper(
        this, windowInfo, client, settings, inspect_element_at);
    CEF_POST_TASK(CEF_UIT, base::BindOnce(ShowDevToolsWithHelper, helper));
    return;
  }

  if (!EnsureDevToolsManager())
    return;
  devtools_manager_->ShowDevTools(windowInfo, client, settings,
                                  inspect_element_at);
}

void CefBrowserHostImpl::CloseDevTools() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHostImpl::CloseDevTools, this));
    return;
  }

  if (!devtools_manager_)
    return;
  devtools_manager_->CloseDevTools();
}

bool CefBrowserHostImpl::HasDevTools() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    NOTREACHED() << "called on invalid thread";
    return false;
  }

  if (!devtools_manager_)
    return false;
  return devtools_manager_->HasDevTools();
}

bool CefBrowserHostImpl::SendDevToolsMessage(const void* message,
                                             size_t message_size) {
  if (!message || message_size == 0)
    return false;

  if (!CEF_CURRENTLY_ON_UIT()) {
    std::string message_str(static_cast<const char*>(message), message_size);
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(
            [](CefRefPtr<CefBrowserHostImpl> self, std::string message_str) {
              self->SendDevToolsMessage(message_str.data(), message_str.size());
            },
            CefRefPtr<CefBrowserHostImpl>(this), std::move(message_str)));
    return false;
  }

  if (!EnsureDevToolsManager())
    return false;
  return devtools_manager_->SendDevToolsMessage(message, message_size);
}

int CefBrowserHostImpl::ExecuteDevToolsMethod(
    int message_id,
    const CefString& method,
    CefRefPtr<CefDictionaryValue> params) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT, base::BindOnce(base::IgnoreResult(
                                    &CefBrowserHostImpl::ExecuteDevToolsMethod),
                                this, message_id, method, params));
    return 0;
  }

  if (!EnsureDevToolsManager())
    return 0;
  return devtools_manager_->ExecuteDevToolsMethod(message_id, method, params);
}

CefRefPtr<CefRegistration> CefBrowserHostImpl::AddDevToolsMessageObserver(
    CefRefPtr<CefDevToolsMessageObserver> observer) {
  if (!observer)
    return nullptr;
  auto registration = CefDevToolsManager::CreateRegistration(observer);
  InitializeDevToolsRegistrationOnUIThread(registration);
  return registration.get();
}

bool CefBrowserHostImpl::EnsureDevToolsManager() {
  CEF_REQUIRE_UIT();
  if (!web_contents())
    return false;

  if (!devtools_manager_) {
    devtools_manager_.reset(new CefDevToolsManager(this));
  }
  return true;
}

void CefBrowserHostImpl::InitializeDevToolsRegistrationOnUIThread(
    CefRefPtr<CefRegistration> registration) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(
            &CefBrowserHostImpl::InitializeDevToolsRegistrationOnUIThread, this,
            registration));
    return;
  }

  if (!EnsureDevToolsManager())
    return;
  devtools_manager_->InitializeRegistrationOnUIThread(registration);
}

void CefBrowserHostImpl::SetAccessibilityState(
    cef_state_t accessibility_state) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHostImpl::SetAccessibilityState,
                                 this, accessibility_state));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->SetAccessibilityState(accessibility_state);
  }
}

void CefBrowserHostImpl::SetAutoResizeEnabled(bool enabled,
                                              const CefSize& min_size,
                                              const CefSize& max_size) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT, base::BindOnce(&CefBrowserHostImpl::SetAutoResizeEnabled, this,
                                enabled, min_size, max_size));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->SetAutoResizeEnabled(enabled, min_size, max_size);
  }
}

CefRefPtr<CefExtension> CefBrowserHostImpl::GetExtension() {
  return extension_;
}

bool CefBrowserHostImpl::IsBackgroundHost() {
  return is_background_host_;
}

void CefBrowserHostImpl::SetMouseCursorChangeDisabled(bool disabled) {
  base::AutoLock lock_scope(state_lock_);
  if (mouse_cursor_change_disabled_ == disabled)
    return;
  mouse_cursor_change_disabled_ = disabled;
}

bool CefBrowserHostImpl::IsMouseCursorChangeDisabled() {
  base::AutoLock lock_scope(state_lock_);
  return mouse_cursor_change_disabled_;
}

bool CefBrowserHostImpl::IsWindowRenderingDisabled() {
  return IsWindowless();
}

void CefBrowserHostImpl::WasResized() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHostImpl::WasResized, this));
    return;
  }

  if (platform_delegate_)
    platform_delegate_->WasResized();
}

void CefBrowserHostImpl::WasHidden(bool hidden) {
  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHost::WasHidden, this, hidden));
    return;
  }

  if (platform_delegate_)
    platform_delegate_->WasHidden(hidden);
}

void CefBrowserHostImpl::NotifyScreenInfoChanged() {
  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&CefBrowserHostImpl::NotifyScreenInfoChanged, this));
    return;
  }

  if (platform_delegate_)
    platform_delegate_->NotifyScreenInfoChanged();
}

void CefBrowserHostImpl::Invalidate(PaintElementType type) {
  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHostImpl::Invalidate, this, type));
    return;
  }

  if (platform_delegate_)
    platform_delegate_->Invalidate(type);
}

void CefBrowserHostImpl::SendExternalBeginFrame() {
  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT, base::Bind(&CefBrowserHostImpl::SendExternalBeginFrame, this));
    return;
  }

  if (platform_delegate_)
    platform_delegate_->SendExternalBeginFrame();
}

void CefBrowserHostImpl::SendKeyEvent(const CefKeyEvent& event) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefBrowserHostImpl::SendKeyEvent,
                                          this, event));
    return;
  }

  if (platform_delegate_)
    platform_delegate_->SendKeyEvent(event);
}

void CefBrowserHostImpl::SendMouseClickEvent(const CefMouseEvent& event,
                                             MouseButtonType type,
                                             bool mouseUp,
                                             int clickCount) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHostImpl::SendMouseClickEvent, this,
                                 event, type, mouseUp, clickCount));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->SendMouseClickEvent(event, type, mouseUp, clickCount);
  }
}

void CefBrowserHostImpl::SendMouseMoveEvent(const CefMouseEvent& event,
                                            bool mouseLeave) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHostImpl::SendMouseMoveEvent, this,
                                 event, mouseLeave));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->SendMouseMoveEvent(event, mouseLeave);
  }
}

void CefBrowserHostImpl::SendMouseWheelEvent(const CefMouseEvent& event,
                                             int deltaX,
                                             int deltaY) {
  if (deltaX == 0 && deltaY == 0) {
    // Nothing to do.
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHostImpl::SendMouseWheelEvent, this,
                                 event, deltaX, deltaY));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->SendMouseWheelEvent(event, deltaX, deltaY);
  }
}

void CefBrowserHostImpl::SendTouchEvent(const CefTouchEvent& event) {
  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::Bind(&CefBrowserHostImpl::SendTouchEvent, this, event));
    return;
  }

  if (platform_delegate_)
    platform_delegate_->SendTouchEvent(event);
}

void CefBrowserHostImpl::SendFocusEvent(bool setFocus) {
  SetFocus(setFocus);
}

void CefBrowserHostImpl::SendCaptureLostEvent() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&CefBrowserHostImpl::SendCaptureLostEvent, this));
    return;
  }

  if (platform_delegate_)
    platform_delegate_->SendCaptureLostEvent();
}

void CefBrowserHostImpl::NotifyMoveOrResizeStarted() {
#if defined(OS_WIN) || (defined(OS_POSIX) && !defined(OS_MAC))
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&CefBrowserHostImpl::NotifyMoveOrResizeStarted, this));
    return;
  }

  if (platform_delegate_)
    platform_delegate_->NotifyMoveOrResizeStarted();
#endif
}

int CefBrowserHostImpl::GetWindowlessFrameRate() {
  // Verify that this method is being called on the UI thread.
  if (!CEF_CURRENTLY_ON_UIT()) {
    NOTREACHED() << "called on invalid thread";
    return 0;
  }

  return osr_util::ClampFrameRate(settings_.windowless_frame_rate);
}

void CefBrowserHostImpl::SetWindowlessFrameRate(int frame_rate) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHostImpl::SetWindowlessFrameRate,
                                 this, frame_rate));
    return;
  }

  settings_.windowless_frame_rate = frame_rate;

  if (platform_delegate_)
    platform_delegate_->SetWindowlessFrameRate(frame_rate);
}

// CefBrowser methods.
// -----------------------------------------------------------------------------

void CefBrowserHostImpl::GoBack() {
  auto callback = base::BindOnce(&CefBrowserHostImpl::GoBack, this);
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, std::move(callback));
  }

  if (browser_info_->IsNavigationLocked(std::move(callback))) {
    return;
  }

  auto wc = web_contents();
  if (wc && wc->GetController().CanGoBack()) {
    wc->GetController().GoBack();
  }
}

void CefBrowserHostImpl::GoForward() {
  auto callback = base::BindOnce(&CefBrowserHostImpl::GoForward, this);
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, std::move(callback));
  }

  if (browser_info_->IsNavigationLocked(std::move(callback))) {
    return;
  }

  auto wc = web_contents();
  if (wc && wc->GetController().CanGoForward()) {
    wc->GetController().GoForward();
  }
}

void CefBrowserHostImpl::Reload() {
  auto callback = base::BindOnce(&CefBrowserHostImpl::Reload, this);
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, std::move(callback));
  }

  if (browser_info_->IsNavigationLocked(std::move(callback))) {
    return;
  }

  auto wc = web_contents();
  if (wc) {
    wc->GetController().Reload(content::ReloadType::NORMAL, true);
  }
}

void CefBrowserHostImpl::ReloadIgnoreCache() {
  auto callback = base::BindOnce(&CefBrowserHostImpl::ReloadIgnoreCache, this);
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, std::move(callback));
  }

  if (browser_info_->IsNavigationLocked(std::move(callback))) {
    return;
  }

  auto wc = web_contents();
  if (wc) {
    wc->GetController().Reload(content::ReloadType::BYPASSING_CACHE, true);
  }
}

void CefBrowserHostImpl::StopLoad() {
  auto callback = base::BindOnce(&CefBrowserHostImpl::StopLoad, this);
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, std::move(callback));
  }

  if (browser_info_->IsNavigationLocked(std::move(callback))) {
    return;
  }

  auto wc = web_contents();
  if (wc) {
    wc->Stop();
  }
}

// CefBrowserHostImpl public methods.
// -----------------------------------------------------------------------------

bool CefBrowserHostImpl::IsWindowless() const {
  return is_windowless_;
}

bool CefBrowserHostImpl::IsViewsHosted() const {
  return is_views_hosted_;
}

bool CefBrowserHostImpl::IsPictureInPictureSupported() const {
  // Not currently supported with OSR.
  return !IsWindowless();
}

void CefBrowserHostImpl::WindowDestroyed() {
  CEF_REQUIRE_UIT();
  DCHECK(!window_destroyed_);
  window_destroyed_ = true;
  CloseBrowser(true);
}

void CefBrowserHostImpl::DestroyBrowser() {
  CEF_REQUIRE_UIT();

  destruction_state_ = DESTRUCTION_STATE_COMPLETED;

  // Notify that this browser has been destroyed. These must be delivered in
  // the expected order.

  // 1. Notify the platform delegate. With Views this will result in a call to
  // CefBrowserViewDelegate::OnBrowserDestroyed().
  platform_delegate_->NotifyBrowserDestroyed();

  // 2. Notify the browser's LifeSpanHandler. This must always be the last
  // notification for this browser.
  OnBeforeClose();

  // Destroy any platform constructs first.
  if (file_dialog_manager_.get())
    file_dialog_manager_->Destroy();
  if (javascript_dialog_manager_.get())
    javascript_dialog_manager_->Destroy();
  if (menu_manager_.get())
    menu_manager_->Destroy();

  // Notify any observers that may have state associated with this browser.
  OnBrowserDestroyed();

  // If the WebContents still exists at this point, signal destruction before
  // browser destruction.
  if (web_contents()) {
    WebContentsDestroyed();
  }

  // Disassociate the platform delegate from this browser.
  platform_delegate_->BrowserDestroyed(this);

  registrar_.reset(nullptr);

  // Delete objects created by the platform delegate that may be referenced by
  // the WebContents.
  file_dialog_manager_.reset(nullptr);
  javascript_dialog_manager_.reset(nullptr);
  menu_manager_.reset(nullptr);

  // Delete the audio capturer
  recently_audible_timer_.Stop();
  audio_capturer_.reset(nullptr);

  devtools_manager_.reset(nullptr);

  // Delete the platform delegate.
  platform_delegate_.reset(nullptr);

  CefBrowserHostBase::DestroyBrowser();
}

#if defined(USE_AURA)
views::Widget* CefBrowserHostImpl::GetWindowWidget() const {
  CEF_REQUIRE_UIT();
  if (!platform_delegate_)
    return nullptr;
  return platform_delegate_->GetWindowWidget();
}

CefRefPtr<CefBrowserView> CefBrowserHostImpl::GetBrowserView() const {
  CEF_REQUIRE_UIT();
  if (IsViewsHosted() && platform_delegate_)
    return platform_delegate_->GetBrowserView();
  return nullptr;
}
#endif

void CefBrowserHostImpl::CancelContextMenu() {
  CEF_REQUIRE_UIT();
  if (menu_manager_)
    menu_manager_->CancelContextMenu();
}

void CefBrowserHostImpl::ViewText(const std::string& text) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHostImpl::ViewText, this, text));
    return;
  }

  if (platform_delegate_)
    platform_delegate_->ViewText(text);
}

SkColor CefBrowserHostImpl::GetBackgroundColor() const {
  // Don't use |platform_delegate_| because it's not thread-safe.
  return CefContext::Get()->GetBackgroundColor(
      &settings_, is_windowless_ ? STATE_ENABLED : STATE_DISABLED);
}

extensions::ExtensionHost* CefBrowserHostImpl::GetExtensionHost() const {
  CEF_REQUIRE_UIT();
  DCHECK(platform_delegate_);
  return platform_delegate_->GetExtensionHost();
}

void CefBrowserHostImpl::OnSetFocus(cef_focus_source_t source) {
  if (CEF_CURRENTLY_ON_UIT()) {
    // SetFocus() might be called while inside the OnSetFocus() callback. If
    // so, don't re-enter the callback.
    if (!is_in_onsetfocus_) {
      if (client_.get()) {
        CefRefPtr<CefFocusHandler> handler = client_->GetFocusHandler();
        if (handler.get()) {
          is_in_onsetfocus_ = true;
          bool handled = handler->OnSetFocus(this, source);
          is_in_onsetfocus_ = false;

          if (handled)
            return;
        }
      }
    }

    if (platform_delegate_)
      platform_delegate_->SendFocusEvent(true);
  } else {
    CEF_POST_TASK(
        CEF_UIT, base::BindOnce(&CefBrowserHostImpl::OnSetFocus, this, source));
  }
}

void CefBrowserHostImpl::RunFileChooser(
    const CefFileDialogRunner::FileChooserParams& params,
    CefFileDialogRunner::RunFileChooserCallback callback) {
  EnsureFileDialogManager();
  file_dialog_manager_->RunFileChooser(params, std::move(callback));
}

bool CefBrowserHostImpl::EmbedsFullscreenWidget() {
  // When using windowless rendering do not allow Flash to create its own
  // full- screen widget.
  return IsWindowless();
}

void CefBrowserHostImpl::EnterFullscreenModeForTab(
    content::RenderFrameHost* requesting_frame,
    const blink::mojom::FullscreenOptions& options) {
  contents_delegate_->EnterFullscreenModeForTab(requesting_frame, options);
  WasResized();
}

void CefBrowserHostImpl::ExitFullscreenModeForTab(
    content::WebContents* web_contents) {
  contents_delegate_->ExitFullscreenModeForTab(web_contents);
  WasResized();
}

bool CefBrowserHostImpl::IsFullscreenForTabOrPending(
    const content::WebContents* web_contents) {
  return is_fullscreen_;
}

blink::mojom::DisplayMode CefBrowserHostImpl::GetDisplayMode(
    const content::WebContents* web_contents) {
  return is_fullscreen_ ? blink::mojom::DisplayMode::kFullscreen
                        : blink::mojom::DisplayMode::kBrowser;
}

void CefBrowserHostImpl::FindReply(content::WebContents* web_contents,
                                   int request_id,
                                   int number_of_matches,
                                   const gfx::Rect& selection_rect,
                                   int active_match_ordinal,
                                   bool final_update) {
  if (client_.get()) {
    CefRefPtr<CefFindHandler> handler = client_->GetFindHandler();
    if (handler.get()) {
      CefRect rect(selection_rect.x(), selection_rect.y(),
                   selection_rect.width(), selection_rect.height());
      handler->OnFindResult(this, request_id, number_of_matches, rect,
                            active_match_ordinal, final_update);
    }
  }
}

void CefBrowserHostImpl::ImeSetComposition(
    const CefString& text,
    const std::vector<CefCompositionUnderline>& underlines,
    const CefRange& replacement_range,
    const CefRange& selection_range) {
  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&CefBrowserHostImpl::ImeSetComposition, this, text,
                       underlines, replacement_range, selection_range));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->ImeSetComposition(text, underlines, replacement_range,
                                          selection_range);
  }
}

void CefBrowserHostImpl::ImeCommitText(const CefString& text,
                                       const CefRange& replacement_range,
                                       int relative_cursor_pos) {
  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHostImpl::ImeCommitText, this, text,
                                 replacement_range, relative_cursor_pos));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->ImeCommitText(text, replacement_range,
                                      relative_cursor_pos);
  }
}

void CefBrowserHostImpl::ImeFinishComposingText(bool keep_selection) {
  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHostImpl::ImeFinishComposingText,
                                 this, keep_selection));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->ImeFinishComposingText(keep_selection);
  }
}

void CefBrowserHostImpl::ImeCancelComposition() {
  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&CefBrowserHostImpl::ImeCancelComposition, this));
    return;
  }

  if (platform_delegate_)
    platform_delegate_->ImeCancelComposition();
}

void CefBrowserHostImpl::DragTargetDragEnter(
    CefRefPtr<CefDragData> drag_data,
    const CefMouseEvent& event,
    CefBrowserHost::DragOperationsMask allowed_ops) {
  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHostImpl::DragTargetDragEnter, this,
                                 drag_data, event, allowed_ops));
    return;
  }

  if (!drag_data.get()) {
    NOTREACHED();
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->DragTargetDragEnter(drag_data, event, allowed_ops);
  }
}

void CefBrowserHostImpl::DragTargetDragOver(
    const CefMouseEvent& event,
    CefBrowserHost::DragOperationsMask allowed_ops) {
  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&CefBrowserHostImpl::DragTargetDragOver, this,
                                 event, allowed_ops));
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->DragTargetDragOver(event, allowed_ops);
  }
}

void CefBrowserHostImpl::DragTargetDragLeave() {
  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(
                               &CefBrowserHostImpl::DragTargetDragLeave, this));
    return;
  }

  if (platform_delegate_)
    platform_delegate_->DragTargetDragLeave();
}

void CefBrowserHostImpl::DragTargetDrop(const CefMouseEvent& event) {
  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefBrowserHostImpl::DragTargetDrop,
                                          this, event));
    return;
  }

  if (platform_delegate_)
    platform_delegate_->DragTargetDrop(event);
}

void CefBrowserHostImpl::DragSourceSystemDragEnded() {
  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&CefBrowserHostImpl::DragSourceSystemDragEnded, this));
    return;
  }

  if (platform_delegate_)
    platform_delegate_->DragSourceSystemDragEnded();
}

void CefBrowserHostImpl::DragSourceEndedAt(
    int x,
    int y,
    CefBrowserHost::DragOperationsMask op) {
  if (!IsWindowless()) {
    NOTREACHED() << "Window rendering is not disabled";
    return;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(
        CEF_UIT,
        base::BindOnce(&CefBrowserHostImpl::DragSourceEndedAt, this, x, y, op));
    return;
  }

  if (platform_delegate_)
    platform_delegate_->DragSourceEndedAt(x, y, op);
}

void CefBrowserHostImpl::SetAudioMuted(bool mute) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::Bind(&CefBrowserHostImpl::SetAudioMuted, this, mute));
    return;
  }
  if (!web_contents())
    return;
  web_contents()->SetAudioMuted(mute);
}

bool CefBrowserHostImpl::IsAudioMuted() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    NOTREACHED() << "called on invalid thread";
    return false;
  }
  if (!web_contents())
    return false;
  return web_contents()->IsAudioMuted();
}

// content::WebContentsDelegate methods.
// -----------------------------------------------------------------------------

// |source| may be NULL if the navigation originates from a guest view via
// AlloyContentBrowserClient::CanCreateWindow.
content::WebContents* CefBrowserHostImpl::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  bool cancel = false;

  if (client_.get()) {
    CefRefPtr<CefRequestHandler> handler = client_->GetRequestHandler();
    if (handler.get()) {
      cancel = handler->OnOpenURLFromTab(
          this, GetFrame(params.frame_tree_node_id), params.url.spec(),
          static_cast<cef_window_open_disposition_t>(params.disposition),
          params.user_gesture);
    }
  }

  if (!cancel) {
    // Start a navigation in the current browser that will result in the
    // creation of a new render process.
    LoadMainFrameURL(params.url.spec(), params.referrer, params.transition,
                     params.extra_headers);
    return source;
  }

  // We don't know where the navigation, if any, will occur.
  return nullptr;
}

bool CefBrowserHostImpl::ShouldTransferNavigation(
    bool is_main_frame_navigation) {
  return platform_delegate_->ShouldTransferNavigation(is_main_frame_navigation);
}

void CefBrowserHostImpl::AddNewContents(
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_rect,
    bool user_gesture,
    bool* was_blocked) {
  platform_delegate_->AddNewContents(source, std::move(new_contents),
                                     target_url, disposition, initial_rect,
                                     user_gesture, was_blocked);
}

void CefBrowserHostImpl::LoadingStateChanged(content::WebContents* source,
                                             bool to_different_document) {
  contents_delegate_->LoadingStateChanged(source, to_different_document);
}

void CefBrowserHostImpl::CloseContents(content::WebContents* source) {
  CEF_REQUIRE_UIT();

  if (destruction_state_ == DESTRUCTION_STATE_COMPLETED)
    return;

  bool close_browser = true;

  // If this method is called in response to something other than
  // WindowDestroyed() ask the user if the browser should close.
  if (client_.get() && (IsWindowless() || !window_destroyed_)) {
    CefRefPtr<CefLifeSpanHandler> handler = client_->GetLifeSpanHandler();
    if (handler.get()) {
      close_browser = !handler->DoClose(this);
    }
  }

  if (close_browser) {
    if (destruction_state_ != DESTRUCTION_STATE_ACCEPTED)
      destruction_state_ = DESTRUCTION_STATE_ACCEPTED;

    if (!IsWindowless() && !window_destroyed_) {
      // A window exists so try to close it using the platform method. Will
      // result in a call to WindowDestroyed() if/when the window is destroyed
      // via the platform window destruction mechanism.
      platform_delegate_->CloseHostWindow();
    } else {
      // Keep a reference to the browser while it's in the process of being
      // destroyed.
      CefRefPtr<CefBrowserHostImpl> browser(this);

      // No window exists. Destroy the browser immediately. Don't call other
      // browser methods after calling DestroyBrowser().
      DestroyBrowser();
    }
  } else if (destruction_state_ != DESTRUCTION_STATE_NONE) {
    destruction_state_ = DESTRUCTION_STATE_NONE;
  }
}

void CefBrowserHostImpl::UpdateTargetURL(content::WebContents* source,
                                         const GURL& url) {
  contents_delegate_->UpdateTargetURL(source, url);
}

bool CefBrowserHostImpl::DidAddMessageToConsole(
    content::WebContents* source,
    blink::mojom::ConsoleMessageLevel level,
    const base::string16& message,
    int32_t line_no,
    const base::string16& source_id) {
  return contents_delegate_->DidAddMessageToConsole(source, level, message,
                                                    line_no, source_id);
}

void CefBrowserHostImpl::BeforeUnloadFired(content::WebContents* source,
                                           bool proceed,
                                           bool* proceed_to_fire_unload) {
  if (destruction_state_ == DESTRUCTION_STATE_ACCEPTED || proceed) {
    *proceed_to_fire_unload = true;
  } else if (!proceed) {
    *proceed_to_fire_unload = false;
    destruction_state_ = DESTRUCTION_STATE_NONE;
  }
}

bool CefBrowserHostImpl::TakeFocus(content::WebContents* source, bool reverse) {
  if (client_.get()) {
    CefRefPtr<CefFocusHandler> handler = client_->GetFocusHandler();
    if (handler.get())
      handler->OnTakeFocus(this, !reverse);
  }

  return false;
}

bool CefBrowserHostImpl::HandleContextMenu(
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params) {
  return HandleContextMenu(web_contents(), params);
}

KeyboardEventProcessingResult CefBrowserHostImpl::PreHandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  if (platform_delegate_ && client_.get()) {
    CefRefPtr<CefKeyboardHandler> handler = client_->GetKeyboardHandler();
    if (handler.get()) {
      CefKeyEvent cef_event;
      if (browser_util::GetCefKeyEvent(event, cef_event)) {
        cef_event.focus_on_editable_field = focus_on_editable_field_;

        CefEventHandle event_handle = platform_delegate_->GetEventHandle(event);
        bool is_keyboard_shortcut = false;
        bool result = handler->OnPreKeyEvent(this, cef_event, event_handle,
                                             &is_keyboard_shortcut);
        if (result)
          return KeyboardEventProcessingResult::HANDLED;
        else if (is_keyboard_shortcut)
          return KeyboardEventProcessingResult::NOT_HANDLED_IS_SHORTCUT;
      }
    }
  }

  return KeyboardEventProcessingResult::NOT_HANDLED;
}

bool CefBrowserHostImpl::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  // Check to see if event should be ignored.
  if (event.skip_in_browser)
    return false;

  if (!platform_delegate_)
    return false;

  if (client_.get()) {
    CefRefPtr<CefKeyboardHandler> handler = client_->GetKeyboardHandler();
    if (handler.get()) {
      CefKeyEvent cef_event;
      if (browser_util::GetCefKeyEvent(event, cef_event)) {
        cef_event.focus_on_editable_field = focus_on_editable_field_;

        CefEventHandle event_handle = platform_delegate_->GetEventHandle(event);
        if (handler->OnKeyEvent(this, cef_event, event_handle))
          return true;
      }
    }
  }

  return platform_delegate_->HandleKeyboardEvent(event);
}

bool CefBrowserHostImpl::PreHandleGestureEvent(
    content::WebContents* source,
    const blink::WebGestureEvent& event) {
  return platform_delegate_->PreHandleGestureEvent(source, event);
}

bool CefBrowserHostImpl::CanDragEnter(content::WebContents* source,
                                      const content::DropData& data,
                                      blink::WebDragOperationsMask mask) {
  CefRefPtr<CefDragHandler> handler;
  if (client_)
    handler = client_->GetDragHandler();
  if (handler) {
    CefRefPtr<CefDragDataImpl> drag_data(new CefDragDataImpl(data));
    drag_data->SetReadOnly(true);
    if (handler->OnDragEnter(
            this, drag_data.get(),
            static_cast<CefDragHandler::DragOperationsMask>(mask))) {
      return false;
    }
  }
  return true;
}

void CefBrowserHostImpl::GetCustomWebContentsView(
    content::WebContents* web_contents,
    const GURL& target_url,
    int opener_render_process_id,
    int opener_render_frame_id,
    content::WebContentsView** view,
    content::RenderViewHostDelegateView** delegate_view) {
  CefBrowserInfoManager::GetInstance()->GetCustomWebContentsView(
      target_url, opener_render_process_id, opener_render_frame_id, view,
      delegate_view);
}

void CefBrowserHostImpl::WebContentsCreated(
    content::WebContents* source_contents,
    int opener_render_process_id,
    int opener_render_frame_id,
    const std::string& frame_name,
    const GURL& target_url,
    content::WebContents* new_contents) {
  CefBrowserSettings settings;
  CefRefPtr<CefClient> client;
  std::unique_ptr<CefBrowserPlatformDelegate> platform_delegate;
  CefRefPtr<CefDictionaryValue> extra_info;

  CefBrowserInfoManager::GetInstance()->WebContentsCreated(
      target_url, opener_render_process_id, opener_render_frame_id, settings,
      client, platform_delegate, extra_info);

  scoped_refptr<CefBrowserInfo> info =
      CefBrowserInfoManager::GetInstance()->CreatePopupBrowserInfo(
          new_contents, platform_delegate->IsWindowless(), extra_info);
  CHECK(info.get());
  CHECK(info->is_popup());

  CefRefPtr<CefBrowserHostImpl> opener = GetBrowserForContents(source_contents);
  if (!opener)
    return;

  // Popups must share the same RequestContext as the parent.
  CefRefPtr<CefRequestContextImpl> request_context = opener->request_context();
  CHECK(request_context);

  // We don't officially own |new_contents| until AddNewContents() is called.
  // However, we need to install observers/delegates here.
  CefRefPtr<CefBrowserHostImpl> browser =
      CreateInternal(settings, client, new_contents, /*own_web_contents=*/false,
                     info, opener, /*is_devtools_popup=*/false, request_context,
                     std::move(platform_delegate), /*cef_extension=*/nullptr);
}

void CefBrowserHostImpl::DidNavigateMainFramePostCommit(
    content::WebContents* web_contents) {
  contents_delegate_->DidNavigateMainFramePostCommit(web_contents);
}

content::JavaScriptDialogManager*
CefBrowserHostImpl::GetJavaScriptDialogManager(content::WebContents* source) {
  if (!javascript_dialog_manager_.get() && platform_delegate_) {
    javascript_dialog_manager_.reset(new CefJavaScriptDialogManager(
        this, platform_delegate_->CreateJavaScriptDialogRunner()));
  }
  return javascript_dialog_manager_.get();
}

void CefBrowserHostImpl::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    scoped_refptr<content::FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  EnsureFileDialogManager();
  file_dialog_manager_->RunFileChooser(listener, params);
}

bool CefBrowserHostImpl::HandleContextMenu(
    content::WebContents* web_contents,
    const content::ContextMenuParams& params) {
  CEF_REQUIRE_UIT();
  if (!menu_manager_.get() && platform_delegate_) {
    menu_manager_.reset(
        new CefMenuManager(this, platform_delegate_->CreateMenuRunner()));
  }
  return menu_manager_->CreateContextMenu(params);
}

void CefBrowserHostImpl::UpdatePreferredSize(content::WebContents* source,
                                             const gfx::Size& pref_size) {
#if defined(OS_WIN) || (defined(OS_POSIX) && !defined(OS_MAC))
  CEF_REQUIRE_UIT();
  if (platform_delegate_)
    platform_delegate_->SizeTo(pref_size.width(), pref_size.height());
#endif
}

void CefBrowserHostImpl::ResizeDueToAutoResize(content::WebContents* source,
                                               const gfx::Size& new_size) {
  CEF_REQUIRE_UIT();

  if (client_) {
    CefRefPtr<CefDisplayHandler> handler = client_->GetDisplayHandler();
    if (handler && handler->OnAutoResize(
                       this, CefSize(new_size.width(), new_size.height()))) {
      return;
    }
  }

  UpdatePreferredSize(source, new_size);
}

void CefBrowserHostImpl::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  CEF_REQUIRE_UIT();

  blink::MediaStreamDevices devices;

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kEnableMediaStream)) {
    // Cancel the request.
    std::move(callback).Run(
        devices, blink::mojom::MediaStreamRequestResult::PERMISSION_DENIED,
        std::unique_ptr<content::MediaStreamUI>());
    return;
  }

  // Based on chrome/browser/media/media_stream_devices_controller.cc
  bool microphone_requested =
      (request.audio_type ==
       blink::mojom::MediaStreamType::DEVICE_AUDIO_CAPTURE);
  bool webcam_requested = (request.video_type ==
                           blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE);
  bool screen_requested =
      (request.video_type ==
       blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE);
  if (microphone_requested || webcam_requested || screen_requested) {
    // Pick the desired device or fall back to the first available of the
    // given type.
    if (microphone_requested) {
      CefMediaCaptureDevicesDispatcher::GetInstance()->GetRequestedDevice(
          request.requested_audio_device_id, true, false, &devices);
    }
    if (webcam_requested) {
      CefMediaCaptureDevicesDispatcher::GetInstance()->GetRequestedDevice(
          request.requested_video_device_id, false, true, &devices);
    }
    if (screen_requested) {
      content::DesktopMediaID media_id;
      if (request.requested_video_device_id.empty()) {
        media_id =
            content::DesktopMediaID(content::DesktopMediaID::TYPE_SCREEN,
                                    -1 /* webrtc::kFullDesktopScreenId */);
      } else {
        media_id =
            content::DesktopMediaID::Parse(request.requested_video_device_id);
      }
      devices.push_back(blink::MediaStreamDevice(
          blink::mojom::MediaStreamType::GUM_DESKTOP_VIDEO_CAPTURE,
          media_id.ToString(), "Screen"));
    }
  }

  std::move(callback).Run(devices, blink::mojom::MediaStreamRequestResult::OK,
                          std::unique_ptr<content::MediaStreamUI>());
}

bool CefBrowserHostImpl::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const GURL& security_origin,
    blink::mojom::MediaStreamType type) {
  // Check media access permission without prompting the user. This is called
  // when loading the Pepper Flash plugin.
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(switches::kEnableMediaStream);
}

bool CefBrowserHostImpl::IsNeverComposited(content::WebContents* web_contents) {
  return platform_delegate_->IsNeverComposited(web_contents);
}

content::PictureInPictureResult CefBrowserHostImpl::EnterPictureInPicture(
    content::WebContents* web_contents,
    const viz::SurfaceId& surface_id,
    const gfx::Size& natural_size) {
  if (!IsPictureInPictureSupported()) {
    return content::PictureInPictureResult::kNotSupported;
  }

  return PictureInPictureWindowManager::GetInstance()->EnterPictureInPicture(
      web_contents, surface_id, natural_size);
}

void CefBrowserHostImpl::ExitPictureInPicture() {
  DCHECK(IsPictureInPictureSupported());
  PictureInPictureWindowManager::GetInstance()->ExitPictureInPicture();
}

// content::WebContentsObserver methods.
// -----------------------------------------------------------------------------

void CefBrowserHostImpl::RenderViewCreated(
    content::RenderViewHost* render_view_host) {
  // May be already registered if the renderer crashed previously.
  if (!registrar_->IsRegistered(
          this, content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE,
          content::Source<content::RenderViewHost>(render_view_host))) {
    registrar_->Add(this, content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE,
                    content::Source<content::RenderViewHost>(render_view_host));
  }

  new CefWidgetHostInterceptor(this, render_view_host);

  platform_delegate_->RenderViewCreated(render_view_host);
}

void CefBrowserHostImpl::RenderViewDeleted(
    content::RenderViewHost* render_view_host) {
  if (registrar_->IsRegistered(
          this, content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE,
          content::Source<content::RenderViewHost>(render_view_host))) {
    registrar_->Remove(
        this, content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE,
        content::Source<content::RenderViewHost>(render_view_host));
  }
}

void CefBrowserHostImpl::RenderViewReady() {
  platform_delegate_->RenderViewReady();
}

void CefBrowserHostImpl::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (web_contents()) {
    auto cef_browser_context =
        static_cast<AlloyBrowserContext*>(web_contents()->GetBrowserContext());
    if (cef_browser_context) {
      cef_browser_context->AddVisitedURLs(
          navigation_handle->GetRedirectChain());
    }
  }
}

void CefBrowserHostImpl::OnAudioStateChanged(bool audible) {
  if (audible) {
    recently_audible_timer_.Stop();
    StartAudioCapturer();
  } else if (audio_capturer_) {
    // If you have a media playing that has a short quiet moment, web_contents
    // will immediately switch to non-audible state. We don't want to stop
    // audio stream so quickly, let's give the stream some time to resume
    // playing.
    recently_audible_timer_.Start(
        FROM_HERE, kRecentlyAudibleTimeout,
        base::BindOnce(&CefBrowserHostImpl::OnRecentlyAudibleTimerFired, this));
  }
}

void CefBrowserHostImpl::OnRecentlyAudibleTimerFired() {
  audio_capturer_.reset();
}

void CefBrowserHostImpl::AccessibilityEventReceived(
    const content::AXEventNotificationDetails& content_event_bundle) {
  // Only needed in windowless mode.
  if (IsWindowless()) {
    if (!web_contents() || !platform_delegate_)
      return;

    platform_delegate_->AccessibilityEventReceived(content_event_bundle);
  }
}

void CefBrowserHostImpl::AccessibilityLocationChangesReceived(
    const std::vector<content::AXLocationChangeNotificationDetails>& locData) {
  // Only needed in windowless mode.
  if (IsWindowless()) {
    if (!web_contents() || !platform_delegate_)
      return;

    platform_delegate_->AccessibilityLocationChangesReceived(locData);
  }
}

void CefBrowserHostImpl::WebContentsDestroyed() {
  auto wc = web_contents();
  content::WebContentsObserver::Observe(nullptr);
  if (platform_delegate_)
    platform_delegate_->WebContentsDestroyed(wc);
}

void CefBrowserHostImpl::StartAudioCapturer() {
  if (!client_.get() || audio_capturer_)
    return;

  CefRefPtr<CefAudioHandler> audio_handler = client_->GetAudioHandler();
  if (!audio_handler.get())
    return;

  CefAudioParameters params;
  params.channel_layout = CEF_CHANNEL_LAYOUT_STEREO;
  params.sample_rate = media::AudioParameters::kAudioCDSampleRate;
  params.frames_per_buffer = 1024;

  if (!audio_handler->GetAudioParameters(this, params))
    return;

  audio_capturer_.reset(new CefAudioCapturer(params, this, audio_handler));
}

// content::NotificationObserver methods.
// -----------------------------------------------------------------------------

void CefBrowserHostImpl::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  DCHECK(type == content::NOTIFICATION_LOAD_STOP ||
         type == content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE);

  if (type == content::NOTIFICATION_LOAD_STOP) {
    content::NavigationController* controller =
        content::Source<content::NavigationController>(source).ptr();
    contents_delegate_->OnTitleChange(controller->GetWebContents()->GetTitle());
  } else if (type == content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE) {
    focus_on_editable_field_ = *content::Details<bool>(details).ptr();
  }
}

// CefBrowserHostImpl private methods.
// -----------------------------------------------------------------------------

CefBrowserHostImpl::CefBrowserHostImpl(
    const CefBrowserSettings& settings,
    CefRefPtr<CefClient> client,
    content::WebContents* web_contents,
    scoped_refptr<CefBrowserInfo> browser_info,
    CefRefPtr<CefBrowserHostImpl> opener,
    CefRefPtr<CefRequestContextImpl> request_context,
    std::unique_ptr<CefBrowserPlatformDelegate> platform_delegate,
    CefRefPtr<CefExtension> extension)
    : CefBrowserHostBase(settings, client, browser_info, request_context),
      content::WebContentsObserver(web_contents),
      opener_(kNullWindowHandle),
      platform_delegate_(std::move(platform_delegate)),
      is_windowless_(platform_delegate_->IsWindowless()),
      is_views_hosted_(platform_delegate_->IsViewsHosted()),
      extension_(extension) {
  contents_delegate_->ObserveWebContents(web_contents);

  if (opener.get() && !platform_delegate_->IsViewsHosted()) {
    // GetOpenerWindowHandle() only returns a value for non-views-hosted
    // popup browsers.
    opener_ = opener->GetWindowHandle();
  }

  registrar_.reset(new content::NotificationRegistrar);

  // When navigating through the history, the restored NavigationEntry's title
  // will be used. If the entry ends up having the same title after we return
  // to it, as will usually be the case, the
  // NOTIFICATION_WEB_CONTENTS_TITLE_UPDATED will then be suppressed, since
  // the NavigationEntry's title hasn't changed.
  registrar_->Add(this, content::NOTIFICATION_LOAD_STOP,
                  content::Source<content::NavigationController>(
                      &web_contents->GetController()));

  // Associate the platform delegate with this browser.
  platform_delegate_->BrowserCreated(this);

  // Make sure RenderViewCreated is called at least one time.
  RenderViewCreated(web_contents->GetRenderViewHost());
}

bool CefBrowserHostImpl::CreateHostWindow() {
  // |host_window_handle_| will not change after initial host creation for
  // non-views-hosted browsers.
  bool success = true;
  if (!IsWindowless())
    success = platform_delegate_->CreateHostWindow();
  if (success && !IsViewsHosted())
    host_window_handle_ = platform_delegate_->GetHostWindowHandle();
  return success;
}

gfx::Point CefBrowserHostImpl::GetScreenPoint(const gfx::Point& view) const {
  CEF_REQUIRE_UIT();
  if (platform_delegate_)
    return platform_delegate_->GetScreenPoint(view);
  return gfx::Point();
}

void CefBrowserHostImpl::StartDragging(
    const content::DropData& drop_data,
    blink::WebDragOperationsMask allowed_ops,
    const gfx::ImageSkia& image,
    const gfx::Vector2d& image_offset,
    const content::DragEventSourceInfo& event_info,
    content::RenderWidgetHostImpl* source_rwh) {
  if (platform_delegate_) {
    platform_delegate_->StartDragging(drop_data, allowed_ops, image,
                                      image_offset, event_info, source_rwh);
  }
}

void CefBrowserHostImpl::UpdateDragCursor(blink::WebDragOperation operation) {
  if (platform_delegate_)
    platform_delegate_->UpdateDragCursor(operation);
}

void CefBrowserHostImpl::EnsureFileDialogManager() {
  CEF_REQUIRE_UIT();
  if (!file_dialog_manager_.get() && platform_delegate_) {
    file_dialog_manager_.reset(new CefFileDialogManager(
        this, platform_delegate_->CreateFileDialogRunner()));
  }
}
