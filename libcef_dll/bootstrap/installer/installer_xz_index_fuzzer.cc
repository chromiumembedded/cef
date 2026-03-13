// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cef/libcef_dll/bootstrap/installer/installer_xz_index.h"

#include <stddef.h>
#include <stdint.h>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/no_destructor.h"

extern "C" {
#include "third_party/lzma_sdk/src/C/7zCrc.h"
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // One-time init: create temp file and neuter CRC table so the fuzzer
  // can freely mutate headers without failing CRC checks immediately.
  // This follows the same pattern as seven_zip_reader_fuzzer.cc.
  static base::NoDestructor<base::File> temp_file;
  [[maybe_unused]] static bool initialized = []() {
    base::FilePath path;
    if (base::CreateTemporaryFile(&path)) {
      temp_file->Initialize(
          path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_READ |
                    base::File::FLAG_WRITE | base::File::FLAG_WIN_TEMPORARY |
                    base::File::FLAG_DELETE_ON_CLOSE |
                    base::File::FLAG_WIN_SHARE_DELETE);
    }

    CrcGenerateTable();
    // Zero CRC table so all CRC checks pass regardless of input.
    // See seven_zip_reader_fuzzer.cc for rationale on these values.
    for (size_t i = 0; i < 256; i++) {
      g_CrcTable[i] = 0xff000000;
      g_CrcTable[i + 0x100] = 0xff000000;
      g_CrcTable[i + 0x200] = 0;
      g_CrcTable[i + 0x300] = 0;
    }
    for (size_t i = 0; i < 256; i++) {
      g_CrcTable[i + 0x400] = 0xffffffff;
      g_CrcTable[i + 0x500] = 0;
      g_CrcTable[i + 0x600] = 0;
      g_CrcTable[i + 0x700] = 0;
    }

    return true;
  }();

  if (!temp_file->IsValid() || size == 0) {
    return 0;
  }

  // Write fuzz data to temp file and parse as XZ index.
  temp_file->SetLength(size);
  temp_file->Write(0, base::span(data, size));

  cef_installer::ParseXzIndex(*temp_file);
  return 0;
}
