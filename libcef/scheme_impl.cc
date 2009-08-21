// Copyright (c) 2009 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "precompiled_libcef.h"
#include "base/lazy_instance.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/worker_pool.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/upload_data.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_job.h"

#include "include/cef.h"
#include "tracker.h"
#include "context.h"
#include "request_impl.h"

#include <map>


// Memory manager.

base::LazyInstance<CefTrackManager> g_scheme_tracker(base::LINKER_INITIALIZED);

class TrackBase : public CefTrackObject
{
public:
  TrackBase(CefBase* base) { base_ = base; }

protected:
  CefRefPtr<CefBase> base_;
};

static void TrackAdd(CefTrackObject* object)
{
  g_scheme_tracker.Pointer()->Add(object);
}

static void TrackDelete(CefTrackObject* object)
{
  g_scheme_tracker.Pointer()->Delete(object);
}


// URLRequestJob implementation.

class CefUrlRequestJob : public URLRequestJob {
public:
  CefUrlRequestJob(URLRequest* request, CefRefPtr<CefSchemeHandler> handler)
    : URLRequestJob(request),
    url_(request->url()),
    handler_(handler),
    response_length_(0),
    remaining_bytes_(0) {  }

  virtual ~CefUrlRequestJob(){}

  virtual void Start()
  {
    handler_->Cancel();
    // Continue asynchronously.
    DCHECK(!async_resolver_);
    async_resolver_ = new AsyncResolver(this);
    WorkerPool::PostTask(FROM_HERE, NewRunnableMethod(
        async_resolver_.get(), &AsyncResolver::Resolve, url_), true);
    return;
  }

  virtual void Kill()
  {
    if (async_resolver_) {
      async_resolver_->Cancel();
      async_resolver_ = NULL;
    }

    URLRequestJob::Kill();
  }

  virtual bool ReadRawData(net::IOBuffer* dest, int dest_size, int *bytes_read)
  {
    DCHECK_NE(dest_size, 0);
    DCHECK(bytes_read);

    // When remaining_bytes_>=0, it means the handler knows the content size
    // before hand. We continue to read until 
    if (remaining_bytes_>=0) {
      if (remaining_bytes_ < dest_size)
        dest_size = static_cast<int>(remaining_bytes_);

      // If we should copy zero bytes because |remaining_bytes_| is zero, short
      // circuit here.
      if (!dest_size) {
        *bytes_read = 0;
        return true;
      }

      // remaining_bytes > 0
      bool rv = handler_->ReadResponse(dest->data(), dest_size, bytes_read);
      remaining_bytes_ -= *bytes_read;
      if (!rv) {
        // handler indicated no further data to read
        *bytes_read = 0;
      }
      return true;
    } else {
      // The handler returns -1 for GetResponseLength, this means the handler
      // doesn't know the content size before hand. We do basically the same
      // thing, except for checking the return value for handler_->ReadResponse,
      // which is an indicator for no further data to be read.
      bool rv = handler_->ReadResponse(dest->data(), dest_size, bytes_read);
      if (!rv)
        // handler indicated no further data to read
        *bytes_read = 0;
      return true;
    }
  }

  virtual bool IsRedirectResponse(GURL* location, int* http_status_code)
  {
    return false;
  }

  virtual bool GetContentEncodings(
      std::vector<Filter::FilterType>* encoding_types)
  {
    DCHECK(encoding_types->empty());

    return !encoding_types->empty();
  }

  virtual bool GetMimeType(std::string* mime_type) const
  {
    DCHECK(request_);
    // call handler to get mime type
    *mime_type = mime_type_;
    return true;
  }

  virtual void SetExtraRequestHeaders(const std::string& headers)
  {
  }

