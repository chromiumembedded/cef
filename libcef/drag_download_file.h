// Copyright (c) 2011 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2009-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _DRAG_DOWNLOAD_FILE_H
#define _DRAG_DOWNLOAD_FILE_H
#pragma once

#include "base/file_path.h"
#include "base/memory/linked_ptr.h"
#include "base/message_loop.h"
#include "googleurl/src/gurl.h"
#include "ui/base/dragdrop/download_file_interface.h"

class BrowserWebViewDelegate;

namespace net {
class FileStream;
}

class DragDownloadFile : public ui::DownloadFileProvider {
 public:
  // On Windows, we need to download into a temporary file. Two threads are
  // involved: background drag-and-drop thread and UI thread.
  // The first parameter file_name_or_path should contain file name while the
  // second parameter file_stream should be NULL.
  //
  // On MacOSX, we need to download into a file stream that has already been
  // created. Only UI thread is involved.
  // The file path and file stream should be provided as the first two
  // parameters.
  DragDownloadFile(const FilePath& file_name_or_path,
                   linked_ptr<net::FileStream> file_stream,
                   const GURL& url,
                   const GURL& referrer,
                   const std::string& referrer_encoding,
                   BrowserWebViewDelegate* view);

  // DownloadFileProvider methods.
  // Called on drag-and-drop thread (Windows).
  // Called on UI thread (MacOSX).
  virtual bool Start(ui::DownloadFileObserver* observer);
  virtual void Stop();
#if defined(OS_WIN)
  virtual IStream* GetStream() { return NULL; }
#endif

 private:
  // Called on drag-and-drop thread (Windows).
  // Called on UI thread (Windows).
  virtual ~DragDownloadFile();

  // Called on drag-and-drop thread (Windows only).
#if defined(OS_WIN)
  void StartNestedMessageLoop();
  void QuitNestedMessageLoop();
#endif

  // Called on either drag-and-drop thread or UI thread (Windows).
  // Called on UI thread (MacOSX).
  void InitiateDownload();
  void DownloadCompleted(bool is_successful);

  // Helper methods to make sure we're in the correct thread.
  void AssertCurrentlyOnDragThread();
  void AssertCurrentlyOnUIThread();

  // Initialized on drag-and-drop thread. Accessed on either thread after that
  // (Windows).
  // Accessed on UI thread (MacOSX).
  FilePath file_path_;
  FilePath file_name_;
  linked_ptr<net::FileStream> file_stream_;
  GURL url_;
  GURL referrer_;
  std::string referrer_encoding_;
  BrowserWebViewDelegate* view_;
  MessageLoop* drag_message_loop_;
  FilePath temp_dir_path_;

  // Accessed on drag-and-drop thread (Windows).
  // Accessed on UI thread (MacOSX).
  bool is_started_;
  bool is_successful_;
  scoped_refptr<ui::DownloadFileObserver> observer_;

  // Accessed on drag-and-drop thread (Windows only).
#if defined(OS_WIN)
  bool is_running_nested_message_loop_;
#endif

  DISALLOW_COPY_AND_ASSIGN(DragDownloadFile);
};

#endif  // _DRAG_DOWNLOAD_FILE_H
