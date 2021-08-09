// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/main_context_impl.h"

#include <algorithm>

#include "include/cef_parser.h"
#include "tests/shared/browser/client_app_browser.h"
#include "tests/shared/common/client_switches.h"

namespace client {

namespace {

// The default URL to load in a browser window.
const char kDefaultUrl[] = "http://www.google.com";

// Returns the ARGB value for |color|.
cef_color_t ParseColor(const std::string& color) {
  std::string colorToLower;
  colorToLower.resize(color.size());
  std::transform(color.begin(), color.end(), colorToLower.begin(), ::tolower);

  if (colorToLower == "black")
    return CefColorSetARGB(255, 0, 0, 0);
  else if (colorToLower == "blue")
    return CefColorSetARGB(255, 0, 0, 255);
  else if (colorToLower == "green")
    return CefColorSetARGB(255, 0, 255, 0);
  else if (colorToLower == "red")
    return CefColorSetARGB(255, 255, 0, 0);
  else if (colorToLower == "white")
    return CefColorSetARGB(255, 255, 255, 255);

  // Use the default color.
  return 0;
}

}  // namespace

MainContextImpl::MainContextImpl(CefRefPtr<CefCommandLine> command_line,
                                 bool terminate_when_all_windows_closed)
    : command_line_(command_line),
      terminate_when_all_windows_closed_(terminate_when_all_windows_closed),
      initialized_(false),
      shutdown_(false),
      background_color_(0),
      browser_background_color_(0),
      windowless_frame_rate_(0),
      use_views_(false) {
  DCHECK(command_line_.get());

  // Set the main URL.
  if (command_line_->HasSwitch(switches::kUrl))
    main_url_ = command_line_->GetSwitchValue(switches::kUrl);
  if (main_url_.empty())
    main_url_ = kDefaultUrl;

  // Whether windowless (off-screen) rendering will be used.
  use_windowless_rendering_ =
      command_line_->HasSwitch(switches::kOffScreenRenderingEnabled);

  if (use_windowless_rendering_ &&
      command_line_->HasSwitch(switches::kOffScreenFrameRate)) {
    windowless_frame_rate_ =
        atoi(command_line_->GetSwitchValue(switches::kOffScreenFrameRate)
                 .ToString()
                 .c_str());
  }

  // Whether transparent painting is used with windowless rendering.
  const bool use_transparent_painting =
      use_windowless_rendering_ &&
      command_line_->HasSwitch(switches::kTransparentPaintingEnabled);

#if defined(OS_WIN)
  // Shared texture is only supported on Windows.
  shared_texture_enabled_ =
      use_windowless_rendering_ &&
      command_line_->HasSwitch(switches::kSharedTextureEnabled);
#endif

  external_begin_frame_enabled_ =
      use_windowless_rendering_ &&
      command_line_->HasSwitch(switches::kExternalBeginFrameEnabled);

  if (windowless_frame_rate_ <= 0) {
// Choose a reasonable default rate based on the OSR mode.
#if defined(OS_WIN)
    windowless_frame_rate_ = shared_texture_enabled_ ? 60 : 30;
#else
    windowless_frame_rate_ = 30;
#endif
  }

  // Enable experimental Chrome runtime. See issue #2969 for details.
  use_chrome_runtime_ =
      command_line_->HasSwitch(switches::kEnableChromeRuntime);

  if (use_windowless_rendering_ && use_chrome_runtime_) {
    LOG(ERROR)
        << "Windowless rendering is not supported with the Chrome runtime.";
    use_chrome_runtime_ = false;
  }

  // Whether the Views framework will be used.
  use_views_ = command_line_->HasSwitch(switches::kUseViews);

  if (use_windowless_rendering_ && use_views_) {
    LOG(ERROR)
        << "Windowless rendering is not supported by the Views framework.";
    use_views_ = false;
  }

  if (use_chrome_runtime_ && !use_views_) {
    // TODO(chrome): Add support for this runtime configuration (e.g. a fully
    // styled Chrome window with cefclient menu customizations). In the mean
    // time this can be demo'd with "cefsimple --enable-chrome-runtime".
    LOG(WARNING) << "Chrome runtime requires the Views framework.";
    use_views_ = true;
  }

  if (use_views_ && command_line->HasSwitch(switches::kHideFrame) &&
      !command_line_->HasSwitch(switches::kUrl)) {
    // Use the draggable regions test as the default URL for frameless windows.
    main_url_ = "http://tests/draggable";
  }

  if (command_line_->HasSwitch(switches::kBackgroundColor)) {
    // Parse the background color value.
    background_color_ =
        ParseColor(command_line_->GetSwitchValue(switches::kBackgroundColor));
  }

  if (background_color_ == 0 && !use_views_) {
    // Set an explicit background color.
    background_color_ = CefColorSetARGB(255, 255, 255, 255);
  }

  // |browser_background_color_| should remain 0 to enable transparent painting.
  if (!use_transparent_painting) {
    browser_background_color_ = background_color_;
  }
}

MainContextImpl::~MainContextImpl() {
  // The context must either not have been initialized, or it must have also
  // been shut down.
  DCHECK(!initialized_ || shutdown_);
}

std::string MainContextImpl::GetConsoleLogPath() {
  return GetAppWorkingDirectory() + "console.log";
}

std::string MainContextImpl::GetMainURL() {
  return main_url_;
}

cef_color_t MainContextImpl::GetBackgroundColor() {
  return background_color_;
}

bool MainContextImpl::UseChromeRuntime() {
  return use_chrome_runtime_;
}

bool MainContextImpl::UseViews() {
  return use_views_;
}

bool MainContextImpl::UseWindowlessRendering() {
  return use_windowless_rendering_;
}

bool MainContextImpl::TouchEventsEnabled() {
  return command_line_->GetSwitchValue("touch-events") == "enabled";
}

void MainContextImpl::PopulateSettings(CefSettings* settings) {
  client::ClientAppBrowser::PopulateSettings(command_line_, *settings);

  if (use_chrome_runtime_)
    settings->chrome_runtime = true;

  CefString(&settings->cache_path) =
      command_line_->GetSwitchValue(switches::kCachePath);

  if (use_windowless_rendering_)
    settings->windowless_rendering_enabled = true;

  if (browser_background_color_ != 0)
    settings->background_color = browser_background_color_;

  if (command_line_->HasSwitch("lang")) {
    // Use the same locale for the Accept-Language HTTP request header.
    CefString(&settings->accept_language_list) =
        command_line_->GetSwitchValue("lang");
  }
}

void MainContextImpl::PopulateBrowserSettings(CefBrowserSettings* settings) {
  settings->windowless_frame_rate = windowless_frame_rate_;

  if (browser_background_color_ != 0)
    settings->background_color = browser_background_color_;
}

void MainContextImpl::PopulateOsrSettings(OsrRendererSettings* settings) {
  settings->show_update_rect =
      command_line_->HasSwitch(switches::kShowUpdateRect);

#if defined(OS_WIN)
  settings->shared_texture_enabled = shared_texture_enabled_;
#endif
  settings->external_begin_frame_enabled = external_begin_frame_enabled_;
  settings->begin_frame_rate = windowless_frame_rate_;

  if (browser_background_color_ != 0)
    settings->background_color = browser_background_color_;
}

RootWindowManager* MainContextImpl::GetRootWindowManager() {
  DCHECK(InValidState());
  return root_window_manager_.get();
}

bool MainContextImpl::Initialize(const CefMainArgs& args,
                                 const CefSettings& settings,
                                 CefRefPtr<CefApp> application,
                                 void* windows_sandbox_info) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!initialized_);
  DCHECK(!shutdown_);

  if (!CefInitialize(args, settings, application, windows_sandbox_info))
    return false;

  // Need to create the RootWindowManager after calling CefInitialize because
  // TempWindowX11 uses cef_get_xdisplay().
  root_window_manager_.reset(
      new RootWindowManager(terminate_when_all_windows_closed_));

  initialized_ = true;

  return true;
}

void MainContextImpl::Shutdown() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(initialized_);
  DCHECK(!shutdown_);

  root_window_manager_.reset();

  CefShutdown();

  shutdown_ = true;
}

}  // namespace client
