// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_RENDERER_MEDIA_CEF_KEY_SYSTEMS_H_
#define CEF_LIBCEF_RENDERER_MEDIA_CEF_KEY_SYSTEMS_H_

#include <memory>
#include <vector>

namespace media {
class KeySystemProperties;
}

// Register the key systems supported by populating |key_systems_properties|.
void AddCefKeySystems(
    std::vector<std::unique_ptr<media::KeySystemProperties>>*
        key_systems_properties);

#endif  // CEF_LIBCEF_RENDERER_MEDIA_CEF_KEY_SYSTEMS_H_
