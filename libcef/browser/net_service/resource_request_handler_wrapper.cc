// Copyright (c) 2019 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/browser/net_service/resource_request_handler_wrapper.h"

#include "libcef/browser/browser_host_impl.h"
#include "libcef/browser/browser_platform_delegate.h"
#include "libcef/browser/content_browser_client.h"
#include "libcef/browser/context.h"
#include "libcef/browser/net_service/cookie_helper.h"
#include "libcef/browser/net_service/proxy_url_loader_factory.h"
#include "libcef/browser/net_service/resource_handler_wrapper.h"
#include "libcef/browser/net_service/response_filter_wrapper.h"
#include "libcef/browser/resource_context.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/content_client.h"
#include "libcef/common/net/scheme_registration.h"
#include "libcef/common/net_service/net_service_util.h"
#include "libcef/common/request_impl.h"
#include "libcef/common/response_impl.h"

#include "chrome/common/chrome_features.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_status_code.h"
#include "ui/base/page_transition_types.h"
#include "url/origin.h"

namespace net_service {

namespace {

const int kLoadNoCookiesFlags =
    net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES;

class RequestCallbackWrapper : public CefRequestCallback {
 public:
  using Callback = base::OnceCallback<void(bool /* allow */)>;
  explicit RequestCallbackWrapper(Callback callback)
      : callback_(std::move(callback)),
        work_thread_task_runner_(base::SequencedTaskRunnerHandle::Get()) {}

  ~RequestCallbackWrapper() override {
    if (!callback_.is_null()) {
      // Make sure it executes on the correct thread.
      work_thread_task_runner_->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback_), true));
    }
  }

  void Continue(bool allow) override {
    if (!work_thread_task_runner_->RunsTasksInCurrentSequence()) {
      work_thread_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&RequestCallbackWrapper::Continue, this, allow));
      return;
    }
    if (!callback_.is_null()) {
      std::move(callback_).Run(allow);
    }
  }

  void Cancel() override { Continue(false); }

 private:
  Callback callback_;

  scoped_refptr<base::SequencedTaskRunner> work_thread_task_runner_;

  IMPLEMENT_REFCOUNTING(RequestCallbackWrapper);
  DISALLOW_COPY_AND_ASSIGN(RequestCallbackWrapper);
};

std::string GetAcceptLanguageList(content::BrowserContext* browser_context,
                                  CefRefPtr<CefBrowserHostImpl> browser) {
  if (browser) {
    const CefBrowserSettings& browser_settings = browser->settings();
    if (browser_settings.accept_language_list.length > 0) {
      return CefString(&browser_settings.accept_language_list);
    }
  }

  // This defaults to the value from CefRequestContextSettings via
  // browser_prefs::CreatePrefService().
  auto prefs = Profile::FromBrowserContext(browser_context)->GetPrefs();
  return prefs->GetString(language::prefs::kAcceptLanguages);
}

// Match the logic in chrome/browser/net/profile_network_context_service.cc.
std::string ComputeAcceptLanguageFromPref(const std::string& language_pref) {
  std::string accept_languages_str =
      base::FeatureList::IsEnabled(features::kUseNewAcceptLanguageHeader)
          ? net::HttpUtil::ExpandLanguageList(language_pref)
          : language_pref;
  return net::HttpUtil::GenerateAcceptLanguageHeader(accept_languages_str);
}

class InterceptedRequestHandlerWrapper : public InterceptedRequestHandler {
 public:
  struct RequestState {
    RequestState() {}

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
    CancelRequestCallback cancel_callback_;
  };

  struct PendingRequest {
    PendingRequest(const RequestId& id,
                   network::ResourceRequest* request,
                   bool request_was_redirected,
                   OnBeforeRequestResultCallback callback,
                   CancelRequestCallback cancel_callback)
        : id_(id),
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

    const RequestId id_;
    network::ResourceRequest* const request_;
    const bool request_was_redirected_;
    OnBeforeRequestResultCallback callback_;
    CancelRequestCallback cancel_callback_;
  };

