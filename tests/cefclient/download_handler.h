// Copyright (c) 2010 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef _CEFCLIENT_DOWNLOAD_HANDLER_H
#define _CEFCLIENT_DOWNLOAD_HANDLER_H

#include "include/cef.h"

// Implement this interface to receive download notifications.
class DownloadListener : public virtual CefBase
{
public:
  // Called when the download is complete.
  virtual void NotifyDownloadComplete(const CefString& fileName) =0;

  // Called if the download fails.
  virtual void NotifyDownloadError(const CefString& fileName) =0;
};

// Create a new download handler to manage download of a single file.
CefRefPtr<CefDownloadHandler> CreateDownloadHandler(
    CefRefPtr<DownloadListener> listener, const CefString& fileName);

#endif // _CEFCLIENT_DOWNLOAD_HANDLER_H
