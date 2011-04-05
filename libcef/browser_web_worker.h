// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _BROWSER_WEB_WORKER_H
#define _BROWSER_WEB_WORKER_H

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebMessagePortChannel.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebWorker.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebWorkerClient.h"

namespace WebKit {
class WebApplicationCacheHost;
class WebApplicationCacheHostClient;
class WebFrame;
class WebNotificationPresenter;
class WebString;
class WebURL;
}

// WebWorkers are not currently functional in CEF. This class effectively
// stubs things out.
class BrowserWebWorker : public WebKit::WebWorker,
                         public WebKit::WebWorkerClient,
                         public base::RefCounted<BrowserWebWorker> {
 public:
  BrowserWebWorker() {
    AddRef();  // Adds the reference held for worker object.
    AddRef();  // Adds the reference held for worker context object.
  }

  // WebWorker methods:
  virtual void startWorkerContext(const WebKit::WebURL& script_url,
                                  const WebKit::WebString& user_agent,
                                  const WebKit::WebString& source_code) {
  }
  virtual void terminateWorkerContext() {
  }
  virtual void postMessageToWorkerContext(
      const WebKit::WebString& message,
      const WebKit::WebMessagePortChannelArray& channel) {
  }
  virtual void workerObjectDestroyed() {
    Release();  // Releases the reference held for worker object.
  }
  virtual void clientDestroyed() {
  }

  // WebWorkerClient methods:
  virtual void postMessageToWorkerObject(
      const WebKit::WebString& message,
      const WebKit::WebMessagePortChannelArray& channel) {
  }
  virtual void postExceptionToWorkerObject(
      const WebKit::WebString& error_message,
      int line_number,
      const WebKit::WebString& source_url) {
  }
  virtual void postConsoleMessageToWorkerObject(
      int destination_id,
      int source_id,
      int message_type,
      int message_level,
      const WebKit::WebString& message,
      int line_number,
      const WebKit::WebString& source_url) {
  }
  virtual void confirmMessageFromWorkerObject(bool has_pending_activity) { }
  virtual void reportPendingActivity(bool has_pending_activity) { }
  virtual void workerContextClosed() { }
  virtual void workerContextDestroyed() {
    Release();    // Releases the reference held for worker context object.
  }
  virtual WebKit::WebWorker* createWorker(WebKit::WebWorkerClient* client) {
    return NULL;
  }
  virtual WebKit::WebNotificationPresenter* notificationPresenter() {
    return NULL;
  }
  virtual WebKit::WebApplicationCacheHost* createApplicationCacheHost(
      WebKit::WebApplicationCacheHostClient*) {
    return NULL;
  }
  virtual bool allowDatabase(WebKit::WebFrame* frame,
                             const WebKit::WebString& name,
                             const WebKit::WebString& display_name,
                             unsigned long estimated_size) {
    return true;
  }

 private:
  friend class base::RefCounted<BrowserWebWorker>;

  ~BrowserWebWorker() {}

  DISALLOW_COPY_AND_ASSIGN(BrowserWebWorker);
};

#endif  // _BROWSER_WEB_WORKER_H
