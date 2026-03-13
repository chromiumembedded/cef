// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/no_destructor.h"
#include "cef/libcef_dll/bootstrap/installer/installer_archive.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Persistent temp dir as parent — avoids repeated OS-level temp creation.
  static base::NoDestructor<base::ScopedTempDir> parent_dir;
  [[maybe_unused]] static bool initialized = []() {
    return parent_dir->CreateUniqueTempDir();
  }();

  if (!parent_dir->IsValid()) {
    return 0;
  }

  // Per-iteration subdirectory — prevents file accumulation across runs.
  // ExtractTarFromBuffer writes files based on tar entry paths, so we
  // need a clean directory each time to avoid stale state.
  base::ScopedTempDir iter_dir;
  if (!iter_dir.CreateUniqueTempDirUnderPath(parent_dir->GetPath())) {
    return 0;
  }

  // Feed raw bytes directly as tar data — no XZ decompression layer.
  // This maximizes fuzzer throughput on the tar parsing logic.
  cef_installer::ExtractTarFromBuffer(
      base::span<const uint8_t>(data, size), iter_dir.GetPath(),
      cef_installer::ExtractionProgressCallback());
  return 0;
}
