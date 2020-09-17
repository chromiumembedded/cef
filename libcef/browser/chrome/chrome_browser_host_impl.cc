// Copyright 2020 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/chrome/chrome_browser_host_impl.h"

#include "libcef/browser/thread_util.h"
#include "libcef/features/runtime_checks.h"

#include "base/logging.h"
#include "base/notreached.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

namespace {

const auto kDefaultWindowDisposition = WindowOpenDisposition::CURRENT_TAB;

}  // namespace

// static
CefRefPtr<ChromeBrowserHostImpl> ChromeBrowserHostImpl::Create(
    const CreateParams& params) {
  // Get or create the request context and profile.
  CefRefPtr<CefRequestContextImpl> request_context_impl =
      CefRequestContextImpl::GetOrCreateForRequestContext(
          params.request_context);
  CHECK(request_context_impl);
  auto cef_browser_context = request_context_impl->GetBrowserContext();
  CHECK(cef_browser_context);
  auto profile = cef_browser_context->AsProfile();

  Browser::CreateParams chrome_params =
      Browser::CreateParams(profile, /*user_gesture=*/false);

  // Pass |params| to cef::BrowserDelegate::Create from the Browser constructor.
  chrome_params.cef_params = base::MakeRefCounted<DelegateCreateParams>(params);

  // Create the Browser. This will indirectly create the ChomeBrowserDelegate.
  // The same params will be used to create a new Browser if the tab is dragged
  // out of the existing Browser.
  auto browser = new Browser(chrome_params);

  // Add a new tab. This will indirectly create a new tab WebContents and
  // call ChromeBrowserDelegate::SetAsDelegate to create the associated
  // ChromeBrowserHostImpl.
  chrome::AddTabAt(browser, params.url, /*idx=*/-1, /*foreground=*/true);

  browser->window()->Show();

  // The new tab WebContents.
  auto web_contents = browser->tab_strip_model()->GetActiveWebContents();
  DCHECK(web_contents);

  // The associated ChromeBrowserHostImpl.
  auto browser_host =
      ChromeBrowserHostImpl::GetBrowserForContents(web_contents);
  DCHECK(browser_host);
  return browser_host;
}

// static
CefRefPtr<ChromeBrowserHostImpl> ChromeBrowserHostImpl::GetBrowserForHost(
    const content::RenderViewHost* host) {
  REQUIRE_CHROME_RUNTIME();
  auto browser = CefBrowserHostBase::GetBrowserForHost(host);
  return static_cast<ChromeBrowserHostImpl*>(browser.get());
}

// static
CefRefPtr<ChromeBrowserHostImpl> ChromeBrowserHostImpl::GetBrowserForHost(
    const content::RenderFrameHost* host) {
  REQUIRE_CHROME_RUNTIME();
  auto browser = CefBrowserHostBase::GetBrowserForHost(host);
  return static_cast<ChromeBrowserHostImpl*>(browser.get());
}

// static
CefRefPtr<ChromeBrowserHostImpl> ChromeBrowserHostImpl::GetBrowserForContents(
    const content::WebContents* contents) {
  REQUIRE_CHROME_RUNTIME();
  auto browser = CefBrowserHostBase::GetBrowserForContents(contents);
  return static_cast<ChromeBrowserHostImpl*>(browser.get());
}

// static
CefRefPtr<ChromeBrowserHostImpl>
ChromeBrowserHostImpl::GetBrowserForFrameTreeNode(int frame_tree_node_id) {
  REQUIRE_CHROME_RUNTIME();
  auto browser =
      CefBrowserHostBase::GetBrowserForFrameTreeNode(frame_tree_node_id);
  return static_cast<ChromeBrowserHostImpl*>(browser.get());
}

// static
CefRefPtr<ChromeBrowserHostImpl> ChromeBrowserHostImpl::GetBrowserForFrameRoute(
    int render_process_id,
    int render_routing_id) {
  REQUIRE_CHROME_RUNTIME();
  auto browser = CefBrowserHostBase::GetBrowserForFrameRoute(render_process_id,
                                                             render_routing_id);
  return static_cast<ChromeBrowserHostImpl*>(browser.get());
}

