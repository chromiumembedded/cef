// Copyright (c) 2016 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/root_window_views.h"

#include <memory>

#include "include/base/cef_build.h"
#include "include/base/cef_callback.h"
#include "include/base/cef_cxx17_backports.h"
#include "include/cef_app.h"
#include "include/wrapper/cef_helpers.h"
#include "tests/cefclient/browser/client_handler_std.h"

namespace client {

namespace {

// Images that are loaded early and cached.
static const char* kDefaultImageCache[] = {"menu_icon", "window_icon"};

}  // namespace

RootWindowViews::RootWindowViews() = default;

RootWindowViews::~RootWindowViews() {
  REQUIRE_MAIN_THREAD();
}

void RootWindowViews::Init(RootWindow::Delegate* delegate,
                           std::unique_ptr<RootWindowConfig> config,
                           const CefBrowserSettings& settings) {
  DCHECK(delegate);
  DCHECK(!config->with_osr);  // Windowless rendering is not supported.
  DCHECK(!initialized_);

  delegate_ = delegate;
  config_ = std::move(config);

  if (config_->initially_hidden && !config_->source_bounds.IsEmpty()) {
    // The window will be sized and positioned in OnAutoResize().
    initial_bounds_ = config_->source_bounds;
    position_on_resize_ = true;
  } else {
    initial_bounds_ = config_->bounds;
  }

  CreateClientHandler(config_->url);
  initialized_ = true;

  // Continue initialization on the UI thread.
  InitOnUIThread(settings, config_->url, delegate_->GetRequestContext(this));
}

void RootWindowViews::InitAsPopup(RootWindow::Delegate* delegate,
                                  bool with_controls,
                                  bool with_osr,
                                  const CefPopupFeatures& popupFeatures,
                                  CefWindowInfo& windowInfo,
                                  CefRefPtr<CefClient>& client,
                                  CefBrowserSettings& settings) {
  CEF_REQUIRE_UI_THREAD();

  DCHECK(delegate);
  DCHECK(!with_osr);  // Windowless rendering is not supported.
  DCHECK(!initialized_);

  delegate_ = delegate;
  config_ = std::make_unique<RootWindowConfig>();
  config_->with_controls = with_controls;
  is_popup_ = true;

  if (popupFeatures.xSet)
    initial_bounds_.x = popupFeatures.x;
  if (popupFeatures.ySet)
    initial_bounds_.y = popupFeatures.y;
  if (popupFeatures.widthSet)
    initial_bounds_.width = popupFeatures.width;
  if (popupFeatures.heightSet)
    initial_bounds_.height = popupFeatures.height;

  CreateClientHandler(std::string());
  initialized_ = true;

  // The Window will be created in ViewsWindow::OnPopupBrowserViewCreated().
  client = client_handler_;
}

void RootWindowViews::Show(ShowMode mode) {
  if (!CefCurrentlyOn(TID_UI)) {
    // Execute this method on the UI thread.
    CefPostTask(TID_UI, base::BindOnce(&RootWindowViews::Show, this, mode));
    return;
  }

  if (!window_)
    return;

  window_->Show();

  switch (mode) {
    case ShowMinimized:
      window_->Minimize();
      break;
    case ShowMaximized:
      window_->Maximize();
      break;
    default:
      break;
  }
}

void RootWindowViews::Hide() {
  if (!CefCurrentlyOn(TID_UI)) {
    // Execute this method on the UI thread.
    CefPostTask(TID_UI, base::BindOnce(&RootWindowViews::Hide, this));
    return;
  }

  if (window_)
    window_->Hide();
}

void RootWindowViews::SetBounds(int x, int y, size_t width, size_t height) {
  if (!CefCurrentlyOn(TID_UI)) {
    // Execute this method on the UI thread.
    CefPostTask(TID_UI, base::BindOnce(&RootWindowViews::SetBounds, this, x, y,
                                       width, height));
    return;
  }

  if (window_) {
    window_->SetBounds(
        CefRect(x, y, static_cast<int>(width), static_cast<int>(height)));
  }
}

void RootWindowViews::Close(bool force) {
  if (!CefCurrentlyOn(TID_UI)) {
    // Execute this method on the UI thread.
    CefPostTask(TID_UI, base::BindOnce(&RootWindowViews::Close, this, force));
    return;
  }

  if (window_)
    window_->Close(force);
}

void RootWindowViews::SetDeviceScaleFactor(float device_scale_factor) {
  REQUIRE_MAIN_THREAD();
  // Windowless rendering is not supported.
  NOTREACHED();
}

float RootWindowViews::GetDeviceScaleFactor() const {
  REQUIRE_MAIN_THREAD();
  // Windowless rendering is not supported.
  NOTREACHED();
  return 0.0;
}

CefRefPtr<CefBrowser> RootWindowViews::GetBrowser() const {
  REQUIRE_MAIN_THREAD();
  return browser_;
}

ClientWindowHandle RootWindowViews::GetWindowHandle() const {
  REQUIRE_MAIN_THREAD();
#if defined(OS_LINUX)
  // ClientWindowHandle is a GtkWidget* on Linux and we don't have one of those.
  return nullptr;
#else
  if (browser_)
    return browser_->GetHost()->GetWindowHandle();
  return kNullWindowHandle;
#endif
}

bool RootWindowViews::WithExtension() const {
  DCHECK(initialized_);
  return config_->with_extension;
}

bool RootWindowViews::WithControls() {
  DCHECK(initialized_);
  return config_->with_controls;
}

bool RootWindowViews::WithExtension() {
  DCHECK(initialized_);
  return config_->with_extension;
}

void RootWindowViews::OnExtensionsChanged(const ExtensionSet& extensions) {
  if (!CefCurrentlyOn(TID_UI)) {
    // Execute this method on the UI thread.
    CefPostTask(TID_UI, base::BindOnce(&RootWindowViews::OnExtensionsChanged,
                                       this, extensions));
    return;
  }

  if (window_) {
    window_->OnExtensionsChanged(extensions);
  } else {
    // Window may not exist yet for popups.
    pending_extensions_ = extensions;
  }
}

bool RootWindowViews::InitiallyHidden() {
  CEF_REQUIRE_UI_THREAD();
  return config_->initially_hidden;
}

CefRefPtr<CefWindow> RootWindowViews::GetParentWindow() {
  CEF_REQUIRE_UI_THREAD();
  return config_->parent_window;
}

CefRect RootWindowViews::GetWindowBounds() {
  CEF_REQUIRE_UI_THREAD();
  return initial_bounds_;
}

scoped_refptr<ImageCache> RootWindowViews::GetImageCache() {
  CEF_REQUIRE_UI_THREAD();
  return image_cache_;
}

void RootWindowViews::OnViewsWindowCreated(CefRefPtr<ViewsWindow> window) {
  CEF_REQUIRE_UI_THREAD();
  DCHECK(!window_);
  window_ = window;
  window_->SetAlwaysOnTop(config_->always_on_top);

  if (!pending_extensions_.empty()) {
    window_->OnExtensionsChanged(pending_extensions_);
    pending_extensions_.clear();
  }
}

void RootWindowViews::OnViewsWindowDestroyed(CefRefPtr<ViewsWindow> window) {
  CEF_REQUIRE_UI_THREAD();
  window_ = nullptr;

  // Continue on the main thread.
  MAIN_POST_CLOSURE(
      base::BindOnce(&RootWindowViews::NotifyViewsWindowDestroyed, this));
}

void RootWindowViews::OnViewsWindowActivated(CefRefPtr<ViewsWindow> window) {
  CEF_REQUIRE_UI_THREAD();

  // Continue on the main thread.
  MAIN_POST_CLOSURE(
      base::BindOnce(&RootWindowViews::NotifyViewsWindowActivated, this));
}

ViewsWindow::Delegate* RootWindowViews::GetDelegateForPopup(
    CefRefPtr<CefClient> client) {
  CEF_REQUIRE_UI_THREAD();
  // |handler| was created in RootWindowViews::InitAsPopup().
  ClientHandlerStd* handler = static_cast<ClientHandlerStd*>(client.get());
  RootWindowViews* root_window =
      static_cast<RootWindowViews*>(handler->delegate());

  // Transfer some state to the child RootWindowViews.
  root_window->image_cache_ = image_cache_;

  return root_window;
}

void RootWindowViews::CreateExtensionWindow(CefRefPtr<CefExtension> extension,
                                            const CefRect& source_bounds,
                                            CefRefPtr<CefWindow> parent_window,
                                            base::OnceClosure close_callback) {
  if (!CURRENTLY_ON_MAIN_THREAD()) {
    // Execute this method on the main thread.
    MAIN_POST_CLOSURE(base::BindOnce(&RootWindowViews::CreateExtensionWindow,
                                     this, extension, source_bounds,
                                     parent_window, std::move(close_callback)));
    return;
  }

  delegate_->CreateExtensionWindow(extension, source_bounds, parent_window,
                                   std::move(close_callback), false);
}

void RootWindowViews::OnTest(int test_id) {
  if (!CURRENTLY_ON_MAIN_THREAD()) {
    // Execute this method on the main thread.
    MAIN_POST_CLOSURE(base::BindOnce(&RootWindowViews::OnTest, this, test_id));
    return;
  }

  delegate_->OnTest(this, test_id);
}

void RootWindowViews::OnExit() {
  if (!CURRENTLY_ON_MAIN_THREAD()) {
    // Execute this method on the main thread.
    MAIN_POST_CLOSURE(base::BindOnce(&RootWindowViews::OnExit, this));
    return;
  }

  delegate_->OnExit(this);
}

void RootWindowViews::OnBrowserCreated(CefRefPtr<CefBrowser> browser) {
  REQUIRE_MAIN_THREAD();
  DCHECK(!browser_);
  browser_ = browser;
  delegate_->OnBrowserCreated(this, browser);
}

void RootWindowViews::OnBrowserClosing(CefRefPtr<CefBrowser> browser) {
  REQUIRE_MAIN_THREAD();
  // Nothing to do here.
}

void RootWindowViews::OnBrowserClosed(CefRefPtr<CefBrowser> browser) {
  REQUIRE_MAIN_THREAD();
  if (browser_) {
    DCHECK_EQ(browser->GetIdentifier(), browser_->GetIdentifier());
    browser_ = nullptr;
  }

  client_handler_->DetachDelegate();
  client_handler_ = nullptr;

  browser_destroyed_ = true;
  NotifyDestroyedIfDone();
}

void RootWindowViews::OnSetAddress(const std::string& url) {
  if (!CefCurrentlyOn(TID_UI)) {
    // Execute this method on the UI thread.
    CefPostTask(TID_UI,
                base::BindOnce(&RootWindowViews::OnSetAddress, this, url));
    return;
  }

  if (window_)
    window_->SetAddress(url);
}

void RootWindowViews::OnSetTitle(const std::string& title) {
  if (!CefCurrentlyOn(TID_UI)) {
    // Execute this method on the UI thread.
    CefPostTask(TID_UI,
                base::BindOnce(&RootWindowViews::OnSetTitle, this, title));
    return;
  }

  if (window_)
    window_->SetTitle(title);
}

void RootWindowViews::OnSetFavicon(CefRefPtr<CefImage> image) {
  if (!CefCurrentlyOn(TID_UI)) {
    // Execute this method on the UI thread.
    CefPostTask(TID_UI,
                base::BindOnce(&RootWindowViews::OnSetFavicon, this, image));
    return;
  }

  if (window_)
    window_->SetFavicon(image);
}

void RootWindowViews::OnSetFullscreen(bool fullscreen) {
  if (!CefCurrentlyOn(TID_UI)) {
    // Execute this method on the UI thread.
    CefPostTask(TID_UI, base::BindOnce(&RootWindowViews::OnSetFullscreen, this,
                                       fullscreen));
    return;
  }

  if (window_)
    window_->SetFullscreen(fullscreen);
}

void RootWindowViews::OnAutoResize(const CefSize& new_size) {
  if (!CefCurrentlyOn(TID_UI)) {
    // Execute this method on the UI thread.
    CefPostTask(TID_UI,
                base::BindOnce(&RootWindowViews::OnAutoResize, this, new_size));
    return;
  }

  bool has_position = false;
  CefPoint position;
  if (position_on_resize_) {
    // Position the window centered on and immediately below the source.
    const int x_offset = (initial_bounds_.width - new_size.width) / 2;
    position.Set(initial_bounds_.x + x_offset,
                 initial_bounds_.y + initial_bounds_.height);
    has_position = true;

    // Don't change the window position on future resizes.
    position_on_resize_ = false;
  }

  if (window_) {
    window_->SetBrowserSize(new_size, has_position, position);
    window_->Show();
  }
}

void RootWindowViews::OnSetLoadingState(bool isLoading,
                                        bool canGoBack,
                                        bool canGoForward) {
  if (!CefCurrentlyOn(TID_UI)) {
    // Execute this method on the UI thread.
    CefPostTask(TID_UI,
                base::BindOnce(&RootWindowViews::OnSetLoadingState, this,
                               isLoading, canGoBack, canGoForward));
    return;
  }

  if (window_) {
    window_->SetLoadingState(isLoading, canGoBack, canGoForward);

    if (isLoading) {
      // Reset to the default window icon when loading begins.
      window_->SetFavicon(
          delegate_->GetImageCache()->GetCachedImage("window_icon"));
    }
  }
}

void RootWindowViews::OnSetDraggableRegions(
    const std::vector<CefDraggableRegion>& regions) {
  if (!CefCurrentlyOn(TID_UI)) {
    // Execute this method on the UI thread.
    CefPostTask(TID_UI, base::BindOnce(&RootWindowViews::OnSetDraggableRegions,
                                       this, regions));
    return;
  }

  if (window_)
    window_->SetDraggableRegions(regions);
}

void RootWindowViews::OnTakeFocus(bool next) {
  if (!CefCurrentlyOn(TID_UI)) {
    // Execute this method on the UI thread.
    CefPostTask(TID_UI,
                base::BindOnce(&RootWindowViews::OnTakeFocus, this, next));
    return;
  }

  if (window_)
    window_->TakeFocus(next);
}

void RootWindowViews::OnBeforeContextMenu(CefRefPtr<CefMenuModel> model) {
  CEF_REQUIRE_UI_THREAD();
  if (window_)
    window_->OnBeforeContextMenu(model);
}

void RootWindowViews::CreateClientHandler(const std::string& url) {
  DCHECK(!client_handler_);

  client_handler_ = new ClientHandlerStd(this, url);
  client_handler_->set_download_favicon_images(true);
}

void RootWindowViews::InitOnUIThread(
    const CefBrowserSettings& settings,
    const std::string& startup_url,
    CefRefPtr<CefRequestContext> request_context) {
  if (!CefCurrentlyOn(TID_UI)) {
    // Execute this method on the UI thread.
    CefPostTask(TID_UI, base::BindOnce(&RootWindowViews::InitOnUIThread, this,
                                       settings, startup_url, request_context));
    return;
  }

  image_cache_ = delegate_->GetImageCache();

  // Populate the default image cache.
  ImageCache::ImageInfoSet image_set;
  for (size_t i = 0U; i < base::size(kDefaultImageCache); ++i)
    image_set.push_back(ImageCache::ImageInfo::Create2x(kDefaultImageCache[i]));

  image_cache_->LoadImages(
      image_set, base::BindOnce(&RootWindowViews::CreateViewsWindow, this,
                                settings, startup_url, request_context));
}

void RootWindowViews::CreateViewsWindow(
    const CefBrowserSettings& settings,
    const std::string& startup_url,
    CefRefPtr<CefRequestContext> request_context,
    const ImageCache::ImageSet& images) {
  CEF_REQUIRE_UI_THREAD();
  DCHECK(!window_);

#ifndef NDEBUG
  // Make sure the default images loaded successfully.
  DCHECK_EQ(images.size(), base::size(kDefaultImageCache));
  for (size_t i = 0U; i < base::size(kDefaultImageCache); ++i) {
    DCHECK(images[i]) << "Default image " << i << " failed to load";
  }
#endif

  // Create the ViewsWindow. It will show itself after creation.
  ViewsWindow::Create(this, client_handler_, startup_url, settings,
                      request_context);
}

void RootWindowViews::NotifyViewsWindowDestroyed() {
  REQUIRE_MAIN_THREAD();
  window_destroyed_ = true;
  NotifyDestroyedIfDone();
}

void RootWindowViews::NotifyViewsWindowActivated() {
  REQUIRE_MAIN_THREAD();
  delegate_->OnRootWindowActivated(this);
}

void RootWindowViews::NotifyDestroyedIfDone() {
  // Notify once both the window and the browser have been destroyed.
  if (window_destroyed_ && browser_destroyed_) {
    delegate_->OnRootWindowDestroyed(this);
    if (!config_->close_callback.is_null())
      std::move(config_->close_callback).Run();
  }
}

}  // namespace client
