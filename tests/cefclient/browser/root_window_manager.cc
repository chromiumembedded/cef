// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/root_window_manager.h"

#include <sstream>

#include "include/base/cef_callback.h"
#include "include/base/cef_logging.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_helpers.h"
#include "tests/cefclient/browser/default_client_handler.h"
#include "tests/cefclient/browser/main_context.h"
#include "tests/cefclient/browser/test_runner.h"
#include "tests/shared/browser/file_util.h"
#include "tests/shared/browser/resource_util.h"
#include "tests/shared/common/client_switches.h"

namespace client {

namespace {

class ClientRequestContextHandler : public CefRequestContextHandler {
 public:
  using CreateCallback = RootWindow::Delegate::RequestContextCallback;

  explicit ClientRequestContextHandler(CreateCallback callback)
      : create_callback_(std::move(callback)) {}

  // CefRequestContextHandler methods:
  void OnRequestContextInitialized(
      CefRefPtr<CefRequestContext> request_context) override {
    CEF_REQUIRE_UI_THREAD();

    // Allow the startup URL to create popups that bypass the popup blocker.
    // For example, via Tests > New Popup from the top menu. This applies for
    // for Chrome style only.
    const auto& startup_url =
        MainContext::Get()->GetMainURL(/*command_line=*/nullptr);
    request_context->SetContentSetting(startup_url, startup_url,
                                       CEF_CONTENT_SETTING_TYPE_POPUPS,
                                       CEF_CONTENT_SETTING_VALUE_ALLOW);

    if (!create_callback_.is_null()) {
      // Execute the callback asynchronously.
      CefPostTask(TID_UI,
                  base::BindOnce(std::move(create_callback_), request_context));
    }
  }

 private:
  CreateCallback create_callback_;

