// Copyright 2015 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/browser/browser_platform_delegate.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "cef/include/views/cef_window.h"
#include "cef/include/views/cef_window_delegate.h"
#include "cef/libcef/browser/browser_host_base.h"
#include "cef/libcef/browser/thread_util.h"
#include "cef/libcef/browser/views/browser_view_impl.h"
#include "cef/libcef/common/cef_switches.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"

namespace {

void ExecuteExternalProtocol(const GURL& url) {
  CEF_REQUIRE_BLOCKING();

  // Check that an application is associated with the scheme.
  if (shell_integration::GetApplicationNameForScheme(url).empty()) {
    return;
  }

  CEF_POST_TASK(TID_UI, base::BindOnce(&platform_util::OpenExternal, url));
}

// Default popup window delegate implementation.
class PopupWindowDelegate : public CefWindowDelegate {
 public:
  explicit PopupWindowDelegate(CefRefPtr<CefBrowserView> browser_view)
      : browser_view_(browser_view) {}

  PopupWindowDelegate(const PopupWindowDelegate&) = delete;
  PopupWindowDelegate& operator=(const PopupWindowDelegate&) = delete;

  void OnWindowCreated(CefRefPtr<CefWindow> window) override {
    window->AddChildView(browser_view_);
    window->Show();
    browser_view_->RequestFocus();
  }

  void OnWindowDestroyed(CefRefPtr<CefWindow> window) override {
    browser_view_ = nullptr;
  }

  bool CanClose(CefRefPtr<CefWindow> window) override {
    CefRefPtr<CefBrowser> browser = browser_view_->GetBrowser();
    if (browser) {
      return browser->GetHost()->TryCloseBrowser();
    }
    return true;
  }

  cef_runtime_style_t GetWindowRuntimeStyle() override {
    return browser_view_->GetRuntimeStyle();
  }

 private:
  CefRefPtr<CefBrowserView> browser_view_;

  IMPLEMENT_REFCOUNTING(PopupWindowDelegate);
};

}  // namespace

CefBrowserPlatformDelegate::CefBrowserPlatformDelegate() = default;

CefBrowserPlatformDelegate::~CefBrowserPlatformDelegate() {
  DCHECK(!browser_);
}

content::WebContents* CefBrowserPlatformDelegate::CreateWebContents(
    CefBrowserCreateParams& create_params,
    bool& own_web_contents) {
  DCHECK(false);
  return nullptr;
}

void CefBrowserPlatformDelegate::CreateViewForWebContents(
    raw_ptr<content::WebContentsView>* view,
    raw_ptr<content::RenderViewHostDelegateView>* delegate_view) {
  DCHECK(false);
}

void CefBrowserPlatformDelegate::WebContentsCreated(
    content::WebContents* web_contents,
    bool owned) {
  // We should not have a browser at this point.
  DCHECK(!browser_);

  DCHECK(!web_contents_);
  web_contents_ = web_contents;
}

void CefBrowserPlatformDelegate::AddNewContents(
    content::WebContents* source,
    std::unique_ptr<content::WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const blink::mojom::WindowFeatures& window_features,
    bool user_gesture,
    bool* was_blocked) {
  DCHECK(false);
}

void CefBrowserPlatformDelegate::WebContentsDestroyed(
    content::WebContents* web_contents) {
  DCHECK(web_contents_ && web_contents_ == web_contents);
  web_contents_ = nullptr;
}

void CefBrowserPlatformDelegate::RenderViewCreated(
    content::RenderViewHost* render_view_host) {
  // Indicate that the view has an external parent (namely us). This setting is
  // required for proper focus handling on Windows and Linux.
  if (HasExternalParent() && render_view_host->GetWidget()->GetView()) {
    render_view_host->GetWidget()->GetView()->SetHasExternalParent(true);
  }
}

void CefBrowserPlatformDelegate::RenderViewReady() {}

void CefBrowserPlatformDelegate::BrowserCreated(CefBrowserHostBase* browser) {
  // We should have an associated WebContents at this point.
  DCHECK(web_contents_);

  DCHECK(!browser_);
  DCHECK(browser);
  browser_ = browser;
}

