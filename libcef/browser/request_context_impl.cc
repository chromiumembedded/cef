// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/request_context_impl.h"
#include "libcef/browser/browser_context_impl.h"
#include "libcef/browser/browser_context_proxy.h"
#include "libcef/browser/content_browser_client.h"
#include "libcef/browser/context.h"
#include "libcef/browser/cookie_manager_impl.h"
#include "libcef/browser/thread_util.h"
#include "base/atomic_sequence_num.h"
#include "base/logging.h"

using content::BrowserThread;

namespace {

base::StaticAtomicSequenceNumber g_next_id;

}  // namespace


// CefBrowserContext

// static
CefRefPtr<CefRequestContext> CefRequestContext::GetGlobalContext() {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return NULL;
  }

  return CefRequestContextImpl::GetForBrowserContext(
      CefContentBrowserClient::Get()->browser_context());
}

// static
CefRefPtr<CefRequestContext> CefRequestContext::CreateContext(
    const CefRequestContextSettings& settings,
    CefRefPtr<CefRequestContextHandler> handler) {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return NULL;
  }

  return new CefRequestContextImpl(settings, handler);
}

// static
CefRefPtr<CefRequestContext> CefRequestContext::CreateContext(
    CefRefPtr<CefRequestContext> other,
    CefRefPtr<CefRequestContextHandler> handler) {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    NOTREACHED() << "context not valid";
    return NULL;
  }

  if (!other.get())
    return NULL;

  return new CefRequestContextImpl(
      static_cast<CefRequestContextImpl*>(other.get()), handler);
}


// CefBrowserContextImpl

CefRequestContextImpl::~CefRequestContextImpl() {
}

// static
CefRefPtr<CefRequestContextImpl> CefRequestContextImpl::GetForRequestContext(
      CefRefPtr<CefRequestContext> request_context) {
   if (request_context.get()) {
    // Use the context from the provided CefRequestContext.
    return static_cast<CefRequestContextImpl*>(request_context.get());
  }
  
  // Use the global context.
  scoped_refptr<CefBrowserContext> browser_context =
      CefContentBrowserClient::Get()->browser_context();
  return GetForBrowserContext(browser_context);
}

// static
CefRefPtr<CefRequestContextImpl> CefRequestContextImpl::GetForBrowserContext(
      scoped_refptr<CefBrowserContext> browser_context) {
  DCHECK(browser_context.get());
  return new CefRequestContextImpl(browser_context);
}

scoped_refptr<CefBrowserContext> CefRequestContextImpl::GetBrowserContext() {
  CEF_REQUIRE_UIT();

  if (!browser_context_) {
    scoped_refptr<CefBrowserContextImpl> parent;

    if (other_.get()) {
      // Share storage with |other_|.
      parent = CefBrowserContextImpl::GetForContext(
          other_->GetBrowserContext().get());
    }

    if (!parent.get()) {
      const base::FilePath& cache_path =
          base::FilePath(CefString(&settings_.cache_path));
      if (!cache_path.empty()) {
        // Check if a CefBrowserContextImpl is already globally registered for
        // the specified cache path. If so then use it.
        parent = CefBrowserContextImpl::GetForCachePath(cache_path);
      }
    }

    if (!parent.get()) {
      // Create a new CefBrowserContextImpl instance. If the cache path is non-
      // empty then this new instance will become the globally registered
      // CefBrowserContextImpl for that path. Otherwise, this new instance will
      // be a completely isolated "incongento mode" context.
      parent = new CefBrowserContextImpl(settings_);
      parent->Initialize();
    }

    // The parent's settings may be different. Force our settings to match the
    // parent.
    settings_ = parent->GetSettings();

    if (handler_.get()) {
      // Use a proxy that will execute handler callbacks where appropriate and
      // otherwise forward all requests to the parent implementation.
      browser_context_ = new CefBrowserContextProxy(handler_, parent);
      browser_context_->Initialize();
    } else {
      // Use the parent implementation directly.
      browser_context_ = parent;
    }

    request_context_impl_ = parent->request_context().get();
    DCHECK(request_context_impl_);

    if (handler_.get()) {
      // Keep the handler alive until the associated request context is
      // destroyed.
      request_context_impl_->AddHandler(handler_);
    }
  }

  if (!request_context_impl_) {
    request_context_impl_ =
        CefBrowserContextImpl::GetForContext(browser_context_.get())->
            request_context().get();
    DCHECK(request_context_impl_);
  }

  if (other_.get()) {
    // Clear the reference to |other_| after setting |request_context_impl_|.
    // This is the reverse order of checks in IsSharedWith().
    other_ = NULL;
  }

  return browser_context_;
}

