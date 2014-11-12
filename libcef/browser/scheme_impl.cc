// Copyright (c) 2012 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "include/cef_browser.h"
#include "include/cef_scheme.h"
#include "libcef/browser/browser_context.h"
#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/content_browser_client.h"
#include "libcef/browser/context.h"
#include "libcef/browser/resource_request_job.h"
#include "libcef/browser/scheme_handler.h"
#include "libcef/browser/thread_util.h"
#include "libcef/browser/url_request_context_getter.h"
#include "libcef/common/request_impl.h"
#include "libcef/common/response_impl.h"
#include "libcef/common/scheme_registration.h"
#include "libcef/common/upload_data.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_http_job.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_util.h"

using net::URLRequestStatus;

namespace {

bool IsStandardScheme(const std::string& scheme) {
  url::Component scheme_comp(0, scheme.length());
  return url::IsStandard(scheme.c_str(), scheme_comp);
}

// Copied from net/url_request/url_request_job_manager.cc.
struct SchemeToFactory {
  const char* scheme;
  net::URLRequest::ProtocolFactory* factory;
};
static const SchemeToFactory kBuiltinFactories[] = {
  { "http", net::URLRequestHttpJob::Factory },
  { "https", net::URLRequestHttpJob::Factory },
};

bool IsBuiltinScheme(const std::string& scheme) {
  for (size_t i = 0; i < arraysize(kBuiltinFactories); ++i)
    if (LowerCaseEqualsASCII(scheme, kBuiltinFactories[i].scheme))
      return true;
  return false;
}

net::URLRequestJob* GetBuiltinSchemeRequestJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    const std::string& scheme) {
  // See if the request should be handled by a built-in protocol factory.
  for (size_t i = 0; i < arraysize(kBuiltinFactories); ++i) {
    if (scheme == kBuiltinFactories[i].scheme) {
      net::URLRequestJob* job =
          (kBuiltinFactories[i].factory)(request, network_delegate, scheme);
      DCHECK(job);  // The built-in factories are not expected to fail!
      return job;
    }
  }

  return NULL;
}

std::string ToLower(const std::string& str) {
  std::string str_lower = str;
  std::transform(str_lower.begin(), str_lower.end(), str_lower.begin(),
      towlower);
  return str;
}

// Class that manages the CefSchemeHandlerFactory instances.
class CefUrlRequestManager {
 protected:
  // Class used for creating URLRequestJob instances. The lifespan of this
  // object is managed by URLRequestJobFactory.
  class ProtocolHandler : public net::URLRequestJobFactory::ProtocolHandler {
   public:
    explicit ProtocolHandler(const std::string& scheme)
      : scheme_(scheme) {}

    // From net::URLRequestJobFactory::ProtocolHandler
    net::URLRequestJob* MaybeCreateJob(
        net::URLRequest* request,
        net::NetworkDelegate* network_delegate) const override {
      CEF_REQUIRE_IOT();
      return CefUrlRequestManager::GetInstance()->GetRequestJob(
          request, network_delegate, scheme_);
    }

   private:
    std::string scheme_;
  };

 public:
  CefUrlRequestManager() {}

  // Retrieve the singleton instance.
  static CefUrlRequestManager* GetInstance();

  bool AddFactory(const std::string& scheme,
                  const std::string& domain,
                  CefRefPtr<CefSchemeHandlerFactory> factory) {
    if (!factory.get()) {
      RemoveFactory(scheme, domain);
      return true;
    }

    CEF_REQUIRE_IOT();

    std::string scheme_lower = ToLower(scheme);
    std::string domain_lower = ToLower(domain);

    // Hostname is only supported for standard schemes.
    if (!IsStandardScheme(scheme_lower))
      domain_lower.clear();

    SetProtocolHandlerIfNecessary(scheme_lower, true);

    handler_map_[make_pair(scheme_lower, domain_lower)] = factory;

    return true;
  }

  void RemoveFactory(const std::string& scheme,
                     const std::string& domain) {
    CEF_REQUIRE_IOT();

    std::string scheme_lower = ToLower(scheme);
    std::string domain_lower = ToLower(domain);

    // Hostname is only supported for standard schemes.
    if (!IsStandardScheme(scheme_lower))
      domain_lower.clear();

    HandlerMap::iterator iter =
        handler_map_.find(make_pair(scheme_lower, domain_lower));
    if (iter != handler_map_.end()) {
      handler_map_.erase(iter);

      SetProtocolHandlerIfNecessary(scheme_lower, false);
    }
  }

  // Clear all the existing URL handlers and unregister the ProtocolFactory.
  void ClearFactories() {
    CEF_REQUIRE_IOT();

    net::URLRequestJobFactoryImpl* job_factory = GetJobFactoryImpl();

    // Create a unique set of scheme names.
    std::set<std::string> schemes;
    for (HandlerMap::const_iterator i = handler_map_.begin();
        i != handler_map_.end(); ++i) {
      schemes.insert(i->first.first);
    }

    for (std::set<std::string>::const_iterator scheme = schemes.begin();
        scheme != schemes.end(); ++scheme) {
      const std::string& scheme_name = *scheme;
      if (!scheme::IsInternalProtectedScheme(scheme_name)) {
        bool set_protocol = job_factory->SetProtocolHandler(scheme_name, NULL);
        DCHECK(set_protocol);
      }
    }

    handler_map_.clear();
  }

  // Helper for chaining ProtocolHandler implementations.
  net::URLRequestJob* GetRequestJob(net::URLRequest* request,
                                    net::NetworkDelegate* network_delegate) {
    CEF_REQUIRE_IOT();
    return GetRequestJob(request, network_delegate, request->url().scheme());
  }

