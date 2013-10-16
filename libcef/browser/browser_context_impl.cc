// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/browser_context_impl.h"

#include <map>

#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/context.h"
#include "libcef/browser/download_manager_delegate.h"
#include "libcef/browser/thread_util.h"
#include "libcef/browser/url_request_context_getter.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/threading/thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/geolocation_permission_context.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/storage_partition.h"

using content::BrowserThread;

namespace {

class CefGeolocationPermissionContext
    : public content::GeolocationPermissionContext {
 public:
  // CefGeolocationCallback implementation.
  class CallbackImpl : public CefGeolocationCallback {
   public:
    typedef base::Callback<void(bool)>  // NOLINT(readability/function)
        CallbackType;

    explicit CallbackImpl(
        CefGeolocationPermissionContext* context,
        int bridge_id,
        const CallbackType& callback)
        : context_(context),
          bridge_id_(bridge_id),
          callback_(callback) {}

    virtual void Continue(bool allow) OVERRIDE {
      if (CEF_CURRENTLY_ON_IOT()) {
        if (!callback_.is_null()) {
          // Callback must be executed on the UI thread.
          CEF_POST_TASK(CEF_UIT,
              base::Bind(&CallbackImpl::Run, callback_, allow));
          context_->RemoveCallback(bridge_id_);
        }
      } else {
        CEF_POST_TASK(CEF_IOT,
            base::Bind(&CallbackImpl::Continue, this, allow));
      }
    }

    void Disconnect() {
      callback_.Reset();
      context_ = NULL;
    }

   private:
    static void Run(const CallbackType& callback, bool allow) {
      CEF_REQUIRE_UIT();
      callback.Run(allow);
    }

    CefGeolocationPermissionContext* context_;
    int bridge_id_;
    CallbackType callback_;

    IMPLEMENT_REFCOUNTING(CallbackImpl);
  };

  CefGeolocationPermissionContext() {}

  virtual void RequestGeolocationPermission(
      int render_process_id,
      int render_view_id,
      int bridge_id,
      const GURL& requesting_frame,
      base::Callback<void(bool)> callback)  // NOLINT(readability/function)
      OVERRIDE {
    CEF_REQUIRE_IOT();

    CefRefPtr<CefBrowserHostImpl> browser =
        CefBrowserHostImpl::GetBrowserByRoutingID(render_process_id,
                                                  render_view_id);
    if (browser.get()) {
      CefRefPtr<CefClient> client = browser->GetClient();
      if (client.get()) {
        CefRefPtr<CefGeolocationHandler> handler =
            client->GetGeolocationHandler();
        if (handler.get()) {
          CefRefPtr<CallbackImpl> callbackPtr(
              new CallbackImpl(this, bridge_id, callback));

          // Add the callback reference to the map.
          callback_map_.insert(std::make_pair(bridge_id, callbackPtr));

          // Notify the handler.
          handler->OnRequestGeolocationPermission(browser.get(),
              requesting_frame.spec(), bridge_id, callbackPtr.get());
          return;
        }
      }
    }

    // Disallow geolocation access by default.
    CEF_POST_TASK(CEF_UIT, base::Bind(callback, false));
  }

  virtual void CancelGeolocationPermissionRequest(
      int render_process_id,
      int render_view_id,
      int bridge_id,
      const GURL& requesting_frame) OVERRIDE {
    RemoveCallback(bridge_id);

    CefRefPtr<CefBrowserHostImpl> browser =
        CefBrowserHostImpl::GetBrowserByRoutingID(render_process_id,
                                                  render_view_id);
    if (browser.get()) {
      CefRefPtr<CefClient> client = browser->GetClient();
      if (client.get()) {
        CefRefPtr<CefGeolocationHandler> handler =
            client->GetGeolocationHandler();
        if (handler.get()) {
          // Notify the handler.
          handler->OnCancelGeolocationPermission(browser.get(),
              requesting_frame.spec(), bridge_id);
        }
      }
    }
  }

  void RemoveCallback(int bridge_id) {
    CEF_REQUIRE_IOT();

    // Disconnect the callback and remove the reference from the map.
    CallbackMap::iterator it = callback_map_.find(bridge_id);
    if (it != callback_map_.end()) {
      it->second->Disconnect();
      callback_map_.erase(it);
    }
  }

 private:
  // Map of bridge ids to callback references.
  typedef std::map<int, CefRefPtr<CallbackImpl> > CallbackMap;
  CallbackMap callback_map_;

  DISALLOW_COPY_AND_ASSIGN(CefGeolocationPermissionContext);
};

}  // namespace

