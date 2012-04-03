// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/browser_context.h"

#include <map>

#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/browser_main.h"
#include "libcef/browser/download_manager_delegate.h"
#include "libcef/browser/resource_context.h"
#include "libcef/browser/thread_util.h"
#include "libcef/browser/url_request_context_getter.h"

#include "base/bind.h"
#include "base/environment.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/threading/thread.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/geolocation_permission_context.h"
#include "content/public/browser/speech_recognition_preferences.h"

#if defined(OS_WIN)
#include "base/base_paths_win.h"
#elif defined(OS_LINUX)
#include "base/nix/xdg_util.h"
#endif

using content::BrowserThread;

namespace {

#if defined(OS_LINUX)
const char kDotConfigDir[] = ".config";
const char kXdgConfigHomeEnvVar[] = "XDG_CONFIG_HOME";
#endif


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
    callback.Run(false);
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

class CefSpeechRecognitionPreferences
    : public content::SpeechRecognitionPreferences {
 public:
  CefSpeechRecognitionPreferences() {
  }

  // Overridden from SpeechRecognitionPreferences:
  virtual bool FilterProfanities() const OVERRIDE {
    return false;
  }

  virtual void SetFilterProfanities(bool filter_profanities) OVERRIDE {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CefSpeechRecognitionPreferences);
};

}  // namespace

CefBrowserContext::CefBrowserContext(
    CefBrowserMainParts* main_parts)
    : main_parts_(main_parts) {
  // Initialize the request context getter.
  url_request_getter_ = new CefURLRequestContextGetter(
      GetPath(),
      BrowserThread::UnsafeGetMessageLoopForThread(BrowserThread::IO),
      BrowserThread::UnsafeGetMessageLoopForThread(BrowserThread::FILE));
}

CefBrowserContext::~CefBrowserContext() {
  if (resource_context_.get()) {
    BrowserThread::DeleteSoon(
      BrowserThread::IO, FROM_HERE, resource_context_.release());
  }
}

FilePath CefBrowserContext::GetPath() {
  if (!path_.empty())
    return path_;

#if defined(OS_WIN)
  CHECK(PathService::Get(base::DIR_LOCAL_APP_DATA, &path_));
  path_ = path_.Append(std::wstring(L"cef_data"));
#elif defined(OS_LINUX)
  scoped_ptr<base::Environment> env(base::Environment::Create());
  FilePath config_dir(base::nix::GetXDGDirectory(env.get(),
                                                 kXdgConfigHomeEnvVar,
                                                 kDotConfigDir));
  path_ = config_dir.Append("cef_data");
#elif defined(OS_MACOSX)
  CHECK(PathService::Get(base::DIR_APP_DATA, &path_));
  path_ = path_.Append("cef_data");
#else
  NOTIMPLEMENTED();
#endif

  if (!file_util::PathExists(path_))
    file_util::CreateDirectory(path_);

  return path_;
}

bool CefBrowserContext::IsOffTheRecord() const {
  return false;
}

content::DownloadManager* CefBrowserContext::GetDownloadManager() {
  if (!download_manager_.get()) {
    download_manager_delegate_ = new CefDownloadManagerDelegate();
    download_manager_ = new DownloadManagerImpl(download_manager_delegate_,
                                                NULL);
    download_manager_delegate_->SetDownloadManager(download_manager_.get());
    download_manager_->Init(this);
  }
  return download_manager_.get();
}

net::URLRequestContextGetter* CefBrowserContext::GetRequestContext() {
  return url_request_getter_;
}

net::URLRequestContextGetter*
    CefBrowserContext::GetRequestContextForRenderProcess(
        int renderer_child_id) {
  CefRefPtr<CefBrowserHostImpl> browser =
      CefBrowserHostImpl::GetBrowserByChildID(renderer_child_id);
  if (browser.get())
    return browser->GetRequestContext();
  return GetRequestContext();
}

net::URLRequestContextGetter*
    CefBrowserContext::GetRequestContextForMedia() {
  return GetRequestContext();
}

content::ResourceContext* CefBrowserContext::GetResourceContext() {
  if (!resource_context_.get()) {
    resource_context_.reset(new CefResourceContext(
        static_cast<CefURLRequestContextGetter*>(GetRequestContext())));
  }
  return resource_context_.get();
}

content::GeolocationPermissionContext*
    CefBrowserContext::GetGeolocationPermissionContext() {
  if (!geolocation_permission_context_) {
    geolocation_permission_context_ =
        new CefGeolocationPermissionContext();
  }
  return geolocation_permission_context_;
}

content::SpeechRecognitionPreferences*
    CefBrowserContext::GetSpeechRecognitionPreferences() {
  if (!speech_recognition_preferences_.get())
    speech_recognition_preferences_ = new CefSpeechRecognitionPreferences();
  return speech_recognition_preferences_.get();
}

bool CefBrowserContext::DidLastSessionExitCleanly() {
  return true;
}

quota::SpecialStoragePolicy* CefBrowserContext::GetSpecialStoragePolicy() {
  return NULL;
}
