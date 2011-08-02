// Copyright (c) 2011 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _DOWNLOAD_UTIL_H
#define _DOWNLOAD_UTIL_H
#pragma once

#include <string>

class GURL;
class FilePath;

namespace download_util {

// Create a file name based on the response from the server.
void GenerateFileName(const GURL& url,
                      const std::string& content_disposition,
                      const std::string& referrer_charset,
                      const std::string& mime_type,
                      const std::string& suggested_name,
                      FilePath* generated_name);

} // namespace download_util

#endif // _DOWNLOAD_UTIL_H
