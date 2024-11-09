// Copyright 2020 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/browser/chrome/chrome_browser_host_impl.h"

#include "base/logging.h"
#include "base/notreached.h"
#include "cef/libcef/browser/browser_platform_delegate.h"
#include "cef/libcef/browser/chrome/browser_platform_delegate_chrome.h"
#include "cef/libcef/browser/chrome/views/chrome_browser_frame.h"
#include "cef/libcef/browser/chrome/views/chrome_browser_view.h"
#include "cef/libcef/browser/thread_util.h"
#include "cef/libcef/browser/views/browser_view_impl.h"
#include "cef/libcef/common/net/url_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "chrome/common/pref_names.h"

// static
CefRefPtr<ChromeBrowserHostImpl> ChromeBrowserHostImpl::Create(
    const CefBrowserCreateParams& params) {
  auto browser = CreateBrowser(params, std::nullopt);

  GURL url = url_util::MakeGURL(params.url, /*fixup=*/true);
  const bool url_is_empty = url.is_empty();
  if (url_is_empty) {
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

  if (!url_is_empty) {
    // Match Alloy-style behavior of requesting focus after initial navigation.
    browser_host->OnSetFocus(FOCUS_SOURCE_NAVIGATION);
  }

  return browser_host;
}

// static
CefRefPtr<ChromeBrowserHostImpl> ChromeBrowserHostImpl::FromBaseChecked(
    CefRefPtr<CefBrowserHostBase> host_base) {
  if (!host_base) {
    return nullptr;
  }
  CHECK(host_base->IsChromeStyle());
  return static_cast<ChromeBrowserHostImpl*>(host_base.get());
}

// static
CefRefPtr<ChromeBrowserHostImpl> ChromeBrowserHostImpl::GetBrowserForHost(
    const content::RenderViewHost* host) {
  return FromBaseChecked(CefBrowserHostBase::GetBrowserForHost(host));
}

// static
CefRefPtr<ChromeBrowserHostImpl> ChromeBrowserHostImpl::GetBrowserForHost(
    const content::RenderFrameHost* host) {
  return FromBaseChecked(CefBrowserHostBase::GetBrowserForHost(host));
}

// static
CefRefPtr<ChromeBrowserHostImpl> ChromeBrowserHostImpl::GetBrowserForContents(
    const content::WebContents* contents) {
  return FromBaseChecked(CefBrowserHostBase::GetBrowserForContents(contents));
}

// static
CefRefPtr<ChromeBrowserHostImpl> ChromeBrowserHostImpl::GetBrowserForGlobalId(
    const content::GlobalRenderFrameHostId& global_id) {
  return FromBaseChecked(CefBrowserHostBase::GetBrowserForGlobalId(global_id));
}

// static
CefRefPtr<ChromeBrowserHostImpl> ChromeBrowserHostImpl::GetBrowserForBrowser(
    const Browser* browser) {
  // Return the ChromeBrowserHostImpl that is currently active.
  // Views-hosted Browsers will contain a single ChromeBrowserHostImpl.
  // Otherwise, there will be a ChromeBrowserHostImpl per Tab/WebContents.
  // |contents| may be nullptr during Browser initialization or destruction.
  auto contents = browser->tab_strip_model()->GetActiveWebContents();
  if (!contents) {
    return nullptr;
  }
  return GetBrowserForContents(contents);
}

ChromeBrowserHostImpl::~ChromeBrowserHostImpl() = default;

void ChromeBrowserHostImpl::AddNewContents(
    std::unique_ptr<content::WebContents> contents,
    std::optional<Browser::CreateParams> browser_create_params) {
  DCHECK(contents);
  DCHECK(!browser_);

  // We should already be associated with the WebContents.
  DCHECK_EQ(GetWebContents(), contents.get());

  CefBrowserCreateParams params;
  params.request_context = request_context();
  params.browser_view = GetBrowserView();

  // Create the new Browser representation.
  auto browser = CreateBrowser(params, std::move(browser_create_params));

  // Add the WebContents to the Browser.
  browser->tab_strip_model()->AddWebContents(
      std::move(contents), /*index=*/TabStripModel::kNoTab,
      ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL,
      AddTabTypes::ADD_ACTIVE);

  SetBrowser(browser);
}

void ChromeBrowserHostImpl::OnWebContentsDestroyed(
    content::WebContents* web_contents) {
  DestroyWebContents(web_contents);

  if (!is_destroying_browser_) {
    // Destroy the browser asynchronously to allow the current call stack
    // to unwind (we may have been called via the TabStripModel owned by the
    // Browser).
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&ChromeBrowserHostImpl::DestroyBrowser, this));
  }
}

