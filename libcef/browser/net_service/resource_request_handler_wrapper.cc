// Copyright (c) 2019 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/net_service/resource_request_handler_wrapper.h"

#include <memory>

#include "libcef/browser/browser_host_base.h"
#include "libcef/browser/context.h"
#include "libcef/browser/iothread_state.h"
#include "libcef/browser/net_service/cookie_helper.h"
#include "libcef/browser/net_service/proxy_url_loader_factory.h"
#include "libcef/browser/net_service/resource_handler_wrapper.h"
#include "libcef/browser/net_service/response_filter_wrapper.h"
#include "libcef/browser/prefs/browser_prefs.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/app_manager.h"
#include "libcef/common/net/scheme_registration.h"
#include "libcef/common/net_service/net_service_util.h"
#include "libcef/common/request_impl.h"
#include "libcef/common/response_impl.h"

#include "chrome/browser/profiles/profile.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#include "ui/base/page_transition_types.h"
#include "url/origin.h"

namespace net_service {

namespace {

const int kLoadNoCookiesFlags =
    net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES;

class RequestCallbackWrapper : public CefCallback {
 public:
  using Callback = base::OnceCallback<void(bool /* allow */)>;
  explicit RequestCallbackWrapper(Callback callback)
      : callback_(std::move(callback)),
        work_thread_task_runner_(
            base::SequencedTaskRunner::GetCurrentDefault()) {}

  RequestCallbackWrapper(const RequestCallbackWrapper&) = delete;
  RequestCallbackWrapper& operator=(const RequestCallbackWrapper&) = delete;

  ~RequestCallbackWrapper() override {
    if (!callback_.is_null()) {
      // Make sure it executes on the correct thread.
      work_thread_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback_), true));
    }
  }

  void Continue() override { ContinueNow(true); }

  void Cancel() override { ContinueNow(false); }

 private:
  void ContinueNow(bool allow) {
    if (!work_thread_task_runner_->RunsTasksInCurrentSequence()) {
      work_thread_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&RequestCallbackWrapper::ContinueNow, this, allow));
      return;
    }
    if (!callback_.is_null()) {
      std::move(callback_).Run(allow);
    }
  }

  Callback callback_;

  scoped_refptr<base::SequencedTaskRunner> work_thread_task_runner_;

  IMPLEMENT_REFCOUNTING(RequestCallbackWrapper);
};

class InterceptedRequestHandlerWrapper : public InterceptedRequestHandler {
 public:
  struct RequestState {
    RequestState() = default;

    void Reset(CefRefPtr<CefResourceRequestHandler> handler,
               CefRefPtr<CefSchemeHandlerFactory> scheme_factory,
               CefRefPtr<CefRequestImpl> request,
               bool request_was_redirected,
               CancelRequestCallback cancel_callback) {
      handler_ = handler;
      scheme_factory_ = scheme_factory;
      cookie_filter_ = nullptr;
      pending_request_ = request;
      pending_response_ = nullptr;
      request_was_redirected_ = request_was_redirected;
      was_custom_handled_ = false;
      cancel_callback_ = std::move(cancel_callback);
    }

    CefRefPtr<CefResourceRequestHandler> handler_;
    CefRefPtr<CefSchemeHandlerFactory> scheme_factory_;
    CefRefPtr<CefCookieAccessFilter> cookie_filter_;
    CefRefPtr<CefRequestImpl> pending_request_;
    CefRefPtr<CefResponseImpl> pending_response_;
    bool request_was_redirected_ = false;
    bool was_custom_handled_ = false;
    bool accept_language_added_ = false;
    CancelRequestCallback cancel_callback_;
  };

  struct PendingRequest {
    PendingRequest(int32_t request_id,
                   network::ResourceRequest* request,
                   bool request_was_redirected,
                   OnBeforeRequestResultCallback callback,
                   CancelRequestCallback cancel_callback)
        : id_(request_id),
          request_(request),
          request_was_redirected_(request_was_redirected),
          callback_(std::move(callback)),
          cancel_callback_(std::move(cancel_callback)) {}

    ~PendingRequest() {
      if (cancel_callback_) {
        std::move(cancel_callback_).Run(net::ERR_ABORTED);
      }
    }

    void Run(InterceptedRequestHandlerWrapper* self) {
      self->OnBeforeRequest(id_, request_, request_was_redirected_,
                            std::move(callback_), std::move(cancel_callback_));
    }

    const int32_t id_;
    network::ResourceRequest* const request_;
    const bool request_was_redirected_;
    OnBeforeRequestResultCallback callback_;
    CancelRequestCallback cancel_callback_;
  };

  // Observer to receive notification of CEF context or associated browser
  // destruction. Only one of the *Destroyed() methods will be called.
  class DestructionObserver : public CefBrowserHostBase::Observer,
                              public CefContext::Observer {
   public:
    explicit DestructionObserver(CefBrowserHostBase* browser) {
      if (browser) {
        browser_info_ = browser->browser_info();
        browser->AddObserver(this);
      } else {
        CefContext::Get()->AddObserver(this);
      }
    }

    DestructionObserver(const DestructionObserver&) = delete;
    DestructionObserver& operator=(const DestructionObserver&) = delete;

    ~DestructionObserver() override {
      CEF_REQUIRE_UIT();
      if (!registered_) {
        return;
      }

      // Verify that the browser or context still exists before attempting to
      // remove the observer.
      if (browser_info_) {
        auto browser = browser_info_->browser();
        if (browser) {
          browser->RemoveObserver(this);
        }
      } else if (CefContext::Get()) {
        // Network requests may be torn down during shutdown, so we can't check
        // CONTEXT_STATE_VALID() here.
        CefContext::Get()->RemoveObserver(this);
      }
    }

    void SetWrapper(base::WeakPtr<InterceptedRequestHandlerWrapper> wrapper) {
      CEF_REQUIRE_IOT();
      wrapper_ = wrapper;
    }

    void OnBrowserDestroyed(CefBrowserHostBase* browser) override {
      CEF_REQUIRE_UIT();
      browser->RemoveObserver(this);
      registered_ = false;
      browser_info_ = nullptr;
      NotifyOnDestroyed();
    }

    void OnContextDestroyed() override {
      CEF_REQUIRE_UIT();
      CefContext::Get()->RemoveObserver(this);
      registered_ = false;
      NotifyOnDestroyed();
    }

   private:
    void NotifyOnDestroyed() {
      if (wrapper_.MaybeValid()) {
        // This will be a no-op if the WeakPtr is invalid.
        CEF_POST_TASK(
            CEF_IOT,
            base::BindOnce(&InterceptedRequestHandlerWrapper::OnDestroyed,
                           wrapper_));
      }
    }

    scoped_refptr<CefBrowserInfo> browser_info_;
    bool registered_ = true;

    base::WeakPtr<InterceptedRequestHandlerWrapper> wrapper_;
  };