void CefBrowserPlatformDelegate::NotifyBrowserCreated() {}

void CefBrowserPlatformDelegate::NotifyBrowserDestroyed() {}

void CefBrowserPlatformDelegate::BrowserDestroyed(CefBrowserHostBase* browser) {
  // WebContentsDestroyed should already be called.
  DCHECK(!web_contents_);

  DCHECK(browser_ && browser_ == browser);
  browser_ = nullptr;
}

bool CefBrowserPlatformDelegate::CreateHostWindow() {
  DCHECK(false);
  return true;
}

void CefBrowserPlatformDelegate::CloseHostWindow() {
  DCHECK(false);
}

CefWindowHandle CefBrowserPlatformDelegate::GetHostWindowHandle() const {
  DCHECK(false);
  return kNullWindowHandle;
}

views::Widget* CefBrowserPlatformDelegate::GetWindowWidget() const {
  DCHECK(false);
  return nullptr;
}

CefRefPtr<CefBrowserView> CefBrowserPlatformDelegate::GetBrowserView() const {
  return nullptr;
}

void CefBrowserPlatformDelegate::SetBrowserView(
    CefRefPtr<CefBrowserView> browser_view) {
  DCHECK(false);
}

web_modal::WebContentsModalDialogHost*
CefBrowserPlatformDelegate::GetWebContentsModalDialogHost() const {
  DCHECK(false);
  return nullptr;
}

void CefBrowserPlatformDelegate::PopupWebContentsCreated(
    const CefBrowserSettings& settings,
    CefRefPtr<CefClient> client,
    content::WebContents* new_web_contents,
    CefBrowserPlatformDelegate* new_platform_delegate,
    bool is_devtools) {
  // Default popup handling may not be Views-hosted.
  if (!new_platform_delegate->IsViewsHosted()) {
    return;
  }

  CefRefPtr<CefBrowserViewDelegate> new_delegate;
  CefRefPtr<CefBrowserViewDelegate> opener_delegate;
  cef_runtime_style_t opener_runtime_style = CEF_RUNTIME_STYLE_DEFAULT;

  auto browser_view = GetBrowserView();
  if (browser_view) {
    // When |this| (the popup opener) is Views-hosted use the current delegate.
    opener_delegate =
        static_cast<CefBrowserViewImpl*>(browser_view.get())->delegate();
  }
  if (!opener_delegate) {
    opener_delegate =
        new_platform_delegate->GetDefaultBrowserViewDelegateForPopupOpener();
  }
  if (opener_delegate) {
    new_delegate = opener_delegate->GetDelegateForPopupBrowserView(
        browser_view, settings, client, is_devtools);
  }

  if (browser_view) {
    opener_runtime_style = browser_view->GetRuntimeStyle();
  } else if (opener_delegate) {
    opener_runtime_style = opener_delegate->GetBrowserRuntimeStyle();
  }

  // Create a new BrowserView for the popup.
  CefRefPtr<CefBrowserViewImpl> new_browser_view =
      CefBrowserViewImpl::CreateForPopup(settings, new_delegate, is_devtools,
                                         opener_runtime_style);

  // Associate the PlatformDelegate with the new BrowserView.
  new_platform_delegate->SetBrowserView(new_browser_view);

  // Keep the BrowserView alive until PopupBrowserCreated() is called.
  new_browser_view->AddRef();
}

void CefBrowserPlatformDelegate::PopupBrowserCreated(
    CefBrowserPlatformDelegate* new_platform_delegate,
    CefBrowserHostBase* new_browser,
    bool is_devtools) {
  // Default popup handling may not be Views-hosted.
  if (!new_platform_delegate->IsViewsHosted()) {
    return;
  }

  CefRefPtr<CefBrowserView> new_browser_view =
      CefBrowserView::GetForBrowser(new_browser);
  CHECK(new_browser_view);

  bool popup_handled = false;

  CefRefPtr<CefBrowserViewDelegate> opener_delegate;
  auto browser_view = GetBrowserView();
  if (browser_view) {
    // When |this| (the popup opener) is Views-hosted use the current delegate.
    opener_delegate =
        static_cast<CefBrowserViewImpl*>(browser_view.get())->delegate();
  }
  if (!opener_delegate) {
    opener_delegate =
        new_platform_delegate->GetDefaultBrowserViewDelegateForPopupOpener();
  }
  if (opener_delegate) {
    popup_handled = opener_delegate->OnPopupBrowserViewCreated(
        browser_view, new_browser_view.get(), is_devtools);
  }

  if (!popup_handled) {
    CefWindow::CreateTopLevelWindow(
        new PopupWindowDelegate(new_browser_view.get()));
  }

  // Release the reference added in PopupWebContentsCreated().
  new_browser_view->Release();
}

