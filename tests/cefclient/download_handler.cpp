// Copyright (c) 2010 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "download_handler.h"
#include "util.h"
#include <sstream>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#endif // _WIN32


namespace {

// Template for creating a task that executes a method with no arguments.
template <class T, class Method>
class Task : public CefThreadSafeBase<CefTask>
{
public:
  Task(T* object, Method method)
    : object_(object), method_(method) {}

  virtual void Execute(CefThreadId threadId)
  {
    (object_->*method_)();
  }

protected:
  CefRefPtr<T> object_;
  Method method_;
};

// Helper method for posting a task on a specific thread.
template <class T, class Method>
inline void PostOnThread(CefThreadId threadId,
                         T* object,
                         Method method) {
  CefRefPtr<CefTask> task = new Task<T,Method>(object, method);
  CefPostTask(threadId, task);
}

} // namespace

// Implementation of the CefDownloadHandler interface.
class ClientDownloadHandler : public CefThreadSafeBase<CefDownloadHandler>
{
public:
  ClientDownloadHandler(CefRefPtr<DownloadListener> listener,
                        const std::wstring& fileName)
    : listener_(listener), filename_(fileName), file_(NULL)
  {
    // Open the file on the FILE thread.
    PostOnThread(TID_FILE, this, &ClientDownloadHandler::OnOpen);
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
      class TaskCloseFile : public CefThreadSafeBase<CefTask>
      {
      public:
        TaskCloseFile(FILE* file) : file_(file) {}
        virtual void Execute(CefThreadId threadId) { fclose(file_); }
      private:
        FILE* file_;
      };
      CefPostTask(TID_FILE, new TaskCloseFile(file_));
      
      // Notify the listener that the download failed.
      listener_->NotifyDownloadError(filename_);
    }
  }

  // --------------------------------------------------
  // The following methods are called on the UI thread.
  // --------------------------------------------------

  // A portion of the file contents have been received. This method will be
  // called multiple times until the download is complete. Return |true| to
  // continue receiving data and |false| to cancel.
  virtual bool ReceivedData(void* data, int data_size)
  {
    ASSERT(CefCurrentlyOn(TID_UI));

    if(data_size == 0)
      return true;

    // Create a new vector for the data.
    std::vector<char>* buffer = new std::vector<char>(data_size);
    memcpy(&(*buffer)[0], data, data_size);

    // Add the new data vector to the pending data queue.
    Lock();
    pending_data_.push_back(buffer);
    Unlock();

    // Write data to file on the FILE thread.
    PostOnThread(TID_FILE, this, &ClientDownloadHandler::OnReceivedData);
    return true;
  }

  // The download is complete.
  virtual void Complete()
  {
    ASSERT(CefCurrentlyOn(TID_UI));

    // Flush and close the file on the FILE thread.
    PostOnThread(TID_FILE, this, &ClientDownloadHandler::OnComplete);
  }

  // ----------------------------------------------------
  // The following methods are called on the FILE thread.
  // ----------------------------------------------------

  void OnOpen()
  {
    ASSERT(CefCurrentlyOn(TID_FILE));

    if(file_)
      return;
    
#ifdef _WIN32
    TCHAR szFolderPath[MAX_PATH];

    // Save the file in the user's "My Documents" folder.
    if(SUCCEEDED(SHGetFolderPath(NULL, CSIDL_PERSONAL|CSIDL_FLAG_CREATE, 
                                 NULL, 0, szFolderPath))) {
      LPWSTR name = PathFindFileName(filename_.c_str());
      LPWSTR ext = PathFindExtension(filename_.c_str());
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

      Lock();
      filename_ = ss.str();
      Unlock();

      file_ = _wfopen(ss.str().c_str(), L"wb");
      ASSERT(file_ != NULL);
    }
#endif // _WIN32
  }

  void OnComplete()
  {
    ASSERT(CefCurrentlyOn(TID_FILE));

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
    ASSERT(CefCurrentlyOn(TID_FILE));

    std::vector<std::vector<char>*> data;

    // Remove all data from the pending data queue.
    Lock();
    if(!pending_data_.empty()) {
      data = pending_data_;
      pending_data_.clear();
    }
    Unlock();

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

private:
  CefRefPtr<DownloadListener> listener_;
  std::wstring filename_;
  FILE* file_;
  std::vector<std::vector<char>*> pending_data_;
};

CefRefPtr<CefDownloadHandler> CreateDownloadHandler(
    CefRefPtr<DownloadListener> listener, const std::wstring& fileName)
{
  return new ClientDownloadHandler(listener, fileName);
}