  // Observer to receive notification of CEF context or associated browser
  // destruction. Only one of the *Destroyed() methods will be called.
  class DestructionObserver : public CefBrowserHostImpl::Observer,
                              public CefContext::Observer {
   public:
    DestructionObserver() = default;

    void SetWrapper(base::WeakPtr<InterceptedRequestHandlerWrapper> wrapper) {
      CEF_REQUIRE_IOT();
      wrapper_ = wrapper;
    }

    void OnBrowserDestroyed(CefBrowserHostImpl* browser) override {
      CEF_REQUIRE_UIT();
      browser->RemoveObserver(this);
      NotifyOnDestroyed();
    }

    void OnContextDestroyed() override {
      CEF_REQUIRE_UIT();
      CefContext::Get()->RemoveObserver(this);
      NotifyOnDestroyed();
    }

   private:
    void NotifyOnDestroyed() {
      // It's not safe to test the WeakPtr on the UI thread, so we'll just post
      // a task which will be ignored if the WeakPtr is invalid.
      CEF_POST_TASK(CEF_IOT, base::BindOnce(
                                 &InterceptedRequestHandlerWrapper::OnDestroyed,
                                 wrapper_));
    }

    base::WeakPtr<InterceptedRequestHandlerWrapper> wrapper_;
    DISALLOW_COPY_AND_ASSIGN(DestructionObserver);
  };

  InterceptedRequestHandlerWrapper() : weak_ptr_factory_(this) {}

  ~InterceptedRequestHandlerWrapper() override {
    // There should be no pending or in-progress requests during destruction.
    DCHECK(pending_requests_.empty());
    DCHECK(request_map_.empty());

    if (destruction_observer_) {
      destruction_observer_->SetWrapper(nullptr);
      CEF_POST_TASK(
          CEF_UIT, base::BindOnce(&InterceptedRequestHandlerWrapper::
                                      RemoveDestructionObserverOnUIThread,
                                  browser_ ? browser_->browser_info() : nullptr,
                                  std::move(destruction_observer_)));
    }
  }

  static void RemoveDestructionObserverOnUIThread(
      scoped_refptr<CefBrowserInfo> browser_info,
      std::unique_ptr<DestructionObserver> observer) {
    // Verify that the browser or context is still valid before attempting to
    // remove the observer.
    if (browser_info) {
      auto browser = browser_info->browser();
      if (browser)
        browser->RemoveObserver(observer.get());
    } else if (CONTEXT_STATE_VALID()) {
      CefContext::Get()->RemoveObserver(observer.get());
    }
  }

