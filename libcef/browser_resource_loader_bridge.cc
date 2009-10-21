// Copyright (c) 2008 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains an implementation of the ResourceLoaderBridge class.
// The class is implemented using URLRequest, meaning it is a "simple" version
// that directly issues requests. The more complicated one used in the
// browser uses IPC.
//
// Because URLRequest only provides an asynchronous resource loading API, this
// file makes use of URLRequest from a background IO thread.  Requests for
// cookies and synchronously loaded resources result in the main thread of the
// application blocking until the IO thread completes the operation.  (See
// GetCookies and SyncLoad)
//
// Main thread                          IO thread
// -----------                          ---------
// ResourceLoaderBridge <---o---------> RequestProxy (normal case)
//                           \            -> URLRequest
//                            o-------> SyncRequestProxy (synchronous case)
//                                        -> URLRequest
// SetCookie <------------------------> CookieSetter
//                                        -> net_util::SetCookie
// GetCookies <-----------------------> CookieGetter
//                                        -> net_util::GetCookies
//
// NOTE: The implementation in this file may be used to have WebKit fetch
// resources in-process.  For example, it is handy for building a single-
// process WebKit embedding (e.g., test_shell) that can use URLRequest to
// perform URL loads.  See renderer/resource_dispatcher.h for details on an
// alternate implementation that defers fetching to another process.

#include "browser_appcache_system.h"
#include "browser_resource_loader_bridge.h"
#include "browser_request_context.h"
#include "browser_socket_stream_bridge.h"
#include "browser_impl.h"
#include "request_impl.h"

#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/timer.h"
#include "base/thread.h"
#include "base/waitable_event.h"
#include "net/base/cookie_policy.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/upload_data.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request.h"
#include "webkit/api/public/WebFrame.h"
#include "webkit/api/public/WebView.h"
#include "webkit/appcache/appcache_interfaces.h"
#include "webkit/glue/resource_loader_bridge.h"

using webkit_glue::ResourceLoaderBridge;
using net::CookiePolicy;
using net::HttpResponseHeaders;

namespace {

//-----------------------------------------------------------------------------

URLRequestContext* request_context = NULL;
base::Thread* io_thread = NULL;

class IOThread : public base::Thread {
 public:
  IOThread() : base::Thread("IOThread") {
  }

  ~IOThread() {
    // We cannot rely on our base class to stop the thread since we want our
    // CleanUp function to run.
    Stop();
  }

  virtual void Init() {
    BrowserAppCacheSystem::InitializeOnIOThread(request_context);
    BrowserSocketStreamBridge::InitializeOnIOThread(request_context);
  }