  // Holds state information for InterceptedRequestHandlerWrapper. State is
  // initialized on the UI thread and later passed to the *Wrapper object on
  // the IO thread.
  struct InitState {
    InitState() = default;

    ~InitState() {
      if (destruction_observer_) {
        if (initialized_) {
          // Clear the reference added in
          // InterceptedRequestHandlerWrapper::SetInitialized().
          destruction_observer_->SetWrapper(nullptr);
        }
        DeleteDestructionObserver();
      }
    }

    void Initialize(content::BrowserContext* browser_context,
                    CefRefPtr<CefBrowserHostBase> browser,
                    CefRefPtr<CefFrame> frame,
                    const content::GlobalRenderFrameHostId& global_id,
                    bool is_navigation,
                    bool is_download,
                    const url::Origin& request_initiator,
                    const base::RepeatingClosure& unhandled_request_callback) {
      CEF_REQUIRE_UIT();

      auto profile = Profile::FromBrowserContext(browser_context);
      auto cef_browser_context = CefBrowserContext::FromProfile(profile);
      browser_context_getter_ = cef_browser_context->getter();
      iothread_state_ = cef_browser_context->iothread_state();
      CHECK(iothread_state_);
      cookieable_schemes_ = cef_browser_context->GetCookieableSchemes();

      // We register to be notified of CEF context or browser destruction so
      // that we can stop accepting new requests and cancel pending/in-progress
      // requests in a timely manner (e.g. before we start asserting about
      // leaked objects during CEF shutdown).
      destruction_observer_ =
          std::make_unique<DestructionObserver>(browser.get());

      if (browser) {
        // These references will be released in OnDestroyed().
        browser_ = browser;
        frame_ = frame;
      }

      global_id_ = global_id;
      is_navigation_ = is_navigation;
      is_download_ = is_download;
      request_initiator_ = request_initiator.Serialize();
      unhandled_request_callback_ = unhandled_request_callback;

      // Default values for standard headers.
      accept_language_ = browser_prefs::GetAcceptLanguageList(profile);
      DCHECK(!accept_language_.empty());
      user_agent_ =
          CefAppManager::Get()->GetContentClient()->browser()->GetUserAgent();
      DCHECK(!user_agent_.empty());
    }

    void DeleteDestructionObserver() {
      DCHECK(destruction_observer_);
      CEF_POST_TASK(
          CEF_UIT,
          base::BindOnce(&InitState::DeleteDestructionObserverOnUIThread,
                         std::move(destruction_observer_)));
    }

    static void DeleteDestructionObserverOnUIThread(
        std::unique_ptr<DestructionObserver> observer) {}

    // Only accessed on the UI thread.
    CefBrowserContext::Getter browser_context_getter_;

    bool initialized_ = false;

    CefRefPtr<CefBrowserHostBase> browser_;
    CefRefPtr<CefFrame> frame_;
    scoped_refptr<CefIOThreadState> iothread_state_;
    CefBrowserContext::CookieableSchemes cookieable_schemes_;
    content::GlobalRenderFrameHostId global_id_;
    bool is_navigation_ = true;
    bool is_download_ = false;
    CefString request_initiator_;
    base::RepeatingClosure unhandled_request_callback_;

    // Default values for standard headers.
    std::string accept_language_;
    std::string user_agent_;

    // Used to route authentication and certificate callbacks through the
    // associated StoragePartition instance.
    mojo::PendingRemote<network::mojom::URLLoaderNetworkServiceObserver>
        url_loader_network_observer_;
    bool did_try_create_url_loader_network_observer_ = false;

    // Used to receive destruction notification.
    std::unique_ptr<DestructionObserver> destruction_observer_;
  };

  // Manages InterceptedRequestHandlerWrapper initialization. The *Wrapper
  // object is owned by ProxyURLLoaderFactory and may be deleted before
  // SetInitialized() is called.
  struct InitHelper : base::RefCountedThreadSafe<InitHelper> {
   public:
    explicit InitHelper(InterceptedRequestHandlerWrapper* wrapper)
        : wrapper_(wrapper) {}

    InitHelper(const InitHelper&) = delete;
    InitHelper& operator=(const InitHelper&) = delete;

    void MaybeSetInitialized(std::unique_ptr<InitState> init_state) {
      CEF_POST_TASK(CEF_IOT, base::BindOnce(&InitHelper::SetInitialized, this,
                                            std::move(init_state)));
    }

    void Disconnect() {
      base::AutoLock lock_scope(lock_);
      wrapper_ = nullptr;
    }

   private:
    void SetInitialized(std::unique_ptr<InitState> init_state) {
      base::AutoLock lock_scope(lock_);
      // May be nullptr if the InterceptedRequestHandlerWrapper has already
      // been deleted.
      if (!wrapper_) {
        return;
      }
      wrapper_->SetInitialized(std::move(init_state));
      wrapper_ = nullptr;
    }

    base::Lock lock_;
    InterceptedRequestHandlerWrapper* wrapper_;
  };

  InterceptedRequestHandlerWrapper()
      : init_helper_(base::MakeRefCounted<InitHelper>(this)),
        weak_ptr_factory_(this) {}

