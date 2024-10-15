// Copyright (c) 2014 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/wrapper/cef_message_router.h"

#include <algorithm>
#include <limits>
#include <map>
#include <set>

#include "include/base/cef_callback.h"
#include "include/cef_task.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_helpers.h"
#include "libcef_dll/wrapper/cef_browser_info_map.h"
#include "libcef_dll/wrapper/cef_message_router_utils.h"

namespace {

// ID value reserved for internal use.
const int kReservedId = 0;

// Appended to the JS function name for related IPC messages.
const char kMessageSuffix[] = "Msg";

// JS object member argument names for cefQuery.
const char kMemberRequest[] = "request";
const char kMemberOnSuccess[] = "onSuccess";
const char kMemberOnFailure[] = "onFailure";
const char kMemberPersistent[] = "persistent";

// Default error information when a query is canceled.
const int kCanceledErrorCode = -1;
const char kCanceledErrorMessage[] = "The query has been canceled";

// Value of 16KB is chosen as a result of performance tests available at
// http://tests/ipc_performance
constexpr size_t kResponseSizeThreshold = 16384;

// Validate configuration settings.
bool ValidateConfig(CefMessageRouterConfig& config) {
  // Must specify function names.
  if (config.js_cancel_function.empty() || config.js_query_function.empty()) {
    return false;
  }

  return true;
}

namespace cmru = cef_message_router_utils;

/**
 * @brief A helper template for generating ID values.
 *
 * This class generates monotonically increasing ID values within the interval
 * [kReservedId + 1, numeric_limits<T>::max()].
 *
 * @tparam T The data type for the ID values.
 */
template <typename T>
class IdGenerator {
 public:
  IdGenerator() : next_id_(kReservedId) {}

  IdGenerator(const IdGenerator&) = delete;
  IdGenerator& operator=(const IdGenerator&) = delete;

  T GetNextId() {
    if (next_id_ == std::numeric_limits<T>::max()) {
      next_id_ = kReservedId;
    }
    return ++next_id_;
  }

 private:
  T next_id_;
};

// Browser-side router implementation.
class CefMessageRouterBrowserSideImpl : public CefMessageRouterBrowserSide {
 public:
  // Implementation of the Callback interface.
  class CallbackImpl : public CefMessageRouterBrowserSide::Callback {
   public:
    CallbackImpl(CefRefPtr<CefMessageRouterBrowserSideImpl> router,
                 int browser_id,
                 int64_t query_id,
                 bool persistent,
                 size_t message_size_threshold,
                 const std::string& query_message_name)
        : router_(router),
          browser_id_(browser_id),
          query_id_(query_id),
          persistent_(persistent),
          message_size_threshold_(message_size_threshold),
          query_message_name_(query_message_name) {}

    CallbackImpl(const CallbackImpl&) = delete;
    CallbackImpl& operator=(const CallbackImpl&) = delete;

    ~CallbackImpl() override {
      // Hitting this DCHECK means that you didn't call Success or Failure
      // on the Callback after returning true from Handler::OnQuery. You must
      // call Failure to terminate persistent queries.
      DCHECK(!router_);
    }

    void Success(const CefString& response) override {
      auto builder = cmru::CreateBrowserResponseBuilder(
          message_size_threshold_, query_message_name_, response);

      // We need to post task here for two reasons:
      // 1) To safely access member variables.
      // 2) To let the router to persist the query information before
      // the Success callback is executed.
      CefPostTask(TID_UI,
                  base::BindOnce(&CallbackImpl::SuccessImpl, this, builder));
    }

    void Success(const void* data, size_t size) override {
      auto builder = cmru::CreateBrowserResponseBuilder(
          message_size_threshold_, query_message_name_, data, size);

      // We need to post task here for two reasons:
      // 1) To safely access member variables.
      // 2) To let the router to persist the query information before
      // the Success callback is executed.
      CefPostTask(TID_UI,
                  base::BindOnce(&CallbackImpl::SuccessImpl, this, builder));
    }

    void Failure(int error_code, const CefString& error_message) override {
      // We need to post task here for two reasons:
      // 1) To safely access member variables.
      // 2) To give previosly submitted tasks by the Success calls to execute
      // before we invalidate the callback.
      CefPostTask(TID_UI, base::BindOnce(&CallbackImpl::FailureImpl, this,
                                         error_code, error_message));
    }

    void Detach() {
      CEF_REQUIRE_UI_THREAD();
      router_ = nullptr;
    }

   private:
    void SuccessImpl(const CefRefPtr<cmru::BrowserResponseBuilder>& builder) {
      if (!router_) {
        return;
      }

      router_->OnCallbackSuccess(browser_id_, query_id_, builder);

      if (!persistent_) {
        // Non-persistent callbacks are only good for a single use.
        router_ = nullptr;
      }
    }