void CefRequestContextImpl::GetBrowserContext(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const BrowserContextCallback& callback) {
  if (!task_runner.get())
    task_runner = base::MessageLoop::current()->task_runner();
  GetBrowserContextOnUIThread(task_runner, callback);
}

void CefRequestContextImpl::GetRequestContextImpl(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const RequestContextCallback& callback) {
  if (!task_runner.get())
    task_runner = base::MessageLoop::current()->task_runner();
  if (request_context_impl_) {
    // The browser context already exists.
    DCHECK(browser_context_.get());
    GetRequestContextImplOnIOThread(task_runner, callback, browser_context_);
  } else {
    // Need to initialize the browser context first.
    GetBrowserContextOnUIThread(
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO),
        base::Bind(&CefRequestContextImpl::GetRequestContextImplOnIOThread,
                   this, task_runner, callback));
  }
}

bool CefRequestContextImpl::IsSame(CefRefPtr<CefRequestContext> other) {
  CefRequestContextImpl* other_impl =
      static_cast<CefRequestContextImpl*>(other.get());
  if (!other_impl)
    return false;

  // Compare CefBrowserContext pointers if one has been associated.
  if (browser_context_ && other_impl->browser_context_)
    return (browser_context_ == other_impl->browser_context_);
  else if (browser_context_ || other_impl->browser_context_)
    return false;

  // Otherwise compare unique IDs.
  return (unique_id_ == other_impl->unique_id_);
}

bool CefRequestContextImpl::IsSharingWith(CefRefPtr<CefRequestContext> other) {
  CefRequestContextImpl* other_impl =
      static_cast<CefRequestContextImpl*>(other.get());
  if (!other_impl)
    return false;

  if (IsSame(other))
    return true;

  CefRefPtr<CefRequestContext> pending_other = other_;
  if (pending_other.get()) {
    // This object is not initialized but we know what context this object will
    // share with. Compare to that other context instead.
    return pending_other->IsSharingWith(other);
  }

  pending_other = other_impl->other_;
  if (pending_other.get()) {
    // The other object is not initialized but we know what context that object
    // will share with. Compare to that other context instead.
    return pending_other->IsSharingWith(this);
  }

  if (request_context_impl_ && other_impl->request_context_impl_) {
    // Both objects are initialized. Compare the request context objects.
    return (request_context_impl_ == other_impl->request_context_impl_);
  }

  // This or the other object is not initialized. Compare the cache path values.
  // If both are non-empty and the same then they'll share the same storage.
  if (settings_.cache_path.length > 0 &&
      other_impl->settings_.cache_path.length > 0) {
    return (base::FilePath(CefString(&settings_.cache_path)) ==
            base::FilePath(CefString(&other_impl->settings_.cache_path)));
  }

  return false;
}

bool CefRequestContextImpl::IsGlobal() {
  return (browser_context_ ==
          CefContentBrowserClient::Get()->browser_context());
}

CefRefPtr<CefRequestContextHandler> CefRequestContextImpl::GetHandler() {
  return handler_;
}

CefString CefRequestContextImpl::GetCachePath() {
  return CefString(&settings_.cache_path);
}

CefRefPtr<CefCookieManager> CefRequestContextImpl::GetDefaultCookieManager(
    CefRefPtr<CefCompletionCallback> callback) {
  CefRefPtr<CefCookieManagerImpl> cookie_manager = new CefCookieManagerImpl();
  cookie_manager->Initialize(this, CefString(), false, callback);
  return cookie_manager.get();
}