  InterceptedRequestHandlerWrapper(const InterceptedRequestHandlerWrapper&) =
      delete;
  InterceptedRequestHandlerWrapper& operator=(
      const InterceptedRequestHandlerWrapper&) = delete;

  ~InterceptedRequestHandlerWrapper() override {
    CEF_REQUIRE_IOT();

    // There should be no in-progress requests during destruction.
    DCHECK(request_map_.empty());

    // Don't continue with initialization if we get deleted before
    // SetInitialized is called asynchronously.
    init_helper_->Disconnect();
  }

  scoped_refptr<InitHelper> init_helper() const { return init_helper_; }

  void SetInitialized(std::unique_ptr<InitState> init_state) {
    CEF_REQUIRE_IOT();
    DCHECK(!init_state_);
    init_state_ = std::move(init_state);

    // Check that the CEF context or associated browser was not destroyed
    // between the calls to Initialize and SetInitialized, in which case
    // we won't get an OnDestroyed callback from DestructionObserver.
    if (init_state_->browser_) {
      if (!init_state_->browser_->browser_info()->browser()) {
        OnDestroyed();
        return;
      }
    } else if (!CONTEXT_STATE_VALID()) {
      OnDestroyed();
      return;
    }

    init_state_->initialized_ = true;
    init_state_->destruction_observer_->SetWrapper(
        weak_ptr_factory_.GetWeakPtr());

    // Continue any pending requests.
    if (!pending_requests_.empty()) {
      for (const auto& request : pending_requests_) {
        request->Run(this);
      }
      pending_requests_.clear();
    }
  }

  static void TryCreateURLLoaderNetworkObserver(
      std::unique_ptr<PendingRequest> pending_request,
      CefRefPtr<CefFrame> frame,
      const CefBrowserContext::Getter& browser_context_getter,
      base::WeakPtr<InterceptedRequestHandlerWrapper> self) {
    CEF_REQUIRE_UIT();

    mojo::PendingRemote<network::mojom::URLLoaderNetworkServiceObserver>
        url_loader_network_observer;

    if (frame) {
      // The request will be associated with this frame/browser if it's valid,
      // otherwise the request will be canceled.
      content::RenderFrameHostImpl* rfh =
          static_cast<content::RenderFrameHostImpl*>(
              static_cast<CefFrameHostImpl*>(frame.get())
                  ->GetRenderFrameHost());
      if (rfh) {
        if (rfh->frame_tree_node() &&
            rfh->frame_tree_node()->navigation_request()) {
          // Associate the Observer with the current NavigationRequest. This is
          // necessary for |is_main_frame_request| to report true (the expected
          // value) in AllowCertificateError.
          // TODO(cef): This approach for retrieving the NavigationRequest is
          // deprecated, see https://crbug.com/1179502#c36.
          url_loader_network_observer =
              rfh->GetStoragePartition()
                  ->CreateURLLoaderNetworkObserverForNavigationRequest(
                      *(rfh->frame_tree_node()->navigation_request()));
        } else {
          // Associate the Observer with the RenderFrameHost.
          url_loader_network_observer = rfh->CreateURLLoaderNetworkObserver();
        }
      }
    } else {
      auto cef_browser_context = browser_context_getter.Run();
      auto browser_context = cef_browser_context
                                 ? cef_browser_context->AsBrowserContext()
                                 : nullptr;
      if (browser_context) {
        url_loader_network_observer =
            static_cast<content::StoragePartitionImpl*>(
                browser_context->GetDefaultStoragePartition())
                ->CreateAuthCertObserverForServiceWorker(
                    content::ChildProcessHost::kInvalidUniqueID);
      }
    }

    CEF_POST_TASK(CEF_IOT,
                  base::BindOnce(&InterceptedRequestHandlerWrapper::
                                     ContinueCreateURLLoaderNetworkObserver,
                                 self, std::move(pending_request),
                                 std::move(url_loader_network_observer)));
  }

  void ContinueCreateURLLoaderNetworkObserver(
      std::unique_ptr<PendingRequest> pending_request,
      mojo::PendingRemote<network::mojom::URLLoaderNetworkServiceObserver>
          url_loader_network_observer) {
    CEF_REQUIRE_IOT();

    DCHECK(!init_state_->did_try_create_url_loader_network_observer_);
    init_state_->did_try_create_url_loader_network_observer_ = true;
    init_state_->url_loader_network_observer_ =
        std::move(url_loader_network_observer);
    pending_request->Run(this);
  }

