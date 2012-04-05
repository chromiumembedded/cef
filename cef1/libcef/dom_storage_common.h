// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_DOM_STORAGE_COMMON_H_
#define CEF_LIBCEF_DOM_STORAGE_COMMON_H_
#pragma once

#include "base/basictypes.h"

const int64 kLocalStorageNamespaceId = 0;
const int64 kInvalidSessionStorageNamespaceId = kLocalStorageNamespaceId;

enum DOMStorageType {
  DOM_STORAGE_LOCAL = 0,
  DOM_STORAGE_SESSION
};

#endif  // CEF_LIBCEF_DOM_STORAGE_COMMON_H_
