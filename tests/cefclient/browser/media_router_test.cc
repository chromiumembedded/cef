// Copyright (c) 2020 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/media_router_test.h"

#include <string>
#include <vector>

#include "include/base/cef_logging.h"
#include "include/cef_media_router.h"
#include "include/cef_parser.h"
#include "tests/cefclient/browser/test_runner.h"

namespace client {
namespace media_router_test {

namespace {

const char kTestUrlPath[] = "/media_router";

// Application-specific error codes.
const int kMessageFormatError = 1;
const int kRequestFailedError = 2;

// Message strings.
const char kNameKey[] = "name";
const char kNameValueSubscribe[] = "subscribe";
const char kNameValueCreateRoute[] = "createRoute";
const char kNameValueTerminateRoute[] = "terminateRoute";
const char kNameValueSendMessage[] = "sendMessage";
const char kSourceKey[] = "source_urn";
const char kSinkKey[] = "sink_id";
const char kRouteKey[] = "route_id";
const char kMessageKey[] = "message";
const char kSuccessKey[] = "success";
const char kPayloadKey[] = "payload";

// Convert a dictionary value to a JSON string.
CefString GetJSON(CefRefPtr<CefDictionaryValue> dictionary) {
  CefRefPtr<CefValue> value = CefValue::Create();
  value->SetDictionary(dictionary);
  return CefWriteJSON(value, JSON_WRITER_DEFAULT);
}

typedef CefMessageRouterBrowserSide::Callback CallbackType;

void SendSuccess(CefRefPtr<CallbackType> callback,
                 CefRefPtr<CefDictionaryValue> result) {
  callback->Success(GetJSON(result));
}

void SendFailure(CefRefPtr<CallbackType> callback,
                 int error_code,
                 const std::string& error_message) {
  callback->Failure(error_code, error_message);
}

// Callback for CefMediaRouter::CreateRoute.
class MediaRouteCreateCallback : public CefMediaRouteCreateCallback {
 public:
  explicit MediaRouteCreateCallback(CefRefPtr<CallbackType> create_callback)
      : create_callback_(create_callback) {}

  // CefMediaRouteCreateCallback method:
  void OnMediaRouteCreateFinished(RouteCreateResult result,
                                  const CefString& error,
                                  CefRefPtr<CefMediaRoute> route) OVERRIDE {
    CEF_REQUIRE_UI_THREAD();
    if (result == CEF_MRCR_OK) {
      CefRefPtr<CefDictionaryValue> dict = CefDictionaryValue::Create();
      dict->SetString(kRouteKey, route->GetId());
      SendSuccess(create_callback_, dict);
    } else {
      SendFailure(create_callback_, kRequestFailedError + result, error);
    }
    create_callback_ = NULL;
  }

 private:
  CefRefPtr<CallbackType> create_callback_;

  IMPLEMENT_REFCOUNTING(MediaRouteCreateCallback);
  DISALLOW_COPY_AND_ASSIGN(MediaRouteCreateCallback);
};

// Observes MediaRouter events. Only accessed on the UI thread.
class MediaObserver : public CefMediaObserver {
 public:
  typedef std::vector<CefRefPtr<CefMediaRoute>> MediaRouteVector;
  typedef std::vector<CefRefPtr<CefMediaSink>> MediaSinkVector;

  MediaObserver(CefRefPtr<CefMediaRouter> media_router,
                CefRefPtr<CallbackType> subscription_callback)
      : media_router_(media_router),
        subscription_callback_(subscription_callback),
        next_sink_query_id_(0),
        pending_sink_query_id_(-1),
        pending_sink_callbacks_(0U) {}

  ~MediaObserver() OVERRIDE { ClearSinkInfoMap(); }

  bool CreateRoute(const std::string& source_urn,
                   const std::string& sink_id,
                   CefRefPtr<CallbackType> callback,
                   std::string& error) {
    CefRefPtr<CefMediaSource> source = GetSource(source_urn);
    if (!source) {
      error = "Invalid source: " + source_urn;
      return false;
    }

    CefRefPtr<CefMediaSink> sink = GetSink(sink_id);
    if (!sink) {
      error = "Invalid sink: " + sink_id;
      return false;
    }

    media_router_->CreateRoute(source, sink,
                               new MediaRouteCreateCallback(callback));
    return true;
  }

