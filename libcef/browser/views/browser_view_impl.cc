// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "cef/libcef/browser/views/browser_view_impl.h"

#include <memory>
#include <optional>

#include "cef/libcef/browser/browser_event_util.h"
#include "cef/libcef/browser/browser_host_base.h"
#include "cef/libcef/browser/chrome/views/chrome_browser_view.h"
#include "cef/libcef/browser/context.h"
#include "cef/libcef/browser/request_context_impl.h"
#include "cef/libcef/browser/thread_util.h"
#include "cef/libcef/browser/views/widget.h"
#include "cef/libcef/browser/views/window_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "components/input/native_web_keyboard_event.h"
#include "ui/content_accelerators/accelerator_util.h"

namespace {

std::optional<cef_gesture_command_t> GetGestureCommand(
    ui::GestureEvent* event) {
#if BUILDFLAG(IS_MAC)
  if (event->details().type() == ui::EventType::kGestureSwipe) {
    if (event->details().swipe_left()) {
      return CEF_GESTURE_COMMAND_BACK;
    } else if (event->details().swipe_right()) {
      return CEF_GESTURE_COMMAND_FORWARD;
    }
  }
#endif
  return std::nullopt;
}

bool ComputeAlloyStyle(
    CefBrowserViewDelegate* cef_delegate,
    bool is_devtools_popup,
    std::optional<cef_runtime_style_t> opener_runtime_style) {
  if (is_devtools_popup) {
    // Alloy style is not supported with Chrome DevTools popups.
    if (cef_delegate &&
        cef_delegate->GetBrowserRuntimeStyle() == CEF_RUNTIME_STYLE_ALLOY) {
      LOG(ERROR) << "GetBrowserRuntimeStyle() requested Alloy style; only "
                    "Chrome style is supported for DevTools popups";
    }
    return false;
  }

  if (opener_runtime_style) {
    // Popup style must match the opener style.
    const bool opener_alloy_style =
        *opener_runtime_style == CEF_RUNTIME_STYLE_ALLOY;
    if (cef_delegate) {
      const auto requested_style = cef_delegate->GetBrowserRuntimeStyle();
      if (requested_style != CEF_RUNTIME_STYLE_DEFAULT &&
          requested_style != (opener_alloy_style ? CEF_RUNTIME_STYLE_ALLOY
                                                 : CEF_RUNTIME_STYLE_CHROME)) {
        LOG(ERROR)
            << "GetBrowserRuntimeStyle() for popups must match opener style";
      }
    }
    return opener_alloy_style;
  }

  // Chrome style is the default unless Alloy is specifically requested.
  return cef_delegate &&
         cef_delegate->GetBrowserRuntimeStyle() == CEF_RUNTIME_STYLE_ALLOY;
}

}  // namespace

// static
CefRefPtr<CefBrowserView> CefBrowserView::CreateBrowserView(
    CefRefPtr<CefClient> client,
    const CefString& url,
    const CefBrowserSettings& settings,
    CefRefPtr<CefDictionaryValue> extra_info,
    CefRefPtr<CefRequestContext> request_context,
    CefRefPtr<CefBrowserViewDelegate> delegate) {
  return CefBrowserViewImpl::Create(CefWindowInfo(), client, url, settings,
                                    extra_info, request_context, delegate);
}

// static
CefRefPtr<CefBrowserView> CefBrowserView::GetForBrowser(
    CefRefPtr<CefBrowser> browser) {
  CEF_REQUIRE_UIT_RETURN(nullptr);

  auto browser_impl = CefBrowserHostBase::FromBrowser(browser);
  if (browser_impl && browser_impl->is_views_hosted()) {
    return browser_impl->GetBrowserView();
  }
  return nullptr;
}

// static
CefRefPtr<CefBrowserViewImpl> CefBrowserViewImpl::Create(
    const CefWindowInfo& window_info,
    CefRefPtr<CefClient> client,
    const CefString& url,
    const CefBrowserSettings& settings,
    CefRefPtr<CefDictionaryValue> extra_info,
    CefRefPtr<CefRequestContext> request_context,
    CefRefPtr<CefBrowserViewDelegate> delegate) {
  CEF_REQUIRE_UIT_RETURN(nullptr);

  if (!request_context) {
    request_context = CefRequestContext::GetGlobalContext();
  }

  // Verify that the browser context is valid. Do this here instead of risking
  // potential browser creation failure when this view is added to the window.
  auto request_context_impl =
      static_cast<CefRequestContextImpl*>(request_context.get());
  if (!request_context_impl->VerifyBrowserContext()) {
    return nullptr;
  }

  CefRefPtr<CefBrowserViewImpl> browser_view =
      new CefBrowserViewImpl(delegate, /*is_devtools_popup=*/false,
                             /*opener_runtime_style=*/std::nullopt);
  browser_view->SetPendingBrowserCreateParams(
      window_info, client, url, settings, extra_info, request_context);
  browser_view->Initialize();
  browser_view->SetDefaults(settings);
  return browser_view;
}

