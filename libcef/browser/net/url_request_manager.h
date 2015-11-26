// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_NET_URL_REQUEST_MANAGER_H_
#define CEF_LIBCEF_BROWSER_NET_URL_REQUEST_MANAGER_H_
#pragma once

#include <map>

#include "include/cef_scheme.h"

namespace net {
class NetworkDelegate;
class URLRequest;
class URLRequestJob;
class URLRequestJobFactoryImpl;
}

class CefProtocolHandler;

// Class that manages CefSchemeHandlerFactory instances. Only accessed on the IO
// thread.
class CefURLRequestManager {
 public:
  explicit CefURLRequestManager(net::URLRequestJobFactoryImpl* job_factory);
  ~CefURLRequestManager();

  // Add |factory| for the specified |scheme| and |domain|. See documentation on
  // CefRequestContext::RegisterSchemeHandlerFactory() for usage.
  bool AddFactory(const std::string& scheme,
                  const std::string& domain,
                  CefRefPtr<CefSchemeHandlerFactory> factory);

  // Remove all factories associated with the specified |scheme| and |domain|.
  void RemoveFactory(const std::string& scheme,
                     const std::string& domain);

  // Clear all the existing URL handlers and unregister the ProtocolFactory.
  void ClearFactories();

  // Helper for chaining ProtocolHandler implementations.
  net::URLRequestJob* GetRequestJob(net::URLRequest* request,
                                    net::NetworkDelegate* network_delegate);

 private:
  friend class CefProtocolHandler;

  // Add or remove the protocol handler if necessary. |scheme| will already be
  // in lower case.
  void SetProtocolHandlerIfNecessary(const std::string& scheme, bool add);

  // Returns true if any factory currently exists for |scheme|. |scheme| will
  // already be in lower case.
  bool HasFactory(const std::string& scheme);

  // Retrieve the matching handler factory, if any. |scheme| will already be in
  // lower case.
  CefRefPtr<CefSchemeHandlerFactory> GetHandlerFactory(
      net::URLRequest* request, const std::string& scheme);

  // Create the job that will handle the request. |scheme| will already be in
  // lower case.
  net::URLRequestJob* GetRequestJob(net::URLRequest* request,
                                    net::NetworkDelegate* network_delegate,
                                    const std::string& scheme);

  // Life span of |job_factory_| is guaranteed by
  // CefURLRequestContextGetterImpl which also owns this object.
  net::URLRequestJobFactoryImpl* job_factory_;

  // Map (scheme, domain) to factories.
  typedef std::map<std::pair<std::string, std::string>,
      CefRefPtr<CefSchemeHandlerFactory> > HandlerMap;
  HandlerMap handler_map_;

  DISALLOW_COPY_AND_ASSIGN(CefURLRequestManager);
};

#endif  // CEF_LIBCEF_BROWSER_NET_URL_REQUEST_MANAGER_H_
