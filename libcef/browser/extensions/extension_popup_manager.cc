// Copyright 2025 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef/browser/extensions/extension_popup_manager.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "cef/include/cef_browser.h"
#include "cef/libcef/browser/browser_context.h"
#include "cef/libcef/browser/browser_host_base.h"
#include "cef/libcef/browser/chrome/chrome_browser_host_impl.h"
#include "cef/libcef/browser/chrome/extensions/chrome_extension_util.h"
#include "cef/libcef/browser/chrome/views/chrome_browser_view.h"
#include "cef/libcef/browser/extensions/extension_action_impl.h"
#include "cef/libcef/browser/request_context_impl.h"
#include "cef/libcef/browser/thread_util.h"
#include "chrome/browser/extensions/extension_action_runner.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/extensions/extension_view_host_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/extensions/extension_popup_types.h"
#include "chrome/browser/ui/views/extensions/extension_popup.h"
#include "chrome/browser/ui/views/extensions/extension_view_views.h"
#include "components/input/native_web_keyboard_event.h"
#include "content/public/browser/eye_dropper.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/mojom/view_type.mojom.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/bubble/bubble_anchor.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "url/gurl.h"

namespace cef {

namespace {

// A Browser-free ExtensionViewHost::Delegate for Alloy-style source tabs.
//
// Chrome's own popup hosting routes these hooks through a chrome Browser (see
// ExtensionViewHostBrowserDelegate in extension_view_host_factory.cc), but an
// Alloy style browser has no Browser. The popup's web hosting, messaging and
// chrome.* APIs are driven by the ExtensionViewHost / BrowserContext and do not
// need this delegate; only a few peripheral capabilities route through here, so
// we no-op them exactly like Chrome's Android delegate does.
class AlloyPopupViewHostDelegate
    : public extensions::ExtensionViewHost::Delegate {
 public:
  AlloyPopupViewHostDelegate() = default;

  AlloyPopupViewHostDelegate(const AlloyPopupViewHostDelegate&) = delete;
  AlloyPopupViewHostDelegate& operator=(const AlloyPopupViewHostDelegate&) =
      delete;

  ~AlloyPopupViewHostDelegate() override = default;

  // extensions::ExtensionViewHost::Delegate:
  content::WebContents* OpenURL(
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override {
    return nullptr;
  }

  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      content::WebContents* source,
      const input::NativeWebKeyboardEvent& event) override {
    return content::KeyboardEventProcessingResult::NOT_HANDLED;
  }

  std::unique_ptr<content::EyeDropper> OpenEyeDropper(
      content::RenderFrameHost* frame,
      content::EyeDropperListener* listener) override {
    return nullptr;
  }

  extensions::WindowController* GetExtensionWindowController() override {
    return nullptr;
  }
};

// A Browser-free extension action popup bubble for Alloy-style source tabs.
//
// This mirrors the essential parts of Chrome's ExtensionPopup but drops
// everything that requires a chrome Browser (tab strip observation, security
// dialog tracking, the browser-window anchor). It hosts Chrome's own
// ExtensionViewHost via ExtensionViewViews -- so we keep Chrome's web hosting,
// auto-resize and content focus -- inside a plain views bubble anchored to the
// caller's on-screen icon rect.
//
// Because BubbleDialogDelegateView's constructors are sealed behind a friend
// list ("DO NOT ADD TO THIS LIST"), this composes the non-View
// views::BubbleDialogDelegate (which has a public constructor) and hosts the
// ExtensionViewViews as its contents view. A supplied non-View delegate is not
// owned by its Widget, so the popup owns itself: it schedules its own deletion
// (after the Widget finishes tearing down) from WindowClosing().
class CefAlloyExtensionPopup : public views::BubbleDialogDelegate,
                               public ExtensionViewViews::Container,
                               public content::WebContentsObserver,
                               public extensions::ExtensionRegistryObserver {
 public:
  // Matches ExtensionPopup: the minimum is a little larger than a toolbar icon
  // and the maximum is smaller than most screens.
  static constexpr gfx::Size kMinSize = {25, 25};
  static constexpr gfx::Size kMaxSize = {800, 600};

  // Creates the bubble (initially hidden) and its Widget. The bubble is shown
  // once its contents finish loading. The returned popup owns itself and must
  // not be deleted by the caller; it is valid until its Widget closes. Returns
  // nullptr if the Widget could not be created.
  static CefAlloyExtensionPopup* Create(
      std::unique_ptr<extensions::ExtensionViewHost> host,
      gfx::NativeView parent,
      const gfx::Rect& anchor_screen_rect) {
    auto* popup =
        new CefAlloyExtensionPopup(std::move(host), parent, anchor_screen_rect);
    if (!views::BubbleDialogDelegate::CreateBubbleDeprecated(
            popup, views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET)) {
      delete popup;
      return nullptr;
    }

    // If the host had already finished loading (e.g. single-process mode), the
    // load-completed notification was missed; show now that the Widget exists.
    if (popup->host_->has_loaded_once()) {
      popup->Observe(nullptr);
      popup->ShowBubble();
    }
    return popup;
  }

  CefAlloyExtensionPopup(const CefAlloyExtensionPopup&) = delete;
  CefAlloyExtensionPopup& operator=(const CefAlloyExtensionPopup&) = delete;

  ~CefAlloyExtensionPopup() override = default;

  // ExtensionViewViews::Container:
  gfx::Size GetMinBounds() override { return kMinSize; }
  gfx::Size GetMaxBounds() override {
    gfx::Size max_size = kMaxSize;
    // This bubble anchors to an on-screen rect rather than an anchor View, so
    // Chrome's GetMaxAvailableScreenSpaceToPlaceBubble (which dereferences a
    // View*/TrackedElement anchor) can't be used. Clamp to the work area of the
    // display containing the anchor so the popup never exceeds the screen.
    if (display::Screen* screen = display::Screen::Get()) {
      const gfx::Rect anchor_rect = GetAnchorRect();
      const display::Display display =
          screen->GetDisplayNearestPoint(anchor_rect.CenterPoint());
      max_size.SetToMin(display.work_area().size());
    }
    max_size.SetToMax(kMinSize);
    return max_size;
  }

  // content::WebContentsObserver:
  void DocumentOnLoadCompletedInPrimaryMainFrame() override {
    // Show when the content finishes loading and its size is known, to avoid
    // UI flashing/resizing.
    Observe(nullptr);
    ShowBubble();
  }

  // extensions::ExtensionRegistryObserver:
  void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      extensions::UnloadedExtensionReason reason) override {
    if (host_ && extension->id() == host_->extension_id()) {
      CloseWidget();
    }
  }