  void Initialize(content::BrowserContext* browser_context,
                  CefRefPtr<CefBrowserHostImpl> browser,
                  CefRefPtr<CefFrame> frame,
                  int render_process_id,
                  int render_frame_id,
                  int frame_tree_node_id,
                  bool is_navigation,
                  bool is_download,
                  const url::Origin& request_initiator,
                  bool is_external) {
    CEF_REQUIRE_UIT();

    browser_context_ = browser_context;
    resource_context_ =
        static_cast<CefResourceContext*>(browser_context->GetResourceContext());
    DCHECK(resource_context_);

    // We register to be notified of CEF context or browser destruction so that
    // we can stop accepting new requests and cancel pending/in-progress
    // requests in a timely manner (e.g. before we start asserting about leaked
    // objects during CEF shutdown).
    destruction_observer_.reset(new DestructionObserver());

    if (browser) {
      // These references will be released in OnDestroyed().
      browser_ = browser;
      frame_ = frame;
      browser->AddObserver(destruction_observer_.get());
    } else {
      CefContext::Get()->AddObserver(destruction_observer_.get());
    }

    render_process_id_ = render_process_id;
    render_frame_id_ = render_frame_id;
    frame_tree_node_id_ = frame_tree_node_id;
    is_navigation_ = is_navigation;
    is_download_ = is_download;
    request_initiator_ = request_initiator.Serialize();
    is_external_ = is_external;

    // Default values for standard headers.
    accept_language_ = ComputeAcceptLanguageFromPref(
        GetAcceptLanguageList(browser_context, browser));
    DCHECK(!accept_language_.empty());
    user_agent_ = CefContentClient::Get()->browser()->GetUserAgent();
    DCHECK(!user_agent_.empty());

    CEF_POST_TASK(
        CEF_IOT,
        base::BindOnce(&InterceptedRequestHandlerWrapper::SetInitialized,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  void SetInitialized() {
    CEF_REQUIRE_IOT();
    initialized_ = true;

    // Check that the CEF context or associated browser was not destroyed
    // between the calls to Initialize and SetInitialized, in which case
    // we won't get an OnDestroyed callback from DestructionObserver.
    if (browser_) {
      if (!browser_->browser_info()->browser()) {
        OnDestroyed();
        return;
      }
    } else if (!CONTEXT_STATE_VALID()) {
      OnDestroyed();
      return;
    }

    destruction_observer_->SetWrapper(weak_ptr_factory_.GetWeakPtr());

    // Continue any pending requests.
    if (!pending_requests_.empty()) {
      for (const auto& request : pending_requests_)
        request->Run(this);
      pending_requests_.clear();
    }
  }

  // InterceptedRequestHandler methods:
  void OnBeforeRequest(const RequestId& id,
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

    if (!initialized_) {
      // Queue requests until we're initialized.
      pending_requests_.push_back(std::make_unique<PendingRequest>(
          id, request, request_was_redirected, std::move(callback),
          std::move(cancel_callback)));
      return;
    }

    // State may already exist for restarted requests.
    RequestState* state = GetOrCreateState(id);

    // Add standard headers, if currently unspecified.
    request->headers.SetHeaderIfMissing(
        net::HttpRequestHeaders::kAcceptLanguage, accept_language_);
    request->headers.SetHeaderIfMissing(net::HttpRequestHeaders::kUserAgent,
                                        user_agent_);

    // External requests will not have a default handler.
    bool intercept_only = is_external_;

    CefRefPtr<CefRequestImpl> requestPtr;
    CefRefPtr<CefResourceRequestHandler> handler =
        GetHandler(id, request, &intercept_only, requestPtr);

    CefRefPtr<CefSchemeHandlerFactory> scheme_factory =
        resource_context_->GetSchemeHandlerFactory(request->url);
    if (scheme_factory && !requestPtr) {
      requestPtr = MakeRequest(request, id.hash(), true);
    }

    // True if there's a possibility that the client might handle the request.
    const bool maybe_intercept_request = handler || scheme_factory;
    if (!maybe_intercept_request && requestPtr)
      requestPtr = nullptr;

    // May have a handler and/or scheme factory.
    state->Reset(handler, scheme_factory, requestPtr, request_was_redirected,
                 std::move(cancel_callback));

    if (handler) {
      state->cookie_filter_ =
          handler->GetCookieAccessFilter(browser_, frame_, requestPtr.get());
    }

    auto exec_callback =
        base::BindOnce(std::move(callback), maybe_intercept_request,
                       is_external_ ? true : intercept_only);

    if (!maybe_intercept_request) {
      // Cookies will be handled by the NetworkService.
      std::move(exec_callback).Run();
      return;
    }

    MaybeLoadCookies(id, state, request, std::move(exec_callback));
  }

  void MaybeLoadCookies(const RequestId& id,
                        RequestState* state,
                        network::ResourceRequest* request,
                        base::OnceClosure callback) {
    CEF_REQUIRE_IOT();

    // We need to load/save cookies ourselves for custom-handled requests, or
    // if we're using a cookie filter.
    auto allow_cookie_callback =
        state->cookie_filter_
            ? base::BindRepeating(
                  &InterceptedRequestHandlerWrapper::AllowCookieLoad,
                  weak_ptr_factory_.GetWeakPtr(), id)
            : base::BindRepeating(
                  &InterceptedRequestHandlerWrapper::AllowCookieAlways);
    auto done_cookie_callback = base::BindOnce(
        &InterceptedRequestHandlerWrapper::ContinueWithLoadedCookies,
        weak_ptr_factory_.GetWeakPtr(), id, request, std::move(callback));
    net_service::LoadCookies(browser_context_, *request, allow_cookie_callback,
                             std::move(done_cookie_callback));
  }

  static void AllowCookieAlways(const net::CanonicalCookie& cookie,
                                bool* allow) {
    *allow = true;
  }

  void AllowCookieLoad(const RequestId& id,
                       const net::CanonicalCookie& cookie,
                       bool* allow) {
    CEF_REQUIRE_IOT();

    RequestState* state = GetState(id);
    if (!state) {
      // The request may have been canceled while the async callback was
      // pending.
      return;
    }

    DCHECK(state->cookie_filter_);

    CefCookie cef_cookie;
    if (net_service::MakeCefCookie(cookie, cef_cookie)) {
      *allow = state->cookie_filter_->CanSendCookie(
          browser_, frame_, state->pending_request_.get(), cef_cookie);
    }
  }

  void ContinueWithLoadedCookies(const RequestId& id,
                                 network::ResourceRequest* request,
                                 base::OnceClosure callback,
                                 int total_count,
                                 net::CookieList allowed_cookies) {
    CEF_REQUIRE_IOT();

    RequestState* state = GetState(id);
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
      const RequestId& id,
      network::ResourceRequest* request,
      ShouldInterceptRequestResultCallback callback) override {
    CEF_REQUIRE_IOT();

    RequestState* state = GetState(id);
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
              weak_ptr_factory_.GetWeakPtr(), id, base::Unretained(request),
              base::Passed(std::move(callback))));

      cef_return_value_t retval = state->handler_->OnBeforeResourceLoad(
          browser_, frame_, state->pending_request_.get(), callbackPtr.get());
      if (retval != RV_CONTINUE_ASYNC) {
        // Continue or cancel the request immediately.
        callbackPtr->Continue(retval == RV_CONTINUE);
      }
    } else {
      // The scheme factory may choose to handle it.
      ContinueShouldInterceptRequest(id, request, std::move(callback), true);
    }
  }

