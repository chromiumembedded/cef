// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser_webkit_glue.h"
#import "libcef/web_drag_utils_mac.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/sys_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "net/base/mime_util.h"
#include "net/base/net_util.h"
#import "third_party/mozilla/NSPasteboard+Utils.h"
#include "webkit/plugins/npapi/plugin_list.h"

namespace drag_util {

BOOL PopulateURLAndTitleFromPasteBoard(GURL* url,
                                       string16* title,
                                       NSPasteboard* pboard,
                                       BOOL convert_filenames) {
  CHECK(url);

  // Bail out early if there's no URL data.
  if (![pboard containsURLData])
    return NO;

  // -getURLs:andTitles:convertingFilenames: will already validate URIs so we
  // don't need to again. The arrays returned are both of NSStrings.
  NSArray* url_array = nil;
  NSArray* title_array = nil;
  [pboard getURLs:&url_array andTitles:&title_array
      convertingFilenames:convert_filenames];
  DCHECK_EQ([url_array count], [title_array count]);
  // It's possible that no URLs were actually provided!
  if (![url_array count])
    return NO;
  NSString* url_string = [url_array objectAtIndex:0];
  if ([url_string length]) {
    // Check again just to make sure to not assign NULL into a std::string,
    // which throws an exception.
    const char* utf8_url = [url_string UTF8String];
    if (utf8_url) {
      *url = GURL(utf8_url);
      // Extra paranoia check.
      if (title && [title_array count])
        *title = base::SysNSStringToUTF16([title_array objectAtIndex:0]);
    }
  }
  return YES;
}

GURL GetFileURLFromDropData(id<NSDraggingInfo> info) {
  if ([[info draggingPasteboard] containsURLData]) {
    GURL url;
    PopulateURLAndTitleFromPasteBoard(&url,
                                      NULL,
                                      [info draggingPasteboard],
                                      YES);

    if (url.SchemeIs("file"))
      return url;
  }
  return GURL();
}

static BOOL IsSupportedFileURL(const GURL& url) {
  FilePath full_path;
  net::FileURLToFilePath(url, &full_path);

  std::string mime_type;
  net::GetMimeTypeFromFile(full_path, &mime_type);

  // This logic mirrors |BufferedResourceHandler::ShouldDownload()|.
  // TODO(asvitkine): Refactor this out to a common location instead of
  //                  duplicating code.
  if (net::IsSupportedMimeType(mime_type))
    return YES;

  // Check whether there is a plugin that supports the mime type. (e.g. PDF)
  bool allow_wildcard = false;
  std::vector<webkit::WebPluginInfo> plugins;
  webkit::npapi::PluginList::Singleton()->GetPluginInfoArray(
      GURL(), mime_type, allow_wildcard, NULL, &plugins, NULL);

  // If any associated plugins exist and are enabled don't allow the download.
  if (!plugins.empty()) {
    std::vector<webkit::WebPluginInfo>::const_iterator it = plugins.begin();
    for (; it != plugins.end(); ++it) {
      if (webkit_glue::IsPluginEnabled(*it))
        return YES;
    }
  }

  return NO;
}

BOOL IsUnsupportedDropData(id<NSDraggingInfo> info) {
  GURL url = GetFileURLFromDropData(info);
  if (!url.is_empty()) {
    // If dragging a file, only allow dropping supported file types (that the
    // web view can display).
    return !IsSupportedFileURL(url);
  }
  return NO;
}

}  // namespace drag_util