ChromeBrowserHostImpl::~ChromeBrowserHostImpl() = default;

void ChromeBrowserHostImpl::OnWebContentsDestroyed() {
  DestroyBrowser();
}

void ChromeBrowserHostImpl::OnSetFocus(cef_focus_source_t source) {
  NOTIMPLEMENTED();
}

void ChromeBrowserHostImpl::ViewText(const std::string& text) {
  NOTIMPLEMENTED();
}

void ChromeBrowserHostImpl::CloseBrowser(bool force_close) {
  NOTIMPLEMENTED();
}

bool ChromeBrowserHostImpl::TryCloseBrowser() {
  NOTIMPLEMENTED();
  return false;
}

void ChromeBrowserHostImpl::SetFocus(bool focus) {
  NOTIMPLEMENTED();
}

CefWindowHandle ChromeBrowserHostImpl::GetWindowHandle() {
  NOTIMPLEMENTED();
  return kNullWindowHandle;
}

CefWindowHandle ChromeBrowserHostImpl::GetOpenerWindowHandle() {
  NOTIMPLEMENTED();
  return kNullWindowHandle;
}

bool ChromeBrowserHostImpl::HasView() {
  NOTIMPLEMENTED();
  return false;
}

double ChromeBrowserHostImpl::GetZoomLevel() {
  NOTIMPLEMENTED();
  return 0.0;
}

void ChromeBrowserHostImpl::SetZoomLevel(double zoomLevel) {
  NOTIMPLEMENTED();
}

void ChromeBrowserHostImpl::RunFileDialog(
    FileDialogMode mode,
    const CefString& title,
    const CefString& default_file_path,
    const std::vector<CefString>& accept_filters,
    int selected_accept_filter,
    CefRefPtr<CefRunFileDialogCallback> callback) {
  NOTIMPLEMENTED();
  callback->OnFileDialogDismissed(0, {});
}

void ChromeBrowserHostImpl::Print() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&ChromeBrowserHostImpl::Print, this));
    return;
  }

  if (browser_) {
    chrome::Print(browser_);
  }
}

void ChromeBrowserHostImpl::PrintToPDF(
    const CefString& path,
    const CefPdfPrintSettings& settings,
    CefRefPtr<CefPdfPrintCallback> callback) {
  NOTIMPLEMENTED();
  callback->OnPdfPrintFinished(CefString(), false);
}

void ChromeBrowserHostImpl::Find(int identifier,
                                 const CefString& searchText,
                                 bool forward,
                                 bool matchCase,
                                 bool findNext) {
  NOTIMPLEMENTED();
}

void ChromeBrowserHostImpl::StopFinding(bool clearSelection) {
  NOTIMPLEMENTED();
}

void ChromeBrowserHostImpl::ShowDevTools(const CefWindowInfo& windowInfo,
                                         CefRefPtr<CefClient> client,
                                         const CefBrowserSettings& settings,
                                         const CefPoint& inspect_element_at) {
  NOTIMPLEMENTED();
}

void ChromeBrowserHostImpl::CloseDevTools() {
  NOTIMPLEMENTED();
}

bool ChromeBrowserHostImpl::HasDevTools() {
  NOTIMPLEMENTED();
  return false;
}

bool ChromeBrowserHostImpl::SendDevToolsMessage(const void* message,
                                                size_t message_size) {
  NOTIMPLEMENTED();
  return false;
}

int ChromeBrowserHostImpl::ExecuteDevToolsMethod(
    int message_id,
    const CefString& method,
    CefRefPtr<CefDictionaryValue> params) {
  NOTIMPLEMENTED();
  return 0;
}

CefRefPtr<CefRegistration> ChromeBrowserHostImpl::AddDevToolsMessageObserver(
    CefRefPtr<CefDevToolsMessageObserver> observer) {
  NOTIMPLEMENTED();
  return nullptr;
}

void ChromeBrowserHostImpl::SetMouseCursorChangeDisabled(bool disabled) {
  NOTIMPLEMENTED();
}

