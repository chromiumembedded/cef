// Copyright (c) 2008 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _BROWSER_PLUGIN_STREAM_URL_H
#define _BROWSER_PLUGIN_STREAM_URL_H

#include "browser_plugin_stream.h"

#include "webkit/glue/webplugin.h"
#include "googleurl/src/gurl.h"

namespace NPAPI {

class BrowserPluginInstance;

// A NPAPI Stream based on a URL.
class BrowserPluginStreamUrl : public BrowserPluginStream,
                               public WebPluginResourceClient {
 public:
  // Create a new stream for sending to the plugin by fetching
  // a URL. If notifyNeeded is set, then the plugin will be notified
  // when the stream has been fully sent to the plugin.  Initialize
  // must be called before the object is used.
  BrowserPluginStreamUrl(int resource_id,
                  const GURL &url,
                  BrowserPluginInstance *instance,
                  bool notify_needed,
                  void *notify_data);
  virtual ~BrowserPluginStreamUrl();

  // Stop sending the stream to the client.
  // Overrides the base Close so we can cancel our fetching the URL if
  // it is still loading.
  virtual bool Close(NPReason reason);

  virtual WebPluginResourceClient* AsResourceClient() {
    return static_cast<WebPluginResourceClient*>(this);
  }

  virtual void CancelRequest();

  //
  // WebPluginResourceClient methods
  //
  void WillSendRequest(const GURL& url);
  void DidReceiveResponse(const std::string& mime_type,
                          const std::string& headers,
                          uint32 expected_length,
                          uint32 last_modified,
                          bool request_is_seekable,
                          bool* cancel);
  void DidReceiveData(const char* buffer, int length, int data_offset);
  void DidFinishLoading();
  void DidFail();
  bool IsMultiByteResponseExpected() {
    return seekable();
  }

 private:
  GURL url_;
  int id_;

  DISALLOW_EVIL_CONSTRUCTORS(BrowserPluginStreamUrl);
};

} // namespace NPAPI

#endif // _BROWSER_PLUGIN_STREAM_URL_H
