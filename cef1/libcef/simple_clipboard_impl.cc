// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/simple_clipboard_impl.h"

#include "base/lazy_instance.h"
#include "base/stl_util.h"
#include "base/string16.h"
#include "googleurl/src/gurl.h"
#include "third_party/skia/include/core/SkBitmap.h"
#if defined(USE_SYSTEM_ZLIB)
#include <zlib.h>
#else
#include "third_party/zlib/zlib.h"
#endif
#include "ui/base/clipboard/clipboard.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/size.h"
#include "webkit/glue/webkit_glue.h"

SimpleClipboardClient::SimpleClipboardClient() {
}

SimpleClipboardClient::~SimpleClipboardClient() {
}

ui::Clipboard* SimpleClipboardClient::GetClipboard() {
  return ui::Clipboard::GetForCurrentThread();
}

uint64 SimpleClipboardClient::GetSequenceNumber(ui::Clipboard::Buffer buffer) {
  return GetClipboard()->GetSequenceNumber(buffer);
}

bool SimpleClipboardClient::IsFormatAvailable(
    const ui::Clipboard::FormatType& format,
    ui::Clipboard::Buffer buffer) {
  return GetClipboard()->IsFormatAvailable(format, buffer);
}

void SimpleClipboardClient::Clear(ui::Clipboard::Buffer buffer) {
  GetClipboard()->Clear(buffer);
}

void SimpleClipboardClient::ReadAvailableTypes(ui::Clipboard::Buffer buffer,
                                               std::vector<string16>* types,
                                               bool* contains_filenames) {
  return GetClipboard()->ReadAvailableTypes(buffer, types,
                                                     contains_filenames);
}

void SimpleClipboardClient::ReadText(ui::Clipboard::Buffer buffer,
                                     string16* result) {
  GetClipboard()->ReadText(buffer, result);
}

void SimpleClipboardClient::ReadAsciiText(ui::Clipboard::Buffer buffer,
                                          std::string* result) {
  GetClipboard()->ReadAsciiText(buffer, result);
}

void SimpleClipboardClient::ReadHTML(ui::Clipboard::Buffer buffer,
                                     string16* markup,
                                     GURL* url, uint32* fragment_start,
                                     uint32* fragment_end) {
  std::string url_str;
  GetClipboard()->ReadHTML(buffer, markup, url ? &url_str : NULL,
                           fragment_start, fragment_end);
  if (url)
    *url = GURL(url_str);
}

void SimpleClipboardClient::ReadRTF(ui::Clipboard::Buffer buffer,
                                    std::string* result) {
  GetClipboard()->ReadRTF(buffer, result);
}

void SimpleClipboardClient::ReadImage(ui::Clipboard::Buffer buffer,
                                      std::string* data) {
  SkBitmap bitmap = GetClipboard()->ReadImage(buffer);
  if (bitmap.isNull())
    return;

  std::vector<unsigned char> png_data;
  SkAutoLockPixels lock(bitmap);
  if (gfx::PNGCodec::EncodeWithCompressionLevel(
          static_cast<const unsigned char*>(bitmap.getPixels()),
          gfx::PNGCodec::FORMAT_BGRA,
          gfx::Size(bitmap.width(), bitmap.height()),
          bitmap.rowBytes(),
          false,
          std::vector<gfx::PNGCodec::Comment>(),
          Z_BEST_SPEED,
          &png_data)) {
    data->assign(reinterpret_cast<char*>(vector_as_array(&png_data)),
                 png_data.size());
  }
}

void SimpleClipboardClient::ReadCustomData(ui::Clipboard::Buffer buffer,
                                           const string16& type,
                                           string16* data) {
  GetClipboard()->ReadCustomData(buffer, type, data);
}

webkit_glue::ClipboardClient::WriteContext*
SimpleClipboardClient::CreateWriteContext() {
  return NULL;
}