  void ContinueShouldInterceptRequest(
      const RequestId& id,
      network::ResourceRequest* request,
      ShouldInterceptRequestResultCallback callback,
      bool allow) {
    CEF_REQUIRE_IOT();

    RequestState* state = GetState(id);
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
          browser_, frame_, state->pending_request_.get());
    }
    if (!resource_handler && state->scheme_factory_) {
      // Does the scheme factory want to handle the request?
      resource_handler = state->scheme_factory_->Create(
          browser_, frame_, request->url.scheme(),
          state->pending_request_.get());
    }

    std::unique_ptr<ResourceResponse> resource_response;
    if (resource_handler) {
      resource_response = CreateResourceResponse(id, resource_handler);
      DCHECK(resource_response);
      state->was_custom_handled_ = true;
    }

    // Continue the request.
    std::move(callback).Run(std::move(resource_response));
  }

  void ProcessResponseHeaders(
      const RequestId& id,
      const network::ResourceRequest& request,
      const GURL& redirect_url,
      const network::ResourceResponseHead& head) override {
    CEF_REQUIRE_IOT();

    RequestState* state = GetState(id);
    if (!state) {
      // The request may have been canceled during destruction.
      return;
    }

    if (!state->handler_)
      return;

    if (!state->pending_response_)
      state->pending_response_ = new CefResponseImpl();
    else
      state->pending_response_->SetReadOnly(false);

    if (head.headers)
      state->pending_response_->SetResponseHeaders(*head.headers);

    state->pending_response_->SetReadOnly(true);
  }

  void OnRequestResponse(const RequestId& id,
                         network::ResourceRequest* request,
                         const network::ResourceResponseHead& head,
                         base::Optional<net::RedirectInfo> redirect_info,
                         OnRequestResponseResultCallback callback) override {
    CEF_REQUIRE_IOT();

    RequestState* state = GetState(id);
    if (!state) {
      // The request may have been canceled during destruction.
      return;
    }

    if (!state->handler_) {
      // Cookies may come from a scheme handler.
      MaybeSaveCookies(
          id, state, request, head,
          base::BindOnce(
              std::move(callback), ResponseMode::CONTINUE, nullptr,
              redirect_info.has_value() ? redirect_info->new_url : GURL()));
      return;
    }

    DCHECK(state->pending_request_);
    DCHECK(state->pending_response_);

    if (redirect_info.has_value()) {
      HandleRedirect(id, state, request, head, *redirect_info,
                     std::move(callback));
    } else {
      HandleResponse(id, state, request, head, std::move(callback));
    }
  }

  void HandleRedirect(const RequestId& id,
                      RequestState* state,
                      network::ResourceRequest* request,
                      const network::ResourceResponseHead& head,
                      const net::RedirectInfo& redirect_info,
                      OnRequestResponseResultCallback callback) {
    GURL new_url = redirect_info.new_url;
    CefString newUrl = redirect_info.new_url.spec();
    CefString oldUrl = newUrl;
    bool url_changed = false;
    state->handler_->OnResourceRedirect(browser_, frame_,
                                        state->pending_request_.get(),
                                        state->pending_response_.get(), newUrl);
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

    MaybeSaveCookies(id, state, request, head, std::move(exec_callback));
  }

  void HandleResponse(const RequestId& id,
                      RequestState* state,
                      network::ResourceRequest* request,
                      const network::ResourceResponseHead& head,
                      OnRequestResponseResultCallback callback) {
    // The client may modify |pending_request_| in OnResourceResponse.
    state->pending_request_->SetReadOnly(false);
    state->pending_request_->SetTrackChanges(true, true /* backup_on_change */);

    auto response_mode = ResponseMode::CONTINUE;
    GURL new_url;

    if (state->handler_->OnResourceResponse(browser_, frame_,
                                            state->pending_request_.get(),
                                            state->pending_response_.get())) {
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
      if (state->cookie_filter_) {
        // Remove the flags that were added in ContinueWithLoadedCookies.
        request->load_flags &= ~kLoadNoCookiesFlags;
      }

      // Get any cookies after the restart.
      std::move(exec_callback).Run();
      return;
    }

    MaybeSaveCookies(id, state, request, head, std::move(exec_callback));
  }

  void MaybeSaveCookies(const RequestId& id,
                        RequestState* state,
                        network::ResourceRequest* request,
                        const network::ResourceResponseHead& head,
                        base::OnceClosure callback) {
    CEF_REQUIRE_IOT();

    if (!state->cookie_filter_ && !state->was_custom_handled_) {
      // The NetworkService saves the cookies for default-handled requests.
      std::move(callback).Run();
      return;
    }

    // We need to load/save cookies ourselves for custom-handled requests, or
    // if we're using a cookie filter.
    auto allow_cookie_callback =
        state->cookie_filter_
            ? base::BindRepeating(
                  &InterceptedRequestHandlerWrapper::AllowCookieSave,
                  weak_ptr_factory_.GetWeakPtr(), id)
            : base::BindRepeating(
                  &InterceptedRequestHandlerWrapper::AllowCookieAlways);
    auto done_cookie_callback = base::BindOnce(
        &InterceptedRequestHandlerWrapper::ContinueWithSavedCookies,
        weak_ptr_factory_.GetWeakPtr(), id, std::move(callback));
    net_service::SaveCookies(browser_context_, *request, head,
                             allow_cookie_callback,
                             std::move(done_cookie_callback));
  }

  void AllowCookieSave(const RequestId& id,
                       const net::CanonicalCookie& cookie,
                       bool* allow) {
    CEF_REQUIRE_IOT();

    RequestState* state = GetState(id);
    if (!state) {
      // The request may have been canceled while the async callback was
      // pending.
      return;
    }

    DCHECK(state->cookie_filter_);

    CefCookie cef_cookie;
    if (net_service::MakeCefCookie(cookie, cef_cookie)) {
      *allow = state->cookie_filter_->CanSaveCookie(
          browser_, frame_, state->pending_request_.get(),
          state->pending_response_.get(), cef_cookie);
    }
  }

  void ContinueWithSavedCookies(const RequestId& id,
                                base::OnceClosure callback,
                                int total_count,
                                net::CookieList allowed_cookies) {
    CEF_REQUIRE_IOT();
    std::move(callback).Run();
  }

  mojo::ScopedDataPipeConsumerHandle OnFilterResponseBody(
      const RequestId& id,
      const network::ResourceRequest& request,
      mojo::ScopedDataPipeConsumerHandle body) override {
    CEF_REQUIRE_IOT();

    RequestState* state = GetState(id);
    if (!state) {
      // The request may have been canceled during destruction.
      return body;
    }

    if (state->handler_) {
      auto filter = state->handler_->GetResourceResponseFilter(
          browser_, frame_, state->pending_request_.get(),
          state->pending_response_.get());
      if (filter) {
        return CreateResponseFilterHandler(
            filter, std::move(body),
            base::BindOnce(&InterceptedRequestHandlerWrapper::OnFilterError,
                           weak_ptr_factory_.GetWeakPtr(), id));
      }
    }

    return body;
  }

  void OnFilterError(const RequestId& id) {
    CEF_REQUIRE_IOT();

    RequestState* state = GetState(id);
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
      const RequestId& id,
      const network::ResourceRequest& request,
      const network::URLLoaderCompletionStatus& status) override {
    CEF_REQUIRE_IOT();

    RequestState* state = GetState(id);
    if (!state) {
      // The request may have been canceled during destruction.
      return;
    }

    // Redirection of standard custom schemes is handled with a restart, so we
    // get completion notifications for both the original (redirected) request
    // and the final request. Don't report completion of the redirected request.
    const bool ignore_result = is_external_ && request.url.IsStandard() &&
                               status.error_code == net::ERR_ABORTED &&
                               state->pending_response_.get() &&
                               net::HttpResponseHeaders::IsRedirectResponseCode(
                                   state->pending_response_->GetStatus());

    if (state->handler_ && !ignore_result) {
      DCHECK(state->pending_request_);

      CallHandlerOnComplete(state, status);

      if (status.error_code != 0 && is_external_) {
        bool allow_os_execution = false;
        state->handler_->OnProtocolExecution(browser_, frame_,
                                             state->pending_request_.get(),
                                             allow_os_execution);
        if (allow_os_execution) {
          CefBrowserPlatformDelegate::HandleExternalProtocol(request.url);
        }
      }
    }

    RemoveState(id);
  }

 private:
  void CallHandlerOnComplete(RequestState* state,
                             const network::URLLoaderCompletionStatus& status) {
    if (!state->handler_ || !state->pending_request_)
      return;

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
        browser_, frame_, state->pending_request_.get(),
        state->pending_response_.get(),
        status.error_code == 0 ? UR_SUCCESS : UR_FAILED,
        status.encoded_body_length);
  }

  // Returns the handler, if any, that should be used for this request.
  CefRefPtr<CefResourceRequestHandler> GetHandler(
      const RequestId& id,
      network::ResourceRequest* request,
      bool* intercept_only,
      CefRefPtr<CefRequestImpl>& requestPtr) const {
    CefRefPtr<CefResourceRequestHandler> handler;

    const int64 request_id = id.hash();

    if (browser_) {
      // Maybe the browser's client wants to handle it?
      CefRefPtr<CefClient> client = browser_->GetHost()->GetClient();
      if (client) {
        CefRefPtr<CefRequestHandler> request_handler =
            client->GetRequestHandler();
        if (request_handler) {
          requestPtr = MakeRequest(request, request_id, true);

          handler = request_handler->GetResourceRequestHandler(
              browser_, frame_, requestPtr.get(), is_navigation_, is_download_,
              request_initiator_, *intercept_only);
        }
      }
    }

    if (!handler) {
      // Maybe the request context wants to handle it?
      CefRefPtr<CefRequestContextHandler> context_handler =
          resource_context_->GetHandler(render_process_id_,
                                        request->render_frame_id,
                                        frame_tree_node_id_, false);
      if (context_handler) {
        if (!requestPtr)
          requestPtr = MakeRequest(request, request_id, true);

        handler = context_handler->GetResourceRequestHandler(
            browser_, frame_, requestPtr.get(), is_navigation_, is_download_,
            request_initiator_, *intercept_only);
      }
    }

    return handler;
  }

  RequestState* GetOrCreateState(const RequestId& id) {
    RequestState* state = GetState(id);
    if (!state) {
      state = new RequestState();
      request_map_.insert(std::make_pair(id, base::WrapUnique(state)));
    }
    return state;
  }

  RequestState* GetState(const RequestId& id) const {
    RequestMap::const_iterator it = request_map_.find(id);
    if (it != request_map_.end())
      return it->second.get();
    return nullptr;
  }

  void RemoveState(const RequestId& id) {
    RequestMap::iterator it = request_map_.find(id);
    DCHECK(it != request_map_.end());
    if (it != request_map_.end())
      request_map_.erase(it);
  }

  // Stop accepting new requests and cancel pending/in-flight requests when the
  // CEF context or associated browser is destroyed.
  void OnDestroyed() {
    CEF_REQUIRE_IOT();
    DCHECK(initialized_);

    DCHECK(destruction_observer_);
    destruction_observer_.reset();

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

    if (browser_) {
      // Clear objects that reference the browser.
      browser_ = nullptr;
      frame_ = nullptr;
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
      int64 request_id,
      bool read_only) {
    CefRefPtr<CefRequestImpl> requestPtr = new CefRequestImpl();
    requestPtr->Set(request, request_id);
    if (read_only)
      requestPtr->SetReadOnly(true);
    else
      requestPtr->SetTrackChanges(true);
    return requestPtr;
  }

  bool initialized_ = false;
  bool shutting_down_ = false;

  // Only accessed on the UI thread.
  content::BrowserContext* browser_context_ = nullptr;

  CefRefPtr<CefBrowserHostImpl> browser_;
  CefRefPtr<CefFrame> frame_;
  CefResourceContext* resource_context_ = nullptr;
  int render_process_id_ = 0;
  int render_frame_id_ = -1;
  int frame_tree_node_id_ = -1;
  bool is_navigation_ = true;
  bool is_download_ = false;
  CefString request_initiator_;
  bool is_external_ = false;

  // Default values for standard headers.
  std::string accept_language_;
  std::string user_agent_;

  using RequestMap = std::map<RequestId, std::unique_ptr<RequestState>>;
  RequestMap request_map_;

  using PendingRequests = std::vector<std::unique_ptr<PendingRequest>>;
  PendingRequests pending_requests_;

  // Used to receive destruction notification.
  std::unique_ptr<DestructionObserver> destruction_observer_;

  base::WeakPtrFactory<InterceptedRequestHandlerWrapper> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(InterceptedRequestHandlerWrapper);
};