  // InterceptedRequestHandler methods:
  void OnBeforeRequest(int32_t request_id,
                       network::ResourceRequest* request,
                       bool request_was_redirected,
                       OnBeforeRequestResultCallback callback,
                       CancelRequestCallback cancel_callback) override {
    CEF_REQUIRE_IOT();

    if (shutting_down_) {
      // Abort immediately.
      std::move(cancel_callback).Run(net::ERR_ABORTED);
      return;
    }

    if (!init_state_) {
      // Queue requests until we're initialized.
      pending_requests_.push_back(std::make_unique<PendingRequest>(
          request_id, request, request_was_redirected, std::move(callback),
          std::move(cancel_callback)));
      return;
    }

    if (request->trusted_params &&
        !request->trusted_params->url_loader_network_observer &&
        !init_state_->did_try_create_url_loader_network_observer_) {
      // Restarted/redirected requests won't already have an observer, so we
      // need to create one.
      CEF_POST_TASK(
          CEF_UIT,
          base::BindOnce(&InterceptedRequestHandlerWrapper::
                             TryCreateURLLoaderNetworkObserver,
                         std::make_unique<PendingRequest>(
                             request_id, request, request_was_redirected,
                             std::move(callback), std::move(cancel_callback)),
                         init_state_->frame_,
                         init_state_->browser_context_getter_,
                         weak_ptr_factory_.GetWeakPtr()));
      return;
    }

    // State may already exist for restarted requests.
    RequestState* state = GetOrCreateState(request_id);

    if (init_state_->did_try_create_url_loader_network_observer_) {
      if (init_state_->url_loader_network_observer_) {
        request->trusted_params->url_loader_network_observer =
            std::move(init_state_->url_loader_network_observer_);
      }

      // Reset state so that the observer will be recreated on the next
      // restart/redirect.
      init_state_->did_try_create_url_loader_network_observer_ = false;
    }

    // Add standard headers, if currently unspecified.
    if (!request->headers.HasHeader(net::HttpRequestHeaders::kAcceptLanguage)) {
      request->headers.SetHeaderIfMissing(
          net::HttpRequestHeaders::kAcceptLanguage,
          init_state_->accept_language_);
      state->accept_language_added_ = true;
    }
    request->headers.SetHeaderIfMissing(net::HttpRequestHeaders::kUserAgent,
                                        init_state_->user_agent_);

    const bool is_external = IsExternalRequest(request);

    // External requests will not have a default handler.
    bool intercept_only = is_external;

    CefRefPtr<CefRequestImpl> requestPtr;
    CefRefPtr<CefResourceRequestHandler> handler =
        GetHandler(request_id, request, &intercept_only, requestPtr);

    CefRefPtr<CefSchemeHandlerFactory> scheme_factory =
        init_state_->iothread_state_->GetSchemeHandlerFactory(request->url);
    if (scheme_factory && !requestPtr) {
      requestPtr = MakeRequest(request, request_id, true);
    }

    // True if there's a possibility that the client might handle the request.
    const bool maybe_intercept_request = handler || scheme_factory;
    if (!maybe_intercept_request && requestPtr) {
      requestPtr = nullptr;
    }

    // May have a handler and/or scheme factory.
    state->Reset(handler, scheme_factory, requestPtr, request_was_redirected,
                 std::move(cancel_callback));

    if (handler) {
      state->cookie_filter_ = handler->GetCookieAccessFilter(
          init_state_->browser_, init_state_->frame_, requestPtr.get());
    }

    auto exec_callback =
        base::BindOnce(std::move(callback), maybe_intercept_request,
                       is_external ? true : intercept_only);

    if (!maybe_intercept_request) {
      // Cookies will be handled by the NetworkService.
      std::move(exec_callback).Run();
      return;
    }

    MaybeLoadCookies(request_id, state, request, std::move(exec_callback));
  }

  void MaybeLoadCookies(int32_t request_id,
                        RequestState* state,
                        network::ResourceRequest* request,
                        base::OnceClosure callback) {
    CEF_REQUIRE_IOT();

    if (!cookie_helper::IsCookieableScheme(request->url,
                                           init_state_->cookieable_schemes_)) {
      // The scheme does not support cookies.
      std::move(callback).Run();
      return;
    }

    // We need to load/save cookies ourselves for custom-handled requests, or
    // if we're using a cookie filter.
    auto allow_cookie_callback =
        state->cookie_filter_
            ? base::BindRepeating(
                  &InterceptedRequestHandlerWrapper::AllowCookieLoad,
                  weak_ptr_factory_.GetWeakPtr(), request_id)
            : base::BindRepeating(
                  &InterceptedRequestHandlerWrapper::AllowCookieAlways);
    auto done_cookie_callback = base::BindOnce(
        &InterceptedRequestHandlerWrapper::ContinueWithLoadedCookies,
        weak_ptr_factory_.GetWeakPtr(), request_id, request,
        std::move(callback));
    cookie_helper::LoadCookies(init_state_->browser_context_getter_, *request,
                               allow_cookie_callback,
                               std::move(done_cookie_callback));
  }

  static void AllowCookieAlways(const net::CanonicalCookie& cookie,
                                bool* allow) {
    *allow = true;
  }

  void AllowCookieLoad(int32_t request_id,
                       const net::CanonicalCookie& cookie,
                       bool* allow) {
    CEF_REQUIRE_IOT();

    RequestState* state = GetState(request_id);
    if (!state) {
      // The request may have been canceled while the async callback was
      // pending.
      return;
    }

    DCHECK(state->cookie_filter_);

    CefCookie cef_cookie;
    if (net_service::MakeCefCookie(cookie, cef_cookie)) {
      *allow = state->cookie_filter_->CanSendCookie(
          init_state_->browser_, init_state_->frame_,
          state->pending_request_.get(), cef_cookie);
    }
  }

  void ContinueWithLoadedCookies(int32_t request_id,
                                 network::ResourceRequest* request,
                                 base::OnceClosure callback,
                                 int total_count,
                                 net::CookieList allowed_cookies) {
    CEF_REQUIRE_IOT();

    RequestState* state = GetState(request_id);
    if (!state) {
      // The request may have been canceled while the async callback was
      // pending.
      return;
    }

    if (state->cookie_filter_) {
      // Also add/save cookies ourselves for default-handled network requests
      // so that we can filter them. This will be a no-op for custom-handled
      // requests.
      request->load_flags |= kLoadNoCookiesFlags;
    }

    if (!allowed_cookies.empty()) {
      const std::string& cookie_line =
          net::CanonicalCookie::BuildCookieLine(allowed_cookies);
      request->headers.SetHeader(net::HttpRequestHeaders::kCookie, cookie_line);

      state->pending_request_->SetReadOnly(false);
      state->pending_request_->SetHeaderByName(net::HttpRequestHeaders::kCookie,
                                               cookie_line, true);
      state->pending_request_->SetReadOnly(true);
    }

    std::move(callback).Run();
  }

