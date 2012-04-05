// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/cef_thread.h"
#include "libcef/drag_download_util.h"

#include "base/bind.h"
#include "base/string_util.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "net/base/file_stream.h"
#include "net/base/net_errors.h"

using net::FileStream;

namespace drag_download_util {

bool ParseDownloadMetadata(const string16& metadata,
                           string16* mime_type,
                           FilePath* file_name,
                           GURL* url) {
  const char16 separator = L':';

  size_t mime_type_end_pos = metadata.find(separator);
  if (mime_type_end_pos == string16::npos)
    return false;

  size_t file_name_end_pos = metadata.find(separator, mime_type_end_pos + 1);
  if (file_name_end_pos == string16::npos)
    return false;

  GURL parsed_url = GURL(metadata.substr(file_name_end_pos + 1));
  if (!parsed_url.is_valid())
    return false;

  if (mime_type)
    *mime_type = metadata.substr(0, mime_type_end_pos);
  if (file_name) {
    string16 file_name_str = metadata.substr(
        mime_type_end_pos + 1, file_name_end_pos - mime_type_end_pos  - 1);
#if defined(OS_WIN)
    *file_name = FilePath(file_name_str);
#else
    *file_name = FilePath(UTF16ToUTF8(file_name_str));
#endif
  }
  if (url)
    *url = parsed_url;

  return true;
}

FileStream* CreateFileStreamForDrop(FilePath* file_path) {
  DCHECK(file_path && !file_path->empty());

  scoped_ptr<FileStream> file_stream(new FileStream(NULL));
  const int kMaxSeq = 99;
  for (int seq = 0; seq <= kMaxSeq; seq++) {
    FilePath new_file_path;
    if (seq == 0) {
      new_file_path = *file_path;
    } else {
#if defined(OS_WIN)
      string16 suffix = ASCIIToUTF16("-") + base::IntToString16(seq);
#else
      std::string suffix = std::string("-") + base::IntToString(seq);
#endif
      new_file_path = file_path->InsertBeforeExtension(suffix);
    }

    // Explicitly (and redundantly check) for file -- despite the fact that our
    // open won't overwrite -- just to avoid log spew.
    if (!file_util::PathExists(new_file_path) &&
        file_stream->OpenSync(new_file_path, base::PLATFORM_FILE_CREATE |
                              base::PLATFORM_FILE_WRITE) == net::OK) {
      *file_path = new_file_path;
      return file_stream.release();
    }
  }

  return NULL;
}

PromiseFileFinalizer::PromiseFileFinalizer(
    DragDownloadFile* drag_file_downloader)
    : drag_file_downloader_(drag_file_downloader) {
}

PromiseFileFinalizer::~PromiseFileFinalizer() {}

void PromiseFileFinalizer::Cleanup() {
  if (drag_file_downloader_.get())
    drag_file_downloader_ = NULL;
}

void PromiseFileFinalizer::OnDownloadCompleted(const FilePath& file_path) {
  CefThread::PostTask(
      CefThread::UI, FROM_HERE,
      base::Bind(&PromiseFileFinalizer::Cleanup, this));
}

void PromiseFileFinalizer::OnDownloadAborted() {
  CefThread::PostTask(
      CefThread::UI, FROM_HERE,
      base::Bind(&PromiseFileFinalizer::Cleanup, this));
}

}  // namespace drag_download_util