void InitOnUIThread(
    InterceptedRequestHandlerWrapper* wrapper,
    content::ResourceRequestInfo::WebContentsGetter web_contents_getter,
    int frame_tree_node_id,
    const network::ResourceRequest& request) {
  CEF_REQUIRE_UIT();

  // May return nullptr if the WebContents was destroyed while this callback was
  // in-flight.
  content::WebContents* web_contents = web_contents_getter.Run();
  if (!web_contents) {
    return;
  }

  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  DCHECK(browser_context);

  const int render_process_id =
      web_contents->GetRenderViewHost()->GetProcess()->GetID();

  content::RenderFrameHost* frame = nullptr;

  if (request.render_frame_id >= 0) {
    // TODO(network): Are these main frame checks equivalent?
    if (request.is_main_frame ||
        static_cast<content::ResourceType>(request.resource_type) ==
            content::ResourceType::kMainFrame) {
      frame = web_contents->GetMainFrame();
      DCHECK(frame);
    } else {
      // May return null for newly created iframes.
      frame = content::RenderFrameHost::FromID(render_process_id,
                                               request.render_frame_id);
      if (!frame && frame_tree_node_id >= 0) {
        // May return null for frames in inner WebContents.
        frame = web_contents->FindFrameByFrameTreeNodeId(frame_tree_node_id,
                                                         render_process_id);
      }
      if (!frame) {
        // Use the main frame for the CefBrowserHost.
        frame = web_contents->GetMainFrame();
        DCHECK(frame);
      }
    }
  }

  CefRefPtr<CefBrowserHostImpl> browserPtr;
  CefRefPtr<CefFrame> framePtr;

  // |frame| may be null for service worker requests.
  if (frame) {
#if DCHECK_IS_ON()
    if (frame_tree_node_id >= 0) {
      // Sanity check that we ended up with the expected frame.
      DCHECK_EQ(frame_tree_node_id, frame->GetFrameTreeNodeId());
    }
#endif

    browserPtr = CefBrowserHostImpl::GetBrowserForHost(frame);
    if (browserPtr) {
      framePtr = browserPtr->GetFrameForHost(frame);
      if (frame_tree_node_id < 0)
        frame_tree_node_id = frame->GetFrameTreeNodeId();
      DCHECK(framePtr);
    }
  }

  const bool is_navigation = ui::PageTransitionIsNewNavigation(
      static_cast<ui::PageTransition>(request.transition_type));
  // TODO(navigation): Can we determine the |is_download| value?
  const bool is_download = false;
  url::Origin request_initiator;
  if (request.request_initiator.has_value())
    request_initiator = *request.request_initiator;

  wrapper->Initialize(browser_context, browserPtr, framePtr, render_process_id,
                      request.render_frame_id, frame_tree_node_id,
                      is_navigation, is_download, request_initiator,
                      true /* is_external */);
}

}  // namespace