  CefRefPtr<CefSchemeHandler> handler_;
  std::string mime_type_;
  int response_length_;

protected:
  GURL url_;

private:
  void DidResolve(const GURL& url)
  {
    async_resolver_ = NULL;

    // We may have been orphaned...
    if (!request_)
      return;

    remaining_bytes_ = response_length_;
    if (remaining_bytes_>0)
      set_expected_content_size(remaining_bytes_);
    NotifyHeadersComplete();
  }

  int64 remaining_bytes_;
  std::string m_response;

  class AsyncResolver :
    public base::RefCountedThreadSafe<AsyncResolver> {
  public:
    explicit AsyncResolver(CefUrlRequestJob* owner)
      : owner_(owner), owner_loop_(MessageLoop::current()) {
    }

    void Resolve(const GURL& url) {
      AutoLock locked(lock_);
      //////////////////////////////////////////////////////////////////////////
      // safe to perform long operation here
      CefRefPtr<CefRequest> req(CefRequest::CreateRequest());
      req->SetURL(UTF8ToWide(url.spec()));
      req->SetMethod(UTF8ToWide(owner_->request()->method()));

      // check to see if we have post data
      net::UploadData* data = owner_->request()->get_upload();
      if (data) {
        CefRefPtr<CefPostData> postdata(CefPostData::CreatePostData());
        static_cast<CefPostDataImpl*>(postdata.get())->Set(*data);
        req->SetPostData(postdata);
      }
      owner_->handler_->Cancel();
      std::wstring mime_type;
      int response_length = 0;
      // handler should complete content generation in ProcessRequest
      bool res = owner_->handler_->ProcessRequest(req, mime_type,
          &response_length);
      if (res) {
        owner_->mime_type_ = WideToUTF8(mime_type);
        owner_->response_length_ = response_length;
      }
      //////////////////////////////////////////////////////////////////////////
      if (owner_loop_) {
        owner_loop_->PostTask(FROM_HERE, NewRunnableMethod(
            this, &AsyncResolver::ReturnResults, url));
      }
    }

    void Cancel() {
      owner_->handler_->Cancel();

      owner_ = NULL;

      AutoLock locked(lock_);
      owner_loop_ = NULL;
    }

  private:
    void ReturnResults(const GURL& url) {
      if (owner_)
        owner_->DidResolve(url);
    }

    CefUrlRequestJob* owner_;

    Lock lock_;
    MessageLoop* owner_loop_;
  };

  friend class AsyncResolver;
  scoped_refptr<AsyncResolver> async_resolver_;

  DISALLOW_COPY_AND_ASSIGN(CefUrlRequestJob);
};


// URLRequestFilter clone that manages the CefSchemeHandlerFactory pointers.

class CefUrlRequestFilter {
public:
  // scheme,hostname -> ProtocolFactory
  typedef std::map<std::pair<std::string, std::string>,
      CefSchemeHandlerFactory*> HandlerMap;

  // Singleton instance for use.
  static CefUrlRequestFilter* GetInstance()
  {
    if (!shared_instance_)
      shared_instance_ = new CefUrlRequestFilter;
    return shared_instance_;
  }

  static URLRequestJob* Factory(URLRequest* request,
      const std::string& scheme)
  {
    // Returning null here just means that the built-in handler will be used.
    return GetInstance()->FindRequestHandler(request, scheme);
  }

  void AddHostnameHandler(const std::string& scheme,
      const std::string& hostname,
      CefSchemeHandlerFactory* factory)
  {
    handler_map_[make_pair(scheme, hostname)] = factory;

    // Register with the ProtocolFactory.
    URLRequest::RegisterProtocolFactory(scheme,
        &CefUrlRequestFilter::Factory);
  }

  void RemoveHostnameHandler(const std::string& scheme,
      const std::string& hostname)
  {
    HandlerMap::iterator iter =
        handler_map_.find(make_pair(scheme, hostname));
    DCHECK(iter != handler_map_.end());

    handler_map_.erase(iter);
  }