    void FailureImpl(int error_code, const CefString& error_message) {
      if (!router_) {
        return;
      }

      router_->OnCallbackFailure(browser_id_, query_id_, error_code,
                                 error_message);

      // Failure always invalidates the callback.
      router_ = nullptr;
    }

    CefRefPtr<CefMessageRouterBrowserSideImpl> router_;
    const int browser_id_;
    const int64_t query_id_;
    const bool persistent_;
    const size_t message_size_threshold_;
    const std::string query_message_name_;

    IMPLEMENT_REFCOUNTING(CallbackImpl);
  };

  explicit CefMessageRouterBrowserSideImpl(const CefMessageRouterConfig& config)
      : config_(config),
        query_message_name_(config.js_query_function.ToString() +
                            kMessageSuffix),
        cancel_message_name_(config.js_cancel_function.ToString() +
                             kMessageSuffix) {}

  CefMessageRouterBrowserSideImpl(const CefMessageRouterBrowserSideImpl&) =
      delete;
  CefMessageRouterBrowserSideImpl& operator=(
      const CefMessageRouterBrowserSideImpl&) = delete;

  ~CefMessageRouterBrowserSideImpl() override {
    // There should be no pending queries when the router is deleted.
    DCHECK(browser_query_info_map_.empty());
  }

  bool AddHandler(Handler* handler, bool first) override {
    CEF_REQUIRE_UI_THREAD();
    if (std::find(handlers_.begin(), handlers_.end(), handler) ==
        handlers_.end()) {
      handlers_.insert(first ? handlers_.begin() : handlers_.end(), handler);
      return true;
    }
    return false;
  }

  bool RemoveHandler(Handler* handler) override {
    CEF_REQUIRE_UI_THREAD();
    auto it = std::find(handlers_.begin(), handlers_.end(), handler);
    if (it != handlers_.end()) {
      handlers_.erase(it);
      CancelPendingFor(nullptr, handler, true);
      return true;
    }
    return false;
  }

  void CancelPending(CefRefPtr<CefBrowser> browser, Handler* handler) override {
    CancelPendingFor(browser, handler, true);
  }

  int GetPendingCount(CefRefPtr<CefBrowser> browser,
                      Handler* handler) override {
    CEF_REQUIRE_UI_THREAD();

    if (browser_query_info_map_.empty()) {
      return 0;
    }

    if (handler) {
      // Need to iterate over each QueryInfo object to test the handler.
      class Visitor : public BrowserQueryInfoMap::Visitor {
       public:
        explicit Visitor(Handler* handler) : handler_(handler) {}

        bool OnNextInfo(int browser_id,
                        InfoIdType info_id,
                        InfoObjectType info,
                        bool* remove) override {
          if (info->handler == handler_) {
            count_++;
          }
          return true;
        }

        int count() const { return count_; }

       private:
        Handler* handler_;
        int count_ = 0;
      };

      Visitor visitor(handler);

      if (browser.get()) {
        // Count queries associated with the specified browser.
        browser_query_info_map_.FindAll(browser->GetIdentifier(), &visitor);
      } else {
        // Count all queries for all browsers.
        browser_query_info_map_.FindAll(&visitor);
      }

      return visitor.count();
    } else if (browser.get()) {
      return static_cast<int>(
          browser_query_info_map_.size(browser->GetIdentifier()));
    }

    return static_cast<int>(browser_query_info_map_.size());
  }

  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
    CancelPendingFor(browser, nullptr, false);
  }

  void OnRenderProcessTerminated(CefRefPtr<CefBrowser> browser) override {
    CancelPendingFor(browser, nullptr, false);
  }