std::unique_ptr<InterceptedRequestHandler> CreateInterceptedRequestHandler(
    content::BrowserContext* browser_context,
    content::RenderFrameHost* frame,
    int render_process_id,
    bool is_navigation,
    bool is_download,
    const url::Origin& request_initiator) {
  CEF_REQUIRE_UIT();
  auto wrapper = std::make_unique<InterceptedRequestHandlerWrapper>();

  CefRefPtr<CefBrowserHostImpl> browserPtr;
  CefRefPtr<CefFrame> framePtr;
  int render_frame_id = -1;
  int frame_tree_node_id = -1;

  // |frame| may be null for service worker requests.
  if (frame) {
    render_frame_id = frame->GetRoutingID();
    frame_tree_node_id = frame->GetFrameTreeNodeId();
    browserPtr = CefBrowserHostImpl::GetBrowserForHost(frame);
    if (browserPtr) {
      framePtr = browserPtr->GetFrameForHost(frame);
      DCHECK(framePtr);
    }
  }

  // Flag subresource loads of custom schemes.
  const bool is_external =
      !is_navigation && !is_download && !request_initiator.scheme().empty() &&
      !scheme::IsInternalHandledScheme(request_initiator.scheme());

  wrapper->Initialize(browser_context, browserPtr, framePtr, render_process_id,
                      render_frame_id, frame_tree_node_id, is_navigation,
                      is_download, request_initiator, is_external);
  return wrapper;
}

std::unique_ptr<InterceptedRequestHandler> CreateInterceptedRequestHandler(
    content::ResourceRequestInfo::WebContentsGetter web_contents_getter,
    int frame_tree_node_id,
    const network::ResourceRequest& request) {
  CEF_REQUIRE_IOT();
  auto wrapper = std::make_unique<InterceptedRequestHandlerWrapper>();
  CEF_POST_TASK(CEF_UIT, base::BindOnce(
                             InitOnUIThread, base::Unretained(wrapper.get()),
                             web_contents_getter, frame_tree_node_id, request));
  return wrapper;
}

}  // namespace net_service
