// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>
#include <commctrl.h>
#include <Objbase.h>

#include "libcef/browser/browser_main.h"

#include "chrome/common/chrome_utility_messages.h"
#include "content/public/browser/utility_process_host.h"
#include "content/public/browser/utility_process_host_client.h"
#include "content/public/common/dwrite_font_platform_win.h"
#include "grit/cef_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/win/direct_write.h"

namespace {

void ExecuteFontCacheBuildTask(const base::FilePath& path) {
  base::WeakPtr<content::UtilityProcessHost> utility_process_host(
      content::UtilityProcessHost::Create(NULL, NULL)->AsWeakPtr());
  utility_process_host->SetName(l10n_util::GetStringUTF16(
      IDS_UTILITY_PROCESS_FONT_CACHE_BUILDER_NAME));
  utility_process_host->DisableSandbox();
  utility_process_host->Send(
      new ChromeUtilityHostMsg_BuildDirectWriteFontCache(path));
}

}  // namespace

void CefBrowserMainParts::PlatformInitialize() {
  HRESULT res;

  // Initialize common controls.
  res = CoInitialize(NULL);
  DCHECK(SUCCEEDED(res));
  INITCOMMONCONTROLSEX InitCtrlEx;
  InitCtrlEx.dwSize = sizeof(INITCOMMONCONTROLSEX);
  InitCtrlEx.dwICC  = ICC_STANDARD_CLASSES;
  InitCommonControlsEx(&InitCtrlEx);

  // Start COM stuff.
  res = OleInitialize(NULL);
  DCHECK(SUCCEEDED(res));
}

void CefBrowserMainParts::PlatformPreMainMessageLoopRun() {
  // From ChromeBrowserMainPartsWin::PostProfileInit().
  // DirectWrite support is mainly available on Windows 7 and up.
  if (gfx::win::ShouldUseDirectWrite()) {
    const base::FilePath& cache_path = global_browser_context_->GetPath();
    if (cache_path.empty())
      return;

    const base::FilePath& font_cache_path =
        cache_path.AppendASCII(content::kFontCacheSharedSectionName);
    // This function will create a read only section if cache file exists
    // otherwise it will spawn utility process to build cache file, which will
    // be used during next browser start/postprofileinit.
    if (!content::LoadFontCache(font_cache_path)) {
      // We delay building of font cache until first startup page loads.
      // During first renderer start there are lot of things happening
      // simultaneously some of them are:
      // - Renderer is going through all font files on the system to create
      //   a font collection.
      // - Renderer loading up startup URL, accessing HTML/JS File cache,
      //   net activity etc.
      // - Extension initialization.
      // We delay building of cache mainly to avoid parallel font file
      // loading along with Renderer. Some systems have significant number of
      // font files which takes long time to process.
      // Related information is at http://crbug.com/436195.
      const int kBuildFontCacheDelaySec = 30;
      content::BrowserThread::PostDelayedTask(
          content::BrowserThread::IO,
          FROM_HERE,
          base::Bind(ExecuteFontCacheBuildTask, font_cache_path),
          base::TimeDelta::FromSeconds(kBuildFontCacheDelaySec));
    }
  }
}
