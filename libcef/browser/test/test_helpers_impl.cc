// Copyright (c) 2022 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "cef/include/test/cef_test_helpers.h"
#include "cef/libcef/browser/browser_host_base.h"
#include "cef/libcef/browser/chrome/chrome_browser_host_impl.h"
#include "cef/libcef/browser/thread_util.h"
#include "cef/libcef/common/api_version_util.h"
#include "chrome/browser/password_manager/factories/profile_password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/password_store/smart_bubble_stats_store.h"
#include "net/base/features.h"
#include "services/network/public/cpp/features.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"
#include "url/origin.h"

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