  void ShouldInterceptRequest(
      int32_t request_id,
      network::ResourceRequest* request,
      ShouldInterceptRequestResultCallback callback) override {
    CEF_REQUIRE_IOT();

    RequestState* state = GetState(request_id);
    if (!state) {
      // The request may have been canceled during destruction.
      return;
    }

    // Must have a handler and/or scheme factory.
    DCHECK(state->handler_ || state->scheme_factory_);
    DCHECK(state->pending_request_);

    if (state->handler_) {
      // The client may modify |pending_request_| before executing the callback.
      state->pending_request_->SetReadOnly(false);
      state->pending_request_->SetTrackChanges(true,
                                               true /* backup_on_change */);

      CefRefPtr<RequestCallbackWrapper> callbackPtr =
          new RequestCallbackWrapper(base::BindOnce(
              &InterceptedRequestHandlerWrapper::ContinueShouldInterceptRequest,
              weak_ptr_factory_.GetWeakPtr(), request_id,
              base::Unretained(request), std::move(callback)));

      cef_return_value_t retval = state->handler_->OnBeforeResourceLoad(
          init_state_->browser_, init_state_->frame_,
          state->pending_request_.get(), callbackPtr.get());
      if (retval != RV_CONTINUE_ASYNC) {
        if (retval == RV_CONTINUE) {
          // Continue the request immediately.
          callbackPtr->Continue();
        } else {
          // Cancel the request immediately.
          callbackPtr->Cancel();
        }
      }
    } else {
      // The scheme factory may choose to handle it.
      ContinueShouldInterceptRequest(request_id, request, std::move(callback),
                                     true);
    }
  }

  void ContinueShouldInterceptRequest(
      int32_t request_id,
      network::ResourceRequest* request,
      ShouldInterceptRequestResultCallback callback,
      bool allow) {
    CEF_REQUIRE_IOT();

    RequestState* state = GetState(request_id);
    if (!state) {
      // The request may have been canceled while the async callback was
      // pending.
      return;
    }

    // Must have a handler and/or scheme factory.
    DCHECK(state->handler_ || state->scheme_factory_);
    DCHECK(state->pending_request_);

    if (state->handler_) {
      if (allow) {
        // Apply any |requestPtr| changes to |request|.
        state->pending_request_->Get(request, true /* changed_only */);
      }

      const bool redirect =
          (state->pending_request_->GetChanges() & CefRequestImpl::kChangedUrl);
      if (redirect) {
        // Revert any changes for now. We'll get them back after the redirect.
        state->pending_request_->RevertChanges();
      }

      state->pending_request_->SetReadOnly(true);
      state->pending_request_->SetTrackChanges(false);

      if (!allow) {
        // Cancel the request.
        if (state->cancel_callback_) {
          std::move(state->cancel_callback_).Run(net::ERR_ABORTED);
        }
        return;
      }

      if (redirect) {
        // Performing a redirect.
        std::move(callback).Run(nullptr);
        return;
      }
    }

    CefRefPtr<CefResourceHandler> resource_handler;

    if (state->handler_) {
      // Does the client want to handle the request?
      resource_handler = state->handler_->GetResourceHandler(
          init_state_->browser_, init_state_->frame_,
          state->pending_request_.get());
    }
    if (!resource_handler && state->scheme_factory_) {
      // Does the scheme factory want to handle the request?
      resource_handler = state->scheme_factory_->Create(
          init_state_->browser_, init_state_->frame_, request->url.scheme(),
          state->pending_request_.get());
    }

    std::unique_ptr<ResourceResponse> resource_response;
    if (resource_handler) {
      resource_response = CreateResourceResponse(request_id, resource_handler);
      DCHECK(resource_response);
      state->was_custom_handled_ = true;
    } else if (state->accept_language_added_) {
      // The request will be handled by the NetworkService. Remove the
      // "Accept-Language" header here so that it can be re-added in
      // URLRequestHttpJob::AddExtraHeaders with correct ordering applied.
      request->headers.RemoveHeader(net::HttpRequestHeaders::kAcceptLanguage);
    }

    // Continue the request.
    std::move(callback).Run(std::move(resource_response));
  }

  void ProcessResponseHeaders(int32_t request_id,
                              const network::ResourceRequest& request,
                              const GURL& redirect_url,
                              net::HttpResponseHeaders* headers) override {
    CEF_REQUIRE_IOT();

    RequestState* state = GetState(request_id);
    if (!state) {
      // The request may have been canceled during destruction.
      return;
    }

    if (!state->handler_) {
      return;
    }

    if (!state->pending_response_) {
      state->pending_response_ = new CefResponseImpl();
    } else {
      state->pending_response_->SetReadOnly(false);
    }

    if (headers) {
      state->pending_response_->SetResponseHeaders(*headers);
    }

    state->pending_response_->SetReadOnly(true);
  }

  void OnRequestResponse(int32_t request_id,
                         network::ResourceRequest* request,
                         net::HttpResponseHeaders* headers,
                         absl::optional<net::RedirectInfo> redirect_info,
                         OnRequestResponseResultCallback callback) override {
    CEF_REQUIRE_IOT();

    RequestState* state = GetState(request_id);
    if (!state) {
      // The request may have been canceled during destruction.
      return;
    }

    if (state->cookie_filter_) {
      // Remove the flags that were added in ContinueWithLoadedCookies.
      request->load_flags &= ~kLoadNoCookiesFlags;
    }

    if (!state->handler_) {
      // Cookies may come from a scheme handler.
      MaybeSaveCookies(
          request_id, state, request, headers,
          base::BindOnce(
              std::move(callback), ResponseMode::CONTINUE, nullptr,
              redirect_info.has_value() ? redirect_info->new_url : GURL()));
      return;
    }

    DCHECK(state->pending_request_);
    DCHECK(state->pending_response_);

    if (redirect_info.has_value()) {
      HandleRedirect(request_id, state, request, headers, *redirect_info,
                     std::move(callback));
    } else {
      HandleResponse(request_id, state, request, headers, std::move(callback));
    }
  }

  void HandleRedirect(int32_t request_id,
                      RequestState* state,
                      network::ResourceRequest* request,
                      net::HttpResponseHeaders* headers,
                      const net::RedirectInfo& redirect_info,
                      OnRequestResponseResultCallback callback) {
    GURL new_url = redirect_info.new_url;
    CefString newUrl = redirect_info.new_url.spec();
    CefString oldUrl = newUrl;
    bool url_changed = false;
    state->handler_->OnResourceRedirect(
        init_state_->browser_, init_state_->frame_,
        state->pending_request_.get(), state->pending_response_.get(), newUrl);
    if (newUrl != oldUrl) {
      // Also support relative URLs.
      const GURL& url = redirect_info.new_url.Resolve(newUrl.ToString());
      if (url.is_valid()) {
        url_changed = true;
        new_url = url;
      }
    }

    // Update the |pending_request_| object with the new info.
    state->pending_request_->SetReadOnly(false);
    state->pending_request_->Set(redirect_info);
    if (url_changed) {
      state->pending_request_->SetURL(new_url.spec());
    }
    state->pending_request_->SetReadOnly(true);

    auto exec_callback = base::BindOnce(
        std::move(callback), ResponseMode::CONTINUE, nullptr, new_url);

    MaybeSaveCookies(request_id, state, request, headers,
                     std::move(exec_callback));
  }