void ChromeBrowserHostImpl::OnSetFocus(cef_focus_source_t source) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&ChromeBrowserHostImpl::OnSetFocus,
                                          this, source));
    return;
  }

  if (contents_delegate_.OnSetFocus(source)) {
    return;
  }

  if (platform_delegate_) {
    platform_delegate_->SetFocus(true);
  }

  if (browser_) {
    const int tab_index = GetCurrentTabIndex();
    if (tab_index != TabStripModel::kNoTab) {
      chrome::SelectNumberedTab(browser_, tab_index);
    }
  }
}

void ChromeBrowserHostImpl::CloseBrowser(bool force_close) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&ChromeBrowserHostImpl::CloseBrowser,
                                          this, force_close));
    return;
  }

  if (!force_close) {
    TryCloseBrowser();
    return;
  }

  // Always do this asynchronously because TabStripModel is not re-entrant.
  CEF_POST_TASK(CEF_UIT,
                base::BindOnce(&ChromeBrowserHostImpl::DoCloseBrowser, this));
}

bool ChromeBrowserHostImpl::TryCloseBrowser() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    DCHECK(false) << "called on invalid thread";
    return false;
  }

  if (auto* web_contents = GetWebContents()) {
    // This check works as follows:
    // 1. Returns false if the main frame is ready to close
    //    (IsPageReadyToBeClosed returns true).
    // 2. Otherwise returns true if any frame in the frame tree needs to run
    //    beforeunload or unload-time event handlers.
    // 3. Otherwise returns false.
    if (web_contents->NeedToFireBeforeUnloadOrUnloadEvents()) {
      // Will result in a call to Browser::BeforeUnloadFired and, if the close
      // isn't canceled, Browser::CloseContents which indirectly calls
      // TabStripModel::CloseWebContentsAt (similar to DoCloseBrowser but
      // without CLOSE_USER_GESTURE). Additional calls to DispatchBeforeUnload
      // while the unload is pending will be ignored.
      web_contents->DispatchBeforeUnload(/*auto_cancel=*/false);
      return false;
    }
  }

  CloseBrowser(/*force_close=*/true);
  return true;
}

CefWindowHandle ChromeBrowserHostImpl::GetWindowHandle() {
  if (CEF_CURRENTLY_ON_UIT()) {
    // Always return the most up-to-date window handle for a views-hosted
    // browser since it may change if the view is re-parented.
    if (platform_delegate_) {
      return platform_delegate_->GetHostWindowHandle();
    }
  }
  return host_window_handle_;
}

CefWindowHandle ChromeBrowserHostImpl::GetOpenerWindowHandle() {
  NOTIMPLEMENTED();
  return kNullWindowHandle;
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

void ChromeBrowserHostImpl::SetAutoResizeEnabled(bool enabled,
                                                 const CefSize& min_size,
                                                 const CefSize& max_size) {
  NOTIMPLEMENTED();
}

bool ChromeBrowserHostImpl::CanExecuteChromeCommand(int command_id) {
  // Verify that this method is being called on the UI thread.
  if (!CEF_CURRENTLY_ON_UIT()) {
    DCHECK(false) << "called on invalid thread";
    return false;
  }

  if (browser_) {
    return chrome::SupportsCommand(browser_, command_id) &&
           chrome::IsCommandEnabled(browser_, command_id);
  }
  return false;
}

void ChromeBrowserHostImpl::ExecuteChromeCommand(
    int command_id,
    cef_window_open_disposition_t disposition) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
                  base::BindOnce(&ChromeBrowserHostImpl::ExecuteChromeCommand,
                                 this, command_id, disposition));
    return;
  }

  if (browser_) {
    chrome::ExecuteCommandWithDisposition(
        browser_, command_id, static_cast<WindowOpenDisposition>(disposition));
  }
}