  IMPLEMENT_REFCOUNTING(ClientRequestContextHandler);
  DISALLOW_COPY_AND_ASSIGN(ClientRequestContextHandler);
};

// Ensure a compatible set of window creation attributes.
void SanityCheckWindowConfig(const bool is_devtools,
                             const bool use_views,
                             bool& use_alloy_style,
                             bool& with_osr) {
  // This configuration is not supported by cefclient architecture and
  // should use default window creation instead.
  CHECK(!(is_devtools && !use_views));

  if (is_devtools && use_alloy_style) {
    LOG(WARNING) << "Alloy style is not supported with Chrome runtime DevTools;"
                    " using Chrome style.";
    use_alloy_style = false;
  }

  if (!use_alloy_style && with_osr) {
    LOG(WARNING) << "Windowless rendering is not supported with Chrome style;"
                    " using windowed rendering.";
    with_osr = false;
  }

  if (use_views && with_osr) {
    LOG(WARNING) << "Windowless rendering is not supported with Views;"
                    " using windowed rendering.";
    with_osr = false;
  }
}

}  // namespace

RootWindowManager::RootWindowManager(bool terminate_when_all_windows_closed)
    : terminate_when_all_windows_closed_(terminate_when_all_windows_closed) {
  CefRefPtr<CefCommandLine> command_line =
      CefCommandLine::GetGlobalCommandLine();
  DCHECK(command_line.get());
  request_context_per_browser_ =
      command_line->HasSwitch(switches::kRequestContextPerBrowser);
  request_context_shared_cache_ =
      command_line->HasSwitch(switches::kRequestContextSharedCache);
}

RootWindowManager::~RootWindowManager() {
  // All root windows should already have been destroyed.
  DCHECK(root_windows_.empty());
}

scoped_refptr<RootWindow> RootWindowManager::CreateRootWindow(
    std::unique_ptr<RootWindowConfig> config) {
  CefBrowserSettings settings;
  MainContext::Get()->PopulateBrowserSettings(&settings);

  SanityCheckWindowConfig(/*is_devtools=*/false, config->use_views,
                          config->use_alloy_style, config->with_osr);

  scoped_refptr<RootWindow> root_window =
      RootWindow::Create(config->use_views, config->use_alloy_style);
  root_window->Init(this, std::move(config), settings);

  // Store a reference to the root window on the main thread.
  OnRootWindowCreated(root_window);

  return root_window;
}

scoped_refptr<RootWindow> RootWindowManager::CreateRootWindowAsPopup(
    bool use_views,
    bool use_alloy_style,
    bool with_controls,
    bool with_osr,
    int opener_browser_id,
    int popup_id,
    bool is_devtools,
    const CefPopupFeatures& popupFeatures,
    CefWindowInfo& windowInfo,
    CefRefPtr<CefClient>& client,
    CefBrowserSettings& settings) {
  CEF_REQUIRE_UI_THREAD();

  if (MainContext::Get()->UseDefaultPopup() || (is_devtools && !use_views)) {
    // Use default window creation for the popup. A new |client| instance is
    // required by cefclient architecture if the type is not already
    // DefaultClientHandler.
    if (!DefaultClientHandler::GetForClient(client)) {
      client = new DefaultClientHandler(use_alloy_style);
    }
    return nullptr;
  }

  CHECK_GT(opener_browser_id, 0);
  CHECK(popup_id > 0 || is_devtools);

  SanityCheckWindowConfig(is_devtools, use_views, use_alloy_style, with_osr);

  if (!temp_window_ && !use_views) {
    // TempWindow must be created on the UI thread. It is only used with
    // native (non-Views) parent windows.
    temp_window_.reset(new TempWindow());
  }

  MainContext::Get()->PopulateBrowserSettings(&settings);

  scoped_refptr<RootWindow> root_window =
      RootWindow::Create(use_views, use_alloy_style);
  if (!is_devtools) {
    root_window->SetPopupId(opener_browser_id, popup_id);
  }
  root_window->InitAsPopup(this, with_controls, with_osr, popupFeatures,
                           windowInfo, client, settings);

  // Store a reference to the root window on the main thread.
  OnRootWindowCreated(root_window);

  return root_window;
}

void RootWindowManager::AbortOrClosePopup(int opener_browser_id, int popup_id) {
  CEF_REQUIRE_UI_THREAD();
  // Continue on the main thread.
  OnAbortOrClosePopup(opener_browser_id, popup_id);
}

scoped_refptr<RootWindow> RootWindowManager::GetWindowForBrowser(
    int browser_id) const {
  REQUIRE_MAIN_THREAD();

  for (auto root_window : root_windows_) {
    CefRefPtr<CefBrowser> browser = root_window->GetBrowser();
    if (browser.get() && browser->GetIdentifier() == browser_id) {
      return root_window;
    }
  }
  return nullptr;
}

scoped_refptr<RootWindow> RootWindowManager::GetActiveRootWindow() const {
  REQUIRE_MAIN_THREAD();
  return active_root_window_;
}

void RootWindowManager::CloseAllWindows(bool force) {
  if (!CURRENTLY_ON_MAIN_THREAD()) {
    // Execute this method on the main thread.
    MAIN_POST_CLOSURE(base::BindOnce(&RootWindowManager::CloseAllWindows,
                                     base::Unretained(this), force));
    return;
  }

  if (root_windows_.empty()) {
    return;
  }

  // Use a copy of |root_windows_| because the original set may be modified
  // in OnRootWindowDestroyed while iterating.
  RootWindowSet root_windows = root_windows_;

  for (auto root_window : root_windows) {
    root_window->Close(force);
  }
}

void RootWindowManager::OtherBrowserCreated(int browser_id,
                                            int opener_browser_id) {
  if (!CURRENTLY_ON_MAIN_THREAD()) {
    // Execute this method on the main thread.
    MAIN_POST_CLOSURE(base::BindOnce(&RootWindowManager::OtherBrowserCreated,
                                     base::Unretained(this), browser_id,
                                     opener_browser_id));
    return;
  }

  other_browser_ct_++;

  // Track ownership of popup browsers that don't have a RootWindow.
  if (opener_browser_id > 0) {
    other_browser_owners_[opener_browser_id].insert(browser_id);
  }
}

void RootWindowManager::OtherBrowserClosed(int browser_id,
                                           int opener_browser_id) {
  if (!CURRENTLY_ON_MAIN_THREAD()) {
    // Execute this method on the main thread.
    MAIN_POST_CLOSURE(base::BindOnce(&RootWindowManager::OtherBrowserClosed,
                                     base::Unretained(this), browser_id,
                                     opener_browser_id));
    return;
  }

  DCHECK_GT(other_browser_ct_, 0);
  other_browser_ct_--;

  // Track ownership of popup browsers that don't have a RootWindow.
  if (opener_browser_id > 0) {
    DCHECK(other_browser_owners_.find(opener_browser_id) !=
           other_browser_owners_.end());
    auto& child_set = other_browser_owners_[opener_browser_id];
    DCHECK(child_set.find(browser_id) != child_set.end());
    child_set.erase(browser_id);
    if (child_set.empty()) {
      other_browser_owners_.erase(opener_browser_id);
    }
  }

  MaybeCleanup();
}

void RootWindowManager::OnRootWindowCreated(
    scoped_refptr<RootWindow> root_window) {
  if (!CURRENTLY_ON_MAIN_THREAD()) {
    // Execute this method on the main thread.
    MAIN_POST_CLOSURE(base::BindOnce(&RootWindowManager::OnRootWindowCreated,
                                     base::Unretained(this), root_window));
    return;
  }

  root_windows_.insert(root_window);

  if (root_windows_.size() == 1U) {
    // The first root window should be considered the active window.
    OnRootWindowActivated(root_window.get());
  }
}

void RootWindowManager::OnAbortOrClosePopup(int opener_browser_id,
                                            int popup_id) {
  if (!CURRENTLY_ON_MAIN_THREAD()) {
    // Execute this method on the main thread.
    MAIN_POST_CLOSURE(base::BindOnce(&RootWindowManager::OnAbortOrClosePopup,
                                     base::Unretained(this), opener_browser_id,
                                     popup_id));
    return;
  }

  // Use a copy of |root_windows_| because the original set may be modified
  // in OnRootWindowDestroyed while iterating.
  RootWindowSet root_windows = root_windows_;

  // Close or destroy the associated RootWindow(s). This may be a specific popup
  // (|popup_id| > 0), or all popups if the opener is closing (|popup_id| < 0).
  for (auto root_window : root_windows) {
    if (root_window->IsPopupIdMatch(opener_browser_id, popup_id)) {
      const bool window_created = root_window->IsWindowCreated();
      LOG(INFO) << (window_created ? "Closing" : "Aborting") << " popup "
                << root_window->popup_id() << " of browser "
                << opener_browser_id;
      if (window_created) {
        // Close the window in the usual way. Will result in a call to
        // OnRootWindowDestroyed.
        root_window->Close(/*force=*/false);
      } else {
        // The window was not created, so destroy directly.
        OnRootWindowDestroyed(root_window.get());
      }
    }
  }

  // Close all other associated popups if the opener is closing. These popups
  // don't have a RootWindow (e.g. when running with `--use-default-popup`).
  if (popup_id < 0 && other_browser_owners_.find(opener_browser_id) !=
                          other_browser_owners_.end()) {
    // Use a copy as the original set may be modified in OtherBrowserClosed
    // while iterating.
    auto set = other_browser_owners_[opener_browser_id];
    for (auto browser_id : set) {
      if (auto browser = CefBrowserHost::GetBrowserByIdentifier(browser_id)) {
        LOG(INFO) << "Closing popup browser " << browser_id << " of browser "
                  << opener_browser_id;
        browser->GetHost()->CloseBrowser(/*force=*/false);
      }
    }
  }
}

CefRefPtr<CefRequestContext> RootWindowManager::GetRequestContext() {
  REQUIRE_MAIN_THREAD();
  return CreateRequestContext(RequestContextCallback());
}

void RootWindowManager::GetRequestContext(RequestContextCallback callback) {
  DCHECK(!callback.is_null());

  if (!CURRENTLY_ON_MAIN_THREAD()) {
    // Execute on the main thread.
    MAIN_POST_CLOSURE(base::BindOnce(
        base::IgnoreResult(&RootWindowManager::CreateRequestContext),
        base::Unretained(this), std::move(callback)));
  } else {
    CreateRequestContext(std::move(callback));
  }
}

CefRefPtr<CefRequestContext> RootWindowManager::CreateRequestContext(
    RequestContextCallback callback) {
  REQUIRE_MAIN_THREAD();

  if (request_context_per_browser_) {
    // Synchronous use of non-global request contexts is not safe.
    CHECK(!callback.is_null());

    // Create a new request context for each browser.
    CefRequestContextSettings settings;

    CefRefPtr<CefCommandLine> command_line =
        CefCommandLine::GetGlobalCommandLine();
    if (command_line->HasSwitch(switches::kCachePath)) {
      if (request_context_shared_cache_) {
        // Give each browser the same cache path. The resulting context objects
        // will share the same storage internally.
        CefString(&settings.cache_path) =
            command_line->GetSwitchValue(switches::kCachePath);
      } else {
        // Give each browser a unique cache path. This will create completely
        // isolated context objects.
        std::stringstream ss;
        ss << command_line->GetSwitchValue(switches::kCachePath).ToString()
           << file_util::kPathSep << time(nullptr);
        CefString(&settings.cache_path) = ss.str();
      }
    }

    return CefRequestContext::CreateContext(
        settings, new ClientRequestContextHandler(std::move(callback)));
  }

  // All browsers will share the global request context.
  if (!shared_request_context_) {
    shared_request_context_ = CefRequestContext::CreateContext(
        CefRequestContext::GetGlobalContext(),
        new ClientRequestContextHandler(std::move(callback)));
  } else if (!callback.is_null()) {
    // Execute the callback on the UI thread.
    CefPostTask(TID_UI,
                base::BindOnce(std::move(callback), shared_request_context_));
  }

  return shared_request_context_;
}

scoped_refptr<ImageCache> RootWindowManager::GetImageCache() {
  CEF_REQUIRE_UI_THREAD();

  if (!image_cache_) {
    image_cache_ = new ImageCache;
  }
  return image_cache_;
}

void RootWindowManager::OnTest(RootWindow* root_window, int test_id) {
  REQUIRE_MAIN_THREAD();

  test_runner::RunTest(root_window->GetBrowser(), test_id);
}

void RootWindowManager::OnExit(RootWindow* root_window) {
  REQUIRE_MAIN_THREAD();

  CloseAllWindows(false);
}

void RootWindowManager::OnRootWindowDestroyed(RootWindow* root_window) {
  REQUIRE_MAIN_THREAD();

  RootWindowSet::iterator it = root_windows_.find(root_window);
  DCHECK(it != root_windows_.end());
  if (it != root_windows_.end()) {
    root_windows_.erase(it);
  }

  if (root_window == active_root_window_) {
    active_root_window_ = nullptr;
  }

  MaybeCleanup();
}

void RootWindowManager::OnRootWindowActivated(RootWindow* root_window) {
  REQUIRE_MAIN_THREAD();

  if (root_window == active_root_window_) {
    return;
  }

  active_root_window_ = root_window;
}

void RootWindowManager::MaybeCleanup() {
  REQUIRE_MAIN_THREAD();
  if (terminate_when_all_windows_closed_ && root_windows_.empty() &&
      other_browser_ct_ == 0) {
    // All windows and browsers have closed. Clean up on the UI thread.
    CefPostTask(TID_UI, base::BindOnce(&RootWindowManager::CleanupOnUIThread,
                                       base::Unretained(this)));
  }
}

void RootWindowManager::CleanupOnUIThread() {
  CEF_REQUIRE_UI_THREAD();

  if (temp_window_) {
    // TempWindow must be destroyed on the UI thread.
    temp_window_.reset(nullptr);
  }

  if (image_cache_) {
    image_cache_ = nullptr;
  }

  // Quit the main message loop.
  MainMessageLoop::Get()->Quit();
}

}  // namespace client