  virtual void CleanUp() {
    BrowserSocketStreamBridge::Cleanup();
    if (request_context) {
      request_context->Release();
      request_context = NULL;
    }
  }
};

//-----------------------------------------------------------------------------

struct RequestParams {
  std::string method;
  GURL url;
  GURL first_party_for_cookies;
  GURL referrer;
  std::string headers;
  int load_flags;
  ResourceType::Type request_type;
  int appcache_host_id;
  scoped_refptr<net::UploadData> upload;
};

// The interval for calls to RequestProxy::MaybeUpdateUploadProgress
static const int kUpdateUploadProgressIntervalMsec = 100;

// The RequestProxy does most of its work on the IO thread.  The Start and
// Cancel methods are proxied over to the IO thread, where an URLRequest object
// is instantiated.
class RequestProxy : public URLRequest::Delegate,
                     public base::RefCountedThreadSafe<RequestProxy> {
 public:
  // Takes ownership of the params.
  RequestProxy(CefRefPtr<CefBrowser> browser)
    : browser_(browser),
      buf_(new net::IOBuffer(kDataSize)),
      last_upload_position_(0)
  {
  }

  virtual ~RequestProxy() {
    // If we have a request, then we'd better be on the io thread!
    DCHECK(!request_.get() ||
           MessageLoop::current() == io_thread->message_loop());
  }

  void DropPeer() {
    peer_ = NULL;
  }

  void Start(ResourceLoaderBridge::Peer* peer, RequestParams* params) {
    peer_ = peer;
    owner_loop_ = MessageLoop::current();

    // proxy over to the io thread
    io_thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        this, &RequestProxy::AsyncStart, params));
  }

  void Cancel() {
    // proxy over to the io thread
    io_thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        this, &RequestProxy::AsyncCancel));
  }

 protected:
  // --------------------------------------------------------------------------
  // The following methods are called on the owner's thread in response to
  // various URLRequest callbacks.  The event hooks, defined below, trigger
  // these methods asynchronously.

  void NotifyUploadProgress(uint64 position, uint64 size) {
    if (peer_)
      peer_->OnUploadProgress(position, size);
  }

  void NotifyReceivedRedirect(const GURL& new_url,
                              const ResourceLoaderBridge::ResponseInfo& info) {
    if (peer_ && peer_->OnReceivedRedirect(new_url, info)) {
      io_thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
          this, &RequestProxy::AsyncFollowDeferredRedirect));
    } else {
      Cancel();
    }
  }

  void NotifyReceivedResponse(const ResourceLoaderBridge::ResponseInfo& info,
                              bool content_filtered) {
    if (peer_)
      peer_->OnReceivedResponse(info, content_filtered);
  }

  void NotifyReceivedData(int bytes_read) {
    if (!peer_)
      return;

    // Make a local copy of buf_, since AsyncReadData reuses it.
    scoped_array<char> buf_copy(new char[bytes_read]);
    memcpy(buf_copy.get(), buf_->data(), bytes_read);

    // Continue reading more data into buf_
    // Note: Doing this before notifying our peer ensures our load events get
    // dispatched in a manner consistent with DumpRenderTree (and also avoids a
    // race condition).  If the order of the next 2 functions were reversed, the
    // peer could generate new requests in reponse to the received data, which
    // when run on the io thread, could race against this function in doing
    // another InvokeLater.  See bug 769249.
    io_thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
        this, &RequestProxy::AsyncReadData));

    peer_->OnReceivedData(buf_copy.get(), bytes_read);
  }

  void NotifyCompletedRequest(const URLRequestStatus& status,
                              const std::string& security_info) {
    if (peer_) {
      peer_->OnCompletedRequest(status, security_info);
      DropPeer();  // ensure no further notifications
    }
  }

  // --------------------------------------------------------------------------
  // The following methods are called on the io thread.  They correspond to
  // actions performed on the owner's thread.

  void AsyncStart(RequestParams* params) {
    bool handled = false;

    if (browser_.get()) {
      CefRefPtr<CefHandler> handler = browser_->GetHandler();
      if(handler.get())
      {
        // Build the request object for passing to the handler
        CefRefPtr<CefRequest> request(new CefRequestImpl());
        CefRequestImpl* requestimpl = static_cast<CefRequestImpl*>(request.get());

        requestimpl->SetURL(UTF8ToWide(params->url.spec()));
        requestimpl->SetMethod(UTF8ToWide(params->method));
        
        // Transfer request headers
        CefRequest::HeaderMap headerMap;
        CefRequestImpl::GetHeaderMap(params->headers, headerMap);
        headerMap.insert(
          std::make_pair(L"Referrer", UTF8ToWide(params->referrer.spec())));
        requestimpl->SetHeaderMap(headerMap);

        // Transfer post data, if any
        scoped_refptr<net::UploadData> upload = params->upload;
        if(upload.get()) {
          CefRefPtr<CefPostData> postdata(new CefPostDataImpl());
          static_cast<CefPostDataImpl*>(postdata.get())->Set(*upload.get());
          requestimpl->SetPostData(postdata);
        }

        int loadFlags = params->load_flags;

        // Handler output will be returned in these variables
        std::wstring redirectUrl;
        CefRefPtr<CefStreamReader> resourceStream;
        std::wstring mimeType;
        
        CefHandler::RetVal rv = handler->HandleBeforeResourceLoad(
            browser_, request, redirectUrl, resourceStream, mimeType, loadFlags);
        if(rv == RV_HANDLED) {
          // cancel the resource load
          handled = true;
          OnCompletedRequest(
              URLRequestStatus(URLRequestStatus::CANCELED, net::ERR_ABORTED),
              std::string());
        } else if(!redirectUrl.empty()) {
          // redirect to the specified URL
          params->url = GURL(WideToUTF8(redirectUrl));
          ResourceLoaderBridge::ResponseInfo info;
          bool defer_redirect;
          OnReceivedRedirect(params->url, info, &defer_redirect);
        } else if(resourceStream.get()) {
          // load from the provided resource stream
          handled = true;

          long offset = resourceStream->Seek(0, SEEK_END);
          resourceStream->Seek(0, SEEK_SET);

          resource_stream_ = resourceStream;

          ResourceLoaderBridge::ResponseInfo info;
          info.content_length = static_cast<int64>(offset);
          if(!mimeType.empty())
            info.mime_type = WideToUTF8(mimeType);
          OnReceivedResponse(info, false);
          AsyncReadData();
        }
      }
    }

    if(!handled)
    {
      request_.reset(new URLRequest(params->url, this));
      request_->set_method(params->method);
      request_->set_first_party_for_cookies(params->first_party_for_cookies);
      request_->set_referrer(params->referrer.spec());
      request_->SetExtraRequestHeaders(params->headers);
      request_->set_load_flags(params->load_flags);
      request_->set_upload(params->upload.get());
      request_->set_context(request_context);
      BrowserAppCacheSystem::SetExtraRequestInfo(
          request_.get(), params->appcache_host_id, params->request_type);

      request_->Start();

      if (request_->has_upload() &&
          params->load_flags & net::LOAD_ENABLE_UPLOAD_PROGRESS) {
        upload_progress_timer_.Start(
            base::TimeDelta::FromMilliseconds(kUpdateUploadProgressIntervalMsec),
            this, &RequestProxy::MaybeUpdateUploadProgress);
      }
    }

    delete params;
  }

  void AsyncCancel() {
    // This can be null in cases where the request is already done.
    if (!resource_stream_.get() && !request_.get())
      return;

    request_->Cancel();
    Done();
  }

  void AsyncFollowDeferredRedirect() {
    // This can be null in cases where the request is already done.
    if (!request_.get())
      return;

    request_->FollowDeferredRedirect();
  }

  void AsyncReadData() {
    if(resource_stream_.get()) {
      // Read from the handler-provided resource stream
      int bytes_read = resource_stream_->Read(buf_->data(), 1, kDataSize);
      if(bytes_read > 0) {
        OnReceivedData(bytes_read);
      } else {
        Done();
      }
      return;
    }

    // This can be null in cases where the request is already done.
    if (!request_.get())
      return;

    if (request_->status().is_success()) {
      int bytes_read;
      if (request_->Read(buf_, kDataSize, &bytes_read) && bytes_read) {
        OnReceivedData(bytes_read);
      } else if (!request_->status().is_io_pending()) {
        Done();
      }  // else wait for OnReadCompleted
    } else {
      Done();
    }
  }

  // --------------------------------------------------------------------------
  // The following methods are event hooks (corresponding to URLRequest
  // callbacks) that run on the IO thread.  They are designed to be overridden
  // by the SyncRequestProxy subclass.

  virtual void OnReceivedRedirect(
      const GURL& new_url,
      const ResourceLoaderBridge::ResponseInfo& info,
      bool* defer_redirect) {
    *defer_redirect = true;  // See AsyncFollowDeferredRedirect
    owner_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &RequestProxy::NotifyReceivedRedirect, new_url, info));
  }

  virtual void OnReceivedResponse(
      const ResourceLoaderBridge::ResponseInfo& info,
      bool content_filtered) {
    owner_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &RequestProxy::NotifyReceivedResponse, info, content_filtered));
  }

  virtual void OnReceivedData(int bytes_read) {
    owner_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &RequestProxy::NotifyReceivedData, bytes_read));
  }

  virtual void OnCompletedRequest(const URLRequestStatus& status,
                                  const std::string& security_info) {
    owner_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &RequestProxy::NotifyCompletedRequest, status, security_info));
  }

  // --------------------------------------------------------------------------
  // URLRequest::Delegate implementation:

  virtual void OnReceivedRedirect(URLRequest* request,
                                  const GURL& new_url,
                                  bool* defer_redirect) {
    DCHECK(request->status().is_success());
    ResourceLoaderBridge::ResponseInfo info;
    PopulateResponseInfo(request, &info);
    OnReceivedRedirect(new_url, info, defer_redirect);
  }

  virtual void OnResponseStarted(URLRequest* request) {
    if (request->status().is_success()) {
      ResourceLoaderBridge::ResponseInfo info;
      PopulateResponseInfo(request, &info);
      OnReceivedResponse(info, false);
      AsyncReadData();  // start reading
    } else {
      Done();
    }
  }

  virtual void OnSSLCertificateError(URLRequest* request,
                                     int cert_error,
                                     net::X509Certificate* cert) {
    // Allow all certificate errors.
    request->ContinueDespiteLastError();
  }

  virtual void OnReadCompleted(URLRequest* request, int bytes_read) {
    if (request->status().is_success() && bytes_read > 0) {
      OnReceivedData(bytes_read);
    } else {
      Done();
    }
  }

  // --------------------------------------------------------------------------
  // Helpers and data:

  void Done() {
    if(resource_stream_.get()) {
      // Resource stream reads always complete successfully
      OnCompletedRequest(URLRequestStatus(URLRequestStatus::SUCCESS, 0),
          std::string());
      resource_stream_ = NULL;
    } else {
      if (upload_progress_timer_.IsRunning()) {
        MaybeUpdateUploadProgress();
        upload_progress_timer_.Stop();
      }

      DCHECK(request_.get());
      OnCompletedRequest(request_->status(), std::string());
      request_.reset();  // destroy on the io thread
    }
  }

  // Called on the IO thread.
  void MaybeUpdateUploadProgress() {
    // If a redirect is received upload is cancelled in URLRequest, we should
    // try to stop the |upload_progress_timer_| timer and return.
    if (!request_->has_upload()) {
      if (upload_progress_timer_.IsRunning())
        upload_progress_timer_.Stop();
      return;
    }

    uint64 size = request_->get_upload()->GetContentLength();
    uint64 position = request_->GetUploadProgress();
    if (position == last_upload_position_)
      return;  // no progress made since last time

    const uint64 kHalfPercentIncrements = 200;
    const base::TimeDelta kOneSecond = base::TimeDelta::FromMilliseconds(1000);

    uint64 amt_since_last = position - last_upload_position_;
    base::TimeDelta time_since_last = base::TimeTicks::Now() -
                                      last_upload_ticks_;

    bool is_finished = (size == position);
    bool enough_new_progress = (amt_since_last > (size /
                                                  kHalfPercentIncrements));
    bool too_much_time_passed = time_since_last > kOneSecond;

    if (is_finished || enough_new_progress || too_much_time_passed) {
      owner_loop_->PostTask(FROM_HERE, NewRunnableMethod(
          this, &RequestProxy::NotifyUploadProgress, position, size));
      last_upload_ticks_ = base::TimeTicks::Now();
      last_upload_position_ = position;
    }
  }

  void PopulateResponseInfo(URLRequest* request,
                            ResourceLoaderBridge::ResponseInfo* info) const {
    info->request_time = request->request_time();
    info->response_time = request->response_time();
    info->headers = request->response_headers();
    request->GetMimeType(&info->mime_type);
    request->GetCharset(&info->charset);
    info->content_length = request->GetExpectedContentSize();
    BrowserAppCacheSystem::GetExtraResponseInfo(
        request,
        &info->appcache_id,
        &info->appcache_manifest_url);
  }

  scoped_ptr<URLRequest> request_;
  CefRefPtr<CefStreamReader> resource_stream_;

  // Size of our async IO data buffers
  static const int kDataSize = 16*1024;

  // read buffer for async IO
  scoped_refptr<net::IOBuffer> buf_;

  CefRefPtr<CefBrowser> browser_;

  MessageLoop* owner_loop_;

  // This is our peer in WebKit (implemented as ResourceHandleInternal). We do
  // not manage its lifetime, and we may only access it from the owner's
  // message loop (owner_loop_).
  ResourceLoaderBridge::Peer* peer_;

  // Timer used to pull upload progress info.
  base::RepeatingTimer<RequestProxy> upload_progress_timer_;

  // Info used to determine whether or not to send an upload progress update.
  uint64 last_upload_position_;
  base::TimeTicks last_upload_ticks_;
};

