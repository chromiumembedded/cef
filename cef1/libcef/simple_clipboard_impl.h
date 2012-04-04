// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_SIMPLE_CLIPBOARD_IMPL_H_
#define CEF_LIBCEF_SIMPLE_CLIPBOARD_IMPL_H_
#pragma once

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "webkit/glue/clipboard_client.h"

class SimpleClipboardClient : public webkit_glue::ClipboardClient {
 public:
  SimpleClipboardClient();
  virtual ~SimpleClipboardClient();

  virtual ui::Clipboard* GetClipboard() OVERRIDE;
  virtual uint64 GetSequenceNumber(ui::Clipboard::Buffer buffer) OVERRIDE;
  virtual bool IsFormatAvailable(const ui::Clipboard::FormatType& format,
                                 ui::Clipboard::Buffer buffer) OVERRIDE;
  virtual void Clear(ui::Clipboard::Buffer buffer) OVERRIDE;
  virtual void ReadAvailableTypes(ui::Clipboard::Buffer buffer,
                                  std::vector<string16>* types,
                                  bool* contains_filenames) OVERRIDE;
  virtual void ReadText(ui::Clipboard::Buffer buffer,
                        string16* result) OVERRIDE;
  virtual void ReadAsciiText(ui::Clipboard::Buffer buffer,
                             std::string* result) OVERRIDE;
  virtual void ReadHTML(ui::Clipboard::Buffer buffer, string16* markup,
                        GURL* url, uint32* fragment_start,
                        uint32* fragment_end) OVERRIDE;
  virtual void ReadImage(ui::Clipboard::Buffer buffer,
                         std::string* data) OVERRIDE;
  virtual void ReadCustomData(ui::Clipboard::Buffer buffer,
                              const string16& type,
                              string16* data) OVERRIDE;
  virtual WriteContext* CreateWriteContext() OVERRIDE;
};

#endif  // CEF_LIBCEF_SIMPLE_CLIPBOARD_IMPL_H_