  void HandleResponse(int32_t request_id,
                      RequestState* state,
                      network::ResourceRequest* request,
                      net::HttpResponseHeaders* headers,
                      OnRequestResponseResultCallback callback) {
    // The client may modify |pending_request_| in OnResourceResponse.
    state->pending_request_->SetReadOnly(false);
    state->pending_request_->SetTrackChanges(true, true /* backup_on_change */);

    auto response_mode = ResponseMode::CONTINUE;
    GURL new_url;

    if (state->handler_->OnResourceResponse(
            init_state_->browser_, init_state_->frame_,
            state->pending_request_.get(), state->pending_response_.get())) {
      // The request may have been modified.
      const auto changes = state->pending_request_->GetChanges();
      if (changes) {
        state->pending_request_->Get(request, true /* changed_only */);

        if (changes & CefRequestImpl::kChangedUrl) {
          // Redirect to the new URL.
          new_url = GURL(state->pending_request_->GetURL().ToString());
        } else {
          // Restart the request.
          response_mode = ResponseMode::RESTART;
        }
      }
    }

    // Revert any changes for now. We'll get them back after the redirect or
    // restart.
    state->pending_request_->RevertChanges();

    state->pending_request_->SetReadOnly(true);
    state->pending_request_->SetTrackChanges(false);

    auto exec_callback =
        base::BindOnce(std::move(callback), response_mode, nullptr, new_url);

    if (response_mode == ResponseMode::RESTART) {
      // Get any cookies after the restart.
      std::move(exec_callback).Run();
      return;
    }

    MaybeSaveCookies(request_id, state, request, headers,
                     std::move(exec_callback));
  }

  void MaybeSaveCookies(int32_t request_id,
                        RequestState* state,
                        network::ResourceRequest* request,
                        net::HttpResponseHeaders* headers,
                        base::OnceClosure callback) {
    CEF_REQUIRE_IOT();

    if (!state->cookie_filter_ && !state->was_custom_handled_) {
      // The NetworkService saves the cookies for default-handled requests.
      std::move(callback).Run();
      return;
    }

    if (!cookie_helper::IsCookieableScheme(request->url,
                                           init_state_->cookieable_schemes_)) {
      // The scheme does not support cookies.
      std::move(callback).Run();
      return;
    }

    // We need to load/save cookies ourselves for custom-handled requests, or
    // if we're using a cookie filter.
    auto allow_cookie_callback =
        state->cookie_filter_
            ? base::BindRepeating(
                  &InterceptedRequestHandlerWrapper::AllowCookieSave,
                  weak_ptr_factory_.GetWeakPtr(), request_id)
            : base::BindRepeating(
                  &InterceptedRequestHandlerWrapper::AllowCookieAlways);
    auto done_cookie_callback = base::BindOnce(
        &InterceptedRequestHandlerWrapper::ContinueWithSavedCookies,
        weak_ptr_factory_.GetWeakPtr(), request_id, std::move(callback));
    cookie_helper::SaveCookies(init_state_->browser_context_getter_, *request,
                               headers, allow_cookie_callback,
                               std::move(done_cookie_callback));
  }

  void AllowCookieSave(int32_t request_id,
                       const net::CanonicalCookie& cookie,
                       bool* allow) {
    CEF_REQUIRE_IOT();

    RequestState* state = GetState(request_id);
    if (!state) {
      // The request may have been canceled while the async callback was
      // pending.
      return;
    }

    DCHECK(state->cookie_filter_);

    CefCookie cef_cookie;
    if (net_service::MakeCefCookie(cookie, cef_cookie)) {
      *allow = state->cookie_filter_->CanSaveCookie(
          init_state_->browser_, init_state_->frame_,
          state->pending_request_.get(), state->pending_response_.get(),
          cef_cookie);
    }
  }

  void ContinueWithSavedCookies(int32_t request_id,
                                base::OnceClosure callback,
                                int total_count,
                                net::CookieList allowed_cookies) {
    CEF_REQUIRE_IOT();
    std::move(callback).Run();
  }

  mojo::ScopedDataPipeConsumerHandle OnFilterResponseBody(
      int32_t request_id,
      const network::ResourceRequest& request,
      mojo::ScopedDataPipeConsumerHandle body) override {
    CEF_REQUIRE_IOT();

    RequestState* state = GetState(request_id);
    if (!state) {
      // The request may have been canceled during destruction.
      return body;
    }

    if (state->handler_) {
      auto filter = state->handler_->GetResourceResponseFilter(
          init_state_->browser_, init_state_->frame_,
          state->pending_request_.get(), state->pending_response_.get());
      if (filter) {
        return CreateResponseFilterHandler(
            filter, std::move(body),
            base::BindOnce(&InterceptedRequestHandlerWrapper::OnFilterError,
                           weak_ptr_factory_.GetWeakPtr(), request_id));
      }
    }

    return body;
  }

  void OnFilterError(int32_t request_id) {
    CEF_REQUIRE_IOT();

    RequestState* state = GetState(request_id);
    if (!state) {
      // The request may have been canceled while the async callback was
      // pending.
      return;
    }

    if (state->cancel_callback_) {
      std::move(state->cancel_callback_).Run(net::ERR_CONTENT_DECODING_FAILED);
    }
  }