// static
CefRefPtr<CefBrowserViewImpl> CefBrowserViewImpl::CreateForPopup(
    const CefBrowserSettings& settings,
    CefRefPtr<CefBrowserViewDelegate> delegate,
    bool is_devtools,
    cef_runtime_style_t opener_runtime_style) {
  CEF_REQUIRE_UIT_RETURN(nullptr);

  CefRefPtr<CefBrowserViewImpl> browser_view =
      new CefBrowserViewImpl(delegate, is_devtools, opener_runtime_style);
  browser_view->Initialize();
  browser_view->SetDefaults(settings);
  return browser_view;
}

CefBrowserViewImpl::~CefBrowserViewImpl() {
  // We want no further callbacks to this object.
  weak_ptr_factory_.InvalidateWeakPtrs();

  // |browser_| may exist here if the BrowserView was removed from the Views
  // hierarchy prior to tear-down and the last BrowserView reference was
  // released. In that case DisassociateFromWidget() will be called when the
  // BrowserView is removed from the Window but Detach() will not be called
  // because the BrowserView was not destroyed via the Views hierarchy
  // tear-down.
  DCHECK(!cef_widget_);
  if (browser_ && !browser_->WillBeDestroyed()) {
    // With Alloy style |browser_| will disappear when WindowDestroyed()
    // indirectly calls BrowserDestroyed() so keep a reference.
    CefRefPtr<CefBrowserHostBase> browser = browser_;

    // Force the browser to be destroyed.
    browser->WindowDestroyed();
  }
}

void CefBrowserViewImpl::WebContentsCreated(
    content::WebContents* web_contents) {
  if (web_view()) {
    web_view()->SetWebContents(web_contents);
  }
}

void CefBrowserViewImpl::WebContentsDestroyed(
    content::WebContents* web_contents) {
  // This will always be called before BrowserDestroyed().
  DisassociateFromWidget();

  if (web_view()) {
    web_view()->SetWebContents(nullptr);
  }
}

void CefBrowserViewImpl::BrowserCreated(
    CefBrowserHostBase* browser,
    base::RepeatingClosure on_bounds_changed) {
  browser_ = browser;
  on_bounds_changed_ = on_bounds_changed;
}

void CefBrowserViewImpl::BrowserDestroyed(CefBrowserHostBase* browser) {
  DCHECK_EQ(browser, browser_);
  browser_ = nullptr;

  // If this BrowserView belonged to a Widget then we expect to have received a
  // call to DisassociateFromWidget().
  DCHECK(!cef_widget_);
}

void CefBrowserViewImpl::RequestFocusSync() {
  // With Chrome style the root_view() type (ChromeBrowserView) does not accept
  // focus, so always give focus to the WebView directly.
  if (web_view()) {
    if (auto widget = web_view()->GetWidget(); widget->IsMinimized()) {
      // Don't activate a minimized Widget, or it will be shown.
      return;
    }

    // Activate the Widget and indirectly call WebContents::Focus().
    web_view()->RequestFocus();
  }
}

bool CefBrowserViewImpl::HandleKeyboardEvent(
    const input::NativeWebKeyboardEvent& event) {
  if (!root_view()) {
    return false;
  }

  views::FocusManager* focus_manager = root_view()->GetFocusManager();
  if (!focus_manager) {
    return false;
  }

  if (HandleAccelerator(event, focus_manager)) {
    return true;
  }

  // Give the CefWindowDelegate a chance to handle the event.
  if (auto* window_impl = cef_window_impl()) {
    CefKeyEvent cef_event;
    if (GetCefKeyEvent(event, cef_event) &&
        window_impl->OnKeyEvent(cef_event)) {
      return true;
    }
  }

  // Proceed with default native handling.
  return unhandled_keyboard_event_handler_.HandleKeyboardEvent(event,
                                                               focus_manager);
}

CefRefPtr<CefBrowser> CefBrowserViewImpl::GetBrowser() {
  CEF_REQUIRE_VALID_RETURN(nullptr);
  return browser_;
}