  // views::WidgetDelegate:
  void WindowClosing() override {
    // The Widget is tearing down. Stop observing and schedule our own deletion
    // for after teardown completes -- the Widget still references us (e.g. via
    // DeleteDelegate) through the rest of its destruction, so we cannot delete
    // synchronously here.
    registry_observation_.Reset();
    Observe(nullptr);
    if (!delete_scheduled_) {
      delete_scheduled_ = true;
      base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                 this);
    }
  }

 private:
  CefAlloyExtensionPopup(std::unique_ptr<extensions::ExtensionViewHost> host,
                         gfx::NativeView parent,
                         const gfx::Rect& anchor_screen_rect)
      : views::BubbleDialogDelegate(/*anchor=*/nullptr,
                                    views::BubbleBorder::TOP_RIGHT,
                                    views::BubbleBorder::STANDARD_SHADOW,
                                    /*autosize=*/true),
        host_(std::move(host)) {
    SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
    set_use_round_corners(false);
    set_margins(gfx::Insets());
    // Dismiss the popup as soon as it loses activation (a click outside it),
    // matching how the demo expects an action popup to behave.
    set_close_on_deactivate(true);
    set_adjust_if_offscreen(views::PlatformStyle::kAdjustBubbleIfOffscreen);
    // The bubble has no anchor view, so give it a parent window to stack above
    // and anchor it to the caller's on-screen icon rect.
    set_parent_window(parent);
    SetAnchorRect(anchor_screen_rect);

    Profile* profile = Profile::FromBrowserContext(host_->browser_context());
    auto view = std::make_unique<ExtensionViewViews>(profile, host_.get());
    extension_view_ = view.get();
    SetContentsView(std::move(view));
    extension_view_->SetContainer(this);
    extension_view_->Init();

    // Handle the hosted page calling window.close().
    host_->SetCloseHandler(base::BindOnce(
        &CefAlloyExtensionPopup::OnHostCloseRequested, base::Unretained(this)));

    registry_observation_.Observe(
        extensions::ExtensionRegistry::Get(host_->browser_context()));

    // Wait to show until the contents finish loading (see Create() for the
    // already-loaded race).
    Observe(host_->host_contents());
  }

  void ShowBubble() {
    if (views::Widget* widget = GetWidget()) {
      widget->Show();
    }
    if (host_ && host_->host_contents()) {
      host_->host_contents()->Focus();
    }
  }

  void CloseWidget() {
    if (views::Widget* widget = GetWidget()) {
      widget->Close();
    }
  }

