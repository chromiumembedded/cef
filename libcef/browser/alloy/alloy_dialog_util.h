// Copyright 2021 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_ALLOY_ALLOY_DIALOG_UTIL_H_
#define CEF_LIBCEF_BROWSER_ALLOY_ALLOY_DIALOG_UTIL_H_
#pragma once

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"

namespace content {
class WebContents;
}

namespace alloy {

// The argument vector will be empty if the dialog was canceled.
using RunFileChooserCallback =
    base::OnceCallback<void(int /*selected_accept_filter*/,
                            const std::vector<base::FilePath>& /*file_paths*/)>;

// Display the file chooser dialog. Execute |callback| on completion.
// Called from patched chrome/ files.
void RunFileChooser(content::WebContents* web_contents,
                    const blink::mojom::FileChooserParams& params,
                    RunFileChooserCallback callback);

std::u16string FilePathTypeToString16(const base::FilePath::StringType& str);

}  // namespace alloy

#endif  // CEF_LIBCEF_BROWSER_ALLOY_ALLOY_DIALOG_UTIL_H_