CefRefPtr<CefView> CefBrowserViewImpl::GetChromeToolbar() {
  CEF_REQUIRE_VALID_RETURN(nullptr);
  if (!is_alloy_style_) {
    return chrome_browser_view()->cef_toolbar();
  }

  return nullptr;
}

void CefBrowserViewImpl::SetPreferAccelerators(bool prefer_accelerators) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  if (web_view()) {
    web_view()->set_allow_accelerators(prefer_accelerators);
  }
}

cef_runtime_style_t CefBrowserViewImpl::GetRuntimeStyle() {
  CEF_REQUIRE_VALID_RETURN(CEF_RUNTIME_STYLE_DEFAULT);
  return IsAlloyStyle() ? CEF_RUNTIME_STYLE_ALLOY : CEF_RUNTIME_STYLE_CHROME;
}

void CefBrowserViewImpl::RequestFocus() {
  CEF_REQUIRE_VALID_RETURN_VOID();
  // Always execute asynchronously to work around issue #3040.
  CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefBrowserViewImpl::RequestFocusSync,
                                        weak_ptr_factory_.GetWeakPtr()));
}

void CefBrowserViewImpl::SetBackgroundColor(cef_color_t color) {
  CEF_REQUIRE_VALID_RETURN_VOID();
  ParentClass::SetBackgroundColor(color);
  if (web_view()) {
    web_view()->SetResizeBackgroundColor(color);
  }
}

void CefBrowserViewImpl::Detach() {
  ParentClass::Detach();

  // root_view() will be nullptr now.
  DCHECK(!root_view());

  if (browser_) {
    // With Alloy style |browser_| will disappear when WindowDestroyed()
    // indirectly calls BrowserDestroyed() so keep a reference.
    CefRefPtr<CefBrowserHostBase> browser = browser_;

    // Force the browser to be destroyed.
    browser->WindowDestroyed();
  }
}

void CefBrowserViewImpl::GetDebugInfo(base::Value::Dict* info,
                                      bool include_children) {
  ParentClass::GetDebugInfo(info, include_children);
  if (browser_) {
    info->Set("url", browser_->GetMainFrame()->GetURL().ToString());
  }
}

void CefBrowserViewImpl::AddedToWidget() {
  DCHECK(!cef_widget_);

  views::Widget* widget = root_view()->GetWidget();
  DCHECK(widget);
  CefWidget* cef_widget = CefWidget::GetForWidget(widget);
  DCHECK(cef_widget);

  if (!browser_ && !is_alloy_style_) {
    if (cef_widget->IsAlloyStyle()) {
      LOG(ERROR) << "Cannot add Chrome style BrowserView to Alloy style Window";
      return;
    }

    if (cef_widget->IsChromeStyle() && cef_widget->GetThemeProfile()) {
      LOG(ERROR) << "Cannot add multiple Chrome style BrowserViews";
      return;
    }
  }

  if (!browser_ && pending_browser_create_params_) {
    // Top-level browsers will be created when this view is added to the views
    // hierarchy.
    pending_browser_create_params_->browser_view = this;

    CefBrowserHostBase::Create(*pending_browser_create_params_);
    DCHECK(browser_);

    pending_browser_create_params_.reset(nullptr);
  }

  cef_widget_ = cef_widget;
  profile_ = Profile::FromBrowserContext(browser_->GetBrowserContext());
  DCHECK(profile_);

  // May call Widget::ThemeChanged().
  cef_widget_->AddAssociatedProfile(profile_);
}

void CefBrowserViewImpl::RemovedFromWidget() {
  // With Chrome style this may be called after BrowserDestroyed(), in which
  // case the following call will be a no-op.
  DisassociateFromWidget();
}

void CefBrowserViewImpl::OnBoundsChanged() {
  if (!on_bounds_changed_.is_null()) {
    on_bounds_changed_.Run();
  }
}

bool CefBrowserViewImpl::OnGestureEvent(ui::GestureEvent* event) {
  if (auto command = GetGestureCommand(event)) {
    if (delegate() && delegate()->OnGestureCommand(this, *command)) {
      return true;
    }

    if (is_alloy_style_ && browser_) {
      // Default handling for Alloy style.
      switch (*command) {
        case CEF_GESTURE_COMMAND_BACK:
          browser_->GoBack();
          break;
        case CEF_GESTURE_COMMAND_FORWARD:
          browser_->GoForward();
          break;
      }
      return true;
    }
  }

  return false;
}