//-----------------------------------------------------------------------------

class SyncRequestProxy : public RequestProxy {
 public:
  explicit SyncRequestProxy(CefRefPtr<CefBrowser> browser,
                            ResourceLoaderBridge::SyncLoadResponse* result)
      : RequestProxy(browser), result_(result), event_(true, false) {
  }

  void WaitForCompletion() {
    if (!event_.Wait())
      NOTREACHED();
  }

  // --------------------------------------------------------------------------
  // Event hooks that run on the IO thread:

  virtual void OnReceivedRedirect(
      const GURL& new_url,
      const ResourceLoaderBridge::ResponseInfo& info,
      bool* defer_redirect) {
    // TODO(darin): It would be much better if this could live in WebCore, but
    // doing so requires API changes at all levels.  Similar code exists in
    // WebCore/platform/network/cf/ResourceHandleCFNet.cpp :-(
    if (new_url.GetOrigin() != result_->url.GetOrigin()) {
      DLOG(WARNING) << "Cross origin redirect denied";
      Cancel();
      return;
    }
    result_->url = new_url;
  }

  virtual void OnReceivedResponse(
      const ResourceLoaderBridge::ResponseInfo& info,
      bool content_filtered) {
    *static_cast<ResourceLoaderBridge::ResponseInfo*>(result_) = info;
  }

