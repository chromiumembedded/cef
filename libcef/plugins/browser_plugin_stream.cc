// Copyright (c) 2008 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO : Support NP_ASFILEONLY mode
// TODO : Support NP_SEEK mode
// TODO : Support SEEKABLE=true in NewStream

#include "precompiled_libcef.h"
#include "browser_plugin_stream.h"
#include "browser_plugin_instance.h"

#include "base/string_util.h"
#include "base/message_loop.h"
#include "webkit/glue/webkit_glue.h"
#include "googleurl/src/gurl.h"

namespace NPAPI {

BrowserPluginStream::~BrowserPluginStream() {
  // always close our temporary files.
  CloseTempFile();
  free(const_cast<char*>(stream_.url));
}

bool BrowserPluginStream::Open(const std::string &mime_type,
                               const std::string &headers,
                               uint32 length,
                               uint32 last_modified,
                               bool request_is_seekable) {
  headers_ = headers;
  NPP id = instance_->npp();
  stream_.end = length;
  stream_.lastmodified = last_modified;
  stream_.pdata = 0;
  stream_.ndata = id->ndata;
  stream_.notifyData = notify_data_;

  bool seekable_stream = false;
  if (request_is_seekable && !headers_.empty()) {
    stream_.headers = headers_.c_str();
    if (headers_.find("Accept-Ranges: bytes") != std::string::npos) {
      seekable_stream = true;
    }
  }

  const char *char_mime_type = "application/x-unknown-content-type";
  std::string temp_mime_type;
  if (!mime_type.empty()) {
    char_mime_type = mime_type.c_str();
  } else {
    GURL gurl(stream_.url);
    std::wstring path(UTF8ToWide(gurl.path()));
    if (webkit_glue::GetMimeTypeFromFile(path, &temp_mime_type))
      char_mime_type = temp_mime_type.c_str();
  }

  // Silverlight expects a valid mime type
  DCHECK(strlen(char_mime_type) != 0);
  NPError err = instance_->NPP_NewStream((NPMIMEType)char_mime_type,
                                         &stream_, seekable_stream,
                                         &requested_plugin_mode_);
  if (err != NPERR_NO_ERROR) {
    Notify(err);
    return false;
  }

  opened_ = true;

  if (requested_plugin_mode_ == NP_SEEK) {
    seekable_stream_ = true;
  }
  // If the plugin has requested certain modes, then we need a copy
  // of this file on disk.  Open it and save it as we go.
  if (requested_plugin_mode_ == NP_ASFILEONLY ||
    requested_plugin_mode_ == NP_ASFILE) {
    if (OpenTempFile() == false)
      return false;
  }

  mime_type_ = char_mime_type;
  return true;
}

int BrowserPluginStream::Write(const char *buffer, const int length,
                               int data_offset) {
  // There may be two streams to write to - the plugin and the file.
  // It is unclear what to do if we cannot write to both.  The rules of
  // this function are that the plugin must consume at least as many
  // bytes as returned by the WriteReady call.  So, we will attempt to
  // write that many to both streams.  If we can't write that many bytes
  // to each stream, we'll return failure.

  DCHECK(opened_);
  if (WriteToFile(buffer, length) &&
      WriteToPlugin(buffer, length, data_offset))
    return length;

  return -1;
}

bool BrowserPluginStream::WriteToFile(const char *buf, size_t length) {
  // For ASFILEONLY, ASFILE, and SEEK modes, we need to write
  // to the disk
  if (TempFileIsValid() &&
      (requested_plugin_mode_ == NP_ASFILE ||
       requested_plugin_mode_ == NP_ASFILEONLY) ) {
    size_t totalBytesWritten = 0, bytes;
    do {
      bytes = WriteBytes(buf, length);
      totalBytesWritten += bytes;
    } while (bytes > 0U && totalBytesWritten < length);

    if (totalBytesWritten != length)
      return false;
  }

  return true;
}

bool BrowserPluginStream::WriteToPlugin(const char *buf, const int length,
                                        const int data_offset) {
  // For NORMAL and ASFILE modes, we send the data to the plugin now
  if (requested_plugin_mode_ != NP_NORMAL &&
      requested_plugin_mode_ != NP_ASFILE &&
      requested_plugin_mode_ != NP_SEEK)
    return true;

  int written = TryWriteToPlugin(buf, length, data_offset);
  if (written == -1)
      return false;

  if (written < length) {
    // Buffer the remaining data.
    size_t remaining = length - written;
    size_t previous_size = delivery_data_.size();
    delivery_data_.resize(previous_size + remaining);
    data_offset_ = data_offset;
    memcpy(&delivery_data_[previous_size], buf + written, remaining);
    MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
        this, &BrowserPluginStream::OnDelayDelivery));
  }

  return true;
}

void BrowserPluginStream::OnDelayDelivery() {
  // It is possible that the plugin stream may have closed before the task
  // was hit.
  if (!opened_) {
    return;
  }

  int size = static_cast<int>(delivery_data_.size());
  int written = TryWriteToPlugin(&delivery_data_.front(), size,
                                 data_offset_);
  if (written > 0) {
    // Remove the data that we already wrote.
    delivery_data_.erase(delivery_data_.begin(),
                         delivery_data_.begin() + written);
  }
}

int BrowserPluginStream::TryWriteToPlugin(const char *buf, const int length,
                                          const int data_offset) {
  int byte_offset = 0;

  if (data_offset > 0)
    data_offset_ = data_offset;

  while (byte_offset < length) {
    int bytes_remaining = length - byte_offset;
    int bytes_to_write = instance_->NPP_WriteReady(&stream_);
    if (bytes_to_write > bytes_remaining)
      bytes_to_write = bytes_remaining;

    if (bytes_to_write == 0)
      return byte_offset;

    int bytes_consumed = instance_->NPP_Write(
        &stream_, data_offset_, bytes_to_write,
        const_cast<char*>(buf + byte_offset));
    if (bytes_consumed < 0) {
      // The plugin failed, which means that we need to close the stream.
      Close(NPRES_NETWORK_ERR);
      return -1;
    }
    if (bytes_consumed == 0) {
      // The plugin couldn't take all of the data now.
      return byte_offset;
    }

    // The plugin might report more that we gave it.
    bytes_consumed = std::min(bytes_consumed, bytes_to_write);

    data_offset_ += bytes_consumed;
    byte_offset += bytes_consumed;
  }

  if (close_on_write_data_)
    Close(NPRES_DONE);

  return length;
}

bool BrowserPluginStream::Close(NPReason reason) {
  if (opened_ == true) {
    opened_ = false;

    if (delivery_data_.size()) {
      if (reason == NPRES_DONE) {
        // There is more data to be streamed, don't destroy the stream now.
        close_on_write_data_ = true;
        return true;
      } else {
        // Stop any pending data from being streamed
        delivery_data_.resize(0);
      }
    }

    // If we have a temp file, be sure to close it.
    // Also, allow the plugin to access it now.
    if (TempFileIsValid()) {
      CloseTempFile();
      WriteAsFile();
    }

    if (stream_.ndata != NULL) {
      // Stream hasn't been closed yet.
      NPError err = instance_->NPP_DestroyStream(&stream_, reason);
      DCHECK(err == NPERR_NO_ERROR);
    }
  }

  Notify(reason);
  return true;
}

void BrowserPluginStream::Notify(NPReason reason) {
  if (notify_needed_) {
    instance_->NPP_URLNotify(stream_.url, reason, notify_data_);
    notify_needed_ = false;
  }
}

}  // namespace NPAPI

