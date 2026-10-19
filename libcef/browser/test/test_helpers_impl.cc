// Copyright (c) 2022 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <memory>

#include "base/auto_reset.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "cef/include/test/cef_test_helpers.h"
#include "cef/libcef/browser/browser_host_base.h"
#include "cef/libcef/browser/chrome/chrome_browser_host_impl.h"
#include "cef/libcef/browser/thread_util.h"
#include "cef/libcef/browser/views/view_util.h"
#include "cef/libcef/common/api_version_util.h"
#include "chrome/browser/password_manager/factories/profile_password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/password_store/smart_bubble_stats_store.h"
#include "net/base/features.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/features.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_MAC)
#include "ui/views/test/event_generator_delegate_mac.h"
#else
#include "ui/aura/test/default_event_generator_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#endif

#if !BUILDFLAG(IS_MAC)
namespace {

class CefEventGeneratorDelegateAura
    : public aura::test::DefaultEventGeneratorDelegate {
 public:
  explicit CefEventGeneratorDelegateAura(gfx::NativeWindow root_window)
      : DefaultEventGeneratorDelegate(root_window) {}

  void ConvertPointToTarget(const ui::EventTarget* target,
                            gfx::Point* point) const override {
    DefaultEventGeneratorDelegate::ConvertPointToTarget(target, point);
    // EventGenerator supplies screen DIPs, but WindowEventDispatcher expects
    // host pixels and applies the inverse root transform before dispatch.
    static_cast<const aura::Window*>(target)->GetHost()->ConvertDIPToPixels(
        point);
  }
};

}  // namespace
#endif  // !BUILDFLAG(IS_MAC)

bool CefSendMouseClickForTesting(CefRefPtr<CefWindow> owner,
                                 int target_id,
                                 bool is_menu,
                                 uint32_t modifiers) {
  CEF_API_REQUIRE_ADDED(CEF_NEXT);
  CEF_REQUIRE_UIT_RETURN(false);

  const auto fail = [target_id, is_menu, modifiers](const char* reason) {
    LOG(WARNING) << "CefSendMouseClickForTesting: " << reason
                 << " (target_id=" << target_id << ", is_menu=" << is_menu
                 << ", modifiers=" << modifiers << ")";
    return false;
  };

  // The Mac delegate and the event clock are process-wide test state.
  static bool dispatching = false;
  if (dispatching) {
    return fail("Another click is being dispatched");
  }
  base::AutoReset<bool> dispatching_reset(&dispatching, true);

  constexpr uint32_t kModifiers = EVENTFLAG_SHIFT_DOWN |
                                  EVENTFLAG_CONTROL_DOWN | EVENTFLAG_ALT_DOWN |
                                  EVENTFLAG_COMMAND_DOWN;
  if (!owner || !owner->IsValid() || owner->IsClosed()) {
    return fail("Owner is missing, invalid or closed");
  }
  if (modifiers & ~kModifiers) {
    return fail("Unsupported modifiers");
  }
  auto* owner_view = view_util::GetFor(owner);
  auto* owner_widget = owner_view ? owner_view->GetWidget() : nullptr;
  if (!owner_widget || !owner_widget->IsVisible()) {
    return fail("Owner widget is missing or hidden");
  }

  views::View* target = nullptr;
  if (is_menu) {
    auto* controller = views::MenuController::GetForOwnerWidget(owner_widget);
    auto* selected = controller ? controller->GetSelectedMenuItem() : nullptr;
    if (!selected) {
      return fail("Owner has no open Views menu with a selected item");
    }
    target = selected->GetRootMenuItem()->GetMenuItemByID(target_id);
  } else {
    target = owner_view->GetViewByID(target_id);
  }
  if (!target) {
    return fail("Target was not found");
  }
  if (!target->IsDrawn()) {
    return fail("Target is not drawn");
  }
  if (!target->GetEnabledInViewsSubtree()) {
    return fail("Target or an ancestor is disabled");
  }
  if (target->GetVisibleBounds().IsEmpty()) {
    return fail("Target has empty visible bounds");
  }
  auto* widget = target->GetWidget();
  if (!widget || !widget->IsVisible()) {
    return fail("Target widget is missing or hidden");
  }
  gfx::Point point = target->GetVisibleBounds().CenterPoint();
  views::View::ConvertPointToScreen(target, &point);

  // Each call targets the actual host, including menu and custom popup hosts.
  // Do not infer the host from screen coordinates or the active window.
  auto native_window = widget->GetNativeWindow();
  if (!native_window) {
    return fail("Target widget has no native window");
  }
#if BUILDFLAG(IS_MAC)
  ui::test::EventGeneratorDelegate::SetFactoryFunction(
      base::BindRepeating(&views::test::CreateEventGeneratorDelegateMac));
  ui::test::EventGenerator generator(native_window);
#else
  ui::test::EventGenerator generator(
      std::make_unique<CefEventGeneratorDelegateAura>(
          native_window->GetRootWindow()));
#endif
  int flags = ui::EF_NONE;
  if (modifiers & EVENTFLAG_SHIFT_DOWN) {
    flags |= ui::EF_SHIFT_DOWN;
  }
  if (modifiers & EVENTFLAG_CONTROL_DOWN) {
    flags |= ui::EF_CONTROL_DOWN;
  }
  if (modifiers & EVENTFLAG_ALT_DOWN) {
    flags |= ui::EF_ALT_DOWN;
  }
  if (modifiers & EVENTFLAG_COMMAND_DOWN) {
    flags |= ui::EF_COMMAND_DOWN;
  }
  generator.set_flags(flags);
  views::ViewTracker tracker(target);
  auto widget_weak = widget->GetWeakPtr();
  generator.MoveMouseTo(point);
  if (!tracker.view() || !widget_weak) {
    return fail("Target or its widget was destroyed while moving the mouse");
  }
  generator.PressLeftButton();
  // A press can synchronously close the target's widget.
  if (widget_weak) {
    generator.ReleaseLeftButton();
  }
  return true;
}

