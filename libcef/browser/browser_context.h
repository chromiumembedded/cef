// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_BROWSER_CONTEXT_H_
#define CEF_LIBCEF_BROWSER_BROWSER_CONTEXT_H_
#pragma once

#include "include/cef_request_context_handler.h"
#include "libcef/browser/chrome_profile_stub.h"
#include "libcef/browser/net/url_request_context_getter_impl.h"
#include "libcef/browser/resource_context.h"

#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"

/*
// Classes used in request processing (network, storage, service, etc.):
//
// WC = WebContents
//  Content API representation of a browser. Created by BHI or the system (for
//  popups) and owned by BHI. Keeps a pointer to BCI/BCP.
//
// BHI = CefBrowserHostImpl
//  Implements the CefBrowser and CefBrowserHost interfaces which are exposed
//  to clients. References an RCI instance. Owns a WC. Life span is controlled
//  by client references and CefContentBrowserClient.
//
// RCI = CefRequestContextImpl
//  Implements the CefRequestContext interface which is exposed to clients.
//  References the isolated BCI or creates a new BCP.
//
// BCI = CefBrowserContextImpl
//   Entry point from WC when using an isolated RCI. Owns the RC and creates the
//   URCGI. Life span controlled by RCI and (for the global context)
//   CefBrowserMainParts.
//
// BCP = CefBrowserContextProxy
//   Entry point from WC when using a custom RCI. Owns the RC and creates the
//   URCGP. Life span controlled by RCI.
//
// RC = CefResourceContext
//   Acts as a bridge for resource loading. URLRequest life span is tied to this
//   object. Must be destroyed before the associated URCGI/URCGP. Life span is
//   controlled by BCI/BCP.
//
// URCGI = CefURLRequestContextGetterImpl
//   Creates and owns the URCI. Life span is controlled by RC and (for the
//   global context) CefBrowserMainParts.
//
// URCGP = CefURLRequestContextGetterProxy
//   Creates and owns the URCP. Life span is controlled by RC.
//
// URCI = CefURLRequestContextImpl
//   Owns various network-related objects including the isolated cookie manager.
//   Owns URLRequest objects which must be destroyed first. Life span is
//   controlled by URCGI.
//
// URCP = CefURLRequestContextProxy
//   Creates the CSP and forwards requests to the objects owned by URCI. Owns
//   URLRequest objects which must be destroyed first. Life span is controlled
//   by URCGP.
//
// CSP = CefCookieStoreProxy
//   Gives the CefCookieManager instance retrieved via CefRequestContextHandler
//   an opportunity to handle cookie requests. Otherwise forwards requests via
//   URCI to the isolated cookie manager. Life span is controlled by URCP.
//
//
// Relationship diagram:
//   ref = reference (CefRefPtr/scoped_refptr)
//   own = ownership (scoped_ptr)
//   ptr = raw pointer
//
//                    CefBrowserMainParts      isolated cookie manager, etc...
//                       |           |            ^
//                      ref         ref        ref/own
//                       v           v            |
//                /---> BCI -ref-> URCGI --own-> URCI <-ptr-- CSP
//               /       ^           ^                        ^
//             ptr      ref         ref                      /
//             /         |           |                      /
// BHI -own-> WC -ptr-> BCP -ref-> URCGP -own-> URCP --ref-/
//
// BHI -ref-> RCI -ref-> BCI/BCP -own-> RC -ref-> URCGI/URCGP
//
//
// How shutdown works:
// 1. CefBrowserHostImpl is destroyed on any thread due to browser close,
//    ref release, etc.
// 2. CefRequestContextImpl is destroyed on any thread due to
//    CefBrowserHostImpl destruction, ref release, etc.
// 3. CefBrowserContext* is destroyed on the UI thread due to
//    CefRequestContextImpl destruction (*Impl, *Proxy) or ref release in
//    CefBrowserMainParts::PostMainMessageLoopRun() (*Impl).
// 4. CefResourceContext is destroyed asynchronously on the IO thread due to
//    CefBrowserContext* destruction. This cancels/destroys any pending
//    URLRequests.
// 5. CefURLRequestContextGetter* is destroyed asynchronously on the IO thread
//    due to CefResourceContext destruction (*Impl, *Proxy) or ref release in
//    CefBrowserMainParts::PostMainMessageLoopRun() (*Impl). This may be delayed
//    if other network-related objects still have a reference to it.
// 6. CefURLRequestContext* is destroyed on the IO thread due to
//    CefURLRequestContextGetter* destruction. 
*/

class HostContentSettingsMap;
class PrefService;

namespace extensions {
class CefExtensionSystem;
}

// Main entry point for configuring behavior on a per-browser basis. An instance
// of this class is passed to WebContents::Create in CefBrowserHostImpl::
// CreateInternal. Only accessed on the UI thread unless otherwise indicated.
class CefBrowserContext
    : public ChromeProfileStub,
      public base::RefCountedThreadSafe<
          CefBrowserContext, content::BrowserThread::DeleteOnUIThread> {
 public:
  CefBrowserContext();

  // Must be called immediately after this object is created.
  virtual void Initialize();

  // BrowserContext methods.
  content::ResourceContext* GetResourceContext() override;

  // Profile methods.
  ChromeZoomLevelPrefs* GetZoomLevelPrefs() override;

  // Returns the settings associated with this object. Safe to call from any
  // thread.
  virtual const CefRequestContextSettings& GetSettings() const = 0;

  // Returns the handler associated with this object. Safe to call from any
  // thread.
  virtual CefRefPtr<CefRequestContextHandler> GetHandler() const = 0;

  // Called from CefContentBrowserClient to create the URLRequestContextGetter.
  virtual net::URLRequestContextGetter* CreateRequestContext(
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors) = 0;
  virtual net::URLRequestContextGetter* CreateRequestContextForStoragePartition(
      const base::FilePath& partition_path,
      bool in_memory,
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors) = 0;

  // Settings for plugins and extensions.
  virtual HostContentSettingsMap* GetHostContentSettingsMap() = 0;

  // Called from CefBrowserHostImpl::DidNavigateAnyFrame to update the table of
  // visited links.
  virtual void AddVisitedURLs(const std::vector<GURL>& urls) = 0;

  CefResourceContext* resource_context() const {
    return resource_context_.get();
  }
  extensions::CefExtensionSystem* extension_system() const {
    return extension_system_;
  }

#ifndef NDEBUG
  // Simple tracking of allocated objects.
  static base::AtomicRefCount DebugObjCt;  // NOLINT(runtime/int)
#endif

 protected:
  ~CefBrowserContext() override;

  // Must be called before the child object destructor has completed.
  void Shutdown();

 private:
  // Only allow deletion via scoped_refptr().
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<CefBrowserContext>;

  scoped_ptr<CefResourceContext> resource_context_;

  // Owned by the KeyedService system.
  extensions::CefExtensionSystem* extension_system_;

  DISALLOW_COPY_AND_ASSIGN(CefBrowserContext);
};

#endif  // CEF_LIBCEF_BROWSER_BROWSER_CONTEXT_H_