CefRefPtr<CefBrowserViewDelegate>
CefBrowserPlatformDelegate::GetDefaultBrowserViewDelegateForPopupOpener() {
  return nullptr;
}

SkColor CefBrowserPlatformDelegate::GetBackgroundColor() const {
  DCHECK(false);
  return SkColor();
}

void CefBrowserPlatformDelegate::WasResized() {
  DCHECK(false);
}

void CefBrowserPlatformDelegate::SendKeyEvent(const CefKeyEvent& event) {
  NOTIMPLEMENTED();
}

void CefBrowserPlatformDelegate::SendMouseClickEvent(
    const CefMouseEvent& event,
    CefBrowserHost::MouseButtonType type,
    bool mouseUp,
    int clickCount) {
  NOTIMPLEMENTED();
}

void CefBrowserPlatformDelegate::SendMouseMoveEvent(const CefMouseEvent& event,
                                                    bool mouseLeave) {
  NOTIMPLEMENTED();
}

void CefBrowserPlatformDelegate::SendMouseWheelEvent(const CefMouseEvent& event,
                                                     int deltaX,
                                                     int deltaY) {
  NOTIMPLEMENTED();
}

void CefBrowserPlatformDelegate::SendTouchEvent(const CefTouchEvent& event) {
  NOTIMPLEMENTED();
}

void CefBrowserPlatformDelegate::SetFocus(bool setFocus) {}

void CefBrowserPlatformDelegate::SendCaptureLostEvent() {
  NOTIMPLEMENTED();
}

#if BUILDFLAG(IS_WIN) || (BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC))
void CefBrowserPlatformDelegate::NotifyMoveOrResizeStarted() {}

void CefBrowserPlatformDelegate::SizeTo(int width, int height) {}
#endif

gfx::Point CefBrowserPlatformDelegate::GetScreenPoint(
    const gfx::Point& view,
    bool want_dip_coords) const {
  DCHECK(false);
  return gfx::Point();
}

void CefBrowserPlatformDelegate::ViewText(const std::string& text) {
  NOTIMPLEMENTED();
}

bool CefBrowserPlatformDelegate::HandleKeyboardEvent(
    const input::NativeWebKeyboardEvent& event) {
  DCHECK(false);
  return false;
}

// static
void CefBrowserPlatformDelegate::HandleExternalProtocol(const GURL& url) {
  CEF_POST_USER_VISIBLE_TASK(base::BindOnce(ExecuteExternalProtocol, url));
}

CefEventHandle CefBrowserPlatformDelegate::GetEventHandle(
    const input::NativeWebKeyboardEvent& event) const {
  DCHECK(false);
  return kNullEventHandle;
}

std::unique_ptr<CefJavaScriptDialogRunner>
CefBrowserPlatformDelegate::CreateJavaScriptDialogRunner() {
  return nullptr;
}

std::unique_ptr<CefMenuRunner> CefBrowserPlatformDelegate::CreateMenuRunner() {
  NOTIMPLEMENTED();
  return nullptr;
}

bool CefBrowserPlatformDelegate::IsWindowless() const {
  return false;
}

bool CefBrowserPlatformDelegate::IsViewsHosted() const {
  return false;
}

bool CefBrowserPlatformDelegate::HasExternalParent() const {
  // In the majority of cases a Views-hosted browser will not have an external
  // parent, and visa-versa.
  return !IsViewsHosted();
}

void CefBrowserPlatformDelegate::WasHidden(bool hidden) {
  DCHECK(false);
}

bool CefBrowserPlatformDelegate::IsHidden() const {
  DCHECK(false);
  return false;
}