bool ChromeBrowserHostImpl::IsMouseCursorChangeDisabled() {
  NOTIMPLEMENTED();
  return false;
}

bool ChromeBrowserHostImpl::IsWindowRenderingDisabled() {
  return false;
}

void ChromeBrowserHostImpl::WasResized() {
  NOTIMPLEMENTED();
}

void ChromeBrowserHostImpl::WasHidden(bool hidden) {
  NOTIMPLEMENTED();
}

void ChromeBrowserHostImpl::NotifyScreenInfoChanged() {
  NOTIMPLEMENTED();
}

void ChromeBrowserHostImpl::Invalidate(PaintElementType type) {
  NOTIMPLEMENTED();
}

void ChromeBrowserHostImpl::SendExternalBeginFrame() {
  NOTIMPLEMENTED();
}

void ChromeBrowserHostImpl::SendKeyEvent(const CefKeyEvent& event) {
  NOTIMPLEMENTED();
}

void ChromeBrowserHostImpl::SendMouseClickEvent(const CefMouseEvent& event,
                                                MouseButtonType type,
                                                bool mouseUp,
                                                int clickCount) {
  NOTIMPLEMENTED();
}

void ChromeBrowserHostImpl::SendMouseMoveEvent(const CefMouseEvent& event,
                                               bool mouseLeave) {
  NOTIMPLEMENTED();
}

void ChromeBrowserHostImpl::SendMouseWheelEvent(const CefMouseEvent& event,
                                                int deltaX,
                                                int deltaY) {
  NOTIMPLEMENTED();
}

void ChromeBrowserHostImpl::SendTouchEvent(const CefTouchEvent& event) {
  NOTIMPLEMENTED();
}

void ChromeBrowserHostImpl::SendFocusEvent(bool setFocus) {
  NOTIMPLEMENTED();
}

void ChromeBrowserHostImpl::SendCaptureLostEvent() {
  NOTIMPLEMENTED();
}

void ChromeBrowserHostImpl::NotifyMoveOrResizeStarted() {
  NOTIMPLEMENTED();
}

int ChromeBrowserHostImpl::GetWindowlessFrameRate() {
  return 0;
}

void ChromeBrowserHostImpl::SetWindowlessFrameRate(int frame_rate) {}

void ChromeBrowserHostImpl::ImeSetComposition(
    const CefString& text,
    const std::vector<CefCompositionUnderline>& underlines,
    const CefRange& replacement_range,
    const CefRange& selection_range) {
  NOTIMPLEMENTED();
}

void ChromeBrowserHostImpl::ImeCommitText(const CefString& text,
                                          const CefRange& replacement_range,
                                          int relative_cursor_pos) {
  NOTIMPLEMENTED();
}
void ChromeBrowserHostImpl::ImeFinishComposingText(bool keep_selection) {
  NOTIMPLEMENTED();
}

void ChromeBrowserHostImpl::ImeCancelComposition() {
  NOTIMPLEMENTED();
}

void ChromeBrowserHostImpl::DragTargetDragEnter(
    CefRefPtr<CefDragData> drag_data,
    const CefMouseEvent& event,
    DragOperationsMask allowed_ops) {
  NOTIMPLEMENTED();
}

void ChromeBrowserHostImpl::DragTargetDragOver(const CefMouseEvent& event,
                                               DragOperationsMask allowed_ops) {
  NOTIMPLEMENTED();
}

void ChromeBrowserHostImpl::DragTargetDragLeave() {
  NOTIMPLEMENTED();
}

void ChromeBrowserHostImpl::DragTargetDrop(const CefMouseEvent& event) {
  NOTIMPLEMENTED();
}

void ChromeBrowserHostImpl::DragSourceSystemDragEnded() {
  NOTIMPLEMENTED();
}

void ChromeBrowserHostImpl::DragSourceEndedAt(int x,
                                              int y,
                                              DragOperationsMask op) {
  NOTIMPLEMENTED();
}

