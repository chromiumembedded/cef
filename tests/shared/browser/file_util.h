// Copyright 2016 The Chromium Embedded Framework Authors. Portions copyright
// 2012 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#ifndef CEF_TESTS_SHARED_BROWSER_FILE_UTIL_H_
#define CEF_TESTS_SHARED_BROWSER_FILE_UTIL_H_
#pragma once

#include <limits>
#include <string>

namespace client::file_util {

// Platform-specific path separator.
extern const char kPathSep;

// Reads the file at |path| into |contents| and returns true on success and
// false on error.  In case of I/O error, |contents| holds the data that could
// be read from the file before the error occurred.  When the file size exceeds
// max_size|, the function returns false with |contents| holding the file
// truncated to |max_size|. |contents| may be nullptr, in which case this
// function is useful for its side effect of priming the disk cache (could be
// used for unit tests). Calling this function on the browser process UI or IO
// threads is not allowed.
bool ReadFileToString(const std::string& path,
                      std::string* contents,
                      size_t max_size = std::numeric_limits<size_t>::max());

// Writes the given buffer into the file, overwriting any data that was
// previously there. Returns the number of bytes written, or -1 on error.
// Calling this function on the browser process UI or IO threads is not allowed.
int WriteFile(const std::string& path, const char* data, int size);

// Combines |path1| and |path2| with the correct platform-specific path
// separator.
std::string JoinPath(const std::string& path1, const std::string& path2);

// Extracts the file extension from |path|.
std::string GetFileExtension(const std::string& path);

}  // namespace client::file_util

#endif  // CEF_TESTS_SHARED_BROWSER_FILE_UTIL_H_