  void OnHostCloseRequested(extensions::ExtensionHost* host) { CloseWidget(); }

  // Owns the hosted extension page. Destroyed when |this| is deleted, which is
  // after the Widget (and our contents ExtensionViewViews) have torn down.
  std::unique_ptr<extensions::ExtensionViewHost> host_;

  raw_ptr<ExtensionViewViews> extension_view_ = nullptr;

  base::ScopedObservation<extensions::ExtensionRegistry,
                          extensions::ExtensionRegistryObserver>
      registry_observation_{this};

  bool delete_scheduled_ = false;
};

}  // namespace

// Observes the Widget of a single extension popup bubble for bookkeeping. The
// bubble self-manages (it closes on deactivation, tab change, Escape, or
// window.close(), and a Chrome ExtensionPopup is owned by its Widget while a
// CefAlloyExtensionPopup owns itself); this lets us drop our entry when it
// closes.
class CefExtensionPopupManager::PopupTracker : public views::WidgetObserver {
 public:
  PopupTracker(CefExtensionPopupManager* manager,
               std::string extension_id,
               views::Widget* widget)
      : manager_(manager),
        extension_id_(std::move(extension_id)),
        widget_(widget) {
    widget_->AddObserver(this);
  }

  PopupTracker(const PopupTracker&) = delete;
  PopupTracker& operator=(const PopupTracker&) = delete;

  ~PopupTracker() override {
    if (widget_) {
      widget_->RemoveObserver(this);
    }
  }

  // Closes the popup without running the close path (RemovePopup). Used when
  // replacing one popup with another for the same extension, and during
  // shutdown.
  void CloseSilently() {
    notify_on_destroy_ = false;
    if (widget_) {
      widget_->RemoveObserver(this);
      widget_->Close();
      widget_ = nullptr;
    }
  }

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override {
    widget_->RemoveObserver(this);
    widget_ = nullptr;
    if (!notify_on_destroy_) {
      return;
    }
    // RemovePopup() deletes |this|, so capture what we still need first and
    // touch no members afterward.
    CefExtensionPopupManager* manager = manager_;
    const std::string extension_id = extension_id_;
    manager->RemovePopup(extension_id);
  }

 private:
  const raw_ptr<CefExtensionPopupManager> manager_;
  const std::string extension_id_;
  raw_ptr<views::Widget> widget_;
  bool notify_on_destroy_ = true;
};

CefExtensionPopupManager::CefExtensionPopupManager(
    CefRequestContextImpl* request_context)
    : request_context_(request_context) {}

CefExtensionPopupManager::~CefExtensionPopupManager() {
  // Move the trackers out first so any synchronous teardown can't mutate the
  // container we're iterating, then close each popup without notifications.
  auto popups = std::move(popups_);
  popups_.clear();
  for (auto& entry : popups) {
    entry.second->CloseSilently();
  }
}