  void OnRequestComplete(
      int32_t request_id,
      const network::ResourceRequest& request,
      const network::URLLoaderCompletionStatus& status) override {
    CEF_REQUIRE_IOT();

    RequestState* state = GetState(request_id);
    if (!state) {
      // The request may have been aborted during initialization or canceled
      // during destruction. This method will always be called before a request
      // is deleted, so if the request is currently pending also remove it from
      // the list.
      if (!pending_requests_.empty()) {
        PendingRequests::iterator it = pending_requests_.begin();
        for (; it != pending_requests_.end(); ++it) {
          if ((*it)->id_ == request_id) {
            pending_requests_.erase(it);
            break;
          }
        }
      }
      return;
    }

    const bool is_external = IsExternalRequest(&request);

    // Redirection of standard custom schemes is handled with a restart, so we
    // get completion notifications for both the original (redirected) request
    // and the final request. Don't report completion of the redirected request.
    const bool ignore_result = is_external && request.url.IsStandard() &&
                               status.error_code == net::ERR_ABORTED &&
                               state->pending_response_.get() &&
                               net::HttpResponseHeaders::IsRedirectResponseCode(
                                   state->pending_response_->GetStatus());

    if (state->handler_ && !ignore_result) {
      DCHECK(state->pending_request_);

      CallHandlerOnComplete(state, status);

      if (status.error_code != 0 && status.error_code != ERR_ABORTED &&
          is_external) {
        bool allow_os_execution = false;
        state->handler_->OnProtocolExecution(
            init_state_->browser_, init_state_->frame_,
            state->pending_request_.get(), allow_os_execution);
        if (allow_os_execution && init_state_->unhandled_request_callback_) {
          init_state_->unhandled_request_callback_.Run();
        }
      }
    }

    RemoveState(request_id);
  }

 private:
  void CallHandlerOnComplete(RequestState* state,
                             const network::URLLoaderCompletionStatus& status) {
    if (!state->handler_ || !state->pending_request_) {
      return;
    }

    // The request object may be currently flagged as writable in cases where we
    // abort a request that is waiting on a pending callack.
    if (!state->pending_request_->IsReadOnly()) {
      state->pending_request_->SetReadOnly(true);
    }

    if (!state->pending_response_) {
      // If the request failed there may not be a response object yet.
      state->pending_response_ = new CefResponseImpl();
    } else {
      state->pending_response_->SetReadOnly(false);
    }
    state->pending_response_->SetError(
        static_cast<cef_errorcode_t>(status.error_code));
    state->pending_response_->SetReadOnly(true);

    state->handler_->OnResourceLoadComplete(
        init_state_->browser_, init_state_->frame_,
        state->pending_request_.get(), state->pending_response_.get(),
        status.error_code == 0 ? UR_SUCCESS : UR_FAILED,
        status.encoded_body_length);
  }

  // Returns the handler, if any, that should be used for this request.
  CefRefPtr<CefResourceRequestHandler> GetHandler(
      int32_t request_id,
      network::ResourceRequest* request,
      bool* intercept_only,
      CefRefPtr<CefRequestImpl>& requestPtr) const {
    CefRefPtr<CefResourceRequestHandler> handler;

    if (init_state_->browser_) {
      // Maybe the browser's client wants to handle it?
      CefRefPtr<CefClient> client =
          init_state_->browser_->GetHost()->GetClient();
      if (client) {
        CefRefPtr<CefRequestHandler> request_handler =
            client->GetRequestHandler();
        if (request_handler) {
          requestPtr = MakeRequest(request, request_id, true);

          handler = request_handler->GetResourceRequestHandler(
              init_state_->browser_, init_state_->frame_, requestPtr.get(),
              init_state_->is_navigation_, init_state_->is_download_,
              init_state_->request_initiator_, *intercept_only);
        }
      }
    }

    if (!handler) {
      // Maybe the request context wants to handle it?
      CefRefPtr<CefRequestContextHandler> context_handler =
          init_state_->iothread_state_->GetHandler(
              init_state_->global_id_, /*require_frame_match=*/false);
      if (context_handler) {
        if (!requestPtr) {
          requestPtr = MakeRequest(request, request_id, true);
        }

        handler = context_handler->GetResourceRequestHandler(
            init_state_->browser_, init_state_->frame_, requestPtr.get(),
            init_state_->is_navigation_, init_state_->is_download_,
            init_state_->request_initiator_, *intercept_only);
      }
    }

    return handler;
  }

  RequestState* GetOrCreateState(int32_t request_id) {
    RequestState* state = GetState(request_id);
    if (!state) {
      state = new RequestState();
      request_map_.insert(std::make_pair(request_id, base::WrapUnique(state)));
    }
    return state;
  }

  RequestState* GetState(int32_t request_id) const {
    RequestMap::const_iterator it = request_map_.find(request_id);
    if (it != request_map_.end()) {
      return it->second.get();
    }
    return nullptr;
  }

  void RemoveState(int32_t request_id) {
    RequestMap::iterator it = request_map_.find(request_id);
    DCHECK(it != request_map_.end());
    if (it != request_map_.end()) {
      request_map_.erase(it);
    }
  }

  // Stop accepting new requests and cancel pending/in-flight requests when the
  // CEF context or associated browser is destroyed.
  void OnDestroyed() {
    CEF_REQUIRE_IOT();
    DCHECK(init_state_);

    init_state_->DeleteDestructionObserver();

    // Stop accepting new requests.
    shutting_down_ = true;

    // Stop the delivery of pending callbacks.
    weak_ptr_factory_.InvalidateWeakPtrs();

    // Take ownership of any pending requests.
    PendingRequests pending_requests;
    pending_requests.swap(pending_requests_);

    // Take ownership of any in-progress requests.
    RequestMap request_map;
    request_map.swap(request_map_);

    // Notify handlers for in-progress requests.
    for (const auto& pair : request_map) {
      CallHandlerOnComplete(
          pair.second.get(),
          network::URLLoaderCompletionStatus(net::ERR_ABORTED));
    }

    if (init_state_->browser_) {
      // Clear objects that reference the browser.
      init_state_->browser_ = nullptr;
      init_state_->frame_ = nullptr;
    }

    // Execute cancel callbacks and delete pending and in-progress requests.
    // This may result in the request being torn down sooner, or it may be
    // ignored if the request is already in the process of being torn down. When
    // the last callback is executed it may result in |this| being deleted.
    pending_requests.clear();

    for (auto& pair : request_map) {
      auto state = std::move(pair.second);
      if (state->cancel_callback_) {
        std::move(state->cancel_callback_).Run(net::ERR_ABORTED);
      }
    }
  }

