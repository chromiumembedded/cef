// Copyright 2016 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/views/browser_view_impl.h"

#include <memory>

#include "libcef/browser/browser_host_base.h"
#include "libcef/browser/browser_util.h"
#include "libcef/browser/chrome/views/chrome_browser_view.h"
#include "libcef/browser/context.h"
#include "libcef/browser/request_context_impl.h"
#include "libcef/browser/thread_util.h"
#include "libcef/browser/views/window_impl.h"

#include "content/public/common/input/native_web_keyboard_event.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/content_accelerators/accelerator_util.h"

namespace {

absl::optional<cef_gesture_command_t> GetGestureCommand(
    ui::GestureEvent* event) {
#if BUILDFLAG(IS_MAC)
  if (event->details().type() == ui::ET_GESTURE_SWIPE) {
    if (event->details().swipe_left()) {
      return CEF_GESTURE_COMMAND_BACK;
    } else if (event->details().swipe_right()) {
      return CEF_GESTURE_COMMAND_FORWARD;
    }
  }
#endif
  return absl::nullopt;
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

  CefBrowserHostBase* browser_impl =
      static_cast<CefBrowserHostBase*>(browser.get());
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

  CefRefPtr<CefBrowserViewImpl> browser_view = new CefBrowserViewImpl(delegate);
  browser_view->SetPendingBrowserCreateParams(
      window_info, client, url, settings, extra_info, request_context);
  browser_view->Initialize();
  browser_view->SetDefaults(settings);
  return browser_view;
}

// static
CefRefPtr<CefBrowserViewImpl> CefBrowserViewImpl::CreateForPopup(
    const CefBrowserSettings& settings,
    CefRefPtr<CefBrowserViewDelegate> delegate) {
  CEF_REQUIRE_UIT_RETURN(nullptr);

  CefRefPtr<CefBrowserViewImpl> browser_view = new CefBrowserViewImpl(delegate);
  browser_view->Initialize();
  browser_view->SetDefaults(settings);
  return browser_view;
}

void CefBrowserViewImpl::WebContentsCreated(
    content::WebContents* web_contents) {
  if (web_view()) {
    web_view()->SetWebContents(web_contents);
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

  if (web_view()) {
    web_view()->SetWebContents(nullptr);
  }
}

bool CefBrowserViewImpl::HandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
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
    if (browser_util::GetCefKeyEvent(event, cef_event) &&
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
  if (cef::IsChromeRuntimeEnabled()) {
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

void CefBrowserViewImpl::RequestFocus() {
  CEF_REQUIRE_VALID_RETURN_VOID();
  // Always execute asynchronously to work around issue #3040.
  CEF_POST_TASK(CEF_UIT,
                base::BindOnce(&CefBrowserViewImpl::RequestFocusInternal,
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
    // With the Alloy runtime |browser_| will disappear when WindowDestroyed()
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

void CefBrowserViewImpl::OnBrowserViewAdded() {
  if (!browser_ && pending_browser_create_params_) {
    // Top-level browsers will be created when this view is added to the views
    // hierarchy.
    pending_browser_create_params_->browser_view = this;

    CefBrowserHostBase::Create(*pending_browser_create_params_);
    DCHECK(browser_);

    pending_browser_create_params_.reset(nullptr);
  }
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

    if (!cef::IsChromeRuntimeEnabled() && browser_) {
      // Default handling for the Alloy runtime.
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
    CefRefPtr<CefBrowserViewDelegate> delegate)
    : ParentClass(delegate), weak_ptr_factory_(this) {}

void CefBrowserViewImpl::SetPendingBrowserCreateParams(
    const CefWindowInfo& window_info,
    CefRefPtr<CefClient> client,
    const CefString& url,
    const CefBrowserSettings& settings,
    CefRefPtr<CefDictionaryValue> extra_info,
    CefRefPtr<CefRequestContext> request_context) {
  DCHECK(!pending_browser_create_params_);
  pending_browser_create_params_ = std::make_unique<CefBrowserCreateParams>();
  pending_browser_create_params_->MaybeSetWindowInfo(window_info);
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
  if (cef::IsChromeRuntimeEnabled()) {
    return new ChromeBrowserView(this);
  }

  return new CefBrowserViewView(delegate(), this);
}

void CefBrowserViewImpl::InitializeRootView() {
  if (cef::IsChromeRuntimeEnabled()) {
    chrome_browser_view()->Initialize();
  } else {
    static_cast<CefBrowserViewView*>(root_view())->Initialize();
  }
}

views::WebView* CefBrowserViewImpl::web_view() const {
  if (!root_view()) {
    return nullptr;
  }

  if (cef::IsChromeRuntimeEnabled()) {
    return chrome_browser_view()->contents_web_view();
  }

  return static_cast<CefBrowserViewView*>(root_view());
}

ChromeBrowserView* CefBrowserViewImpl::chrome_browser_view() const {
  CHECK(cef::IsChromeRuntimeEnabled());
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
    const content::NativeWebKeyboardEvent& event,
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

void CefBrowserViewImpl::RequestFocusInternal() {
  ParentClass::RequestFocus();
}