void CefExtensionPopupManager::ShowPopup(const CefString& extension_id,
                                         CefRefPtr<CefBrowser> source_browser,
                                         const CefRect& anchor_screen_rect) {
  CEF_REQUIRE_UIT();
  const std::string id = extension_id.ToString();

  // The extension's action, popup URL and click dispatch are all scoped to the
  // tab's BrowserContext/WebContents and work for both runtime styles -- an
  // Alloy style tab participates fully in Chrome's extension system. Only the
  // popup's window hosting differs (Chrome anchors to its Browser window; Alloy
  // has no Browser), which is the branch at the bottom of this method.
  auto host_base = CefBrowserHostBase::FromBrowser(source_browser);
  content::WebContents* source_wc =
      host_base ? host_base->GetWebContents() : nullptr;
  if (!source_wc) {
    return;
  }

  content::BrowserContext* browser_context = source_wc->GetBrowserContext();
  auto* registry = extensions::ExtensionRegistry::Get(browser_context);
  const extensions::Extension* extension =
      registry ? registry->GetExtensionById(
                     id, extensions::ExtensionRegistry::ENABLED)
               : nullptr;
  if (!extension) {
    return;
  }

  const int tab_id = extensions::ExtensionTabUtil::GetTabId(source_wc);
  const GURL popup_url =
      GetExtensionActionPopupUrl(browser_context, *extension, tab_id);

  if (popup_url.is_empty()) {
    // The action declares no popup: dispatch the click instead, mirroring a
    // toolbar icon press for a no-popup action.
    if (auto* runner =
            extensions::ExtensionActionRunner::GetForWebContents(source_wc)) {
      runner->RunAction(extension, /*grant_tab_permissions=*/true);
    }
    return;
  }

  // At most one popup per extension; silently replace any existing one so its
  // async close doesn't later erase the new entry.
  if (auto it = popups_.find(id); it != popups_.end()) {
    it->second->CloseSilently();
    popups_.erase(it);
  }

  const gfx::Rect anchor_rect(anchor_screen_rect.x, anchor_screen_rect.y,
                              anchor_screen_rect.width,
                              anchor_screen_rect.height);

  views::Widget* widget = nullptr;

  if (host_base->IsAlloyStyle()) {
    // Alloy style: there is no chrome Browser to host Chrome's ExtensionPopup,
    // so construct the ExtensionViewHost directly (with a Browser-free
    // delegate) and host it in our own bubble anchored to the icon rect.
    auto popup_host = std::make_unique<extensions::ExtensionViewHost>(
        extension, browser_context, popup_url,
        extensions::mojom::ViewType::kExtensionPopup,
        std::make_unique<AlloyPopupViewHostDelegate>());

    // An Alloy browser has no BrowserWindowInterface, so its tab is invisible
    // to the popup's chrome.tabs.query. Mark the popup's host WebContents with
    // the source tab id so MaybeAppendAlloyPopupQueryTab (patched into
    // tabs_api.cc) can surface this tab to the popup. The marker lives as long
    // as the popup.
    cef::SetAlloyPopupSourceTabId(popup_host->host_contents(), tab_id);

    CefAlloyExtensionPopup* popup = CefAlloyExtensionPopup::Create(
        std::move(popup_host), source_wc->GetNativeView(), anchor_rect);
    widget = popup ? popup->GetWidget() : nullptr;
  } else {
    // Chrome style: use Chrome's own extension action bubble so it matches
    // Chrome exactly (border shadow, auto-size, dismiss-on-deactivate).
    auto chrome_host = ChromeBrowserHostImpl::FromBaseChecked(host_base);
    Browser* browser = chrome_host->browser();
    ChromeBrowserView* browser_view = chrome_host->chrome_browser_view();
    if (!browser || !browser_view) {
      return;
    }

    auto popup_host = extensions::ExtensionViewHostFactory::CreatePopupHost(
        *extension, popup_url, browser);
    if (!popup_host) {
      return;
    }

    // ExtensionPopup::ShowPopup() is now async and returns void; the popup
    // self-manages (owned by its Widget). Tracking would require observing the
    // ExtensionHostRegistry to clean up popups_; skip for the demo and let the
    // popup self-dismiss via its own Widget lifecycle.
    //
    // Chrome's ExtensionPopup forces set_close_on_deactivate(false) in its
    // constructor and relies on OnWidgetTreeActivated to close when another
    // widget in the anchor tree gets focus. In the cefclient demo the anchor
    // is the outer browser view, but the focused tab is a separate child
    // browser/widget tree, so activations there don't reach the popup -- the
    // popup only closes if you click back on the original tab. Flip close-on-
    // deactivate to true once the popup is shown so any click outside it
    // (including on the other child tab) dismisses it.
    auto enable_close_on_deactivate =
        [](extensions::ExtensionHost* popup_host) {
          if (!popup_host) {
            return;
          }
          auto* view_host =
              static_cast<extensions::ExtensionViewHost*>(popup_host);
          auto* view = static_cast<ExtensionViewViews*>(view_host->view());
          if (!view) {
            return;
          }
          views::Widget* widget = view->GetWidget();
          if (!widget) {
            return;
          }
          auto* bubble = static_cast<views::BubbleDialogDelegate*>(
              widget->widget_delegate());
          bubble->set_close_on_deactivate(true);
        };
    views::View* anchor_view = browser_view;
    ExtensionPopup::ShowPopup(
        browser, std::move(popup_host), views::BubbleAnchor(anchor_view),
        views::BubbleBorder::TOP_RIGHT, PopupShowAction::kShow,
        base::BindOnce(enable_close_on_deactivate));
    return;
  }

  if (!widget) {
    return;
  }

  popups_[id] = std::make_unique<PopupTracker>(this, id, widget);
}

void CefExtensionPopupManager::ClosePopup(const CefString& extension_id) {
  CEF_REQUIRE_UIT();
  auto it = popups_.find(extension_id.ToString());
  if (it == popups_.end()) {
    return;
  }
  it->second->CloseSilently();
  popups_.erase(it);
}

content::BrowserContext* CefExtensionPopupManager::GetBrowserContext() const {
  CefBrowserContext* context = request_context_->GetBrowserContext();
  return context ? context->AsBrowserContext() : nullptr;
}

void CefExtensionPopupManager::RemovePopup(const std::string& extension_id) {
  popups_.erase(extension_id);
}

}  // namespace cef
