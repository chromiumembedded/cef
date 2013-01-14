// Copyright (c) 2011 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains an implementation of the ResourceLoaderBridge class.
// The class is implemented using net::URLRequest, meaning it is a "simple"
// version that directly issues requests. The more complicated one used in the
// browser uses IPC.
//
// Because net::URLRequest only provides an asynchronous resource loading API,
// this file makes use of net::URLRequest from a background IO thread.  Requests
// for cookies and synchronously loaded resources result in the main thread of
// the application blocking until the IO thread completes the operation.  (See
// GetCookies and SyncLoad)
//
// Main thread                          IO thread
// -----------                          ---------
// ResourceLoaderBridge <---o---------> RequestProxy (normal case)
//                           \            -> net::URLRequest
//                            o-------> SyncRequestProxy (synchronous case)
//                                        -> net::URLRequest
//
// NOTE: The implementation in this file may be used to have WebKit fetch
// resources in-process.  For example, it is handy for building a single-
// process WebKit embedding (e.g., test_shell) that can use net::URLRequest to
// perform URL loads.  See renderer/resource_dispatcher.h for details on an
// alternate implementation that defers fetching to another process.

#include "libcef/browser_resource_loader_bridge.h"
#include "libcef/browser_appcache_system.h"
#include "libcef/browser_request_context.h"
#include "libcef/browser_socket_stream_bridge.h"
#include "libcef/browser_webkit_glue.h"
#include "libcef/browser_impl.h"
#include "libcef/cef_context.h"
#include "libcef/cef_process.h"
#include "libcef/cef_process_io_thread.h"
#include "libcef/external_protocol_handler.h"
#include "libcef/request_impl.h"
#include "libcef/response_impl.h"
#include "libcef/http_header_utils.h"

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/time.h"
#include "base/timer.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "net/base/auth.h"
#include "net/base/file_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/static_cookie_policy.h"
#include "net/base/upload_data.h"
#include "net/http/http_cache.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job_manager.h"
#include "net/url_request/url_request_redirect_job.h"
#include "webkit/appcache/appcache_interfaces.h"
#include "webkit/blob/blob_storage_controller.h"
#include "webkit/blob/shareable_file_reference.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_dir_url_request_job.h"
#include "webkit/fileapi/file_system_url_request_job.h"
#include "webkit/glue/resource_loader_bridge.h"
#include "webkit/glue/resource_request_body.h"
#include "webkit/glue/webkit_glue.h"

#if defined(OS_MACOSX) || defined(OS_WIN)
#include "crypto/nss_util.h"
#endif

using net::HttpResponseHeaders;
using net::StaticCookiePolicy;
using net::URLRequestStatus;
using webkit_blob::ShareableFileReference;
using webkit_glue::ResourceLoaderBridge;
using webkit_glue::ResourceRequestBody;
using webkit_glue::ResourceResponseInfo;


namespace {

static const char kCefUserData[] = "cef_userdata";

struct RequestParams {
  std::string method;
  GURL url;
  GURL first_party_for_cookies;
  GURL referrer;
  WebKit::WebReferrerPolicy referrer_policy;
  std::string headers;
  int load_flags;
  ResourceType::Type request_type;
  int appcache_host_id;
  bool download_to_file;
  scoped_refptr<ResourceRequestBody> request_body;
  net::RequestPriority priority;
};

// The interval for calls to RequestProxy::MaybeUpdateUploadProgress
static const int kUpdateUploadProgressIntervalMsec = 100;

class ExtraRequestInfo : public net::URLRequest::Data {
 public:
  ExtraRequestInfo(CefBrowserImpl* browser, ResourceType::Type resource_type)
    : browser_(browser),
      resource_type_(resource_type),
      allow_download_(resource_type == ResourceType::MAIN_FRAME ||
                      resource_type == ResourceType::SUB_FRAME) {
  }

  // The browser pointer is guaranteed to be valid for the lifespan of the
  // request. The pointer will be NULL in cases where the request was
  // initiated via the CefWebURLRequest API instead of by a browser window.
  CefBrowserImpl* browser() const { return browser_; }

  // Identifies the type of resource, such as subframe, media, etc.
  ResourceType::Type resource_type() const { return resource_type_; }
  bool allow_download() const { return allow_download_; }

 private:
  CefBrowserImpl* browser_;
  ResourceType::Type resource_type_;
  bool allow_download_;
};

// Used to intercept redirect requests.
class RequestInterceptor : public net::URLRequest::Interceptor {
 public:
  RequestInterceptor() {
    REQUIRE_IOT();
    net::URLRequestJobManager::GetInstance()->RegisterRequestInterceptor(this);
  }
  ~RequestInterceptor() {
    REQUIRE_IOT();
    net::URLRequestJobManager::GetInstance()->
        UnregisterRequestInterceptor(this);
  }