  void OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                      CefRefPtr<CefFrame> frame) override {
    if (frame->IsMain()) {
      CancelPendingFor(browser, nullptr, false);
    }
  }

  bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefProcessId source_process,
                                CefRefPtr<CefProcessMessage> message) override {
    CEF_REQUIRE_UI_THREAD();

    const std::string& message_name = message->GetName();
    if (message_name == query_message_name_) {
      cmru::RendererMessage content = cmru::ParseRendererMessage(message);
      const int context_id = content.context_id;
      const int request_id = content.request_id;
      const bool persistent = content.is_persistent;

      if (handlers_.empty()) {
        // No handlers so cancel the query.
        CancelUnhandledQuery(browser, frame, context_id, request_id);
        return true;
      }

      const int browser_id = browser->GetIdentifier();
      const int64_t query_id = query_id_generator_.GetNextId();

      CefRefPtr<CallbackImpl> callback =
          new CallbackImpl(this, browser_id, query_id, persistent,
                           config_.message_size_threshold, query_message_name_);

      // Make a copy of the handler list in case the user adds or removes a
      // handler while we're iterating.
      const HandlerSet handlers = handlers_;

      Handler* handler = std::visit(
          [&](const auto& arg) -> CefMessageRouterBrowserSide::Handler* {
            for (auto handler : handlers) {
              bool handled = handler->OnQuery(browser, frame, query_id, arg,
                                              persistent, callback.get());
              if (handled) {
                return handler;
              }
            }
            return nullptr;
          },
          content.payload);

      // If the query isn't handled nothing should be keeping a reference to
      // the callback.
      DCHECK(handler != nullptr || callback->HasOneRef());

      if (handler) {
        // Persist the query information until the callback executes.
        // It's safe to do this here because the callback will execute
        // asynchronously.
        QueryInfo* info =
            new QueryInfo{browser,    frame,    context_id, request_id,
                          persistent, callback, handler};
        browser_query_info_map_.Add(browser_id, query_id, info);
      } else {
        // Invalidate the callback.
        callback->Detach();

        // No one chose to handle the query so cancel it.
        CancelUnhandledQuery(browser, frame, context_id, request_id);
      }

      return true;
    } else if (message_name == cancel_message_name_) {
      CefRefPtr<CefListValue> args = message->GetArgumentList();
      DCHECK_EQ(args->GetSize(), 2U);

      const int browser_id = browser->GetIdentifier();
      const int context_id = args->GetInt(0);
      const int request_id = args->GetInt(1);

      CancelPendingRequest(browser_id, context_id, request_id);
      return true;
    }

    return false;
  }

 private:
  // Structure representing a pending query.
  struct QueryInfo {
    // Browser and frame originated the query.
    CefRefPtr<CefBrowser> browser;
    CefRefPtr<CefFrame> frame;

    // IDs that uniquely identify the query in the renderer process. These
    // values are opaque to the browser process but must be returned with the
    // response.
    int context_id;
    int request_id;

    // True if the query is persistent.
    bool persistent;

    // Callback associated with the query that must be detached when the query
    // is canceled.
    CefRefPtr<CallbackImpl> callback;

    // Handler that should be notified if the query is automatically canceled.
    Handler* handler;
  };

  // Retrieve a QueryInfo object from the map based on the browser-side query
  // ID. If |always_remove| is true then the QueryInfo object will always be
  // removed from the map. Othewise, the QueryInfo object will only be removed
  // if the query is non-persistent. If |removed| is true the caller is
  // responsible for deleting the returned QueryInfo object.
  QueryInfo* GetQueryInfo(int browser_id,
                          int64_t query_id,
                          bool always_remove,
                          bool* removed) {
    class Visitor : public BrowserQueryInfoMap::Visitor {
     public:
      explicit Visitor(bool always_remove) : always_remove_(always_remove) {}

      bool OnNextInfo(int browser_id,
                      InfoIdType info_id,
                      InfoObjectType info,
                      bool* remove) override {
        *remove = removed_ = (always_remove_ || !info->persistent);
        return true;
      }

      bool removed() const { return removed_; }

     private:
      const bool always_remove_;
      bool removed_ = false;
    };

    Visitor visitor(always_remove);
    QueryInfo* info =
        browser_query_info_map_.Find(browser_id, query_id, &visitor);
    if (info) {
      *removed = visitor.removed();
    }
    return info;
  }

  // Called by CallbackImpl on success.
  void OnCallbackSuccess(
      int browser_id,
      int64_t query_id,
      const CefRefPtr<cmru::BrowserResponseBuilder>& builder) {
    CEF_REQUIRE_UI_THREAD();

    bool removed;
    QueryInfo* info = GetQueryInfo(browser_id, query_id, false, &removed);
    if (info) {
      SendQuerySuccess(info->browser, info->frame, info->context_id,
                       info->request_id, builder);
      if (removed) {
        delete info;
      }
    }
  }

  // Called by CallbackImpl on failure.
  void OnCallbackFailure(int browser_id,
                         int64_t query_id,
                         int error_code,
                         const CefString& error_message) {
    CEF_REQUIRE_UI_THREAD();

    bool removed;
    QueryInfo* info = GetQueryInfo(browser_id, query_id, true, &removed);
    if (info) {
      SendQueryFailure(info, error_code, error_message);
      DCHECK(removed);
      delete info;
    }
  }

  void SendQuerySuccess(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      int context_id,
      int request_id,
      const CefRefPtr<cmru::BrowserResponseBuilder>& builder) {
    if (auto message = builder->Build(context_id, request_id)) {
      frame->SendProcessMessage(PID_RENDERER, message);
    }
  }

  void SendQueryFailure(QueryInfo* info,
                        int error_code,
                        const CefString& error_message) {
    SendQueryFailure(info->browser, info->frame, info->context_id,
                     info->request_id, error_code, error_message);
  }

  void SendQueryFailure(CefRefPtr<CefBrowser> browser,
                        CefRefPtr<CefFrame> frame,
                        int context_id,
                        int request_id,
                        int error_code,
                        const CefString& error_message) {
    CefRefPtr<CefProcessMessage> message =
        CefProcessMessage::Create(query_message_name_);
    CefRefPtr<CefListValue> args = message->GetArgumentList();
    args->SetInt(0, context_id);
    args->SetInt(1, request_id);
    args->SetBool(2, false);  // Indicates a failure result.
    args->SetInt(3, error_code);
    args->SetString(4, error_message);
    frame->SendProcessMessage(PID_RENDERER, message);
  }

  // Cancel a query that has not been sent to a handler.
  void CancelUnhandledQuery(CefRefPtr<CefBrowser> browser,
                            CefRefPtr<CefFrame> frame,
                            int context_id,
                            int request_id) {
    SendQueryFailure(browser, frame, context_id, request_id, kCanceledErrorCode,
                     kCanceledErrorMessage);
  }

  // Cancel a query that has already been sent to a handler.
  void CancelQuery(int64_t query_id, QueryInfo* info, bool notify_renderer) {
    if (notify_renderer) {
      SendQueryFailure(info, kCanceledErrorCode, kCanceledErrorMessage);
    }

    info->handler->OnQueryCanceled(info->browser, info->frame, query_id);

    // Invalidate the callback.
    info->callback->Detach();
  }

  // Cancel all pending queries associated with either |browser| or |handler|.
  // If both |browser| and |handler| are NULL all pending queries will be
  // canceled. Set |notify_renderer| to true if the renderer should be notified.
  void CancelPendingFor(CefRefPtr<CefBrowser> browser,
                        Handler* handler,
                        bool notify_renderer) {
    if (!CefCurrentlyOn(TID_UI)) {
      // Must execute on the UI thread.
      CefPostTask(
          TID_UI,
          base::BindOnce(&CefMessageRouterBrowserSideImpl::CancelPendingFor,
                         this, browser, handler, notify_renderer));
      return;
    }

    if (browser_query_info_map_.empty()) {
      return;
    }

    class Visitor : public BrowserQueryInfoMap::Visitor {
     public:
      Visitor(CefMessageRouterBrowserSideImpl* router,
              Handler* handler,
              bool notify_renderer)
          : router_(router),
            handler_(handler),
            notify_renderer_(notify_renderer) {}

      bool OnNextInfo(int browser_id,
                      InfoIdType info_id,
                      InfoObjectType info,
                      bool* remove) override {
        if (!handler_ || info->handler == handler_) {
          *remove = true;
          router_->CancelQuery(info_id, info, notify_renderer_);
          delete info;
        }
        return true;
      }

     private:
      CefMessageRouterBrowserSideImpl* router_;
      Handler* handler_;
      const bool notify_renderer_;
    };

    Visitor visitor(this, handler, notify_renderer);

    if (browser.get()) {
      // Cancel all queries associated with the specified browser.
      browser_query_info_map_.FindAll(browser->GetIdentifier(), &visitor);
    } else {
      // Cancel all queries for all browsers.
      browser_query_info_map_.FindAll(&visitor);
    }
  }

  // Cancel a query based on the renderer-side IDs. If |request_id| is
  // kReservedId all requests associated with |context_id| will be canceled.
  void CancelPendingRequest(int browser_id, int context_id, int request_id) {
    class Visitor : public BrowserQueryInfoMap::Visitor {
     public:
      Visitor(CefMessageRouterBrowserSideImpl* router,
              int context_id,
              int request_id)
          : router_(router), context_id_(context_id), request_id_(request_id) {}

      bool OnNextInfo(int browser_id,
                      InfoIdType info_id,
                      InfoObjectType info,
                      bool* remove) override {
        if (info->context_id == context_id_ &&
            (request_id_ == kReservedId || info->request_id == request_id_)) {
          *remove = true;
          router_->CancelQuery(info_id, info, false);
          delete info;

          // Stop iterating if only canceling a single request.
          return (request_id_ == kReservedId);
        }
        return true;
      }

     private:
      CefMessageRouterBrowserSideImpl* router_;
      const int context_id_;
      const int request_id_;
    };

    Visitor visitor(this, context_id, request_id);
    browser_query_info_map_.FindAll(browser_id, &visitor);
  }

  const CefMessageRouterConfig config_;
  const std::string query_message_name_;
  const std::string cancel_message_name_;

  IdGenerator<int64_t> query_id_generator_;

  // Set of currently registered handlers. An entry is added when a handler is
  // registered and removed when a handler is unregistered.
  using HandlerSet = std::vector<Handler*>;
  HandlerSet handlers_;

  // Map of query ID to QueryInfo instance. An entry is added when a Handler
  // indicates that it will handle the query and removed when either the query
  // is completed via the Callback, the query is explicitly canceled from the
  // renderer process, or the associated context is (or will be) released.
  using BrowserQueryInfoMap = CefBrowserInfoMap<int64_t, QueryInfo*>;
  BrowserQueryInfoMap browser_query_info_map_;
};

