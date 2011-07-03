// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webkit_glue.h"

#include <string>

#include "base/lazy_instance.h"
#include "base/stl_util-inl.h"
#include "base/string16.h"
#include "googleurl/src/gurl.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/zlib/zlib.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/size.h"
#include "webkit/glue/scoped_clipboard_writer_glue.h"

// Clipboard glue

void ScopedClipboardWriterGlue::WriteBitmapFromPixels(
    const void* pixels, const gfx::Size& size) {
  ScopedClipboardWriter::WriteBitmapFromPixels(pixels, size);
}

ScopedClipboardWriterGlue::~ScopedClipboardWriterGlue() {
}

namespace webkit_glue {

base::LazyInstance<ui::Clipboard> clipboard(base::LINKER_INITIALIZED);

ui::Clipboard* ClipboardGetClipboard() {
  return clipboard.Pointer();
}

bool ClipboardIsFormatAvailable(const ui::Clipboard::FormatType& format,
                                ui::Clipboard::Buffer buffer) {
  return ClipboardGetClipboard()->IsFormatAvailable(format, buffer);
}

// TODO(dcheng): Implement.
void ClipboardReadAvailableTypes(ui::Clipboard::Buffer buffer,
                                 std::vector<string16>* types,
                                 bool* contains_filenames) {
  return;
}

void ClipboardReadText(ui::Clipboard::Buffer buffer, string16* result) {
  ClipboardGetClipboard()->ReadText(buffer, result);
}

void ClipboardReadAsciiText(ui::Clipboard::Buffer buffer, std::string* result) {
  ClipboardGetClipboard()->ReadAsciiText(buffer, result);
}

void ClipboardReadHTML(ui::Clipboard::Buffer buffer, string16* markup,
                       GURL* url) {
  std::string url_str;
  ClipboardGetClipboard()->ReadHTML(buffer, markup, url ? &url_str : NULL);
  if (url)
    *url = GURL(url_str);
}

void ClipboardReadImage(ui::Clipboard::Buffer buffer, std::string* data) {
  SkBitmap bitmap = ClipboardGetClipboard()->ReadImage(buffer);
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

bool ClipboardReadData(ui::Clipboard::Buffer buffer, const string16& type,
                       string16* data, string16* metadata) {
  return false;
}

bool ClipboardReadFilenames(ui::Clipboard::Buffer buffer,
                            std::vector<string16>* filenames) {
  return false;
}

uint64 ClipboardGetSequenceNumber() {
  return 0;
}

}  // namespace webkit_glue