void CefSetDataDirectoryForTests(const CefString& dir) {
  base::PathService::OverrideAndCreateIfNeeded(
      base::DIR_SRC_TEST_DATA_ROOT, base::FilePath(dir), /*is_absolute=*/true,
      /*create=*/false);
}

bool CefIsFeatureEnabledForTests(const CefString& feature_name) {
  // Only includes values that are queried by unit tests.
  const base::Feature* features[] = {
      &net::features::kIgnoreHSTSForLocalhost,
      &network::features::kReduceAcceptLanguage,
      &blink::features::kCompositionForegroundMarkers,
  };

  const std::string& name = feature_name;
  for (auto* feature : features) {
    if (feature->name == name) {
      return base::FeatureList::IsEnabled(*feature);
    }
  }

  LOG(FATAL) << "Feature " << name << " is not supported";
}

int CefGetChromeBrowserOwnedWidgetCountForTests(CefRefPtr<CefBrowser> browser) {
  CEF_API_REQUIRE_ADDED(CEF_EXPERIMENTAL);
  CEF_REQUIRE_UIT_RETURN(-1);

  auto browser_base = CefBrowserHostBase::FromBrowser(browser);
  if (!browser_base || browser_base->IsAlloyStyle()) {
    return -1;
  }

  auto chrome_browser = ChromeBrowserHostImpl::FromBaseChecked(browser_base);
  auto* browser_view =
      BrowserView::GetBrowserViewForBrowser(chrome_browser->browser());
  if (!browser_view) {
    return -1;
  }

  auto* browser_widget = browser_view->GetWidget();
  if (!browser_widget) {
    return -1;
  }

  int count = 0;
  views::Widget::ForEachOwnedWidget(
      browser_widget->GetNativeView(),
      [browser_widget, &count](views::Widget* widget) {
        if (widget != browser_widget) {
          ++count;
        }
      });
  return count;
}

void CefClearChromeBrowserPasswordDismissalStatsForTests(
    CefRefPtr<CefBrowser> browser,
    const CefString& origin,
    CefRefPtr<CefCompletionCallback> callback) {
  CEF_API_REQUIRE_ADDED(CEF_EXPERIMENTAL);
  CEF_REQUIRE_UIT();

  auto complete = base::BindOnce(
      [](CefRefPtr<CefCompletionCallback> callback) {
        if (callback) {
          callback->OnComplete();
        }
      },
      callback);

  auto browser_base = CefBrowserHostBase::FromBrowser(browser);
  if (!browser_base || browser_base->IsAlloyStyle()) {
    std::move(complete).Run();
    return;
  }

  auto chrome_browser = ChromeBrowserHostImpl::FromBaseChecked(browser_base);
  auto password_store = ProfilePasswordStoreFactory::GetForProfile(
      Profile::FromBrowserContext(chrome_browser->GetBrowserContext()),
      ServiceAccessType::IMPLICIT_ACCESS);
  auto* stats_store =
      password_store ? password_store->GetSmartBubbleStatsStore() : nullptr;
  const auto expected_origin = url::Origin::Create(GURL(origin.ToString()));
  if (!stats_store || expected_origin.opaque()) {
    std::move(complete).Run();
    return;
  }

  stats_store->RemoveStatisticsByOriginAndTime(
      base::BindRepeating(
          [](const url::Origin& expected_origin, const GURL& url) {
            return url::Origin::Create(url) == expected_origin;
          },
          expected_origin),
      base::Time(), base::Time::Max(), std::move(complete));
}