// Renderer-side router implementation.
class CefMessageRouterRendererSideImpl : public CefMessageRouterRendererSide {
 public:
  class V8HandlerImpl : public CefV8Handler {
   public:
    V8HandlerImpl(CefRefPtr<CefMessageRouterRendererSideImpl> router,
                  const CefMessageRouterConfig& config)
        : router_(router), config_(config), context_id_(kReservedId) {}

    V8HandlerImpl(const V8HandlerImpl&) = delete;
    V8HandlerImpl& operator=(const V8HandlerImpl&) = delete;

    bool Execute(const CefString& name,
                 CefRefPtr<CefV8Value> object,
                 const CefV8ValueList& arguments,
                 CefRefPtr<CefV8Value>& retval,
                 CefString& exception) override {
      if (name == config_.js_query_function) {
        if (arguments.size() != 1 || !arguments[0]->IsObject()) {
          exception = "Invalid arguments; expecting a single object";
          return true;
        }

        CefRefPtr<CefV8Value> arg = arguments[0];

        CefRefPtr<CefV8Value> requestVal = arg->GetValue(kMemberRequest);
        if (!requestVal.get()) {
          exception = "Invalid arguments; object member '" +
                      std::string(kMemberRequest) + "' is required";
          return true;
        }

        if (!requestVal->IsString() && !requestVal->IsArrayBuffer()) {
          exception = "Invalid arguments; object member '" +
                      std::string(kMemberRequest) +
                      "' must have type string or ArrayBuffer";
          return true;
        }

        CefRefPtr<CefV8Value> successVal = nullptr;
        if (arg->HasValue(kMemberOnSuccess)) {
          successVal = arg->GetValue(kMemberOnSuccess);
          if (!successVal->IsFunction()) {
            exception = "Invalid arguments; object member '" +
                        std::string(kMemberOnSuccess) +
                        "' must have type function";
            return true;
          }
        }

        CefRefPtr<CefV8Value> failureVal = nullptr;
        if (arg->HasValue(kMemberOnFailure)) {
          failureVal = arg->GetValue(kMemberOnFailure);
          if (!failureVal->IsFunction()) {
            exception = "Invalid arguments; object member '" +
                        std::string(kMemberOnFailure) +
                        "' must have type function";
            return true;
          }
        }

        CefRefPtr<CefV8Value> persistentVal = nullptr;
        if (arg->HasValue(kMemberPersistent)) {
          persistentVal = arg->GetValue(kMemberPersistent);
          if (!persistentVal->IsBool()) {
            exception = "Invalid arguments; object member '" +
                        std::string(kMemberPersistent) +
                        "' must have type boolean";
            return true;
          }
        }

        CefRefPtr<CefV8Context> context = CefV8Context::GetCurrentContext();
        const int context_id = GetIDForContext(context);
        const bool persistent =
            (persistentVal.get() && persistentVal->GetBoolValue());

        const int request_id = router_->SendQuery(
            context->GetBrowser(), context->GetFrame(), context_id, requestVal,
            persistent, successVal, failureVal);

        retval = CefV8Value::CreateInt(request_id);

        return true;
      } else if (name == config_.js_cancel_function) {
        if (arguments.size() != 1 || !arguments[0]->IsInt()) {
          exception = "Invalid arguments; expecting a single integer";
          return true;
        }

        bool result = false;
        const int request_id = arguments[0]->GetIntValue();
        if (request_id != kReservedId) {
          CefRefPtr<CefV8Context> context = CefV8Context::GetCurrentContext();
          const int context_id = GetIDForContext(context);
          result =
              router_->SendCancel(context->GetBrowser(), context->GetFrame(),
                                  context_id, request_id);
        }
        retval = CefV8Value::CreateBool(result);
        return true;
      }

      return false;
    }