  virtual void OnReceivedData(int bytes_read) {
    result_->data.append(buf_->data(), bytes_read);
    AsyncReadData();  // read more (may recurse)
  }

  virtual void OnCompletedRequest(const URLRequestStatus& status,
                                  const std::string& security_info) {
    result_->status = status;
    event_.Signal();
  }

 private:
  ResourceLoaderBridge::SyncLoadResponse* result_;
  base::WaitableEvent event_;
};

//-----------------------------------------------------------------------------

class ResourceLoaderBridgeImpl : public ResourceLoaderBridge {
 public:
  ResourceLoaderBridgeImpl(CefRefPtr<CefBrowser> browser,
                           const std::string& method,
                           const GURL& url,
                           const GURL& first_party_for_cookies,
                           const GURL& referrer,
                           const std::string& headers,
                           int load_flags,
                           ResourceType::Type request_type,
                           int appcache_host_id)
      : browser_(browser),
        params_(new RequestParams),
        proxy_(NULL) {
    params_->method = method;
    params_->url = url;
    params_->first_party_for_cookies = first_party_for_cookies;
    params_->referrer = referrer;
    params_->headers = headers;
    params_->load_flags = load_flags;
    params_->request_type = request_type;
    params_->appcache_host_id = appcache_host_id;
  }