  bool TerminateRoute(const std::string& route_id, std::string& error) {
    CefRefPtr<CefMediaRoute> route = GetRoute(route_id);
    if (!route) {
      error = "Invalid route: " + route_id;
      return false;
    }

    route->Terminate();
    return true;
  }

  bool SendRouteMessage(const std::string& route_id,
                        const std::string& message,
                        std::string& error) {
    CefRefPtr<CefMediaRoute> route = GetRoute(route_id);
    if (!route) {
      error = "Invalid route: " + route_id;
      return false;
    }

    route->SendRouteMessage(message.c_str(), message.size());
    return true;
  }

 protected:
  class DeviceInfoCallback : public CefMediaSinkDeviceInfoCallback {
   public:
    // Callback to be executed when the device info is available.
    typedef base::Callback<void(const std::string& sink_id,
                                const CefMediaSinkDeviceInfo& device_info)>
        CallbackType;

    DeviceInfoCallback(const std::string& sink_id, const CallbackType& callback)
        : sink_id_(sink_id), callback_(callback) {}

    void OnMediaSinkDeviceInfo(
        const CefMediaSinkDeviceInfo& device_info) OVERRIDE {
      CEF_REQUIRE_UI_THREAD();
      callback_.Run(sink_id_, device_info);
      callback_.Reset();
    }

   private:
    const std::string sink_id_;
    CallbackType callback_;

    IMPLEMENT_REFCOUNTING(DeviceInfoCallback);
    DISALLOW_COPY_AND_ASSIGN(DeviceInfoCallback);
  };

  // CefMediaObserver methods:
  void OnSinks(const MediaSinkVector& sinks) OVERRIDE {
    CEF_REQUIRE_UI_THREAD();

    ClearSinkInfoMap();

    // Reset pending sink state.
    pending_sink_callbacks_ = sinks.size();
    pending_sink_query_id_ = ++next_sink_query_id_;

    if (sinks.empty()) {
      // No sinks, send the response immediately.
      SendSinksResponse();
      return;
    }

    DeviceInfoCallback::CallbackType callback = base::Bind(
        &MediaObserver::OnSinkDeviceInfo, this, pending_sink_query_id_);

    MediaSinkVector::const_iterator it = sinks.begin();
    for (size_t idx = 0; it != sinks.end(); ++it, ++idx) {
      CefRefPtr<CefMediaSink> sink = *it;
      const std::string& sink_id = sink->GetId();
      SinkInfo* info = new SinkInfo;
      info->sink = sink;
      sink_info_map_.insert(std::make_pair(sink_id, info));

      // Request the device info asynchronously. Send the response once all
      // callbacks have executed.
      sink->GetDeviceInfo(new DeviceInfoCallback(sink_id, callback));
    }
  }

  void OnRoutes(const MediaRouteVector& routes) OVERRIDE {
    CEF_REQUIRE_UI_THREAD();

    route_map_.clear();

    CefRefPtr<CefDictionaryValue> payload = CefDictionaryValue::Create();
    CefRefPtr<CefListValue> routes_list = CefListValue::Create();
    routes_list->SetSize(routes.size());

    MediaRouteVector::const_iterator it = routes.begin();
    for (size_t idx = 0; it != routes.end(); ++it, ++idx) {
      CefRefPtr<CefMediaRoute> route = *it;
      const std::string& route_id = route->GetId();
      route_map_.insert(std::make_pair(route_id, route));

      CefRefPtr<CefDictionaryValue> route_dict = CefDictionaryValue::Create();
      route_dict->SetString("id", route_id);
      route_dict->SetString(kSourceKey, route->GetSource()->GetId());
      route_dict->SetString(kSinkKey, route->GetSink()->GetId());
      routes_list->SetDictionary(idx, route_dict);
    }

    payload->SetList("routes_list", routes_list);
    SendResponse("onRoutes", payload);
  }

  void OnRouteStateChanged(CefRefPtr<CefMediaRoute> route,
                           ConnectionState state) OVERRIDE {
    CEF_REQUIRE_UI_THREAD();

    CefRefPtr<CefDictionaryValue> payload = CefDictionaryValue::Create();
    payload->SetString(kRouteKey, route->GetId());
    payload->SetInt("connection_state", state);
    SendResponse("onRouteStateChanged", payload);
  }