   private:
    // Don't create the context ID until it's actually needed.
    int GetIDForContext(CefRefPtr<CefV8Context> context) {
      if (context_id_ == kReservedId) {
        context_id_ = router_->CreateIDForContext(context);
      }
      return context_id_;
    }

    CefRefPtr<CefMessageRouterRendererSideImpl> router_;
    const CefMessageRouterConfig config_;
    int context_id_;

    IMPLEMENT_REFCOUNTING(V8HandlerImpl);
  };

  explicit CefMessageRouterRendererSideImpl(
      const CefMessageRouterConfig& config)
      : config_(config),
        query_message_name_(config.js_query_function.ToString() +
                            kMessageSuffix),
        cancel_message_name_(config.js_cancel_function.ToString() +
                             kMessageSuffix) {}

  CefMessageRouterRendererSideImpl(const CefMessageRouterRendererSideImpl&) =
      delete;
  CefMessageRouterRendererSideImpl& operator=(
      const CefMessageRouterRendererSideImpl&) = delete;

  int GetPendingCount(CefRefPtr<CefBrowser> browser,
                      CefRefPtr<CefV8Context> context) override {
    CEF_REQUIRE_RENDERER_THREAD();

    if (browser_request_info_map_.empty()) {
      return 0;
    }

    if (context.get()) {
      const int context_id = GetIDForContext(context, false);
      if (context_id == kReservedId) {
        return 0;  // Nothing associated with the specified context.
      }

      // Need to iterate over each RequestInfo object to test the context.
      class Visitor : public BrowserRequestInfoMap::Visitor {
       public:
        explicit Visitor(int context_id) : context_id_(context_id) {}

        bool OnNextInfo(int browser_id,
                        InfoIdType info_id,
                        InfoObjectType info,
                        bool* remove) override {
          if (info_id.first == context_id_) {
            count_++;
          }
          return true;
        }

        int count() const { return count_; }

       private:
        int context_id_;
        int count_ = 0;
      };

      Visitor visitor(context_id);

      if (browser.get()) {
        // Count requests associated with the specified browser.
        browser_request_info_map_.FindAll(browser->GetIdentifier(), &visitor);
      } else {
        // Count all requests for all browsers.
        browser_request_info_map_.FindAll(&visitor);
      }

      return visitor.count();
    } else if (browser.get()) {
      return static_cast<int>(
          browser_request_info_map_.size(browser->GetIdentifier()));
    }

    return static_cast<int>(browser_request_info_map_.size());
  }

