// Copyright 2020 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/chrome/chrome_browser_host_impl.h"

#include "libcef/browser/browser_platform_delegate.h"
#include "libcef/browser/chrome/browser_platform_delegate_chrome.h"
#include "libcef/browser/thread_util.h"
#include "libcef/browser/views/browser_view_impl.h"
#include "libcef/common/net/url_util.h"
#include "libcef/features/runtime_checks.h"

#include "base/logging.h"
#include "base/notreached.h"
#include "chrome/browser/printing/print_view_manager_common.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"

#if defined(TOOLKIT_VIEWS)
#include "libcef/browser/chrome/views/chrome_browser_frame.h"
#include "libcef/browser/chrome/views/chrome_browser_view.h"
#endif

// static
CefRefPtr<ChromeBrowserHostImpl> ChromeBrowserHostImpl::Create(
    const CefBrowserCreateParams& params) {
  auto browser = CreateBrowser(params);

  GURL url = url_util::MakeGURL(params.url, /*fixup=*/true);
  if (url.is_empty()) {
    // Chrome will navigate to kChromeUINewTabURL by default. We want to keep
    // the current CEF behavior of not navigating at all. Use a special URL that
    // will be recognized in HandleNonNavigationAboutURL.
    url = GURL("chrome://ignore/");
  }

  // Add a new tab. This will indirectly create a new tab WebContents and
  // call ChromeBrowserDelegate::OnWebContentsCreated to create the associated
  // ChromeBrowserHostImpl.
  chrome::AddTabAt(browser, url, /*index=*/TabStripModel::kNoTab,
                   /*foreground=*/true);

  // The new tab WebContents.
  auto web_contents = browser->tab_strip_model()->GetActiveWebContents();
  CHECK(web_contents);

  // The associated ChromeBrowserHostImpl.
  auto browser_host =
      ChromeBrowserHostImpl::GetBrowserForContents(web_contents);
  CHECK(browser_host);

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
CefRefPtr<ChromeBrowserHostImpl> ChromeBrowserHostImpl::GetBrowserForGlobalId(
    const content::GlobalRenderFrameHostId& global_id) {
  REQUIRE_CHROME_RUNTIME();
  auto browser = CefBrowserHostBase::GetBrowserForGlobalId(global_id);
  return static_cast<ChromeBrowserHostImpl*>(browser.get());
}

ChromeBrowserHostImpl::~ChromeBrowserHostImpl() = default;

void ChromeBrowserHostImpl::AddNewContents(
    std::unique_ptr<content::WebContents> contents) {
  DCHECK(contents);
  DCHECK(!browser_);

  // We should already be associated with the WebContents.
  DCHECK_EQ(GetWebContents(), contents.get());

  CefBrowserCreateParams params;
  params.request_context = request_context();
#if defined(TOOLKIT_VIEWS)
  params.browser_view = GetBrowserView();
#endif

  // Create the new Browser representation.
  auto browser = CreateBrowser(params);

  // Add the WebContents to the Browser.
  browser->tab_strip_model()->AddWebContents(
      std::move(contents), /*index=*/TabStripModel::kNoTab,
      ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL,
      TabStripModel::ADD_ACTIVE);

  SetBrowser(browser);
}

void ChromeBrowserHostImpl::OnWebContentsDestroyed(
    content::WebContents* web_contents) {
  platform_delegate_->WebContentsDestroyed(web_contents);
  DestroyBrowser();
}

void ChromeBrowserHostImpl::OnSetFocus(cef_focus_source_t source) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&ChromeBrowserHostImpl::OnSetFocus,
                                          this, source));
    return;
  }

  if (contents_delegate_->OnSetFocus(source))
    return;

  if (browser_) {
    const int tab_index = GetCurrentTabIndex();
    if (tab_index != TabStripModel::kNoTab) {
      chrome::SelectNumberedTab(browser_, tab_index);
    }
  }
}

void ChromeBrowserHostImpl::CloseBrowser(bool force_close) {
  // Always do this asynchronously because TabStripModel is not re-entrant.
  CEF_POST_TASK(CEF_UIT, base::BindOnce(&ChromeBrowserHostImpl::DoCloseBrowser,
                                        this, force_close));
}

bool ChromeBrowserHostImpl::TryCloseBrowser() {
  // TODO(chrome): Handle the case where the browser may not close immediately.
  CloseBrowser(true);
  return true;
}

void ChromeBrowserHostImpl::SetFocus(bool focus) {
  if (focus) {
    OnSetFocus(FOCUS_SOURCE_SYSTEM);
  }
}

CefWindowHandle ChromeBrowserHostImpl::GetWindowHandle() {
  NOTIMPLEMENTED();
  return kNullWindowHandle;
}