  virtual net::URLRequestJob* MaybeIntercept(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate)
      OVERRIDE {
    return NULL;
  }

  virtual net::URLRequestJob* MaybeInterceptRedirect(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      const GURL& location) OVERRIDE {
    REQUIRE_IOT();

    ExtraRequestInfo* info =
        static_cast<ExtraRequestInfo*>(request->GetUserData(kCefUserData));
    if (!info)
      return NULL;

    CefRefPtr<CefBrowser> browser = info->browser();
    if (!browser.get())
      return NULL;
    CefRefPtr<CefClient> client = browser->GetClient();
    CefRefPtr<CefRequestHandler> handler;
    if (client.get())
      handler = client->GetRequestHandler();
    if (!handler.get())
      return NULL;

    CefString newUrlStr = location.spec();
    handler->OnResourceRedirect(browser, request->url().spec(), newUrlStr);
    if (newUrlStr != location.spec()) {
      GURL new_url = GURL(std::string(newUrlStr));
      if (!new_url.is_empty() && new_url.is_valid()) {
        return new net::URLRequestRedirectJob(
            request,
            network_delegate,
            new_url,
            net::URLRequestRedirectJob::REDIRECT_302_FOUND);
      }
    }

    return NULL;
  }

  DISALLOW_COPY_AND_ASSIGN(RequestInterceptor);
};

// The RequestProxy does most of its work on the IO thread.  The Start and
// Cancel methods are proxied over to the IO thread, where an net::URLRequest
// object is instantiated.
class RequestProxy : public net::URLRequest::Delegate,
                     public base::RefCountedThreadSafe<RequestProxy> {
 public:
  // Takes ownership of the params.
  explicit RequestProxy(CefRefPtr<CefBrowserImpl> browser)
    : download_to_file_(false),
      buf_(new net::IOBuffer(kDataSize)),
      browser_(browser),
      owner_loop_(NULL),
      peer_(NULL),
      last_upload_position_(0),
      defers_loading_(false),
      defers_loading_want_read_(false) {
  }

  void DropPeer() {
    DCHECK(MessageLoop::current() == owner_loop_);
    peer_ = NULL;
  }

  void Start(ResourceLoaderBridge::Peer* peer, RequestParams* params) {
    peer_ = peer;
    owner_loop_ = MessageLoop::current();

    InitializeParams(params);

    // proxy over to the io thread
    CefThread::PostTask(CefThread::IO, FROM_HERE, base::Bind(
        &RequestProxy::AsyncStart, this, params));
  }

  void Cancel() {
    DCHECK(MessageLoop::current() == owner_loop_);

    if (download_handler_.get()) {
      // WebKit will try to cancel the download but we won't allow it.
      return;
    }

    // proxy over to the io thread
    CefThread::PostTask(CefThread::IO, FROM_HERE, base::Bind(
        &RequestProxy::AsyncCancel, this));
  }

  void SetDefersLoading(bool defer) {
    DCHECK(MessageLoop::current() == owner_loop_);

    CefThread::PostTask(CefThread::IO, FROM_HERE, base::Bind(
        &RequestProxy::AsyncSetDefersLoading, this, defer));
  }

 protected:
  friend class base::RefCountedThreadSafe<RequestProxy>;

  virtual ~RequestProxy() {
    // If we have a request, then we'd better be on the io thread!
    DCHECK(!request_.get() || CefThread::CurrentlyOn(CefThread::IO));
  }

  virtual void InitializeParams(RequestParams* params) {
    params->priority = net::MEDIUM;
  }

  // --------------------------------------------------------------------------
  // The following methods are called on the owner's thread in response to
  // various net::URLRequest callbacks.  The event hooks, defined below, trigger
  // these methods asynchronously.

  void NotifyReceivedRedirect(const GURL& new_url,
                              const ResourceResponseInfo& info) {
    DCHECK(MessageLoop::current() == owner_loop_);

    bool has_new_first_party_for_cookies = false;
    GURL new_first_party_for_cookies;
    if (peer_ && peer_->OnReceivedRedirect(new_url, info,
                                           &has_new_first_party_for_cookies,
                                           &new_first_party_for_cookies)) {
      CefThread::PostTask(CefThread::IO, FROM_HERE, base::Bind(
          &RequestProxy::AsyncFollowDeferredRedirect, this,
          has_new_first_party_for_cookies, new_first_party_for_cookies));
    } else {
      Cancel();
    }
  }

  void NotifyReceivedResponse(const ResourceResponseInfo& info,
                              const GURL& url, bool allow_download) {
    DCHECK(MessageLoop::current() == owner_loop_);

    if (browser_.get() && info.headers.get()) {
      CefRefPtr<CefClient> client = browser_->GetClient();
      CefRefPtr<CefRequestHandler> handler;
      if (client.get())
        handler = client->GetRequestHandler();

      if (handler.get()) {
        CefRefPtr<CefResponse> response = new CefResponseImpl();
        // Transfer response headers
        if (info.headers) {
          CefResponse::HeaderMap headerMap;
          void* header_index = NULL;
          std::string name, value;
          while (info.headers->EnumerateHeaderLines(&header_index, &name,
                                                    &value)) {
            if (!name.empty() && !value.empty())
              headerMap.insert(std::make_pair(name, value));
          }
          response->SetHeaderMap(headerMap);
          response->SetStatusText(info.headers->GetStatusText());
          response->SetStatus(info.headers->response_code());
        }
        response->SetMimeType(info.mime_type);
        handler->OnResourceResponse(browser_.get(), url.spec(), response,
            content_filter_);

        std::string content_disposition;
        info.headers->GetNormalizedHeader("Content-Disposition",
            &content_disposition);

        if (allow_download &&
            webkit_glue::ShouldDownload(content_disposition, info.mime_type)) {
          string16 filename = net::GetSuggestedFilename(url,
              content_disposition, info.charset, "", info.mime_type,
              "download");
          CefRefPtr<CefDownloadHandler> dl_handler;
          if (handler->GetDownloadHandler(browser_.get(), info.mime_type,
                                          filename, info.content_length,
                                          dl_handler)) {
            download_handler_ = dl_handler;
          }
        }
      }
    }

    if (peer_)
      peer_->OnReceivedResponse(info);
  }

  void NotifyReceivedData(int bytes_read) {
    DCHECK(MessageLoop::current() == owner_loop_);

    if (!peer_)
      return;

    // Make a local copy of buf_, since AsyncReadData reuses it.
    scoped_array<char> buf_copy(new char[bytes_read]);
    memcpy(buf_copy.get(), buf_->data(), bytes_read);

    // Continue reading more data into buf_
    // Note: Doing this before notifying our peer ensures our load events get
    // dispatched in a manner consistent with DumpRenderTree (and also avoids a
    // race condition).  If the order of the next 2 functions were reversed, the
    // peer could generate new requests in response to the received data, which
    // when run on the io thread, could race against this function in doing
    // another InvokeLater.  See bug 769249.
    CefThread::PostTask(CefThread::IO, FROM_HERE, base::Bind(
        &RequestProxy::AsyncReadData, this));

    CefRefPtr<CefStreamReader> resourceStream;

    if (content_filter_.get())
      content_filter_->ProcessData(buf_copy.get(), bytes_read, resourceStream);

    if (resourceStream.get()) {
      // The filter made some changes to the data in the buffer.
      resourceStream->Seek(0, SEEK_END);
      bytes_read = resourceStream->Tell();
      resourceStream->Seek(0, SEEK_SET);

      buf_copy.reset(new char[bytes_read]);
      resourceStream->Read(buf_copy.get(), 1, bytes_read);
    }

    if (download_handler_.get() &&
        !download_handler_->ReceivedData(buf_copy.get(), bytes_read)) {
      // Cancel loading by proxying over to the io thread.
      CefThread::PostTask(CefThread::IO, FROM_HERE, base::Bind(
          &RequestProxy::AsyncCancel, this));
    }

    peer_->OnReceivedData(buf_copy.get(), bytes_read, -1);
  }

  void NotifyDownloadedData(int bytes_read) {
    DCHECK(MessageLoop::current() == owner_loop_);

    if (!peer_)
      return;

    // Continue reading more data, see the comment in NotifyReceivedData.
    CefThread::PostTask(CefThread::IO, FROM_HERE, base::Bind(
        &RequestProxy::AsyncReadData, this));

    peer_->OnDownloadedData(bytes_read);
  }

  void NotifyCompletedRequest(int error_code,
                              const std::string& security_info,
                              const base::TimeTicks& complete_time) {
    DCHECK(MessageLoop::current() == owner_loop_);

    // Drain the content filter of all remaining data
    if (content_filter_.get()) {
      CefRefPtr<CefStreamReader> remainder;
      content_filter_->Drain(remainder);

      if (remainder.get()) {
        remainder->Seek(0, SEEK_END);
        int size = static_cast<int>(remainder->Tell());
        if (size > 0) {
          remainder->Seek(0, SEEK_SET);
          scoped_array<char> buf(new char[size]);
          remainder->Read(buf.get(), 1, size);

          if (download_handler_.get() &&
              !download_handler_->ReceivedData(buf.get(), size)) {
            // Cancel loading by proxying over to the io thread.
            CefThread::PostTask(CefThread::IO, FROM_HERE, base::Bind(
                &RequestProxy::AsyncCancel, this));
          }

          if (peer_)
            peer_->OnReceivedData(buf.get(), size, -1);
        }
      }
      content_filter_ = NULL;
    }

    if (download_handler_.get()) {
      download_handler_->Complete();
      download_handler_ = NULL;
    }

    if (peer_) {
      peer_->OnCompletedRequest(error_code, false, security_info, complete_time);
      DropPeer();  // ensure no further notifications
    }
  }

  void NotifyUploadProgress(uint64 position, uint64 size) {
    DCHECK(MessageLoop::current() == owner_loop_);

    if (peer_)
      peer_->OnUploadProgress(position, size);
  }

  // --------------------------------------------------------------------------
  // The following methods are called on the io thread.  They correspond to
  // actions performed on the owner's thread.

  void AsyncStart(RequestParams* params) {
    DCHECK(CefThread::CurrentlyOn(CefThread::IO));

    bool handled = false;

    scoped_ptr<net::UploadDataStream> upload_data_stream;
    if (params->request_body) {
      upload_data_stream.reset(
          params->request_body->ResolveElementsAndCreateUploadDataStream(
              _Context->request_context()->blob_storage_controller()));
    }

    if (browser_.get()) {
      CefRefPtr<CefClient> client = browser_->GetClient();
      CefRefPtr<CefRequestHandler> handler;
      if (client.get())
        handler = client->GetRequestHandler();

      if (handler.get()) {
        // Build the request object for passing to the handler
        CefRefPtr<CefRequest> request(new CefRequestImpl());
        CefRequestImpl* requestimpl =
            static_cast<CefRequestImpl*>(request.get());

        std::string originalUrl(params->url.spec());
        requestimpl->SetURL(originalUrl);
        requestimpl->SetMethod(params->method);

        // Transfer request headers
        CefRequest::HeaderMap headerMap;
        HttpHeaderUtils::ParseHeaders(params->headers, headerMap);
        headerMap.insert(std::make_pair("Referer", params->referrer.spec()));
        requestimpl->SetHeaderMap(headerMap);

        // Transfer post data, if any
        if (upload_data_stream) {
          CefRefPtr<CefPostData> postdata(new CefPostDataImpl());
          static_cast<CefPostDataImpl*>(postdata.get())->Set(
              *upload_data_stream.get());
          requestimpl->SetPostData(postdata);
        }

        int loadFlags = params->load_flags;

        // Handler output will be returned in these variables
        CefString redirectUrl;
        CefRefPtr<CefStreamReader> resourceStream;
        CefRefPtr<CefResponse> response(new CefResponseImpl());

        handled = handler->OnBeforeResourceLoad(browser_.get(), request,
            redirectUrl, resourceStream, response, loadFlags);
        if (!handled) {
          // Observe URL from request.
          const std::string requestUrl(request->GetURL());
          if (requestUrl != originalUrl)
            params->url = GURL(requestUrl);
          else if (!redirectUrl.empty())
            params->url = GURL(std::string(redirectUrl));

          // Observe method from request.
          params->method = request->GetMethod();

          // Observe headers from request.
          request->GetHeaderMap(headerMap);
          CefString referrerStr;
          referrerStr.FromASCII("Referer");
          CefRequest::HeaderMap::iterator referrer =
              headerMap.find(referrerStr);
          if (referrer == headerMap.end()) {
            params->referrer = GURL();
          } else {
            params->referrer = GURL(std::string(referrer->second));
            headerMap.erase(referrer);
          }
          params->headers = HttpHeaderUtils::GenerateHeaders(headerMap);

          // Observe post data from request.
          CefRefPtr<CefPostData> postData = request->GetPostData();
          if (postData.get()) {
            upload_data_stream.reset(
                static_cast<CefPostDataImpl*>(postData.get())->Get());
          } else {
            upload_data_stream.reset(NULL);
          }
        }

        if (handled) {
          // cancel the resource load
          OnCompletedRequest(net::ERR_ABORTED, std::string(),
                             base::TimeTicks());
        } else if (resourceStream.get()) {
          // load from the provided resource stream
          handled = true;

          resourceStream->Seek(0, SEEK_END);
          int64 offset = resourceStream->Tell();
          resourceStream->Seek(0, SEEK_SET);

          resource_stream_ = resourceStream;

          CefResponseImpl* responseImpl =
              static_cast<CefResponseImpl*>(response.get());

          ResourceResponseInfo info;
          info.content_length = offset;
          info.mime_type = response->GetMimeType();
          info.headers = responseImpl->GetResponseHeaders();
          OnReceivedResponse(info, params->url);
          AsyncReadData();
        } else if (response->GetStatus() != 0) {
          // status set, but no resource stream
          handled = true;

          CefResponseImpl* responseImpl =
              static_cast<CefResponseImpl*>(response.get());

          ResourceResponseInfo info;
          info.content_length = 0;
          info.mime_type = response->GetMimeType();
          info.headers = responseImpl->GetResponseHeaders();
          OnReceivedResponse(info, params->url);
          AsyncReadData();
        }

        if (!handled && ResourceType::IsFrame(params->request_type) &&
            !net::URLRequest::IsHandledProtocol(params->url.scheme())) {
          bool allow_os_execution = false;
          handled = handler->OnProtocolExecution(browser_.get(),
              params->url.spec(), allow_os_execution);
          if (!handled && allow_os_execution &&
              ExternalProtocolHandler::HandleExternalProtocol(params->url)) {
            handled = true;
          }

          if (handled)
            OnCompletedRequest(net::OK, std::string(), base::TimeTicks());
        }
      }
    }

    if (!handled) {
      net::URLRequestContext* context = browser_.get() ?
          browser_->request_context_proxy() : _Context->request_context();

      request_.reset(new net::URLRequest(params->url, this, context));
      request_->set_priority(params->priority);
      request_->set_method(params->method);
      request_->set_first_party_for_cookies(params->first_party_for_cookies);
      request_->set_referrer(params->referrer.spec());
      webkit_glue::ConfigureURLRequestForReferrerPolicy(
          request_.get(), params->referrer_policy);
      net::HttpRequestHeaders headers;
      headers.AddHeadersFromString(params->headers);
      request_->SetExtraRequestHeaders(headers);
      request_->set_load_flags(params->load_flags);
      if (upload_data_stream)
        request_->set_upload(upload_data_stream.Pass());
      request_->SetUserData(kCefUserData,
          new ExtraRequestInfo(browser_.get(), params->request_type));
      BrowserAppCacheSystem::SetExtraRequestInfo(
          request_.get(), params->appcache_host_id, params->request_type);

      download_to_file_ = params->download_to_file;
      if (download_to_file_) {
        FilePath path;
        if (file_util::CreateTemporaryFile(&path)) {
          downloaded_file_ = ShareableFileReference::GetOrCreate(
              path, ShareableFileReference::DELETE_ON_FINAL_RELEASE,
              base::MessageLoopProxy::current());
          file_stream_.reset(new net::FileStream(NULL));
          file_stream_->OpenSync(
              path, base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_WRITE);
        }
      }

      request_->Start();

      if (request_.get() && request_->has_upload() &&
          params->load_flags & net::LOAD_ENABLE_UPLOAD_PROGRESS) {
        upload_progress_timer_.Start(FROM_HERE,
            base::TimeDelta::FromMilliseconds(
                kUpdateUploadProgressIntervalMsec),
            this, &RequestProxy::MaybeUpdateUploadProgress);
      }
    }

    delete params;
  }

  void AsyncCancel() {
    DCHECK(CefThread::CurrentlyOn(CefThread::IO));

    // This can be null in cases where the request is already done.
    if (!resource_stream_.get() && !request_.get())
      return;

    if (request_.get())
      request_->Cancel();
    Done();
  }

  void AsyncFollowDeferredRedirect(bool has_new_first_party_for_cookies,
                                   const GURL& new_first_party_for_cookies) {
    DCHECK(CefThread::CurrentlyOn(CefThread::IO));

    // This can be null in cases where the request is already done.
    if (!request_.get())
      return;

    if (has_new_first_party_for_cookies)
      request_->set_first_party_for_cookies(new_first_party_for_cookies);
    request_->FollowDeferredRedirect();
  }

  void AsyncSetDefersLoading(bool defer) {
    DCHECK(CefThread::CurrentlyOn(CefThread::IO));

    if (defers_loading_ != defer) {
      defers_loading_ = defer;
      if (!defers_loading_ && defers_loading_want_read_) {
        // Perform the pending AsyncReadData now.
        defers_loading_want_read_ = false;
        AsyncReadData();
      }
    }
  }

  void AsyncReadData() {
    DCHECK(CefThread::CurrentlyOn(CefThread::IO));

    // Pause downloading if we're in deferred mode.
    if (defers_loading_) {
      defers_loading_want_read_ = true;
      return;
    }

    if (resource_stream_.get()) {
      // Read from the handler-provided resource stream
      int bytes_read = resource_stream_->Read(buf_->data(), 1, kDataSize);
      if (bytes_read > 0) {
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
  // The following methods are event hooks (corresponding to net::URLRequest
  // callbacks) that run on the IO thread.  They are designed to be overridden
  // by the SyncRequestProxy subclass.

  virtual void OnReceivedRedirect(
      const GURL& new_url,
      const ResourceResponseInfo& info,
      bool* defer_redirect) {
    DCHECK(CefThread::CurrentlyOn(CefThread::IO));

    *defer_redirect = true;  // See AsyncFollowDeferredRedirect
    owner_loop_->PostTask(FROM_HERE, base::Bind(
        &RequestProxy::NotifyReceivedRedirect, this, new_url, info));
  }

  virtual void OnReceivedResponse(
      const ResourceResponseInfo& info,
      // only used when loading from a resource stream
      const GURL& simulated_url) {
    DCHECK(CefThread::CurrentlyOn(CefThread::IO));

    GURL url;
    bool allow_download(false);
    if (request_.get()) {
      url = request_->url();
      ExtraRequestInfo* info =
          static_cast<ExtraRequestInfo*>(request_->GetUserData(kCefUserData));
      if (info)
        allow_download = info->allow_download();
    } else if (!simulated_url.is_empty() && simulated_url.is_valid()) {
      url = simulated_url;
    }

    owner_loop_->PostTask(FROM_HERE, base::Bind(
        &RequestProxy::NotifyReceivedResponse, this, info, url,
        allow_download));
  }

  virtual void OnReceivedData(int bytes_read) {
    DCHECK(CefThread::CurrentlyOn(CefThread::IO));

    if (download_to_file_) {
      file_stream_->WriteSync(buf_->data(), bytes_read);
      owner_loop_->PostTask(FROM_HERE, base::Bind(
          &RequestProxy::NotifyDownloadedData, this, bytes_read));
      return;
    }

    owner_loop_->PostTask(FROM_HERE, base::Bind(
        &RequestProxy::NotifyReceivedData, this, bytes_read));
  }

  virtual void OnCompletedRequest(int error_code,
                                  const std::string& security_info,
                                  const base::TimeTicks& complete_time) {
    DCHECK(CefThread::CurrentlyOn(CefThread::IO));

    if (download_to_file_)
      file_stream_.reset();

    owner_loop_->PostTask(FROM_HERE, base::Bind(
        &RequestProxy::NotifyCompletedRequest, this, error_code, security_info,
        complete_time));
  }

  // --------------------------------------------------------------------------
  // net::URLRequest::Delegate implementation:

  virtual void OnReceivedRedirect(net::URLRequest* request,
                                  const GURL& new_url,
                                  bool* defer_redirect) OVERRIDE {
    DCHECK(CefThread::CurrentlyOn(CefThread::IO));

    DCHECK(request->status().is_success());
    ResourceResponseInfo info;
    PopulateResponseInfo(request, &info);
    OnReceivedRedirect(new_url, info, defer_redirect);
  }

  virtual void OnResponseStarted(net::URLRequest* request) OVERRIDE {
    DCHECK(CefThread::CurrentlyOn(CefThread::IO));

    if (request->status().is_success()) {
      ResourceResponseInfo info;
      PopulateResponseInfo(request, &info);
      OnReceivedResponse(info, GURL::EmptyGURL());
      AsyncReadData();  // start reading
    } else {
      Done();
    }
  }

  virtual void OnAuthRequired(net::URLRequest* request,
                              net::AuthChallengeInfo* auth_info) OVERRIDE {
    DCHECK(CefThread::CurrentlyOn(CefThread::IO));

    if (browser_.get()) {
      CefRefPtr<CefClient> client = browser_->GetClient();
      if (client.get()) {
        CefRefPtr<CefRequestHandler> handler = client->GetRequestHandler();
        if (handler.get()) {
          CefString username, password;
          if (handler->GetAuthCredentials(browser_.get(),
                                          auth_info->is_proxy,
                                          auth_info->challenger.host(),
                                          auth_info->challenger.port(),
                                          auth_info->realm,
                                          auth_info->scheme,
                                          username, password)) {
            request->SetAuth(net::AuthCredentials(username, password));
            return;
          }
        }
      }
    }

    request->CancelAuth();
  }

  virtual void OnSSLCertificateError(net::URLRequest* request,
                                     const net::SSLInfo& ssl_info,
                                     bool fatal) OVERRIDE {
    DCHECK(CefThread::CurrentlyOn(CefThread::IO));

    // Allow all certificate errors.
    request->ContinueDespiteLastError();
  }

  virtual void OnReadCompleted(net::URLRequest* request,
                               int bytes_read) OVERRIDE {
    DCHECK(CefThread::CurrentlyOn(CefThread::IO));

    if (request->status().is_success() && bytes_read > 0) {
      OnReceivedData(bytes_read);
    } else {
      Done();
    }
  }

  // --------------------------------------------------------------------------
  // Helpers and data:

  void Done() {
    DCHECK(CefThread::CurrentlyOn(CefThread::IO));

    if (resource_stream_.get()) {
      // Resource stream reads always complete successfully
      OnCompletedRequest(0, std::string(), base::TimeTicks());
      resource_stream_ = NULL;
    } else if (request_.get()) {
      if (upload_progress_timer_.IsRunning()) {
        MaybeUpdateUploadProgress();
        upload_progress_timer_.Stop();
      }
      DCHECK(request_.get());
      OnCompletedRequest(request_->status().error(), std::string(),
                         base::TimeTicks());
      request_.reset();  // destroy on the io thread
    }
  }

  void MaybeUpdateUploadProgress() {
    DCHECK(CefThread::CurrentlyOn(CefThread::IO));

    // If a redirect is received upload is cancelled in net::URLRequest, we
    // should try to stop the |upload_progress_timer_| timer and return.
    if (!request_->has_upload()) {
      if (upload_progress_timer_.IsRunning())
        upload_progress_timer_.Stop();
      return;
    }

    net::UploadProgress progress = request_->GetUploadProgress();
    if (progress.position() == last_upload_position_)
      return;  // no progress made since last time

    const uint64 kHalfPercentIncrements = 200;
    const base::TimeDelta kOneSecond = base::TimeDelta::FromMilliseconds(1000);

    uint64 amt_since_last = progress.position() - last_upload_position_;
    base::TimeDelta time_since_last = base::TimeTicks::Now() -
                                      last_upload_ticks_;

    bool is_finished = (progress.size() == progress.position());
    bool enough_new_progress = (amt_since_last > (progress.size() /
                                                  kHalfPercentIncrements));
    bool too_much_time_passed = time_since_last > kOneSecond;

    if (is_finished || enough_new_progress || too_much_time_passed) {
      owner_loop_->PostTask(
          FROM_HERE,
          base::Bind(&RequestProxy::NotifyUploadProgress, this,
                     progress.position(), progress.size()));
      last_upload_ticks_ = base::TimeTicks::Now();
      last_upload_position_ = progress.position();
    }
  }

  void PopulateResponseInfo(net::URLRequest* request,
                            ResourceResponseInfo* info) const {
    DCHECK(CefThread::CurrentlyOn(CefThread::IO));

    info->request_time = request->request_time();
    info->response_time = request->response_time();
    info->headers = request->response_headers();
    request->GetMimeType(&info->mime_type);
    request->GetCharset(&info->charset);
    info->content_length = request->GetExpectedContentSize();
    if (downloaded_file_)
      info->download_file_path = downloaded_file_->path();
    BrowserAppCacheSystem::GetExtraResponseInfo(
        request,
        &info->appcache_id,
        &info->appcache_manifest_url);
  }

  scoped_ptr<net::URLRequest> request_;
  CefRefPtr<CefStreamReader> resource_stream_;

  // Support for request.download_to_file behavior.
  bool download_to_file_;
  scoped_ptr<net::FileStream> file_stream_;
  scoped_refptr<ShareableFileReference> downloaded_file_;

  // Size of our async IO data buffers. Limited by the sanity check in
  // URLRequestJob::Read().
  static const int kDataSize = 1000000-1;

  // read buffer for async IO
  scoped_refptr<net::IOBuffer> buf_;

  CefRefPtr<CefBrowserImpl> browser_;

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

  CefRefPtr<CefDownloadHandler> download_handler_;
  CefRefPtr<CefContentFilter> content_filter_;

  // True if loading of data is currently deferred.
  bool defers_loading_;

  // True if an AsyncReadData was scheduled while we were deferred.
  bool defers_loading_want_read_;
};

//-----------------------------------------------------------------------------

class SyncRequestProxy : public RequestProxy {
 public:
  explicit SyncRequestProxy(CefRefPtr<CefBrowserImpl> browser,
                            ResourceLoaderBridge::SyncLoadResponse* result)
      : RequestProxy(browser), result_(result), event_(true, false) {
  }

  void WaitForCompletion() {
    event_.Wait();
  }

  // --------------------------------------------------------------------------
  // Event hooks that run on the IO thread:

  virtual void OnReceivedRedirect(
      const GURL& new_url,
      const ResourceResponseInfo& info,
      bool* defer_redirect) {
    DCHECK(CefThread::CurrentlyOn(CefThread::IO));

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

  virtual void OnReceivedResponse(const ResourceResponseInfo& info,
                                  const GURL&) {
    DCHECK(CefThread::CurrentlyOn(CefThread::IO));

    *static_cast<ResourceResponseInfo*>(result_) = info;
  }

  virtual void OnReceivedData(int bytes_read) {
    DCHECK(CefThread::CurrentlyOn(CefThread::IO));

    if (download_to_file_)
      file_stream_->WriteSync(buf_->data(), bytes_read);
    else
      result_->data.append(buf_->data(), bytes_read);
    AsyncReadData();  // read more (may recurse)
  }

  virtual void OnCompletedRequest(int error_code,
                                  const std::string& security_info,
                                  const base::TimeTicks& complete_time) {
    DCHECK(CefThread::CurrentlyOn(CefThread::IO));

    if (download_to_file_)
      file_stream_.reset();

    result_->error_code = error_code;
    event_.Signal();
  }

 protected:
  virtual void InitializeParams(RequestParams* params) {
    // For synchronous requests ignore load limits to avoid a deadlock problem
    // in SyncRequestProxy (issue #192).
    params->load_flags |= net::LOAD_IGNORE_LIMITS;
    params->priority = net::HIGHEST;
  }

 private:
  ResourceLoaderBridge::SyncLoadResponse* result_;
  base::WaitableEvent event_;
};

//-----------------------------------------------------------------------------

class ResourceLoaderBridgeImpl : public ResourceLoaderBridge,
                                 public base::NonThreadSafe {
 public:
  ResourceLoaderBridgeImpl(CefRefPtr<CefBrowserImpl> browser,
      const webkit_glue::ResourceLoaderBridge::RequestInfo& request_info)
      : browser_(browser),
        params_(new RequestParams),
        proxy_(NULL) {
    params_->method = request_info.method;
    params_->url = request_info.url;
    params_->first_party_for_cookies = request_info.first_party_for_cookies;
    params_->referrer = request_info.referrer;
    params_->referrer_policy = request_info.referrer_policy;
    params_->headers = request_info.headers;
    params_->load_flags = request_info.load_flags;
    params_->request_type = request_info.request_type;
    params_->appcache_host_id = request_info.appcache_host_id;
    params_->download_to_file = request_info.download_to_file;
  }

  virtual ~ResourceLoaderBridgeImpl() {
    if (proxy_) {
      proxy_->DropPeer();
      // Let the proxy die on the IO thread
      proxy_->AddRef();
      CefThread::ReleaseSoon(CefThread::IO, FROM_HERE, proxy_.get());
    }
  }

  // --------------------------------------------------------------------------
  // ResourceLoaderBridge implementation:

  virtual void SetRequestBody(ResourceRequestBody* request_body) OVERRIDE {
    DCHECK(CalledOnValidThread());
    DCHECK(params_.get());
    DCHECK(!params_->request_body);
    params_->request_body = request_body;
  }

  virtual bool Start(Peer* peer) OVERRIDE {
    DCHECK(CalledOnValidThread());
    DCHECK(!proxy_);

    proxy_ = new RequestProxy(browser_);
    proxy_->Start(peer, params_.release());

    return true;  // Any errors will be reported asynchronously.
  }

  virtual void Cancel() OVERRIDE {
    DCHECK(CalledOnValidThread());
    DCHECK(proxy_);
    proxy_->Cancel();
  }

  virtual void SetDefersLoading(bool value) OVERRIDE {
    DCHECK(CalledOnValidThread());
    DCHECK(proxy_);
    proxy_->SetDefersLoading(value);
  }

  virtual void SyncLoad(SyncLoadResponse* response) OVERRIDE {
    DCHECK(CalledOnValidThread());
    DCHECK(!proxy_);

    // this may change as the result of a redirect
    response->url = params_->url;

    proxy_ = new SyncRequestProxy(browser_, response);
    proxy_->Start(NULL, params_.release());

    static_cast<SyncRequestProxy*>(proxy_.get())->WaitForCompletion();
  }

 private:
  CefRefPtr<CefBrowserImpl> browser_;

  // Ownership of params_ is transfered to the proxy when the proxy is created.
  scoped_ptr<RequestParams> params_;

  // The request proxy is allocated when we start the request, and then it
  // sticks around until this ResourceLoaderBridge is destroyed.
  scoped_refptr<RequestProxy> proxy_;
};

}  // anonymous namespace

//-----------------------------------------------------------------------------

// static
webkit_glue::ResourceLoaderBridge* BrowserResourceLoaderBridge::Create(
    const webkit_glue::ResourceLoaderBridge::RequestInfo& request_info) {
  DCHECK(CefThread::CurrentlyOn(CefThread::UI));

  CefRefPtr<CefBrowserImpl> browser =
      _Context->GetBrowserByID(request_info.routing_id);
  return new ResourceLoaderBridgeImpl(browser.get(), request_info);
}

//-----------------------------------------------------------------------------

// static
CefRefPtr<CefBrowserImpl> BrowserResourceLoaderBridge::GetBrowserForRequest(
    net::URLRequest* request) {
  REQUIRE_IOT();
  ExtraRequestInfo* extra_info =
      static_cast<ExtraRequestInfo*>(request->GetUserData(kCefUserData));
  if (extra_info)
    return extra_info->browser();
  return NULL;
}

// static
scoped_refptr<base::MessageLoopProxy>
BrowserResourceLoaderBridge::GetCacheThread() {
  return CefThread::GetMessageLoopProxyForThread(CefThread::FILE);
}

// static
net::URLRequest::Interceptor*
BrowserResourceLoaderBridge::CreateRequestInterceptor() {
  return new RequestInterceptor();
}
