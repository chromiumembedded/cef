// Copyright (c) 2008-2009 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "browser_webkit_glue.h"
#include "cef_context.h"
#include "ui/base/resource/resource_bundle.h"

namespace webkit_glue {

base::StringPiece GetDataResource(int resource_id) {
  base::StringPiece piece;
  // Try to load the resource from the resource pack.
  if (piece.empty())
    piece = ResourceBundle::GetSharedInstance().GetRawDataResource(resource_id);

  DCHECK(!piece.empty()) << "Resource "<<resource_id<<" could not be loaded";
  return piece;
}

}