CefBrowserViewImpl::CefBrowserViewImpl(
    CefRefPtr<CefBrowserViewDelegate> delegate,
    bool is_devtools_popup,
    std::optional<cef_runtime_style_t> opener_runtime_style)
    : ParentClass(delegate),
      is_alloy_style_(ComputeAlloyStyle(delegate.get(),
                                        is_devtools_popup,
                                        opener_runtime_style)),
      weak_ptr_factory_(this) {}

void CefBrowserViewImpl::SetPendingBrowserCreateParams(
    const CefWindowInfo& window_info,
    CefRefPtr<CefClient> client,
    const CefString& url,
    const CefBrowserSettings& settings,
    CefRefPtr<CefDictionaryValue> extra_info,
    CefRefPtr<CefRequestContext> request_context) {
  DCHECK(!pending_browser_create_params_);
  pending_browser_create_params_ = std::make_unique<CefBrowserCreateParams>();
  pending_browser_create_params_->MaybeSetWindowInfo(
      window_info, /*allow_alloy_style=*/true, /*allow_chrome_style=*/true);
  pending_browser_create_params_->client = client;
  pending_browser_create_params_->url = url;
  pending_browser_create_params_->settings = settings;
  pending_browser_create_params_->extra_info = extra_info;
  pending_browser_create_params_->request_context = request_context;
}

void CefBrowserViewImpl::SetDefaults(const CefBrowserSettings& settings) {
  SetBackgroundColor(
      CefContext::Get()->GetBackgroundColor(&settings, STATE_DISABLED));
}

views::View* CefBrowserViewImpl::CreateRootView() {
  if (!is_alloy_style_) {
    return new ChromeBrowserView(this);
  }

  return new CefBrowserViewView(delegate(), this);
}

void CefBrowserViewImpl::InitializeRootView() {
  if (!is_alloy_style_) {
    chrome_browser_view()->Initialize();
  } else {
    static_cast<CefBrowserViewView*>(root_view())->Initialize();
  }
}

views::WebView* CefBrowserViewImpl::web_view() const {
  if (!root_view()) {
    return nullptr;
  }

  if (!is_alloy_style_) {
    return chrome_browser_view()->contents_web_view();
  }

  return static_cast<CefBrowserViewView*>(root_view());
}

ChromeBrowserView* CefBrowserViewImpl::chrome_browser_view() const {
  CHECK(!is_alloy_style_);
  return static_cast<ChromeBrowserView*>(root_view());
}

CefWindowImpl* CefBrowserViewImpl::cef_window_impl() const {
  // Same implementation as GetWindow().
  if (!root_view()) {
    return nullptr;
  }
  CefRefPtr<CefWindow> window =
      view_util::GetWindowFor(root_view()->GetWidget());
  return static_cast<CefWindowImpl*>(window.get());
}

bool CefBrowserViewImpl::HandleAccelerator(
    const input::NativeWebKeyboardEvent& event,
    views::FocusManager* focus_manager) {
  // Previous calls to TranslateMessage can generate Char events as well as
  // RawKeyDown events, even if the latter triggered an accelerator.  In these
  // cases, we discard the Char events.
  if (event.GetType() == blink::WebInputEvent::Type::kChar &&
      ignore_next_char_event_) {
    ignore_next_char_event_ = false;
    return true;
  }

  // It's necessary to reset this flag, because a RawKeyDown event may not
  // always generate a Char event.
  ignore_next_char_event_ = false;

  if (event.GetType() == blink::WebInputEvent::Type::kRawKeyDown) {
    ui::Accelerator accelerator =
        ui::GetAcceleratorFromNativeWebKeyboardEvent(event);

    // This is tricky: we want to set ignore_next_char_event_ if
    // ProcessAccelerator returns true. But ProcessAccelerator might delete
    // |this| if the accelerator is a "close tab" one. So we speculatively
    // set the flag and fix it if no event was handled.
    ignore_next_char_event_ = true;

    if (focus_manager->ProcessAccelerator(accelerator)) {
      return true;
    }

    // ProcessAccelerator didn't handle the accelerator, so we know both
    // that |this| is still valid, and that we didn't want to set the flag.
    ignore_next_char_event_ = false;
  }

  return false;
}

void CefBrowserViewImpl::DisassociateFromWidget() {
  if (!cef_widget_) {
    return;
  }

  // May call Widget::ThemeChanged().
  cef_widget_->RemoveAssociatedProfile(profile_);
  cef_widget_ = nullptr;
  profile_ = nullptr;
}
