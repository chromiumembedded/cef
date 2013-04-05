// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_WEB_DRAG_UTILS_MAC_H_
#define CEF_LIBCEF_WEB_DRAG_UTILS_MAC_H_
#pragma once

#import <Cocoa/Cocoa.h>

#include "base/basictypes.h"
#include "googleurl/src/gurl.h"

namespace drag_util {

// Populates the |url| and |title| with URL data in |pboard|. There may be more
// than one, but we only handle dropping the first. |url| must not be |NULL|;
// |title| is an optional parameter. Returns |YES| if URL data was obtained from
// the pasteboard, |NO| otherwise. If |convert_filenames| is |YES|, the function
// will also attempt to convert filenames in |pboard| to file URLs.
BOOL PopulateURLAndTitleFromPasteBoard(GURL* url,
                                       string16* title,
                                       NSPasteboard* pboard,
                                       BOOL convert_filenames);

// Returns the first file URL from |info|, if there is one. If |info| doesn't
// contain any file URLs, an empty |GURL| is returned.
GURL GetFileURLFromDropData(id<NSDraggingInfo> info);

// Determines whether the given drag and drop operation contains content that
// is supported by the web view. In particular, if the content is a local file
// URL, this checks if it is of a type that can be shown in the tab contents.
BOOL IsUnsupportedDropData(id<NSDraggingInfo> info);

}  // namespace drag_util

#endif  // CEF_LIBCEF_WEB_DRAG_UTILS_MAC_H_
