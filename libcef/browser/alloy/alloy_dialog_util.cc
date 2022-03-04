// Copyright 2021 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include "libcef/browser/alloy/alloy_dialog_util.h"

#include "libcef/browser/alloy/alloy_browser_host_impl.h"
#include "libcef/browser/extensions/browser_extensions_util.h"
#include "libcef/browser/file_dialog_runner.h"

#include "base/strings/utf_string_conversions.h"

namespace alloy {

void RunFileChooser(content::WebContents* web_contents,
                    const blink::mojom::FileChooserParams& params,
                    RunFileChooserCallback callback) {
  CefRefPtr<AlloyBrowserHostImpl> browser = static_cast<AlloyBrowserHostImpl*>(
      extensions::GetOwnerBrowserForHost(web_contents->GetRenderViewHost(),
                                         nullptr)
          .get());
  if (!browser) {
    LOG(ERROR) << "Failed to identify browser; canceling file dialog";
    std::move(callback).Run(-1, {});
    return;
  }

  CefFileDialogRunner::FileChooserParams cef_params;
  cef_params.mode = params.mode;
  cef_params.default_file_name = params.default_file_name;
  cef_params.accept_types = params.accept_types;

  browser->RunFileChooser(cef_params, std::move(callback));
}

// Based on net/base/filename_util_internal.cc FilePathToString16().
std::u16string FilePathTypeToString16(const base::FilePath::StringType& str) {
  std::u16string result;
#if BUILDFLAG(IS_WIN)
  result.assign(str.begin(), str.end());
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  if (!str.empty()) {
    base::UTF8ToUTF16(str.c_str(), str.size(), &result);
  }
#endif
  return result;
}

}  // namespace alloy
