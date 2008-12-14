// Copyright (c) 2008 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _BROWSER_PLUGIN_STRING_STREAM_H
#define _BROWSER_PLUGIN_STRING_STREAM_H

#include "browser_plugin_stream.h"

namespace NPAPI {

class BrowserPluginInstance;

// An NPAPI stream from a string.
class BrowserPluginStringStream : public BrowserPluginStream {
 public:
  // Create a new stream for sending to the plugin.
  // If notify_needed, will notify the plugin after the data has
  // all been sent.
  BrowserPluginStringStream(BrowserPluginInstance *instance,
                     const std::string &url,
                     bool notify_needed,
                     void *notify_data);
  virtual ~BrowserPluginStringStream();

  // Initiates the sending of data to the plugin.
  void SendToPlugin(const std::string &data,
                    const std::string &mime_type);

 private:

  DISALLOW_EVIL_CONSTRUCTORS(BrowserPluginStringStream);
};

} // namespace NPAPI

#endif // _BROWSER_PLUGIN_STRING_STREAM_H

