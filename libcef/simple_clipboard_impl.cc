// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "precompiled_libcef.h"
#include "webkit/glue/webkit_glue.h"

#include <string>

#include "base/clipboard.h"
#include "base/lazy_instance.h"
#include "base/string16.h"
#include "googleurl/src/gurl.h"
#include "webkit/glue/scoped_clipboard_writer_glue.h"

#include "SkBitmap.h"

// Clipboard glue

#if defined(OS_WIN)
void ScopedClipboardWriterGlue::WriteBitmapFromPixels(
    const void* pixels, const gfx::Size& size) {
  ScopedClipboardWriter::WriteBitmapFromPixels(pixels, size);
}
#endif

ScopedClipboardWriterGlue::~ScopedClipboardWriterGlue() {
}

namespace webkit_glue {

base::LazyInstance<Clipboard> clipboard(base::LINKER_INITIALIZED);

Clipboard* ClipboardGetClipboard() {
  return clipboard.Pointer();
}

bool ClipboardIsFormatAvailable(const Clipboard::FormatType& format) {
  return ClipboardGetClipboard()->IsFormatAvailable(format);
}

void ClipboardReadText(string16* result) {
  ClipboardGetClipboard()->ReadText(result);
}

void ClipboardReadAsciiText(std::string* result) {
  ClipboardGetClipboard()->ReadAsciiText(result);
}

void ClipboardReadHTML(string16* markup, GURL* url) {
  std::string url_str;
  ClipboardGetClipboard()->ReadHTML(markup, url ? &url_str : NULL);
  if (url)
    *url = GURL(url_str);
}

}  // namespace webkit_glue
