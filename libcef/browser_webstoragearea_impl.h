// Copyright (c) 2010 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef _BROWSER_WEBSTORAGEAREA_IMPL_H
#define _BROWSER_WEBSTORAGEAREA_IMPL_H

#include "base/basictypes.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebStorageArea.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"

class DOMStorageArea;

class BrowserWebStorageAreaImpl : public WebKit::WebStorageArea {
 public:
  BrowserWebStorageAreaImpl(int64 namespace_id, const WebKit::WebString& origin);
  virtual ~BrowserWebStorageAreaImpl();

  // See WebStorageArea.h for documentation on these functions.
  virtual unsigned length();
  virtual WebKit::WebString key(unsigned index);
  virtual WebKit::WebString getItem(const WebKit::WebString& key);
  virtual void setItem(
      const WebKit::WebString& key, const WebKit::WebString& value,
      const WebKit::WebURL& url, WebStorageArea::Result& result,
      WebKit::WebString& old_value);
  virtual void removeItem(
      const WebKit::WebString& key, const WebKit::WebURL& url,
      WebKit::WebString& old_value);
  virtual void clear(const WebKit::WebURL& url, bool& cleared_something);

 private:
  // The object is owned by DOMStorageNamespace.
  DOMStorageArea* area_;
};

#endif  // _BROWSER_WEBSTORAGEAREA_IMPL_H