  virtual ~ResourceLoaderBridgeImpl() {
    if (proxy_) {
      proxy_->DropPeer();
      // Let the proxy die on the IO thread
      io_thread->message_loop()->ReleaseSoon(FROM_HERE, proxy_);
    }
  }

  // --------------------------------------------------------------------------
  // ResourceLoaderBridge implementation:

  virtual void AppendDataToUpload(const char* data, int data_len) {
    DCHECK(params_.get());
    if (!params_->upload)
      params_->upload = new net::UploadData();
    params_->upload->AppendBytes(data, data_len);
  }

  virtual void AppendFileRangeToUpload(const FilePath& file_path,
                                       uint64 offset, uint64 length) {
    DCHECK(params_.get());
    if (!params_->upload)
      params_->upload = new net::UploadData();
    params_->upload->AppendFileRange(file_path, offset, length);
  }

  virtual void SetUploadIdentifier(int64 identifier) {
    DCHECK(params_.get());
    if (!params_->upload)
      params_->upload = new net::UploadData();
    params_->upload->set_identifier(identifier);
  }

  virtual bool Start(Peer* peer) {
    DCHECK(!proxy_);

    if (!BrowserResourceLoaderBridge::EnsureIOThread())
      return false;

    proxy_ = new RequestProxy(browser_);
    proxy_->AddRef();

    proxy_->Start(peer, params_.release());

    return true;  // Any errors will be reported asynchronously.
  }

  virtual void Cancel() {
    DCHECK(proxy_);
    proxy_->Cancel();
  }

  virtual void SetDefersLoading(bool value) {
    // TODO(darin): implement me
  }

  virtual void SyncLoad(SyncLoadResponse* response) {
    DCHECK(!proxy_);

    if (!BrowserResourceLoaderBridge::EnsureIOThread())
      return;

    // this may change as the result of a redirect
    response->url = params_->url;

    proxy_ = new SyncRequestProxy(browser_, response);
    proxy_->AddRef();

    proxy_->Start(NULL, params_.release());

    static_cast<SyncRequestProxy*>(proxy_)->WaitForCompletion();
  }

 private:
  CefRefPtr<CefBrowser> browser_;

  // Ownership of params_ is transfered to the proxy when the proxy is created.
  scoped_ptr<RequestParams> params_;

  // The request proxy is allocated when we start the request, and then it
  // sticks around until this ResourceLoaderBridge is destroyed.
  RequestProxy* proxy_;
};

//-----------------------------------------------------------------------------

class CookieSetter : public base::RefCountedThreadSafe<CookieSetter> {
 public:
  void Set(const GURL& url, const std::string& cookie) {
    DCHECK(MessageLoop::current() == io_thread->message_loop());
    request_context->cookie_store()->SetCookie(url, cookie);
  }
};