  void OnRouteMessageReceived(CefRefPtr<CefMediaRoute> route,
                              const void* message,
                              size_t message_size) OVERRIDE {
    CEF_REQUIRE_UI_THREAD();

    std::string message_str(static_cast<const char*>(message), message_size);

    CefRefPtr<CefDictionaryValue> payload = CefDictionaryValue::Create();
    payload->SetString(kRouteKey, route->GetId());
    payload->SetString(kMessageKey, message_str);
    SendResponse("onRouteMessageReceived", payload);
  }

 private:
  CefRefPtr<CefMediaSource> GetSource(const std::string& source_urn) {
    CefRefPtr<CefMediaSource> source = media_router_->GetSource(source_urn);
    if (!source)
      return NULL;
    return source;
  }

  CefRefPtr<CefMediaSink> GetSink(const std::string& sink_id) {
    SinkInfoMap::const_iterator it = sink_info_map_.find(sink_id);
    if (it != sink_info_map_.end())
      return it->second->sink;
    return NULL;
  }

  void ClearSinkInfoMap() {
    SinkInfoMap::const_iterator it = sink_info_map_.begin();
    for (; it != sink_info_map_.end(); ++it) {
      delete it->second;
    }
    sink_info_map_.clear();
  }

  void OnSinkDeviceInfo(int sink_query_id,
                        const std::string& sink_id,
                        const CefMediaSinkDeviceInfo& device_info) {
    // Discard callbacks that arrive after a new call to OnSinks().
    if (sink_query_id != pending_sink_query_id_)
      return;

    SinkInfoMap::const_iterator it = sink_info_map_.find(sink_id);
    if (it != sink_info_map_.end()) {
      it->second->device_info = device_info;
    }

    // Send the response once we've received all expected callbacks.
    DCHECK_GT(pending_sink_callbacks_, 0U);
    if (--pending_sink_callbacks_ == 0U) {
      SendSinksResponse();
    }
  }

  CefRefPtr<CefMediaRoute> GetRoute(const std::string& route_id) {
    RouteMap::const_iterator it = route_map_.find(route_id);
    if (it != route_map_.end())
      return it->second;
    return NULL;
  }

  void SendResponse(const std::string& name,
                    CefRefPtr<CefDictionaryValue> payload) {
    CefRefPtr<CefDictionaryValue> result = CefDictionaryValue::Create();
    result->SetString(kNameKey, name);
    result->SetDictionary(kPayloadKey, payload);
    SendSuccess(subscription_callback_, result);
  }

  void SendSinksResponse() {
    CefRefPtr<CefDictionaryValue> payload = CefDictionaryValue::Create();
    CefRefPtr<CefListValue> sinks_list = CefListValue::Create();
    sinks_list->SetSize(sink_info_map_.size());

    SinkInfoMap::const_iterator it = sink_info_map_.begin();
    for (size_t idx = 0; it != sink_info_map_.end(); ++it, ++idx) {
      const SinkInfo* info = it->second;

      CefRefPtr<CefDictionaryValue> sink_dict = CefDictionaryValue::Create();
      sink_dict->SetString("id", it->first);
      sink_dict->SetString("name", info->sink->GetName());
      sink_dict->SetString("desc", info->sink->GetDescription());
      sink_dict->SetInt("icon", info->sink->GetIconType());
      sink_dict->SetString("ip_address",
                           CefString(&info->device_info.ip_address));
      sink_dict->SetInt("port", info->device_info.port);
      sink_dict->SetString("model_name",
                           CefString(&info->device_info.model_name));
      sink_dict->SetString("type",
                           info->sink->IsCastSink()
                               ? "cast"
                               : info->sink->IsDialSink() ? "dial" : "unknown");
      sinks_list->SetDictionary(idx, sink_dict);
    }

    payload->SetList("sinks_list", sinks_list);
    SendResponse("onSinks", payload);
  }

  CefRefPtr<CefMediaRouter> media_router_;
  CefRefPtr<CallbackType> subscription_callback_;

