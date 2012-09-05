// Copyright (c) 2010 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"

#include "third_party/WebKit/Source/WebCore/config.h"

#include "browser_webkit_glue.h"

#undef LOG
#include "base/file_util.h"
#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/path_service.h"
#include "base/utf_string_conversions.h"
#include "grit/webkit_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/glue/webkit_glue.h"


namespace webkit_glue {

FilePath GetResourcesFilePath() {
  // Start out with the path to the running executable.
  FilePath execPath;
  PathService::Get(base::FILE_EXE, &execPath);

  // Get the main bundle path.
  FilePath bundlePath = base::mac::GetAppBundlePath(execPath);

  return bundlePath.Append(FILE_PATH_LITERAL("Contents"))
                   .Append(FILE_PATH_LITERAL("Resources"));
}

base::StringPiece GetDataResource(int resource_id) {
  switch (resource_id) {
    case IDR_BROKENIMAGE: {
      // Use webkit's broken image icon (16x16)
      static std::string broken_image_data;
      if (broken_image_data.empty()) {
        FilePath path = GetResourcesFilePath();
        // In order to match WebKit's colors for the missing image, we have to
        // use a PNG. The GIF doesn't have the color range needed to correctly
        // match the TIFF they use in Safari.
        path = path.AppendASCII("missingImage.png");
        bool success = file_util::ReadFileToString(path, &broken_image_data);
        if (!success) {
          LOG(FATAL) << "Failed reading: " << path.value();
        }
      }
      return broken_image_data;
    }
    case IDR_TEXTAREA_RESIZER: {
      // Use webkit's text area resizer image.
      static std::string resize_corner_data;
      if (resize_corner_data.empty()) {
        FilePath path = GetResourcesFilePath();
        path = path.AppendASCII("textAreaResizeCorner.png");
        bool success = file_util::ReadFileToString(path, &resize_corner_data);
        if (!success) {
          LOG(FATAL) << "Failed reading: " << path.value();
        }
      }
      return resize_corner_data;
    }

    default:
      break;
  }

  base::StringPiece piece =
      ResourceBundle::GetSharedInstance().GetRawDataResource(resource_id);
  DCHECK(!piece.empty());
  return piece;
}

void DidLoadPlugin(const std::string& filename) {
}

void DidUnloadPlugin(const std::string& filename) {
}
  
}  // namespace webkit_glue
