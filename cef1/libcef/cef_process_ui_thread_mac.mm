// Copyright (c) 2010 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <AppKit/AppKit.h>

#import "include/cef_application_mac.h"
#include "libcef/cef_process_ui_thread.h"
#include "libcef/browser_webkit_glue.h"
#include "libcef/cef_context.h"
#include "base/command_line.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/base/ui_base_switches.h"

void CefProcessUIThread::PlatformInit() {
  // The NSApplication instance must implement the CefAppProtocol protocol.
  DCHECK([[NSApplication sharedApplication]
          conformsToProtocol:@protocol(CefAppProtocol)]);

  // On Mac, the select popup menus are rendered by the browser.
  WebKit::WebView::setUseExternalPopupMenus(true);
  
  // Core Animation mode for plugins is not supported.
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableCoreAnimationPlugins);
}

void CefProcessUIThread::PlatformCleanUp() {
}
