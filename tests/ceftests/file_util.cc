// Copyright 2016 The Chromium Embedded Framework Authors. Portions copyright
// 2012 The Chromium Authors. All rights reserved. Use of this source code is
// governed by a BSD-style license that can be found in the LICENSE file.

#include "tests/ceftests/file_util.h"

#include "include/base/cef_build.h"
#include "include/base/cef_scoped_ptr.h"
#include "include/cef_task.h"

#include <algorithm>
#include <cstdio>
#include <memory>

namespace file_util {

namespace {

bool AllowFileIO() {
  if (CefCurrentlyOn(TID_UI) || CefCurrentlyOn(TID_IO)) {
    NOTREACHED() << "file IO is not allowed on the current thread";
    return false;
  }
  return true;
}

}  // namespace

#if defined(OS_WIN)
const char kPathSep = '\\';
#else
const char kPathSep = '/';
#endif

bool ReadFileToString(const std::string& path, std::string* contents,
                      size_t max_size) {
  if (!AllowFileIO())
    return false;

  if (contents)
    contents->clear();
  FILE* file = fopen(path.c_str(), "rb");
  if (!file)
    return false;

  const size_t kBufferSize = 1 << 16;
  scoped_ptr<char[]> buf(new char[kBufferSize]);
  size_t len;
  size_t size = 0;
  bool read_status = true;

  // Many files supplied in |path| have incorrect size (proc files etc).
  // Hence, the file is read sequentially as opposed to a one-shot read.
  while ((len = fread(buf.get(), 1, kBufferSize, file)) > 0) {
    if (contents)
      contents->append(buf.get(), std::min(len, max_size - size));

    if ((max_size - size) < len) {
      read_status = false;
      break;
    }

    size += len;
  }
  read_status = read_status && !ferror(file);
  fclose(file);

  return read_status;
}

int WriteFile(const std::string& path, const char* data, int size) {
  if (!AllowFileIO())
    return -1;

  FILE* file = fopen(path.c_str(), "wb");
  if (!file)
    return -1;

  int written = 0;

  do {
    size_t write = fwrite(data + written, 1, size - written, file);
    if (write == 0)
      break;
    written += static_cast<int>(write);
  } while (written < size);

  fclose(file);

  return written;
}

std::string JoinPath(const std::string& path1, const std::string& path2) {
  if (path1.empty() && path2.empty())
    return std::string();
  if (path1.empty())
    return path2;
  if (path2.empty())
    return path1;

  std::string result = path1;
  if (result[result.size() - 1] != kPathSep)
    result += kPathSep;
  if (path2[0] == kPathSep)
    result += path2.substr(1);
  else
    result += path2;
  return result;
}

}  // namespace file_util
