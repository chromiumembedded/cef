// Copyright 2017 The Chromium Embedded Framework Authors. Portions copyright
// 2011 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_UTIL_MAC_H_
#define CEF_LIBCEF_COMMON_UTIL_MAC_H_
#pragma once

namespace base {
class FilePath;
}

namespace util_mac {

// Returns the path to the NSLibraryDirectory (e.g. "~/Library").
bool GetLocalLibraryDirectory(base::FilePath* result);

// Returns the path to the CEF framework directory inside the top-level app
// bundle (e.g. "myapp.app/Contents/Frameworks/Chromium Embedded
// Framework.framework"). May return an empty value if not running in an app
// bundle.
base::FilePath GetFrameworkDirectory();

// Returns the path to the Resources directory inside the CEF framework
// directory (e.g. "myapp.app/Contents/Frameworks/Chromium Embedded
// Framework.framework/Resources"). May return an empty value if not running in
// an app bundle.
base::FilePath GetFrameworkResourcesDirectory();

// Returns the path to the main (running) process executable (e.g.
// "myapp.app/Contents/MacOS/myapp").
base::FilePath GetMainProcessPath();

// Returns the path to the Resources directory inside the top-level app bundle
// (e.g. "myapp.app/Contents/Resources"). May return an empty value if not
// running in an app bundle.
base::FilePath GetMainResourcesDirectory();

// Returns the path to the child process executable (e.g. "myapp.app/
// Contents/Frameworks/myapp Helper.app/Contents/MacOS/myapp Helper"). May
// return an empty value if not running in an app bundle.
base::FilePath GetChildProcessPath();

}  // namespace util_mac

#endif  // CEF_LIBCEF_COMMON_UTIL_MAC_H_