  void OnContextCreated(CefRefPtr<CefBrowser> browser,
                        CefRefPtr<CefFrame> frame,
                        CefRefPtr<CefV8Context> context) override {
    CEF_REQUIRE_RENDERER_THREAD();

    // Register function handlers with the 'window' object.
    CefRefPtr<CefV8Value> window = context->GetGlobal();

    CefRefPtr<V8HandlerImpl> handler = new V8HandlerImpl(this, config_);
    CefV8Value::PropertyAttribute attributes =
        static_cast<CefV8Value::PropertyAttribute>(
            V8_PROPERTY_ATTRIBUTE_READONLY | V8_PROPERTY_ATTRIBUTE_DONTENUM |
            V8_PROPERTY_ATTRIBUTE_DONTDELETE);

    // Add the query function.
    CefRefPtr<CefV8Value> query_func =
        CefV8Value::CreateFunction(config_.js_query_function, handler.get());
    window->SetValue(config_.js_query_function, query_func, attributes);

    // Add the cancel function.
    CefRefPtr<CefV8Value> cancel_func =
        CefV8Value::CreateFunction(config_.js_cancel_function, handler.get());
    window->SetValue(config_.js_cancel_function, cancel_func, attributes);
  }

  void OnContextReleased(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         CefRefPtr<CefV8Context> context) override {
    CEF_REQUIRE_RENDERER_THREAD();

    // Get the context ID and remove the context from the map.
    const int context_id = GetIDForContext(context, true);
    if (context_id != kReservedId) {
      // Cancel all pending requests for the context.
      SendCancel(browser, frame, context_id, kReservedId);
    }
  }

  bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefProcessId source_process,
                                CefRefPtr<CefProcessMessage> message) override {
    CEF_REQUIRE_RENDERER_THREAD();

    if (message->GetName() != query_message_name_) {
      return false;
    }

    cmru::BrowserMessage content = cmru::ParseBrowserMessage(message);
    if (content.is_success) {
      std::visit(
          [&](const auto& arg) {
            ExecuteSuccessCallback(browser->GetIdentifier(), content.context_id,
                                   content.request_id, arg);
          },
          content.payload);

    } else {
      ExecuteFailureCallback(browser->GetIdentifier(), content.context_id,
                             content.request_id, content.error_code,
                             std::get<CefString>(content.payload));
    }

    return true;
  }

 private:
  // Structure representing a pending request.
  struct RequestInfo {
    // True if the request is persistent.
    bool persistent;

    // Success callback function. May be NULL.
    CefRefPtr<CefV8Value> success_callback;

    // Failure callback function. May be NULL.
    CefRefPtr<CefV8Value> failure_callback;
  };

  // Retrieve a RequestInfo object from the map based on the renderer-side
  // IDs. If |always_remove| is true then the RequestInfo object will always be
  // removed from the map. Othewise, the RequestInfo object will only be removed
  // if the query is non-persistent. If |removed| is true the caller is
  // responsible for deleting the returned QueryInfo object.
  RequestInfo* GetRequestInfo(int browser_id,
                              int context_id,
                              int request_id,
                              bool always_remove,
                              bool* removed) {
    class Visitor : public BrowserRequestInfoMap::Visitor {
     public:
      explicit Visitor(bool always_remove) : always_remove_(always_remove) {}

      bool OnNextInfo(int browser_id,
                      InfoIdType info_id,
                      InfoObjectType info,
                      bool* remove) override {
        *remove = removed_ = (always_remove_ || !info->persistent);
        return true;
      }

      bool removed() const { return removed_; }

     private:
      const bool always_remove_;
      bool removed_ = false;
    };

    Visitor visitor(always_remove);
    RequestInfo* info = browser_request_info_map_.Find(
        browser_id, std::make_pair(context_id, request_id), &visitor);
    if (info) {
      *removed = visitor.removed();
    }
    return info;
  }

  // Returns the new request ID.
  int SendQuery(CefRefPtr<CefBrowser> browser,
                CefRefPtr<CefFrame> frame,
                int context_id,
                CefRefPtr<CefV8Value> request,
                bool persistent,
                CefRefPtr<CefV8Value> success_callback,
                CefRefPtr<CefV8Value> failure_callback) {
    CEF_REQUIRE_RENDERER_THREAD();

    const int request_id = request_id_generator_.GetNextId();

    auto* info =
        new RequestInfo{persistent, success_callback, failure_callback};

    browser_request_info_map_.Add(browser->GetIdentifier(),
                                  std::make_pair(context_id, request_id), info);

    CefRefPtr<CefProcessMessage> message = cmru::BuildRendererMsg(
        config_.message_size_threshold, query_message_name_, context_id,
        request_id, request, persistent);

    frame->SendProcessMessage(PID_BROWSER, message);

    return request_id;
  }

  // If |request_id| is kReservedId all requests associated with |context_id|
  // will be canceled, otherwise only the specified |request_id| will be
  // canceled. Returns true if any request was canceled.
  bool SendCancel(CefRefPtr<CefBrowser> browser,
                  CefRefPtr<CefFrame> frame,
                  int context_id,
                  int request_id) {
    CEF_REQUIRE_RENDERER_THREAD();

    const int browser_id = browser->GetIdentifier();

    int cancel_count = 0;
    if (request_id != kReservedId) {
      // Cancel a single request.
      bool removed;
      RequestInfo* info =
          GetRequestInfo(browser_id, context_id, request_id, true, &removed);
      if (info) {
        DCHECK(removed);
        delete info;
        cancel_count = 1;
      }
    } else {
      // Cancel all requests with the specified context ID.
      class Visitor : public BrowserRequestInfoMap::Visitor {
       public:
        explicit Visitor(int context_id) : context_id_(context_id) {}

        bool OnNextInfo(int browser_id,
                        InfoIdType info_id,
                        InfoObjectType info,
                        bool* remove) override {
          if (info_id.first == context_id_) {
            *remove = true;
            delete info;
            cancel_count_++;
          }
          return true;
        }

        int cancel_count() const { return cancel_count_; }

       private:
        const int context_id_;
        int cancel_count_ = 0;
      };

      Visitor visitor(context_id);
      browser_request_info_map_.FindAll(browser_id, &visitor);
      cancel_count = visitor.cancel_count();
    }

    if (cancel_count > 0) {
      CefRefPtr<CefProcessMessage> message =
          CefProcessMessage::Create(cancel_message_name_);

      CefRefPtr<CefListValue> args = message->GetArgumentList();
      args->SetInt(0, context_id);
      args->SetInt(1, request_id);

      frame->SendProcessMessage(PID_BROWSER, message);
      return true;
    }

    return false;
  }

  // Execute the onSuccess JavaScript callback.
  void ExecuteSuccessCallback(int browser_id,
                              int context_id,
                              int request_id,
                              const CefString& response) {
    CEF_REQUIRE_RENDERER_THREAD();

    bool removed;
    RequestInfo* info =
        GetRequestInfo(browser_id, context_id, request_id, false, &removed);
    if (!info) {
      return;
    }

    CefRefPtr<CefV8Context> context = GetContextByID(context_id);
    if (context && info->success_callback) {
      CefV8ValueList args;
      args.push_back(CefV8Value::CreateString(response));
      info->success_callback->ExecuteFunctionWithContext(context, nullptr,
                                                         args);
    }

    if (removed) {
      delete info;
    }
  }

  // Execute the onSuccess JavaScript callback.
  void ExecuteSuccessCallback(int browser_id,
                              int context_id,
                              int request_id,
                              const CefRefPtr<CefBinaryBuffer>& response) {
    CEF_REQUIRE_RENDERER_THREAD();

    bool removed;
    RequestInfo* info =
        GetRequestInfo(browser_id, context_id, request_id, false, &removed);
    if (!info) {
      return;
    }

    CefRefPtr<CefV8Context> context = GetContextByID(context_id);
    if (context && info->success_callback && context->Enter()) {
      CefRefPtr<CefV8Value> value;
#ifdef CEF_V8_ENABLE_SANDBOX
      value = CefV8Value::CreateArrayBufferWithCopy(response->GetData(),
                                                    response->GetSize());
#else
      value = CefV8Value::CreateArrayBuffer(
          response->GetData(), response->GetSize(),
          new cmru::BinaryValueABRCallback(response));
#endif

      context->Exit();

      CefV8ValueList args;
      args.push_back(value);
      info->success_callback->ExecuteFunctionWithContext(context, nullptr,
                                                         args);
    }

    if (removed) {
      delete info;
    }
  }

  // Execute the onFailure JavaScript callback.
  void ExecuteFailureCallback(int browser_id,
                              int context_id,
                              int request_id,
                              int error_code,
                              const CefString& error_message) {
    CEF_REQUIRE_RENDERER_THREAD();

    bool removed;
    RequestInfo* info =
        GetRequestInfo(browser_id, context_id, request_id, true, &removed);
    if (!info) {
      return;
    }

    CefRefPtr<CefV8Context> context = GetContextByID(context_id);
    if (context && info->failure_callback) {
      CefV8ValueList args;
      args.push_back(CefV8Value::CreateInt(error_code));
      args.push_back(CefV8Value::CreateString(error_message));
      info->failure_callback->ExecuteFunctionWithContext(context, nullptr,
                                                         args);
    }

    DCHECK(removed);
    delete info;
  }

  int CreateIDForContext(CefRefPtr<CefV8Context> context) {
    CEF_REQUIRE_RENDERER_THREAD();

    // The context should not already have an associated ID.
    DCHECK_EQ(GetIDForContext(context, false), kReservedId);

    const int context_id = context_id_generator_.GetNextId();
    context_map_.insert(std::make_pair(context_id, context));
    return context_id;
  }

  // Retrieves the existing ID value associated with the specified |context|.
  // If |remove| is true the context will also be removed from the map.
  int GetIDForContext(CefRefPtr<CefV8Context> context, bool remove) {
    CEF_REQUIRE_RENDERER_THREAD();

    ContextMap::iterator it = context_map_.begin();
    for (; it != context_map_.end(); ++it) {
      if (it->second->IsSame(context)) {
        int context_id = it->first;
        if (remove) {
          context_map_.erase(it);
        }
        return context_id;
      }
    }

    return kReservedId;
  }

  CefRefPtr<CefV8Context> GetContextByID(int context_id) {
    CEF_REQUIRE_RENDERER_THREAD();

    ContextMap::const_iterator it = context_map_.find(context_id);
    if (it != context_map_.end()) {
      return it->second;
    }
    return nullptr;
  }

  const CefMessageRouterConfig config_;
  const std::string query_message_name_;
  const std::string cancel_message_name_;

  IdGenerator<int> context_id_generator_;
  IdGenerator<int> request_id_generator_;

  // Map of (request ID, context ID) to RequestInfo for pending queries. An
  // entry is added when a request is initiated via the bound function and
  // removed when either the request completes, is canceled via the bound
  // function, or the associated context is released.
  using BrowserRequestInfoMap =
      CefBrowserInfoMap<std::pair<int, int>, RequestInfo*>;
  BrowserRequestInfoMap browser_request_info_map_;

  // Map of context ID to CefV8Context for existing contexts. An entry is added
  // when a bound function is executed for the first time in the context and
  // removed when the context is released.
  using ContextMap = std::map<int, CefRefPtr<CefV8Context>>;
  ContextMap context_map_;
};

}  // namespace

CefMessageRouterConfig::CefMessageRouterConfig()
    : js_query_function("cefQuery"),
      js_cancel_function("cefQueryCancel"),
      message_size_threshold(kResponseSizeThreshold) {}

// static
CefRefPtr<CefMessageRouterBrowserSide> CefMessageRouterBrowserSide::Create(
    const CefMessageRouterConfig& config) {
  CefMessageRouterConfig validated_config = config;
  if (!ValidateConfig(validated_config)) {
    return nullptr;
  }
  return new CefMessageRouterBrowserSideImpl(validated_config);
}

// static
CefRefPtr<CefMessageRouterRendererSide> CefMessageRouterRendererSide::Create(
    const CefMessageRouterConfig& config) {
  CefMessageRouterConfig validated_config = config;
  if (!ValidateConfig(validated_config)) {
    return nullptr;
  }
  return new CefMessageRouterRendererSideImpl(validated_config);
}