bool CefRequestContextImpl::RegisterSchemeHandlerFactory(
    const CefString& scheme_name,
    const CefString& domain_name,
    CefRefPtr<CefSchemeHandlerFactory> factory) {
  GetRequestContextImpl(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO),
      base::Bind(&CefRequestContextImpl::RegisterSchemeHandlerFactoryInternal,
                 this, scheme_name, domain_name, factory));
  return true;
}

bool CefRequestContextImpl::ClearSchemeHandlerFactories() {
  GetRequestContextImpl(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO),
      base::Bind(&CefRequestContextImpl::ClearSchemeHandlerFactoriesInternal,
                 this));
  return true;
}

CefRequestContextImpl::CefRequestContextImpl(
    scoped_refptr<CefBrowserContext> browser_context)
    : browser_context_(browser_context),
      settings_(browser_context->GetSettings()),
      handler_(browser_context->GetHandler()),
      unique_id_(0),
      request_context_impl_(NULL) {
}

CefRequestContextImpl::CefRequestContextImpl(
    const CefRequestContextSettings& settings,
    CefRefPtr<CefRequestContextHandler> handler)
    : settings_(settings),
      handler_(handler),
      unique_id_(g_next_id.GetNext()),
      request_context_impl_(NULL) {
}

CefRequestContextImpl::CefRequestContextImpl(
    CefRefPtr<CefRequestContextImpl> other,
    CefRefPtr<CefRequestContextHandler> handler)
    : other_(other),
      handler_(handler),
      unique_id_(g_next_id.GetNext()),
      request_context_impl_(NULL) {
}

void CefRequestContextImpl::GetBrowserContextOnUIThread(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const BrowserContextCallback& callback) {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT,
        base::Bind(&CefRequestContextImpl::GetBrowserContextOnUIThread,
                   this, task_runner, callback));
    return;
  }

  // Make sure the browser context exists.
  GetBrowserContext();
  DCHECK(browser_context_.get());
  DCHECK(request_context_impl_);

  if (task_runner->BelongsToCurrentThread()) {
    // Execute the callback immediately.
    callback.Run(browser_context_);
  } else {
    // Execute the callback on the target thread.
    task_runner->PostTask(FROM_HERE, base::Bind(callback, browser_context_));
  }
}

void CefRequestContextImpl::GetRequestContextImplOnIOThread(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const RequestContextCallback& callback,
    scoped_refptr<CefBrowserContext> browser_context) {
  if (!CEF_CURRENTLY_ON_IOT()) {
    CEF_POST_TASK(CEF_IOT,
        base::Bind(&CefRequestContextImpl::GetRequestContextImplOnIOThread,
                   this, task_runner, callback, browser_context));
    return;
  }

  DCHECK(request_context_impl_);

  // Make sure the request context exists.
  request_context_impl_->GetURLRequestContext();

  if (task_runner->BelongsToCurrentThread()) {
    // Execute the callback immediately.
    callback.Run(request_context_impl_);
  } else {
    // Execute the callback on the target thread.
    task_runner->PostTask(FROM_HERE,
        base::Bind(callback, make_scoped_refptr(request_context_impl_)));
  }
}

void CefRequestContextImpl::RegisterSchemeHandlerFactoryInternal(
    const CefString& scheme_name,
    const CefString& domain_name,
    CefRefPtr<CefSchemeHandlerFactory> factory,
    scoped_refptr<CefURLRequestContextGetterImpl> request_context) {
  CEF_REQUIRE_IOT();
  request_context->request_manager()->AddFactory(scheme_name, domain_name,
                                                 factory);
}

void CefRequestContextImpl::ClearSchemeHandlerFactoriesInternal(
    scoped_refptr<CefURLRequestContextGetterImpl> request_context) {
  CEF_REQUIRE_IOT();
  request_context->request_manager()->ClearFactories();
}