CefWindowHandle ChromeBrowserHostImpl::GetOpenerWindowHandle() {
  NOTIMPLEMENTED();
  return kNullWindowHandle;
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
    // Like chrome::Print() but specifying the WebContents.
    printing::StartPrint(GetWebContents(),
                         /*print_renderer=*/mojo::NullAssociatedRemote(),
                         browser_->profile()->GetPrefs()->GetBoolean(
                             prefs::kPrintPreviewDisabled),
                         /*has_selection=*/false);
  }
}

void ChromeBrowserHostImpl::PrintToPDF(
    const CefString& path,
    const CefPdfPrintSettings& settings,
    CefRefPtr<CefPdfPrintCallback> callback) {
  NOTIMPLEMENTED();
  callback->OnPdfPrintFinished(CefString(), false);
}

void ChromeBrowserHostImpl::Find(const CefString& searchText,
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

void ChromeBrowserHostImpl::SendTouchEvent(const CefTouchEvent& event) {
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

bool ChromeBrowserHostImpl::Navigate(const content::OpenURLParams& params) {
  CEF_REQUIRE_UIT();
  if (GetCurrentTabIndex() == TabStripModel::kNoTab) {
    // We can't navigate via the Browser because we don't have a current tab.
    return CefBrowserHostBase::Navigate(params);
  }

  if (browser_) {
    GURL gurl = params.url;
    if (!url_util::FixupGURL(gurl))
      return false;

    // This is generally equivalent to calling Browser::OpenURL, except:
    // 1. It doesn't trigger a call to CefRequestHandler::OnOpenURLFromTab, and
    // 2. It navigates in this CefBrowserHost's WebContents instead of
    //    (a) creating a new WebContents, or (b) using the Browser's active
    //    WebContents (which may not be the same), and
    // 3. There is no risk of triggering chrome's popup blocker.
    NavigateParams nav_params(browser_, gurl, params.transition);
    nav_params.FillNavigateParamsFromOpenURLParams(params);

    // Always navigate in the current tab.
    nav_params.disposition = WindowOpenDisposition::CURRENT_TAB;
    nav_params.source_contents = GetWebContents();

    nav_params.tabstrip_add_types = TabStripModel::ADD_NONE;
    if (params.user_gesture)
      nav_params.window_action = NavigateParams::SHOW_WINDOW;
    ::Navigate(&nav_params);
    return true;
  }
  return false;
}

ChromeBrowserHostImpl::ChromeBrowserHostImpl(
    const CefBrowserSettings& settings,
    CefRefPtr<CefClient> client,
    std::unique_ptr<CefBrowserPlatformDelegate> platform_delegate,
    scoped_refptr<CefBrowserInfo> browser_info,
    CefRefPtr<CefRequestContextImpl> request_context)
    : CefBrowserHostBase(settings,
                         client,
                         std::move(platform_delegate),
                         browser_info,
                         request_context) {}

// static
Browser* ChromeBrowserHostImpl::CreateBrowser(
    const CefBrowserCreateParams& params) {
  // Get or create the request context and profile.
  CefRefPtr<CefRequestContextImpl> request_context_impl =
      CefRequestContextImpl::GetOrCreateForRequestContext(
          params.request_context);
  CHECK(request_context_impl);
  auto cef_browser_context = request_context_impl->GetBrowserContext();
  CHECK(cef_browser_context);
  auto profile = cef_browser_context->AsProfile();
  CHECK(profile);

  Browser::CreateParams chrome_params =
      Browser::CreateParams(profile, /*user_gesture=*/false);

  // Pass |params| to cef::BrowserDelegate::Create from the Browser constructor.
  chrome_params.cef_params = base::MakeRefCounted<DelegateCreateParams>(params);

#if defined(TOOLKIT_VIEWS)
  // Configure Browser creation to use the existing Views-based
  // Widget/BrowserFrame (ChromeBrowserFrame) and BrowserView/BrowserWindow
  // (ChromeBrowserView). See views/chrome_browser_frame.h for related
  // documentation.
  ChromeBrowserView* chrome_browser_view = nullptr;
  if (params.browser_view) {
    // Don't show most controls.
    chrome_params.type = Browser::TYPE_POPUP;
    // Don't show title bar or address.
    chrome_params.trusted_source = true;

    auto view_impl =
        static_cast<CefBrowserViewImpl*>(params.browser_view.get());

    chrome_browser_view =
        static_cast<ChromeBrowserView*>(view_impl->root_view());
    chrome_params.window = chrome_browser_view;

    auto chrome_widget =
        static_cast<ChromeBrowserFrame*>(chrome_browser_view->GetWidget());
    chrome_browser_view->set_frame(chrome_widget);
  }
#endif  // defined(TOOLKIT_VIEWS)

  // Create the Browser. This will indirectly create the ChomeBrowserDelegate.
  // The same params will be used to create a new Browser if the tab is dragged
  // out of the existing Browser. The returned Browser is owned by the
  // associated BrowserView.
  auto browser = Browser::Create(chrome_params);

  bool show_browser = true;

#if defined(TOOLKIT_VIEWS)
  if (chrome_browser_view) {
    // Initialize the BrowserFrame and BrowserView and create the controls that
    // require access to the Browser.
    chrome_browser_view->InitBrowser(base::WrapUnique(browser),
                                     params.browser_view);

    // Don't show the browser by default.
    show_browser = false;
  }
#endif

  if (show_browser) {
    browser->window()->Show();
  }

  return browser;
}

void ChromeBrowserHostImpl::Attach(content::WebContents* web_contents,
                                   CefRefPtr<ChromeBrowserHostImpl> opener) {
  DCHECK(web_contents);

  if (opener) {
    // Give the opener browser's platform delegate an opportunity to modify the
    // new browser's platform delegate.
    opener->platform_delegate_->PopupWebContentsCreated(
        settings_, client_, web_contents, platform_delegate_.get(),
        /*is_devtools_popup=*/false);
  }

  platform_delegate_->WebContentsCreated(web_contents,
                                         /*own_web_contents=*/false);
  contents_delegate_->ObserveWebContents(web_contents);

  // Associate the platform delegate with this browser.
  platform_delegate_->BrowserCreated(this);

  // Associate the base class with the WebContents.
  InitializeBrowser();

  // Notify that the browser has been created. These must be delivered in the
  // expected order.

  // 1. Notify the browser's LifeSpanHandler. This must always be the first
  // notification for the browser.
  {
    // The WebContents won't be added to the Browser's TabStripModel until later
    // in the current call stack. Block navigation until that time.
    auto navigation_lock = browser_info_->CreateNavigationLock();
    OnAfterCreated();
  }

  // 2. Notify the platform delegate. With Views this will result in a call to
  // CefBrowserViewDelegate::OnBrowserCreated().
  platform_delegate_->NotifyBrowserCreated();

  if (opener && opener->platform_delegate_) {
    // 3. Notify the opener browser's platform delegate. With Views this will
    // result in a call to CefBrowserViewDelegate::OnPopupBrowserViewCreated().
    opener->platform_delegate_->PopupBrowserCreated(
        this,
        /*is_devtools_popup=*/false);
  }
}

void ChromeBrowserHostImpl::SetBrowser(Browser* browser) {
  CEF_REQUIRE_UIT();
  browser_ = browser;
  static_cast<CefBrowserPlatformDelegateChrome*>(platform_delegate_.get())
      ->set_chrome_browser(browser);
}

void ChromeBrowserHostImpl::WindowDestroyed() {
  CEF_REQUIRE_UIT();
#if defined(TOOLKIT_VIEWS)
  if (browser_ && is_views_hosted_) {
    auto chrome_browser_view =
        static_cast<ChromeBrowserView*>(browser_->window());
    chrome_browser_view->Destroyed();
  }
#endif

  platform_delegate_->CloseHostWindow();
}

void ChromeBrowserHostImpl::DestroyBrowser() {
  CEF_REQUIRE_UIT();
  browser_ = nullptr;

  OnBeforeClose();
  OnBrowserDestroyed();

  // Disassociate the platform delegate from this browser.
  platform_delegate_->BrowserDestroyed(this);

  CefBrowserHostBase::DestroyBrowser();
}

void ChromeBrowserHostImpl::DoCloseBrowser(bool force_close) {
  CEF_REQUIRE_UIT();
  if (browser_) {
    // Like chrome::CloseTab() but specifying the WebContents.
    const int tab_index = GetCurrentTabIndex();
    if (tab_index != TabStripModel::kNoTab) {
      // TODO(chrome): Handle the case where this method returns false,
      // indicating that the contents were not closed immediately.
      browser_->tab_strip_model()->CloseWebContentsAt(
          tab_index, TabStripModel::CLOSE_CREATE_HISTORICAL_TAB |
                         TabStripModel::CLOSE_USER_GESTURE);
    }
  }
}

int ChromeBrowserHostImpl::GetCurrentTabIndex() const {
  CEF_REQUIRE_UIT();
  if (browser_) {
    return browser_->tab_strip_model()->GetIndexOfWebContents(GetWebContents());
  }
  return TabStripModel::kNoTab;
}
