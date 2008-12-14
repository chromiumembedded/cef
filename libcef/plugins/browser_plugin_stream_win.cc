// Copyright (c) 2008 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "precompiled_libcef.h"
#include "browser_plugin_stream.h"
#include "browser_plugin_instance.h"

#include "base/logging.h"

namespace NPAPI {

BrowserPluginStream::BrowserPluginStream(
    BrowserPluginInstance *instance,
    const char *url,
    bool need_notify,
    void *notify_data)
    : instance_(instance),
      notify_needed_(need_notify),
      notify_data_(notify_data),
      close_on_write_data_(false),
      opened_(false),
      requested_plugin_mode_(NP_NORMAL),
      temp_file_handle_(INVALID_HANDLE_VALUE),
      seekable_stream_(false),
      data_offset_(0) {
  memset(&stream_, 0, sizeof(stream_));
  stream_.url = _strdup(url);
  temp_file_name_[0] = '\0';
}

void BrowserPluginStream::UpdateUrl(const char* url) {
  DCHECK(!opened_);
  free(const_cast<char*>(stream_.url));
  stream_.url = _strdup(url);
}

void BrowserPluginStream::WriteAsFile() {
  if (requested_plugin_mode_ == NP_ASFILE ||
      requested_plugin_mode_ == NP_ASFILEONLY)
    instance_->NPP_StreamAsFile(&stream_, temp_file_name_);
}

size_t BrowserPluginStream::WriteBytes(const char *buf, size_t length) {
  DWORD bytes;

  if (!WriteFile(temp_file_handle_, buf, length, &bytes, 0))
    return 0U;

  return static_cast<size_t>(bytes);
}

bool BrowserPluginStream::OpenTempFile() {
  DCHECK(temp_file_handle_ == INVALID_HANDLE_VALUE);

  // The reason for using all the Ascii versions of these filesystem
  // calls is that the filename which we pass back to the plugin
  // via NPAPI is an ascii filename.  Otherwise, we'd use wide-chars.
  //
  // TODO:
  // This is a bug in NPAPI itself, and it needs to be fixed.
  // The case which will fail is if a user has a multibyte name,
  // but has the system locale set to english.  GetTempPathA will
  // return junk in this case, causing us to be unable to open the
  // file.

  char temp_directory[MAX_PATH];
  if (GetTempPathA(MAX_PATH, temp_directory) == 0)
    return false;
  if (GetTempFileNameA(temp_directory, "npstream", 0, temp_file_name_) == 0)
    return false;
  temp_file_handle_ = CreateFileA(temp_file_name_,
                                  FILE_ALL_ACCESS,
                                  FILE_SHARE_READ,
                                  0,
                                  CREATE_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL,
                                  0);
  if (temp_file_handle_ == INVALID_HANDLE_VALUE) {
    temp_file_name_[0] = '\0';
    return false;
  }
  return true;
}

void BrowserPluginStream::CloseTempFile() {
  if (temp_file_handle_ != INVALID_HANDLE_VALUE) {
    CloseHandle(temp_file_handle_);
    temp_file_handle_ = INVALID_HANDLE_VALUE;
  }
}

bool BrowserPluginStream::TempFileIsValid() {
  return temp_file_handle_ != INVALID_HANDLE_VALUE;
}

}  // namespace NPAPI