void CefBrowserPlatformDelegate::NotifyScreenInfoChanged() {
  DCHECK(false);
}

void CefBrowserPlatformDelegate::Invalidate(cef_paint_element_type_t type) {
  DCHECK(false);
}

void CefBrowserPlatformDelegate::SendExternalBeginFrame() {
  DCHECK(false);
}

void CefBrowserPlatformDelegate::SetWindowlessFrameRate(int frame_rate) {
  DCHECK(false);
}

void CefBrowserPlatformDelegate::ImeSetComposition(
    const CefString& text,
    const std::vector<CefCompositionUnderline>& underlines,
    const CefRange& replacement_range,
    const CefRange& selection_range) {
  DCHECK(false);
}

void CefBrowserPlatformDelegate::ImeCommitText(
    const CefString& text,
    const CefRange& replacement_range,
    int relative_cursor_pos) {
  DCHECK(false);
}

void CefBrowserPlatformDelegate::ImeFinishComposingText(bool keep_selection) {
  DCHECK(false);
}

void CefBrowserPlatformDelegate::ImeCancelComposition() {
  DCHECK(false);
}

void CefBrowserPlatformDelegate::DragTargetDragEnter(
    CefRefPtr<CefDragData> drag_data,
    const CefMouseEvent& event,
    cef_drag_operations_mask_t allowed_ops) {
  DCHECK(false);
}

void CefBrowserPlatformDelegate::DragTargetDragOver(
    const CefMouseEvent& event,
    cef_drag_operations_mask_t allowed_ops) {
  DCHECK(false);
}

void CefBrowserPlatformDelegate::DragTargetDragLeave() {
  DCHECK(false);
}

void CefBrowserPlatformDelegate::DragTargetDrop(const CefMouseEvent& event) {
  DCHECK(false);
}

void CefBrowserPlatformDelegate::StartDragging(
    const content::DropData& drop_data,
    blink::DragOperationsMask allowed_ops,
    const gfx::ImageSkia& image,
    const gfx::Vector2d& image_offset,
    const blink::mojom::DragEventSourceInfo& event_info,
    content::RenderWidgetHostImpl* source_rwh) {
  DCHECK(false);
}

void CefBrowserPlatformDelegate::UpdateDragOperation(
    ui::mojom::DragOperation operation,
    bool document_is_handling_drag) {
  DCHECK(false);
}

void CefBrowserPlatformDelegate::DragSourceEndedAt(
    int x,
    int y,
    cef_drag_operations_mask_t op) {
  DCHECK(false);
}

void CefBrowserPlatformDelegate::DragSourceSystemDragEnded() {
  DCHECK(false);
}

void CefBrowserPlatformDelegate::AccessibilityEventReceived(
    const ui::AXUpdatesAndEvents& details) {
  DCHECK(false);
}

void CefBrowserPlatformDelegate::AccessibilityLocationChangesReceived(
    const std::vector<ui::AXLocationChanges>& details) {
  DCHECK(false);
}

gfx::Point CefBrowserPlatformDelegate::GetDialogPosition(
    const gfx::Size& size) {
  const gfx::Size& max_size = GetMaximumDialogSize();
  return gfx::Point((max_size.width() - size.width()) / 2,
                    (max_size.height() - size.height()) / 2);
}

gfx::Size CefBrowserPlatformDelegate::GetMaximumDialogSize() {
  if (!web_contents_) {
    return gfx::Size();
  }

  // The dialog should try to fit within the overlay for the web contents.
  // Note that, for things like print preview, this is just a suggested maximum.
  return web_contents_->GetContainerBounds().size();
}

void CefBrowserPlatformDelegate::SetAutoResizeEnabled(bool enabled,
                                                      const CefSize& min_size,
                                                      const CefSize& max_size) {
  NOTIMPLEMENTED();
}