  struct SinkInfo {
    CefRefPtr<CefMediaSink> sink;
    CefMediaSinkDeviceInfo device_info;
  };
  typedef std::map<std::string, SinkInfo*> SinkInfoMap;

  // Used to uniquely identify a call to OnSinks(), for the purpose of
  // associating OnMediaSinkDeviceInfo() callbacks.
  int next_sink_query_id_;

  // State from the most recent call to OnSinks().
  SinkInfoMap sink_info_map_;
  int pending_sink_query_id_;
  size_t pending_sink_callbacks_;

  // State from the most recent call to OnRoutes().
  typedef std::map<std::string, CefRefPtr<CefMediaRoute>> RouteMap;
  RouteMap route_map_;

  IMPLEMENT_REFCOUNTING(MediaObserver);
  DISALLOW_COPY_AND_ASSIGN(MediaObserver);
};

// Handle messages in the browser process. Only accessed on the UI thread.
class Handler : public CefMessageRouterBrowserSide::Handler {
 public:
  typedef std::vector<std::string> NameVector;

  Handler() { CEF_REQUIRE_UI_THREAD(); }

  virtual ~Handler() {
    SubscriptionStateMap::iterator it = subscription_state_map_.begin();
    for (; it != subscription_state_map_.end(); ++it) {
      delete it->second;
    }
  }

  // Called due to cefQuery execution in media_router.html.
  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64 query_id,
               const CefString& request,
               bool persistent,
               CefRefPtr<Callback> callback) OVERRIDE {
    CEF_REQUIRE_UI_THREAD();

    // Only handle messages from the test URL.
    const std::string& url = frame->GetURL();
    if (!test_runner::IsTestURL(url, kTestUrlPath))
      return false;

    // Parse |request| as a JSON dictionary.
    CefRefPtr<CefDictionaryValue> request_dict = ParseJSON(request);
    if (!request_dict) {
      SendFailure(callback, kMessageFormatError, "Incorrect message format");
      return true;
    }

    // Verify the "name" key.
    if (!VerifyKey(request_dict, kNameKey, VTYPE_STRING, callback))
      return true;

    const std::string& message_name = request_dict->GetString(kNameKey);
    if (message_name == kNameValueSubscribe) {
      // Subscribe to notifications from the media router.

      if (!persistent) {
        SendFailure(callback, kMessageFormatError,
                    "Subscriptions must be persistent");
        return true;
      }

      if (!CreateSubscription(browser, query_id, callback)) {
        SendFailure(callback, kRequestFailedError,
                    "Browser is already subscribed");
      }
      return true;
    }

    // All other messages require a current subscription.
    CefRefPtr<MediaObserver> media_observer =
        GetMediaObserver(browser->GetIdentifier());
    if (!media_observer) {
      SendFailure(callback, kRequestFailedError,
                  "Browser is not currently subscribed");
    }

    if (message_name == kNameValueCreateRoute) {
      // Create a new route.

      // Verify the "source_urn" key.
      if (!VerifyKey(request_dict, kSourceKey, VTYPE_STRING, callback))
        return true;
      // Verify the "sink_id" key.
      if (!VerifyKey(request_dict, kSinkKey, VTYPE_STRING, callback))
        return true;

      const std::string& source_urn = request_dict->GetString(kSourceKey);
      const std::string& sink_id = request_dict->GetString(kSinkKey);

      // |callback| will be executed once the route is created.
      std::string error;
      if (!media_observer->CreateRoute(source_urn, sink_id, callback, error)) {
        SendFailure(callback, kRequestFailedError, error);
      }
      return true;
    } else if (message_name == kNameValueTerminateRoute) {
      // Terminate an existing route.

      // Verify the "route" key.
      if (!VerifyKey(request_dict, kRouteKey, VTYPE_STRING, callback))
        return true;

      const std::string& route_id = request_dict->GetString(kRouteKey);
      std::string error;
      if (!media_observer->TerminateRoute(route_id, error)) {
        SendFailure(callback, kRequestFailedError, error);
      } else {
        SendSuccessACK(callback);
      }
      return true;
    } else if (message_name == kNameValueSendMessage) {
      // Send a route message.

      // Verify the "route_id" key.
      if (!VerifyKey(request_dict, kRouteKey, VTYPE_STRING, callback))
        return true;
      // Verify the "message" key.
      if (!VerifyKey(request_dict, kMessageKey, VTYPE_STRING, callback))
        return true;

      const std::string& route_id = request_dict->GetString(kRouteKey);
      const std::string& message = request_dict->GetString(kMessageKey);
      std::string error;
      if (!media_observer->SendRouteMessage(route_id, message, error)) {
        SendFailure(callback, kRequestFailedError, error);
      } else {
        SendSuccessACK(callback);
      }
      return true;
    }

    return false;
  }