class CookieGetter : public base::RefCountedThreadSafe<CookieGetter> {
 public:
  CookieGetter() : event_(false, false) {
  }

  void Get(const GURL& url) {
    result_ = request_context->cookie_store()->GetCookies(url);
    event_.Signal();
  }

  std::string GetResult() {
    if (!event_.Wait())
      NOTREACHED();
    return result_;
  }

 private:
  base::WaitableEvent event_;
  std::string result_;
};

}  // anonymous namespace

//-----------------------------------------------------------------------------

namespace webkit_glue {

// factory function
ResourceLoaderBridge* ResourceLoaderBridge::Create(
    const std::string& method,
    const GURL& url,
    const GURL& first_party_for_cookies,
    const GURL& referrer,
    const std::string& frame_origin,
    const std::string& main_frame_origin,
    const std::string& headers,
    int load_flags,
    int requestor_pid,
    ResourceType::Type request_type,
    int appcache_host_id,
    int routing_id) {
  CefRefPtr<CefBrowser> browser = _Context->GetBrowserByID(routing_id);
  return new ResourceLoaderBridgeImpl(browser, method, url,
                                      first_party_for_cookies,
                                      referrer, headers, load_flags,
                                      request_type, appcache_host_id);
}

// Issue the proxy resolve request on the io thread, and wait 
// for the result.
bool FindProxyForUrl(const GURL& url, std::string* proxy_list) {
  DCHECK(request_context);

  scoped_refptr<net::SyncProxyServiceHelper> sync_proxy_service(
      new net::SyncProxyServiceHelper(io_thread->message_loop(),
      request_context->proxy_service()));

  net::ProxyInfo proxy_info;
  int rv = sync_proxy_service->ResolveProxy(url, &proxy_info, NULL);
  if (rv == net::OK) {
    *proxy_list = proxy_info.ToPacString();
  }

  return rv == net::OK;
}

}  // namespace webkit_glue

//-----------------------------------------------------------------------------

// static
void BrowserResourceLoaderBridge::Init(URLRequestContext* context) {
  // Make sure to stop any existing IO thread since it may be using the
  // current request context.
  Shutdown();

  if (context) {
    request_context = context;
  } else {
    request_context = new BrowserRequestContext();
  }
  request_context->AddRef();
  BrowserResourceLoaderBridge::SetAcceptAllCookies(false);
}

// static
void BrowserResourceLoaderBridge::Shutdown() {
  if (io_thread) {
    delete io_thread;
    io_thread = NULL;

    DCHECK(!request_context) << "should have been nulled by thread dtor";
  }
}

// static
void BrowserResourceLoaderBridge::SetCookie(const GURL& url,
                                            const GURL& first_party_for_cookies,
                                            const std::string& cookie) {
  // Proxy to IO thread to synchronize w/ network loading.

  if (!EnsureIOThread()) {
    NOTREACHED();
    return;
  }

  scoped_refptr<CookieSetter> cookie_setter = new CookieSetter();
  io_thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
      cookie_setter.get(), &CookieSetter::Set, url, cookie));
}

// static
std::string BrowserResourceLoaderBridge::GetCookies(
    const GURL& url, const GURL& first_party_for_cookies) {
  // Proxy to IO thread to synchronize w/ network loading

  if (!EnsureIOThread()) {
    NOTREACHED();
    return std::string();
  }

  scoped_refptr<CookieGetter> getter = new CookieGetter();

  io_thread->message_loop()->PostTask(FROM_HERE, NewRunnableMethod(
      getter.get(), &CookieGetter::Get, url));

  return getter->GetResult();
}

// static
bool BrowserResourceLoaderBridge::EnsureIOThread() {
  if (io_thread)
    return true;

  if (!request_context)
    BrowserResourceLoaderBridge::Init(NULL);

  io_thread = new IOThread();
  base::Thread::Options options;
  options.message_loop_type = MessageLoop::TYPE_IO;
  return io_thread->StartWithOptions(options);
}

// static
void BrowserResourceLoaderBridge::SetAcceptAllCookies(bool accept_all_cookies) {
  CookiePolicy::Type policy_type = accept_all_cookies ?
      CookiePolicy::ALLOW_ALL_COOKIES : CookiePolicy::BLOCK_THIRD_PARTY_COOKIES;
  request_context->cookie_policy()->set_type(policy_type);
}
