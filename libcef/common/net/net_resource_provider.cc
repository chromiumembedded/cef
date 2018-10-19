// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/common/net/net_resource_provider.h"

#include "chrome/common/net/net_resource_provider.h"

base::StringPiece NetResourceProvider(int key) {
  // Chrome performs substitution of localized strings for directory listings.
  base::StringPiece value = chrome_common_net::NetResourceProvider(key);
  if (value.empty())
    LOG(ERROR) << "No data resource available for id " << key;
  return value;
}
