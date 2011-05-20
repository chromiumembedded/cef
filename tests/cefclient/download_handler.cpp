// Copyright (c) 2010 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef_runnable.h"
#include "download_handler.h"
#include "util.h"
#include <sstream>
#include <stdio.h>
#include <vector>

#if defined(OS_WIN)
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#endif // OS_WIN

// Implementation of the CefDownloadHandler interface.
class ClientDownloadHandler : public CefDownloadHandler
{
public:
  ClientDownloadHandler(CefRefPtr<DownloadListener> listener,
                        const CefString& fileName)
    : listener_(listener), filename_(fileName), file_(NULL)
  {
  }

  ~ClientDownloadHandler()
  {
    ASSERT(pending_data_.empty());
    ASSERT(file_ == NULL);
    
    if(!pending_data_.empty()) {
      // Delete remaining pending data.
      std::vector<std::vector<char>*>::iterator it = pending_data_.begin();
      for(; it != pending_data_.end(); ++it)
        delete (*it);
    }
    
    if(file_) {
      // Close the dangling file pointer on the FILE thread.
      CefPostTask(TID_FILE,
          NewCefRunnableFunction(&ClientDownloadHandler::CloseDanglingFile,
                                 file_));
      
      // Notify the listener that the download failed.
      listener_->NotifyDownloadError(filename_);
    }
  }

  // --------------------------------------------------
  // The following methods are called on the UI thread.
  // --------------------------------------------------

  void Initialize()
  {
    // Open the file on the FILE thread.
    CefPostTask(TID_FILE,
        NewCefRunnableMethod(this, &ClientDownloadHandler::OnOpen));
  }

  // A portion of the file contents have been received. This method will be
  // called multiple times until the download is complete. Return |true| to
  // continue receiving data and |false| to cancel.
  virtual bool ReceivedData(void* data, int data_size)
  {
    REQUIRE_UI_THREAD();

    if(data_size == 0)
      return true;

    // Create a new vector for the data.
    std::vector<char>* buffer = new std::vector<char>(data_size);
    memcpy(&(*buffer)[0], data, data_size);

    // Add the new data vector to the pending data queue.
    {
      AutoLock lock_scope(this);
      pending_data_.push_back(buffer);
    }

    // Write data to file on the FILE thread.
    CefPostTask(TID_FILE,
        NewCefRunnableMethod(this, &ClientDownloadHandler::OnReceivedData));
    return true;
  }

  // The download is complete.
  virtual void Complete()
  {
    REQUIRE_UI_THREAD();

    // Flush and close the file on the FILE thread.
    CefPostTask(TID_FILE,
        NewCefRunnableMethod(this, &ClientDownloadHandler::OnComplete));
  }

  // ----------------------------------------------------
  // The following methods are called on the FILE thread.
  // ----------------------------------------------------

  void OnOpen()
  {
    REQUIRE_FILE_THREAD();

    if(file_)
      return;
    
#if defined(OS_WIN)
    TCHAR szFolderPath[MAX_PATH];

    // Save the file in the user's "My Documents" folder.
    if(SUCCEEDED(SHGetFolderPath(NULL, CSIDL_PERSONAL|CSIDL_FLAG_CREATE, 
                                 NULL, 0, szFolderPath))) {
      std::wstring fileNameStr = filename_;
      LPWSTR name = PathFindFileName(fileNameStr.c_str());
      LPWSTR ext = PathFindExtension(fileNameStr.c_str());
      int ct = 0;
      std::wstringstream ss;

      if(ext) {
        name[ext-name] = 0;
        ext++;
      }

      // Make sure the file name is unique.
      do {
        if(ct > 0)
          ss.str(L"");
        ss << szFolderPath << L"\\" << name;
        if(ct > 0)
          ss << L" (" << ct << L")";
        if(ext)
          ss << L"." << ext;
        ct++;
      } while(PathFileExists(ss.str().c_str()));

      {
        AutoLock lock_scope(this);
        filename_ = ss.str();
      }

      file_ = _wfopen(ss.str().c_str(), L"wb");
      ASSERT(file_ != NULL);
    }
#else
    // TODO(port): Implement this.
    ASSERT(false); // Not implemented
#endif
  }

  void OnComplete()
  {
    REQUIRE_FILE_THREAD();

    if(!file_)
      return;

    // Make sure any pending data is written.
    OnReceivedData();

    fclose(file_);
    file_ = NULL;

    // Notify the listener that the download completed.
    listener_->NotifyDownloadComplete(filename_);
  }

  void OnReceivedData()
  {
    REQUIRE_FILE_THREAD();

    std::vector<std::vector<char>*> data;

    // Remove all data from the pending data queue.
    {
      AutoLock lock_scope(this);
      if(!pending_data_.empty()) {
        data = pending_data_;
        pending_data_.clear();
      }
    }

    if(data.empty())
      return;

    // Write all pending data to file.
    std::vector<std::vector<char>*>::iterator it = data.begin();
    for(; it != data.end(); ++it) {
      std::vector<char>* buffer = *it;
      if(file_)
        fwrite(&(*buffer)[0], buffer->size(), 1, file_);
      delete buffer;
    }
    data.clear();
  }

  static void CloseDanglingFile(FILE *file)
  {
    fclose(file);
  }

private:
  CefRefPtr<DownloadListener> listener_;
  CefString filename_;
  FILE* file_;
  std::vector<std::vector<char>*> pending_data_;

  IMPLEMENT_REFCOUNTING(ClientDownloadHandler);
  IMPLEMENT_LOCKING(ClientDownloadHandler);
};

CefRefPtr<CefDownloadHandler> CreateDownloadHandler(
    CefRefPtr<DownloadListener> listener, const CefString& fileName)
{
  CefRefPtr<ClientDownloadHandler> handler =
      new ClientDownloadHandler(listener, fileName);
  handler->Initialize();
  return handler.get();
}
