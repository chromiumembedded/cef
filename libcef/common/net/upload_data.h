// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_NET_UPLOAD_DATA_H_
#define CEF_LIBCEF_COMMON_NET_UPLOAD_DATA_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "libcef/common/net/upload_element.h"

#include "base/memory/ref_counted.h"
#include "base/supports_user_data.h"
#include "net/base/net_export.h"

namespace base {
class FilePath;
class Time;
}  // namespace base

namespace net {

//-----------------------------------------------------------------------------
// A very concrete class representing the data to be uploaded as part of a
// URLRequest.
//
// Until there is a more abstract class for this, this one derives from
// SupportsUserData to allow users to stash random data by
// key and ensure its destruction when UploadData is finally deleted.
class UploadData
    : public base::RefCounted<UploadData>,
      public base::SupportsUserData {
 public:
  UploadData();

  void AppendBytes(const char* bytes, int bytes_len);

  void AppendFileRange(const base::FilePath& file_path,
                       uint64_t offset, uint64_t length,
                       const base::Time& expected_modification_time);

  // Initializes the object to send chunks of upload data over time rather
  // than all at once. Chunked data may only contain bytes, not files.
  void set_is_chunked(bool set) { is_chunked_ = set; }
  bool is_chunked() const { return is_chunked_; }

  // set_last_chunk_appended() is only used for serialization.
  void set_last_chunk_appended(bool set) { last_chunk_appended_ = set; }
  bool last_chunk_appended() const { return last_chunk_appended_; }

  using ElementsVector = std::vector<std::unique_ptr<UploadElement>>;

  const ElementsVector& elements() const {
    return elements_;
  }

  ElementsVector* elements_mutable() {
    return &elements_;
  }

  void swap_elements(ElementsVector* elements) {
    elements_.swap(*elements);
  }

  // Identifies a particular upload instance, which is used by the cache to
  // formulate a cache key.  This value should be unique across browser
  // sessions.  A value of 0 is used to indicate an unspecified identifier.
  void set_identifier(int64_t id) { identifier_ = id; }
  int64_t identifier() const { return identifier_; }

 private:
  friend class base::RefCounted<UploadData>;

  ~UploadData() override;

  ElementsVector elements_;
  int64_t identifier_;
  bool is_chunked_;
  bool last_chunk_appended_;

  DISALLOW_COPY_AND_ASSIGN(UploadData);
};

}  // namespace net

#endif  // CEF_LIBCEF_COMMON_NET_UPLOAD_DATA_H_