 private:
  net::URLRequestJobFactoryImpl* GetJobFactoryImpl() {
    return static_cast<CefURLRequestContextGetter*>(
        CefContentBrowserClient::Get()->request_context().get())->
            job_factory_impl();
  }

  // Add or remove the protocol handler if necessary. |scheme| will already be
  // in lower case.
  void SetProtocolHandlerIfNecessary(const std::string& scheme, bool add) {
    // Don't modify a protocol handler for internal protected schemes or if the
    // protocol handler is still needed by other registered factories.
    if (scheme::IsInternalProtectedScheme(scheme) || HasFactory(scheme))
      return;

    net::URLRequestJobFactoryImpl* job_factory = GetJobFactoryImpl();
    bool set_protocol = job_factory->SetProtocolHandler(
        scheme,
        (add ? new ProtocolHandler(scheme) : NULL));
    DCHECK(set_protocol);
  }

  // Returns true if any factory currently exists for |scheme|. |scheme| will
  // already be in lower case.
  bool HasFactory(const std::string& scheme) {
    if (handler_map_.empty())
      return false;

    for (HandlerMap::const_iterator i = handler_map_.begin();
        i != handler_map_.end(); ++i) {
      if (scheme == i->first.first)
        return true;
    }

    return false;
  }

  // Retrieve the matching handler factory, if any. |scheme| will already be in
  // lower case.
  CefRefPtr<CefSchemeHandlerFactory> GetHandlerFactory(
      net::URLRequest* request, const std::string& scheme) {
    CefRefPtr<CefSchemeHandlerFactory> factory;

    if (request->url().is_valid() && IsStandardScheme(scheme)) {
      // Check for a match with a domain first.
      const std::string& domain = request->url().host();

      HandlerMap::iterator i = handler_map_.find(make_pair(scheme, domain));
      if (i != handler_map_.end())
        factory = i->second;
    }

    if (!factory.get()) {
      // Check for a match with no specified domain.
      HandlerMap::iterator i =
          handler_map_.find(make_pair(scheme, std::string()));
      if (i != handler_map_.end())
        factory = i->second;
    }

    return factory;
  }

  // Create the job that will handle the request. |scheme| will already be in
  // lower case.
  net::URLRequestJob* GetRequestJob(net::URLRequest* request,
                                    net::NetworkDelegate* network_delegate,
                                    const std::string& scheme) {
    net::URLRequestJob* job = NULL;
    CefRefPtr<CefSchemeHandlerFactory> factory =
        GetHandlerFactory(request, scheme);
    if (factory.get()) {
      CefRefPtr<CefBrowserHostImpl> browser =
          CefBrowserHostImpl::GetBrowserForRequest(request);
      CefRefPtr<CefFrame> frame;
      if (browser.get())
        frame = browser->GetFrameForRequest(request);

      // Populate the request data.
      CefRefPtr<CefRequestImpl> requestPtr(new CefRequestImpl());
      requestPtr->Set(request);

      // Call the handler factory to create the handler for the request.
      CefRefPtr<CefResourceHandler> handler =
          factory->Create(browser.get(), frame, scheme, requestPtr.get());
      if (handler.get())
        job = new CefResourceRequestJob(request, network_delegate, handler);
    }

    if (!job && IsBuiltinScheme(scheme)) {
      // Give the built-in scheme handler a chance to handle the request.
      job = GetBuiltinSchemeRequestJob(request, network_delegate, scheme);
    }

#ifndef NDEBUG
    if (job)
      DLOG(INFO) << "CefUrlRequestManager hit for " << request->url().spec();
#endif

    return job;
  }

  // Map (scheme, domain) to factories. Will only be accessed on the IO thread.
  typedef std::map<std::pair<std::string, std::string>,
      CefRefPtr<CefSchemeHandlerFactory> > HandlerMap;
  HandlerMap handler_map_;

  DISALLOW_EVIL_CONSTRUCTORS(CefUrlRequestManager);
};

base::LazyInstance<CefUrlRequestManager> g_manager = LAZY_INSTANCE_INITIALIZER;

CefUrlRequestManager* CefUrlRequestManager::GetInstance() {
  return g_manager.Pointer();
}

}  // namespace


bool CefRegisterSchemeHandlerFactory(
    const CefString& scheme_name,
    const CefString& domain_name,
    CefRefPtr<CefSchemeHandlerFactory> factory) {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID())
    return false;

  if (CEF_CURRENTLY_ON(CEF_IOT)) {
    return CefUrlRequestManager::GetInstance()->AddFactory(scheme_name,
                                                           domain_name,
                                                           factory);
  } else {
    CEF_POST_TASK(CEF_IOT,
        base::Bind(base::IgnoreResult(&CefRegisterSchemeHandlerFactory),
                   scheme_name, domain_name, factory));
    return true;
  }
}

bool CefClearSchemeHandlerFactories() {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID())
    return false;

  if (CEF_CURRENTLY_ON(CEF_IOT)) {
    CefUrlRequestManager::GetInstance()->ClearFactories();

    // Register internal scheme handlers.
    scheme::RegisterInternalHandlers();
  } else {
    CEF_POST_TASK(CEF_IOT,
        base::Bind(base::IgnoreResult(&CefClearSchemeHandlerFactories)));
  }

  return true;
}

namespace scheme {

net::URLRequestJob* GetRequestJob(net::URLRequest* request,
                                  net::NetworkDelegate* network_delegate) {
  return CefUrlRequestManager::GetInstance()->GetRequestJob(
      request, network_delegate);
}

}  // namespace scheme