void ChromeBrowserHostImpl::SetAudioMuted(bool mute) {
  NOTIMPLEMENTED();
}

bool ChromeBrowserHostImpl::IsAudioMuted() {
  NOTIMPLEMENTED();
  return false;
}

void ChromeBrowserHostImpl::SetAccessibilityState(
    cef_state_t accessibility_state) {
  NOTIMPLEMENTED();
}

void ChromeBrowserHostImpl::SetAutoResizeEnabled(bool enabled,
                                                 const CefSize& min_size,
                                                 const CefSize& max_size) {
  NOTIMPLEMENTED();
}

CefRefPtr<CefExtension> ChromeBrowserHostImpl::GetExtension() {
  return nullptr;
}

bool ChromeBrowserHostImpl::IsBackgroundHost() {
  return false;
}

void ChromeBrowserHostImpl::GoBack() {
  auto callback = base::BindOnce(&ChromeBrowserHostImpl::GoBack, this);
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, std::move(callback));
  }

  if (browser_info_->IsNavigationLocked(std::move(callback))) {
    return;
  }

  if (browser_ && chrome::CanGoBack(browser_)) {
    chrome::GoBack(browser_, kDefaultWindowDisposition);
  }
}

void ChromeBrowserHostImpl::GoForward() {
  auto callback = base::BindOnce(&ChromeBrowserHostImpl::GoForward, this);
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, std::move(callback));
  }

  if (browser_info_->IsNavigationLocked(std::move(callback))) {
    return;
  }

  if (browser_ && chrome::CanGoForward(browser_)) {
    chrome::GoForward(browser_, kDefaultWindowDisposition);
  }
}

void ChromeBrowserHostImpl::Reload() {
  auto callback = base::BindOnce(&ChromeBrowserHostImpl::Reload, this);
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, std::move(callback));
  }

  if (browser_info_->IsNavigationLocked(std::move(callback))) {
    return;
  }

  if (browser_) {
    chrome::Reload(browser_, kDefaultWindowDisposition);
  }
}

void ChromeBrowserHostImpl::ReloadIgnoreCache() {
  auto callback =
      base::BindOnce(&ChromeBrowserHostImpl::ReloadIgnoreCache, this);
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, std::move(callback));
  }

  if (browser_info_->IsNavigationLocked(std::move(callback))) {
    return;
  }

  if (browser_) {
    chrome::ReloadBypassingCache(browser_, kDefaultWindowDisposition);
  }
}

void ChromeBrowserHostImpl::StopLoad() {
  auto callback = base::BindOnce(&ChromeBrowserHostImpl::StopLoad, this);
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, std::move(callback));
  }

  if (browser_info_->IsNavigationLocked(std::move(callback))) {
    return;
  }

  if (browser_) {
    chrome::Stop(browser_);
  }
}

ChromeBrowserHostImpl::ChromeBrowserHostImpl(
    const CefBrowserSettings& settings,
    CefRefPtr<CefClient> client,
    scoped_refptr<CefBrowserInfo> browser_info,
    CefRefPtr<CefRequestContextImpl> request_context)
    : CefBrowserHostBase(settings, client, browser_info, request_context) {}

void ChromeBrowserHostImpl::Attach(Browser* browser,
                                   content::WebContents* web_contents) {
  DCHECK(browser);
  DCHECK(web_contents);

  SetBrowser(browser);
  contents_delegate_->ObserveWebContents(web_contents);
  InitializeBrowser();
}

void ChromeBrowserHostImpl::SetBrowser(Browser* browser) {
  CEF_REQUIRE_UIT();
  browser_ = browser;
}

void ChromeBrowserHostImpl::InitializeBrowser() {
  CEF_REQUIRE_UIT();
  DCHECK(browser_);

  CefBrowserHostBase::InitializeBrowser();

  OnAfterCreated();
}

void ChromeBrowserHostImpl::DestroyBrowser() {
  CEF_REQUIRE_UIT();
  browser_ = nullptr;

  OnBeforeClose();
  OnBrowserDestroyed();

  CefBrowserHostBase::DestroyBrowser();
}
