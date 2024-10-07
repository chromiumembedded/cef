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

  SanityCheckWindowConfig(is_devtools, use_views, use_alloy_style, with_osr);

  if (!temp_window_ && !use_views) {
    // TempWindow must be created on the UI thread. It is only used with
    // native (non-Views) parent windows.
    temp_window_.reset(new TempWindow());
  }

  MainContext::Get()->PopulateBrowserSettings(&settings);

  scoped_refptr<RootWindow> root_window =
      RootWindow::Create(use_views, use_alloy_style);
  root_window->InitAsPopup(this, with_controls, with_osr, popupFeatures,
                           windowInfo, client, settings);

  // Store a reference to the root window on the main thread.
  OnRootWindowCreated(root_window);

  return root_window;
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

  if (terminate_when_all_windows_closed_ && root_windows_.empty()) {
    // All windows have closed. Clean up on the UI thread.
    CefPostTask(TID_UI, base::BindOnce(&RootWindowManager::CleanupOnUIThread,
                                       base::Unretained(this)));
  }
}

void RootWindowManager::OnRootWindowActivated(RootWindow* root_window) {
  REQUIRE_MAIN_THREAD();

  if (root_window == active_root_window_) {
    return;
  }

  active_root_window_ = root_window;
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