  void OnQueryCanceled(CefRefPtr<CefBrowser> browser,
                       CefRefPtr<CefFrame> frame,
                       int64 query_id) OVERRIDE {
    CEF_REQUIRE_UI_THREAD();
    RemoveSubscription(browser->GetIdentifier(), query_id);
  }

 private:
  static void SendSuccessACK(CefRefPtr<Callback> callback) {
    CefRefPtr<CefDictionaryValue> result = CefDictionaryValue::Create();
    result->SetBool(kSuccessKey, true);
    SendSuccess(callback, result);
  }

  // Convert a JSON string to a dictionary value.
  static CefRefPtr<CefDictionaryValue> ParseJSON(const CefString& string) {
    CefRefPtr<CefValue> value = CefParseJSON(string, JSON_PARSER_RFC);
    if (value.get() && value->GetType() == VTYPE_DICTIONARY)
      return value->GetDictionary();
    return nullptr;
  }

  // Verify that |key| exists in |dictionary| and has type |value_type|. Fails
  // |callback| and returns false on failure.
  static bool VerifyKey(CefRefPtr<CefDictionaryValue> dictionary,
                        const char* key,
                        cef_value_type_t value_type,
                        CefRefPtr<Callback> callback) {
    if (!dictionary->HasKey(key) || dictionary->GetType(key) != value_type) {
      SendFailure(
          callback, kMessageFormatError,
          "Missing or incorrectly formatted message key: " + std::string(key));
      return false;
    }
    return true;
  }

  // Subscription state associated with a single browser.
  struct SubscriptionState {
    int64 query_id;
    CefRefPtr<MediaObserver> observer;
    CefRefPtr<CefRegistration> registration;
  };

  bool CreateSubscription(CefRefPtr<CefBrowser> browser,
                          int64 query_id,
                          CefRefPtr<Callback> callback) {
    const int browser_id = browser->GetIdentifier();
    if (subscription_state_map_.find(browser_id) !=
        subscription_state_map_.end()) {
      // An subscription already exists for this browser.
      return false;
    }

    CefRefPtr<CefMediaRouter> media_router =
        browser->GetHost()->GetRequestContext()->GetMediaRouter(nullptr);

    SubscriptionState* state = new SubscriptionState();
    state->query_id = query_id;
    state->observer = new MediaObserver(media_router, callback);
    state->registration = media_router->AddObserver(state->observer);
    subscription_state_map_.insert(std::make_pair(browser_id, state));

    // Trigger sink and route callbacks.
    media_router->NotifyCurrentSinks();
    media_router->NotifyCurrentRoutes();

    return true;
  }

  void RemoveSubscription(int browser_id, int64 query_id) {
    SubscriptionStateMap::iterator it =
        subscription_state_map_.find(browser_id);
    if (it != subscription_state_map_.end() &&
        it->second->query_id == query_id) {
      delete it->second;
      subscription_state_map_.erase(it);
    }
  }

  CefRefPtr<MediaObserver> GetMediaObserver(int browser_id) {
    SubscriptionStateMap::const_iterator it =
        subscription_state_map_.find(browser_id);
    if (it != subscription_state_map_.end()) {
      return it->second->observer;
    }
    return NULL;
  }

  // Map of browser ID to SubscriptionState object.
  typedef std::map<int, SubscriptionState*> SubscriptionStateMap;
  SubscriptionStateMap subscription_state_map_;

  DISALLOW_COPY_AND_ASSIGN(Handler);
};

}  // namespace

void CreateMessageHandlers(test_runner::MessageHandlerSet& handlers) {
  handlers.insert(new Handler());
}

}  // namespace media_router_test
}  // namespace client
