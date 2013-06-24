// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/common/net_resource_provider.h"
#include "content/public/common/content_client.h"

base::StringPiece NetResourceProvider(int key) {
  return content::GetContentClient()->GetDataResource(key,
                                                      ui::SCALE_FACTOR_NONE);
}
