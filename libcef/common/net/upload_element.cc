// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/common/net/upload_element.h"

#include "net/base/file_stream.h"
#include "net/base/net_errors.h"

namespace net {

UploadElement::UploadElement()
    : type_(TYPE_BYTES),
      bytes_start_(NULL),
      bytes_length_(0),
      file_range_offset_(0),
      file_range_length_(std::numeric_limits<uint64_t>::max()) {
}

UploadElement::~UploadElement() {
}

}  // namespace net