void CefBrowserPlatformDelegate::SetAccessibilityState(
    cef_state_t accessibility_state) {
  // Do nothing if state is set to default. It'll be disabled by default and
  // controlled by the command-line flags "force-renderer-accessibility" and
  // "disable-renderer-accessibility".
  if (accessibility_state == STATE_DEFAULT) {
    return;
  }

  content::WebContentsImpl* web_contents_impl =
      static_cast<content::WebContentsImpl*>(web_contents_);

  if (!web_contents_impl) {
    return;
  }

  ui::AXMode accMode;
  // In windowless mode set accessibility to TreeOnly mode. Else native
  // accessibility APIs, specific to each platform, are also created.
  if (accessibility_state == STATE_ENABLED) {
    accMode = IsWindowless() ? ui::kAXModeWebContentsOnly : ui::kAXModeComplete;
  }
  web_contents_impl->SetAccessibilityMode(accMode);
}

bool CefBrowserPlatformDelegate::IsPrintPreviewSupported() const {
  if (IsWindowless()) {
    // Not supported with windowless rendering.
    return false;
  }

  if (web_contents_) {
    auto cef_browser_context = CefBrowserContext::FromBrowserContext(
        web_contents_->GetBrowserContext());
    if (cef_browser_context->AsProfile()->GetPrefs()->GetBoolean(
            prefs::kPrintPreviewDisabled)) {
      // Disabled on the Profile.
      return false;
    }
  }

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisablePrintPreview)) {
    // Disabled explicitly via the command-line.
    return false;
  }

  const bool default_disabled = IsAlloyStyle();
  if (default_disabled &&
      !command_line->HasSwitch(switches::kEnablePrintPreview)) {
    // Default disabled and not enabled explicitly via the command-line.
    return false;
  }

  return true;
}

void CefBrowserPlatformDelegate::Find(const CefString& searchText,
                                      bool forward,
                                      bool matchCase,
                                      bool findNext) {
  NOTIMPLEMENTED();
}

void CefBrowserPlatformDelegate::StopFinding(bool clearSelection) {
  NOTIMPLEMENTED();
}

// static
int CefBrowserPlatformDelegate::TranslateWebEventModifiers(
    uint32_t cef_modifiers) {
  int result = 0;
  // Set modifiers based on key state.
  if (cef_modifiers & EVENTFLAG_CAPS_LOCK_ON) {
    result |= blink::WebInputEvent::kCapsLockOn;
  }
  if (cef_modifiers & EVENTFLAG_SHIFT_DOWN) {
    result |= blink::WebInputEvent::kShiftKey;
  }
  if (cef_modifiers & EVENTFLAG_CONTROL_DOWN) {
    result |= blink::WebInputEvent::kControlKey;
  }
  if (cef_modifiers & EVENTFLAG_ALT_DOWN) {
    result |= blink::WebInputEvent::kAltKey;
  }
  if (cef_modifiers & EVENTFLAG_LEFT_MOUSE_BUTTON) {
    result |= blink::WebInputEvent::kLeftButtonDown;
  }
  if (cef_modifiers & EVENTFLAG_MIDDLE_MOUSE_BUTTON) {
    result |= blink::WebInputEvent::kMiddleButtonDown;
  }
  if (cef_modifiers & EVENTFLAG_RIGHT_MOUSE_BUTTON) {
    result |= blink::WebInputEvent::kRightButtonDown;
  }
  if (cef_modifiers & EVENTFLAG_COMMAND_DOWN) {
    result |= blink::WebInputEvent::kMetaKey;
  }
  if (cef_modifiers & EVENTFLAG_NUM_LOCK_ON) {
    result |= blink::WebInputEvent::kNumLockOn;
  }
  if (cef_modifiers & EVENTFLAG_IS_KEY_PAD) {
    result |= blink::WebInputEvent::kIsKeyPad;
  }
  if (cef_modifiers & EVENTFLAG_IS_LEFT) {
    result |= blink::WebInputEvent::kIsLeft;
  }
  if (cef_modifiers & EVENTFLAG_IS_RIGHT) {
    result |= blink::WebInputEvent::kIsRight;
  }
  if (cef_modifiers & EVENTFLAG_ALTGR_DOWN) {
    result |= blink::WebInputEvent::kAltGrKey;
  }
  if (cef_modifiers & EVENTFLAG_IS_REPEAT) {
    result |= blink::WebInputEvent::kIsAutoRepeat;
  }
  return result;
}