class CefBrowserContextImpl::CefResourceContext : public content::ResourceContext {
 public:
  CefResourceContext() : getter_(NULL) {}
  virtual ~CefResourceContext() {}

  // ResourceContext implementation:
  virtual net::HostResolver* GetHostResolver() OVERRIDE {
    CHECK(getter_);
    return getter_->host_resolver();
  }
  virtual net::URLRequestContext* GetRequestContext() OVERRIDE {
    CHECK(getter_);
    return getter_->GetURLRequestContext();
  }
  virtual bool AllowMicAccess(const GURL& origin) OVERRIDE {
    return true;
  }
  virtual bool AllowCameraAccess(const GURL& origin) OVERRIDE {
    return true;
  }

  void set_url_request_context_getter(CefURLRequestContextGetter* getter) {
    getter_ = getter;
  }

 private:
  CefURLRequestContextGetter* getter_;

  DISALLOW_COPY_AND_ASSIGN(CefResourceContext);
};

CefBrowserContextImpl::CefBrowserContextImpl()
    : resource_context_(new CefResourceContext) {
}

CefBrowserContextImpl::~CefBrowserContextImpl() {
  // Delete the download manager delegate here because otherwise we'll crash
  // when it's accessed from the content::BrowserContext destructor.
  if (download_manager_delegate_.get())
    download_manager_delegate_.reset(NULL);

  if (resource_context_.get()) {
    BrowserThread::DeleteSoon(
        BrowserThread::IO, FROM_HERE, resource_context_.release());
  }
}

base::FilePath CefBrowserContextImpl::GetPath() const {
  return CefContext::Get()->cache_path();
}

bool CefBrowserContextImpl::IsOffTheRecord() const {
  return false;
}

content::DownloadManagerDelegate*
    CefBrowserContextImpl::GetDownloadManagerDelegate() {
  DCHECK(!download_manager_delegate_.get());

  content::DownloadManager* manager = BrowserContext::GetDownloadManager(this);
  download_manager_delegate_.reset(new CefDownloadManagerDelegate(manager));
  return download_manager_delegate_.get();
}

net::URLRequestContextGetter* CefBrowserContextImpl::GetRequestContext() {
  CEF_REQUIRE_UIT();
  return GetDefaultStoragePartition(this)->GetURLRequestContext();
}

net::URLRequestContextGetter*
    CefBrowserContextImpl::GetRequestContextForRenderProcess(
        int renderer_child_id) {
  return GetRequestContext();
}

net::URLRequestContextGetter*
    CefBrowserContextImpl::GetMediaRequestContext() {
  return GetRequestContext();
}

net::URLRequestContextGetter*
    CefBrowserContextImpl::GetMediaRequestContextForRenderProcess(
        int renderer_child_id)  {
  return GetRequestContext();
}

net::URLRequestContextGetter*
    CefBrowserContextImpl::GetMediaRequestContextForStoragePartition(
        const base::FilePath& partition_path,
        bool in_memory) {
  return GetRequestContext();
}

void CefBrowserContextImpl::RequestMIDISysExPermission(
    int render_process_id,
    int render_view_id,
    const GURL& requesting_frame,
    const MIDISysExPermissionCallback& callback) {
  // TODO(CEF): Implement Web MIDI API permission handling.
  callback.Run(false);
}

content::ResourceContext* CefBrowserContextImpl::GetResourceContext() {
  return resource_context_.get();
}

content::GeolocationPermissionContext*
    CefBrowserContextImpl::GetGeolocationPermissionContext() {
  if (!geolocation_permission_context_) {
    geolocation_permission_context_ =
        new CefGeolocationPermissionContext();
  }
  return geolocation_permission_context_;
}

quota::SpecialStoragePolicy* CefBrowserContextImpl::GetSpecialStoragePolicy() {
  return NULL;
}

net::URLRequestContextGetter* CefBrowserContextImpl::CreateRequestContext(
    content::ProtocolHandlerMap* protocol_handlers) {
  DCHECK(!url_request_getter_);
  url_request_getter_ = new CefURLRequestContextGetter(
      BrowserThread::UnsafeGetMessageLoopForThread(BrowserThread::IO),
      BrowserThread::UnsafeGetMessageLoopForThread(BrowserThread::FILE),
      protocol_handlers);
  resource_context_->set_url_request_context_getter(url_request_getter_.get());
  return url_request_getter_.get();
}

net::URLRequestContextGetter*
    CefBrowserContextImpl::CreateRequestContextForStoragePartition(
        const base::FilePath& partition_path,
        bool in_memory,
        content::ProtocolHandlerMap* protocol_handlers) {
  return NULL;
}