ChromeBrowserView* ChromeBrowserHostImpl::chrome_browser_view() const {
  if (browser_ && is_views_hosted_) {
    return static_cast<ChromeBrowserView*>(browser_->window());
  }
  return nullptr;
}

bool ChromeBrowserHostImpl::Navigate(const content::OpenURLParams& params) {
  CEF_REQUIRE_UIT();
  if (GetCurrentTabIndex() == TabStripModel::kNoTab) {
    // We can't navigate via the Browser because we don't have a current tab.
    return CefBrowserHostBase::Navigate(params);
  }

  if (browser_) {
    GURL gurl = params.url;
    if (!url_util::FixupGURL(gurl)) {
      return false;
    }

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

    nav_params.tabstrip_add_types = AddTabTypes::ADD_NONE;
    if (params.user_gesture) {
      nav_params.window_action = NavigateParams::SHOW_WINDOW;
    }
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
    const CefBrowserCreateParams& params,
    std::optional<Browser::CreateParams> browser_create_params) {
  Browser::CreateParams chrome_params = [&params, &browser_create_params]() {
    if (!browser_create_params.has_value()) {
      auto* profile = CefRequestContextImpl::GetProfile(params.request_context);
      return Browser::CreateParams(profile, /*user_gesture=*/false);
    } else {
      return std::move(*browser_create_params);
    }
  }();

  // Pass |params| to cef::BrowserDelegate::Create from the Browser constructor.
  chrome_params.cef_params = base::MakeRefCounted<DelegateCreateParams>(params);

  // Configure Browser creation to use the existing Views-based
  // Widget/BrowserFrame (ChromeBrowserFrame) and BrowserView/BrowserWindow
  // (ChromeBrowserView). See views/chrome_browser_frame.h for related
  // documentation.
  ChromeBrowserView* chrome_browser_view = nullptr;
  if (params.browser_view) {
    if (chrome_params.type == Browser::TYPE_NORMAL) {
      // Don't show most controls.
      chrome_params.type = Browser::TYPE_POPUP;
      // Don't show title bar or address.
      chrome_params.trusted_source = true;
    }

    auto view_impl =
        static_cast<CefBrowserViewImpl*>(params.browser_view.get());

    chrome_browser_view = view_impl->chrome_browser_view();
    chrome_params.window = chrome_browser_view;

    auto chrome_widget =
        static_cast<ChromeBrowserFrame*>(chrome_browser_view->GetWidget());
    chrome_browser_view->set_frame(chrome_widget);
  }

  // Create the Browser. This will indirectly create the ChomeBrowserDelegate.
  // The same params will be used to create a new Browser if the tab is dragged
  // out of the existing Browser. The returned Browser is owned by the
  // associated BrowserView.
  auto browser = Browser::Create(chrome_params);

  bool show_browser = true;

  if (chrome_browser_view) {
    // Initialize the BrowserFrame and BrowserView and create the controls that
    // require access to the Browser.
    chrome_browser_view->InitBrowser(base::WrapUnique(browser));

    // Don't set theme colors in ContentsWebView::UpdateBackgroundColor.
    chrome_browser_view->contents_web_view()->SetBackgroundVisible(false);

    // Don't show the browser by default.
    show_browser = false;
  }

  if (show_browser) {
    browser->window()->Show();
  }

  return browser;
}

void ChromeBrowserHostImpl::Attach(content::WebContents* web_contents,
                                   bool is_devtools_popup,
                                   CefRefPtr<CefBrowserHostBase> opener) {
  DCHECK(web_contents);

  if (opener) {
    opener_id_ = opener->GetIdentifier();

    // Give the opener browser's platform delegate an opportunity to modify the
    // new browser's platform delegate.
    opener->platform_delegate()->PopupWebContentsCreated(
        settings_, client_, web_contents, platform_delegate_.get(),
        is_devtools_popup);
  }

  platform_delegate_->WebContentsCreated(web_contents,
                                         /*own_web_contents=*/false);
  contents_delegate_.ObserveWebContents(web_contents);

  // Associate the platform delegate with this browser.
  platform_delegate_->BrowserCreated(this);

  // Associate the base class with the WebContents.
  InitializeBrowser();

  // Notify that the browser has been created. These must be delivered in the
  // expected order.

  if (opener) {
    // 1. Notify the opener browser's platform delegate. With Views this will
    // result in a call to CefBrowserViewDelegate::OnPopupBrowserViewCreated().
    // We want to call this method first because the implementation will often
    // create the Widget for the new popup browser. Without that Widget
    // CefBrowserHost::GetWindowHandle() will return kNullWindowHandle in
    // OnAfterCreated(), which breaks client expectations (e.g. clients expect
    // everything about the browser to be valid at that time).
    opener->platform_delegate()->PopupBrowserCreated(platform_delegate_.get(),
                                                     this, is_devtools_popup);
  }

  // 2. Notify the browser's LifeSpanHandler. This must always be the first
  // notification for the browser.
  {
    // The WebContents won't be added to the Browser's TabStripModel until later
    // in the current call stack. Block navigation until that time.
    auto navigation_lock = browser_info_->CreateNavigationLock();
    OnAfterCreated();
  }

  // 3. Notify the platform delegate. With Views this will result in a call to
  // CefBrowserViewDelegate::OnBrowserCreated().
  platform_delegate_->NotifyBrowserCreated();
}

void ChromeBrowserHostImpl::SetBrowser(Browser* browser) {
  CEF_REQUIRE_UIT();
  if (browser == browser_) {
    return;
  }

  browser_ = browser;
  static_cast<CefBrowserPlatformDelegateChrome*>(platform_delegate_.get())
      ->set_chrome_browser(browser);
  if (browser_) {
    // We expect the Browser and CefRequestContext to have the same Profile.
    CHECK_EQ(browser_->profile(),
             request_context()->GetBrowserContext()->AsProfile());

    host_window_handle_ = platform_delegate_->GetHostWindowHandle();
  } else {
    host_window_handle_ = kNullWindowHandle;
  }
}

void ChromeBrowserHostImpl::WindowDestroyed() {
  CEF_REQUIRE_UIT();
  if (auto view = chrome_browser_view()) {
    view->Destroyed();
  }

  platform_delegate_->CloseHostWindow();
}

bool ChromeBrowserHostImpl::WillBeDestroyed() const {
  CEF_REQUIRE_UIT();
  // TODO(chrome): Modify this to support DoClose(), see issue #3294.
  return !!browser_;
}

void ChromeBrowserHostImpl::DestroyBrowser() {
  CEF_REQUIRE_UIT();

  is_destroying_browser_ = true;

  // If the WebContents still exists at this point, close the Browser and
  // WebContents first. See comments on CefBrowserHostBase::DestroyBrowser.
  if (GetWebContents()) {
    // Triggers a call to OnWebContentsDestroyed.
    DoCloseBrowser();
    DCHECK(!GetWebContents());
  }

  // Disassociate the platform delegate from this browser.
  platform_delegate_->BrowserDestroyed(this);

  // Clean up UI thread state.
  browser_ = nullptr;
  weak_ptr_factory_.InvalidateWeakPtrs();

  CefBrowserHostBase::DestroyBrowser();
}

void ChromeBrowserHostImpl::DoCloseBrowser() {
  CEF_REQUIRE_UIT();
  if (browser_) {
    // Like chrome::CloseTab() but specifying the WebContents.
    const int tab_index = GetCurrentTabIndex();
    if (tab_index != TabStripModel::kNoTab) {
      // This will trigger destruction of the Browser and WebContents.
      browser_->tab_strip_model()->CloseWebContentsAt(
          tab_index, TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB |
                         TabCloseTypes::CLOSE_USER_GESTURE);
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