  // Clear all the existing URL handlers and unregister with the
  // ProtocolFactory.  Resets the hit count.
  void ClearHandlers()
  {
    // Unregister with the ProtocolFactory.
    std::set<std::string> schemes;
    for (HandlerMap::const_iterator i = handler_map_.begin();
        i != handler_map_.end(); ++i) {
      schemes.insert(i->first.first);
    }
    for (std::set<std::string>::const_iterator scheme = schemes.begin();
        scheme != schemes.end(); ++scheme) {
      URLRequest::RegisterProtocolFactory(*scheme, NULL);
    }

    handler_map_.clear();
    hit_count_ = 0;
  }

  CefSchemeHandlerFactory* FindRequestHandlerFactory(URLRequest* request,
      const std::string& scheme)
  {
    CefSchemeHandlerFactory* factory = NULL;
    if (request->url().is_valid()) {
      // Check for a map with a hostname first.
      const std::string& hostname = request->url().host();

      HandlerMap::iterator i = handler_map_.find(make_pair(scheme, hostname));
      if (i != handler_map_.end())
        factory = i->second;
    }

    if (!factory) {
      // Check for a map with no specified hostname.
      HandlerMap::iterator i =
          handler_map_.find(make_pair(scheme, std::string()));
      if (i != handler_map_.end())
        factory = i->second;
    }

    return factory;
  }

  // Returns the number of times a handler was used to service a request.
  int hit_count() const { return hit_count_; }

protected:
  CefUrlRequestFilter() : hit_count_(0) { }

  // Helper method that looks up the request in the handler_map_.
  URLRequestJob* FindRequestHandler(URLRequest* request,
      const std::string& scheme)
  {
    URLRequestJob* job = NULL;
    CefSchemeHandlerFactory* factory =
        FindRequestHandlerFactory(request, scheme);
    if (factory) {
      CefRefPtr<CefSchemeHandler> handler = factory->Create();
      if (handler.get())
        job = new CefUrlRequestJob(request, handler);
    }

    if (job) {
      DLOG(INFO) << "URLRequestFilter hit for " << request->url().spec();
      hit_count_++;
    }
    return job;
  }

  // Maps hostnames to factories.  Hostnames take priority over URLs.
  HandlerMap handler_map_;

  int hit_count_;

private:
  // Singleton instance.
  static CefUrlRequestFilter* shared_instance_;

  DISALLOW_EVIL_CONSTRUCTORS(CefUrlRequestFilter);
};

CefUrlRequestFilter* CefUrlRequestFilter::shared_instance_ = NULL;


class SchemeRequestJobWrapper {
public:
  SchemeRequestJobWrapper(const std::string& scheme_name,
      const std::string& host_name,
      CefSchemeHandlerFactory* factory)
    : factory_(factory), scheme_name_(scheme_name), host_name_(host_name)
  {
    // The reference will be released when the application exits.
    TrackAdd(new TrackBase(factory));
  }

  void RegisterScheme()
  {
    // we need to store the pointer of this handler because
    // we can't pass it as a parameter to the factory method
    CefUrlRequestFilter::GetInstance()->AddHostnameHandler(
        scheme_name_, host_name_, factory_);
  }

  void AddRef() {}
  void Release() {}

private:
  CefSchemeHandlerFactory* factory_;
  std::string scheme_name_;
  std::string host_name_;
};

bool CefRegisterScheme(const std::wstring& scheme_name,
                       const std::wstring& host_name,
                       CefRefPtr<CefSchemeHandlerFactory> factory)
{
  // Verify that the context is already initialized
  if(!_Context.get())
    return false;

  SchemeRequestJobWrapper* wrapper = new SchemeRequestJobWrapper(
      WideToUTF8(scheme_name), WideToUTF8(host_name), factory);

  PostTask(FROM_HERE, NewRunnableMethod(wrapper,
      &SchemeRequestJobWrapper::RegisterScheme));
  return true;
}
