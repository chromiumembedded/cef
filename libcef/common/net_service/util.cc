// Copyright (c) 2019 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/common/net_service/util.h"

#include "base/feature_list.h"
#include "services/network/public/cpp/features.h"

namespace net_service {

bool IsEnabled() {
  return base::FeatureList::IsEnabled(network::features::kNetworkService);
}

}  // namespace net_service