  static CefRefPtr<CefRequestImpl> MakeRequest(
      const network::ResourceRequest* request,
      int64_t request_id,
      bool read_only) {
    CefRefPtr<CefRequestImpl> requestPtr = new CefRequestImpl();
    requestPtr->Set(request, request_id);
    if (read_only) {
      requestPtr->SetReadOnly(true);
    } else {
      requestPtr->SetTrackChanges(true);
    }
    return requestPtr;
  }

  // Returns true if |request| cannot be handled internally.
  static bool IsExternalRequest(const network::ResourceRequest* request) {
    return !scheme::IsInternalHandledScheme(request->url.scheme());
  }

  scoped_refptr<InitHelper> init_helper_;
  std::unique_ptr<InitState> init_state_;

  bool shutting_down_ = false;

  using RequestMap = std::map<int32_t, std::unique_ptr<RequestState>>;
  RequestMap request_map_;

  using PendingRequests = std::vector<std::unique_ptr<PendingRequest>>;
  PendingRequests pending_requests_;

  base::WeakPtrFactory<InterceptedRequestHandlerWrapper> weak_ptr_factory_;
};

}  // namespace

std::unique_ptr<InterceptedRequestHandler> CreateInterceptedRequestHandler(
    content::BrowserContext* browser_context,
    content::RenderFrameHost* frame,
    int render_process_id,
    bool is_navigation,
    bool is_download,
    const url::Origin& request_initiator) {
  CEF_REQUIRE_UIT();
  CHECK(browser_context);

  CefRefPtr<CefBrowserHostBase> browserPtr;
  CefRefPtr<CefFrame> framePtr;

  // Default to handlers for the same process in case |frame| doesn't have an
  // associated CefBrowserHost.
  content::GlobalRenderFrameHostId global_id(render_process_id,
                                             MSG_ROUTING_NONE);

  // |frame| may be nullptr for service worker requests.
  if (frame) {
    // May return nullptr for requests originating from guest views.
    browserPtr = CefBrowserHostBase::GetBrowserForHost(frame);
    if (browserPtr) {
      framePtr = browserPtr->GetFrameForHost(frame);
      CHECK(framePtr);
      global_id = frame->GetGlobalId();
    }
  }

  auto init_state =
      std::make_unique<InterceptedRequestHandlerWrapper::InitState>();
  init_state->Initialize(browser_context, browserPtr, framePtr, global_id,
                         is_navigation, is_download, request_initiator,
                         base::RepeatingClosure());

  auto wrapper = std::make_unique<InterceptedRequestHandlerWrapper>();
  wrapper->init_helper()->MaybeSetInitialized(std::move(init_state));

  return wrapper;
}

std::unique_ptr<InterceptedRequestHandler> CreateInterceptedRequestHandler(
    content::WebContents::Getter web_contents_getter,
    int frame_tree_node_id,
    const network::ResourceRequest& request,
    const base::RepeatingClosure& unhandled_request_callback) {
  CEF_REQUIRE_UIT();

  content::WebContents* web_contents = web_contents_getter.Run();
  CHECK(web_contents);

  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  CHECK(browser_context);

  content::RenderFrameHost* frame = nullptr;

  if (request.is_outermost_main_frame ||
      static_cast<blink::mojom::ResourceType>(request.resource_type) ==
          blink::mojom::ResourceType::kMainFrame) {
    frame = web_contents->GetPrimaryMainFrame();
    CHECK(frame);
  } else {
    // May return nullptr for frames in inner WebContents.
    auto node = content::FrameTreeNode::GloballyFindByID(frame_tree_node_id);
    if (node) {
      frame = node->current_frame_host();

      // RFHs can move between FrameTreeNodes. Make sure this one hasn't. See
      // documentation on RenderFrameHost::GetFrameTreeNodeId() for background.
      if (content::WebContents::FromRenderFrameHost(frame) != web_contents) {
        frame = nullptr;
      }
    }

    if (!frame) {
      // Use the main frame for the CefBrowserHost.
      frame = web_contents->GetPrimaryMainFrame();
      CHECK(frame);
    }
  }

  CefRefPtr<CefBrowserHostBase> browserPtr;
  CefRefPtr<CefFrame> framePtr;

  // Default to handlers for the same process in case |frame| doesn't have an
  // associated CefBrowserHost.
  content::GlobalRenderFrameHostId global_id(frame->GetProcess()->GetID(),
                                             MSG_ROUTING_NONE);

  // May return nullptr for requests originating from guest views.
  browserPtr = CefBrowserHostBase::GetBrowserForHost(frame);
  if (browserPtr) {
    framePtr = browserPtr->GetFrameForHost(frame);
    DCHECK(framePtr);
    global_id = frame->GetGlobalId();
  }

  const bool is_navigation = ui::PageTransitionIsNewNavigation(
      static_cast<ui::PageTransition>(request.transition_type));
  // TODO(navigation): Can we determine the |is_download| value?
  const bool is_download = false;
  url::Origin request_initiator;
  if (request.request_initiator.has_value()) {
    request_initiator = *request.request_initiator;
  }

  auto init_state =
      std::make_unique<InterceptedRequestHandlerWrapper::InitState>();
  init_state->Initialize(browser_context, browserPtr, framePtr, global_id,
                         is_navigation, is_download, request_initiator,
                         unhandled_request_callback);

  auto wrapper = std::make_unique<InterceptedRequestHandlerWrapper>();
  wrapper->init_helper()->MaybeSetInitialized(std::move(init_state));

  return wrapper;
}

}  // namespace net_service
