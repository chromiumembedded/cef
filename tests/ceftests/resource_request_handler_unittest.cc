// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <algorithm>
#include <cmath>
#include <memory>
#include <sstream>
#include <string>

#include "include/base/cef_callback.h"
#include "include/cef_request_context_handler.h"
#include "include/cef_scheme.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_stream_resource_handler.h"
#include "tests/ceftests/routing_test_handler.h"
#include "tests/ceftests/test_handler.h"
#include "tests/ceftests/test_util.h"
#include "tests/gtest/include/gtest/gtest.h"

namespace {

// Normal stream resource handler implementation that additionally verifies
// calls to Cancel.
// This also tests the CefStreamResourceHandler implementation.
class NormalResourceHandler : public CefStreamResourceHandler {
 public:
  NormalResourceHandler(int status_code,
                        const CefString& status_text,
                        const CefString& mime_type,
                        CefResponse::HeaderMap header_map,
                        CefRefPtr<CefStreamReader> stream,
                        base::OnceClosure destroy_callback)
      : CefStreamResourceHandler(status_code,
                                 status_text,
                                 mime_type,
                                 header_map,
                                 stream),
        destroy_callback_(std::move(destroy_callback)) {}

  ~NormalResourceHandler() override {
    EXPECT_EQ(1, cancel_ct_);
    std::move(destroy_callback_).Run();
  }

  void Cancel() override {
    EXPECT_IO_THREAD();
    cancel_ct_++;
  }

 private:
  base::OnceClosure destroy_callback_;
  int cancel_ct_ = 0;
};

// Normal stream resource handler implementation that additionally continues
// using the callback object and verifies calls to Cancel.
class CallbackResourceHandler : public CefResourceHandler {
 public:
  enum Mode {
    DELAYED_OPEN,
    DELAYED_READ,
    IMMEDIATE_OPEN,
    IMMEDIATE_READ,
    DELAYED_ALL,
    IMMEDIATE_ALL,
  };

  bool IsDelayedOpen() const {
    return mode_ == DELAYED_OPEN || mode_ == DELAYED_ALL;
  }

  bool IsDelayedRead() const {
    return mode_ == DELAYED_READ || mode_ == DELAYED_ALL;
  }

  bool IsImmediateOpen() const {
    return mode_ == IMMEDIATE_OPEN || mode_ == IMMEDIATE_ALL;
  }

  bool IsImmediateRead() const {
    return mode_ == IMMEDIATE_READ || mode_ == IMMEDIATE_ALL;
  }

  CallbackResourceHandler(Mode mode,
                          int status_code,
                          const CefString& status_text,
                          const CefString& mime_type,
                          CefResponse::HeaderMap header_map,
                          CefRefPtr<CefStreamReader> stream,
                          base::OnceClosure destroy_callback)
      : mode_(mode),
        status_code_(status_code),
        status_text_(status_text),
        mime_type_(mime_type),
        header_map_(header_map),
        stream_(stream),
        destroy_callback_(std::move(destroy_callback)) {
    DCHECK(!mime_type_.empty());
    DCHECK(stream_.get());
  }

  ~CallbackResourceHandler() override {
    EXPECT_EQ(1, cancel_ct_);
    std::move(destroy_callback_).Run();
  }

  bool Open(CefRefPtr<CefRequest> request,
            bool& handle_request,
            CefRefPtr<CefCallback> callback) override {
    EXPECT_FALSE(CefCurrentlyOn(TID_UI) || CefCurrentlyOn(TID_IO));

    if (IsDelayedOpen()) {
      // Continue the request asynchronously by executing the callback.
      CefPostTask(TID_FILE_USER_VISIBLE,
                  base::BindOnce(&CefCallback::Continue, callback));
      handle_request = false;
      return true;
    } else if (IsImmediateOpen()) {
      // Continue the request immediately be executing the callback.
      callback->Continue();
      handle_request = false;
      return true;
    }

    // Continue the request immediately in the default manner.
    handle_request = true;
    return true;
  }

  void GetResponseHeaders(CefRefPtr<CefResponse> response,
                          int64& response_length,
                          CefString& redirectUrl) override {
    response->SetStatus(status_code_);
    response->SetStatusText(status_text_);
    response->SetMimeType(mime_type_);

    if (!header_map_.empty())
      response->SetHeaderMap(header_map_);

    response_length = -1;
  }

  bool Read(void* data_out,
            int bytes_to_read,
            int& bytes_read,
            CefRefPtr<CefResourceReadCallback> callback) override {
    EXPECT_FALSE(CefCurrentlyOn(TID_UI) || CefCurrentlyOn(TID_IO));
    EXPECT_GT(bytes_to_read, 0);

    bytes_read = 0;

    if (IsDelayedRead()) {
      // Continue the request asynchronously by executing the callback.
      CefPostTask(TID_FILE_USER_VISIBLE,
                  base::BindOnce(&CallbackResourceHandler::ContinueRead, this,
                                 data_out, bytes_to_read, callback));
      return true;
    } else if (IsImmediateRead()) {
      // Continue the request immediately be executing the callback.
      ContinueRead(data_out, bytes_to_read, callback);
      return true;
    }

    // Continue the request immediately in the default manner.
    return DoRead(data_out, bytes_to_read, bytes_read);
  }

  void Cancel() override {
    EXPECT_IO_THREAD();
    cancel_ct_++;
  }

 private:
  void ContinueRead(void* data_out,
                    int bytes_to_read,
                    CefRefPtr<CefResourceReadCallback> callback) {
    EXPECT_FALSE(CefCurrentlyOn(TID_UI) || CefCurrentlyOn(TID_IO));

    int bytes_read = 0;
    DoRead(data_out, bytes_to_read, bytes_read);
    callback->Continue(bytes_read);
  }

  bool DoRead(void* data_out, int bytes_to_read, int& bytes_read) {
    DCHECK_GT(bytes_to_read, 0);

    // Read until the buffer is full or until Read() returns 0 to indicate no
    // more data.
    bytes_read = 0;
    int read = 0;
    do {
      read = static_cast<int>(
          stream_->Read(static_cast<char*>(data_out) + bytes_read, 1,
                        bytes_to_read - bytes_read));
      bytes_read += read;
    } while (read != 0 && bytes_read < bytes_to_read);

    return (bytes_read > 0);
  }

  const Mode mode_;

  const int status_code_;
  const CefString status_text_;
  const CefString mime_type_;
  const CefResponse::HeaderMap header_map_;
  const CefRefPtr<CefStreamReader> stream_;

  base::OnceClosure destroy_callback_;
  int cancel_ct_ = 0;

  IMPLEMENT_REFCOUNTING(CallbackResourceHandler);
  DISALLOW_COPY_AND_ASSIGN(CallbackResourceHandler);
};

// Resource handler implementation that never completes. Used to test
// destruction handling behavior for in-progress requests.
class IncompleteResourceHandlerOld : public CefResourceHandler {
 public:
  enum TestMode {
    BLOCK_PROCESS_REQUEST,
    BLOCK_READ_RESPONSE,
  };

  IncompleteResourceHandlerOld(TestMode test_mode,
                               const std::string& mime_type,
                               base::OnceClosure destroy_callback)
      : test_mode_(test_mode),
        mime_type_(mime_type),
        destroy_callback_(std::move(destroy_callback)) {}

  ~IncompleteResourceHandlerOld() override {
    EXPECT_EQ(1, process_request_ct_);

    EXPECT_EQ(1, cancel_ct_);

    if (test_mode_ == BLOCK_READ_RESPONSE) {
      EXPECT_EQ(1, get_response_headers_ct_);
      EXPECT_EQ(1, read_response_ct_);
    } else {
      EXPECT_EQ(0, get_response_headers_ct_);
      EXPECT_EQ(0, read_response_ct_);
    }

    std::move(destroy_callback_).Run();
  }

  bool ProcessRequest(CefRefPtr<CefRequest> request,
                      CefRefPtr<CefCallback> callback) override {
    EXPECT_IO_THREAD();

    process_request_ct_++;

    if (test_mode_ == BLOCK_PROCESS_REQUEST) {
      // Never release or execute this callback.
      incomplete_callback_ = callback;
    } else {
      callback->Continue();
    }
    return true;
  }

  void GetResponseHeaders(CefRefPtr<CefResponse> response,
                          int64& response_length,
                          CefString& redirectUrl) override {
    EXPECT_IO_THREAD();
    EXPECT_EQ(test_mode_, BLOCK_READ_RESPONSE);

    get_response_headers_ct_++;

    response->SetStatus(200);
    response->SetStatusText("OK");
    response->SetMimeType(mime_type_);
    response_length = 100;
  }

  bool ReadResponse(void* data_out,
                    int bytes_to_read,
                    int& bytes_read,
                    CefRefPtr<CefCallback> callback) override {
    EXPECT_IO_THREAD();
    EXPECT_EQ(test_mode_, BLOCK_READ_RESPONSE);

    read_response_ct_++;

    // Never release or execute this callback.
    incomplete_callback_ = callback;
    bytes_read = 0;
    return true;
  }

  void Cancel() override {
    EXPECT_IO_THREAD();
    cancel_ct_++;
  }

 private:
  const TestMode test_mode_;
  const std::string mime_type_;
  base::OnceClosure destroy_callback_;

  int process_request_ct_ = 0;
  int get_response_headers_ct_ = 0;
  int read_response_ct_ = 0;
  int cancel_ct_ = 0;

  CefRefPtr<CefCallback> incomplete_callback_;

  IMPLEMENT_REFCOUNTING(IncompleteResourceHandlerOld);
  DISALLOW_COPY_AND_ASSIGN(IncompleteResourceHandlerOld);
};

class IncompleteResourceHandler : public CefResourceHandler {
 public:
  enum TestMode {
    BLOCK_OPEN,
    BLOCK_READ,
  };

  IncompleteResourceHandler(TestMode test_mode,
                            const std::string& mime_type,
                            base::OnceClosure destroy_callback)
      : test_mode_(test_mode),
        mime_type_(mime_type),
        destroy_callback_(std::move(destroy_callback)) {}

  ~IncompleteResourceHandler() override {
    EXPECT_EQ(1, open_ct_);

    EXPECT_EQ(1, cancel_ct_);

    if (test_mode_ == BLOCK_READ) {
      EXPECT_EQ(1, get_response_headers_ct_);
      EXPECT_EQ(1, read_ct_);
    } else {
      EXPECT_EQ(0, get_response_headers_ct_);
      EXPECT_EQ(0, read_ct_);
    }

    std::move(destroy_callback_).Run();
  }

  bool Open(CefRefPtr<CefRequest> request,
            bool& handle_request,
            CefRefPtr<CefCallback> callback) override {
    EXPECT_FALSE(CefCurrentlyOn(TID_UI) || CefCurrentlyOn(TID_IO));

    open_ct_++;

    if (test_mode_ == BLOCK_OPEN) {
      // Never release or execute this callback.
      incomplete_open_callback_ = callback;
    } else {
      // Continue immediately.
      handle_request = true;
    }
    return true;
  }

  bool ProcessRequest(CefRefPtr<CefRequest> request,
                      CefRefPtr<CefCallback> callback) override {
    EXPECT_TRUE(false);  // Not reached.
    return false;
  }

  void GetResponseHeaders(CefRefPtr<CefResponse> response,
                          int64& response_length,
                          CefString& redirectUrl) override {
    EXPECT_IO_THREAD();
    EXPECT_EQ(test_mode_, BLOCK_READ);

    get_response_headers_ct_++;

    response->SetStatus(200);
    response->SetStatusText("OK");
    response->SetMimeType(mime_type_);
    response_length = 100;
  }

  bool Read(void* data_out,
            int bytes_to_read,
            int& bytes_read,
            CefRefPtr<CefResourceReadCallback> callback) override {
    EXPECT_FALSE(CefCurrentlyOn(TID_UI) || CefCurrentlyOn(TID_IO));
    EXPECT_EQ(test_mode_, BLOCK_READ);

    read_ct_++;

    // Never release or execute this callback.
    incomplete_read_callback_ = callback;
    bytes_read = 0;
    return true;
  }

  bool ReadResponse(void* data_out,
                    int bytes_to_read,
                    int& bytes_read,
                    CefRefPtr<CefCallback> callback) override {
    EXPECT_TRUE(false);  // Not reached.
    bytes_read = -2;
    return false;
  }

  void Cancel() override {
    EXPECT_IO_THREAD();
    cancel_ct_++;
  }

 private:
  const TestMode test_mode_;
  const std::string mime_type_;
  base::OnceClosure destroy_callback_;

  int open_ct_ = 0;
  int get_response_headers_ct_ = 0;
  int read_ct_ = 0;
  int cancel_ct_ = 0;

  CefRefPtr<CefCallback> incomplete_open_callback_;
  CefRefPtr<CefResourceReadCallback> incomplete_read_callback_;

  IMPLEMENT_REFCOUNTING(IncompleteResourceHandler);
  DISALLOW_COPY_AND_ASSIGN(IncompleteResourceHandler);
};

class BasicResponseTest : public TestHandler {
 public:
  enum TestMode {
    // Normal load, nothing fancy.
    LOAD,

    // Close the browser in OnAfterCreated to verify destruction handling of
    // uninitialized requests.
    ABORT_AFTER_CREATED,

    // Close the browser in OnBeforeBrowse to verify destruction handling of
    // uninitialized requests.
    ABORT_BEFORE_BROWSE,

    // Don't continue from OnBeforeResourceLoad, then close the browser to
    // verify destruction handling of in-progress requests.
    INCOMPLETE_BEFORE_RESOURCE_LOAD,

    // Modify the request (add headers) in OnBeforeResourceLoad.
    MODIFY_BEFORE_RESOURCE_LOAD,

    // Redirect the request (change the URL) in OnBeforeResourceLoad.
    REDIRECT_BEFORE_RESOURCE_LOAD,

    // Return a CefResourceHandler from GetResourceHandler that continues
    // immediately by using the callback object instead of the return value.
    IMMEDIATE_REQUEST_HANDLER_OPEN,
    IMMEDIATE_REQUEST_HANDLER_READ,
    IMMEDIATE_REQUEST_HANDLER_ALL,

    // Return a CefResourceHandler from GetResourceHandler that continues with
    // a delay by using the callback object.
    DELAYED_REQUEST_HANDLER_OPEN,
    DELAYED_REQUEST_HANDLER_READ,
    DELAYED_REQUEST_HANDLER_ALL,

    // Return a CefResourceHandler from GetResourceHandler that never completes,
    // then close the browser to verify destruction handling of in-progress
    // requests.
    INCOMPLETE_REQUEST_HANDLER_OPEN,
    INCOMPLETE_REQUEST_HANDLER_READ,

    // Redirect the request using a CefResourceHandler returned from
    // GetResourceHandler.
    REDIRECT_REQUEST_HANDLER,

    // Redirect the request (change the URL) an additional time in
    // OnResourceRedirect after using a CefResourceHandler returned from
    // GetResourceHandler for the first redirect.
    REDIRECT_RESOURCE_REDIRECT,

    // Redirect the request (change the URL) in OnResourceResponse.
    REDIRECT_RESOURCE_RESPONSE,

    // Restart the request (add headers) in OnResourceResponse.
    RESTART_RESOURCE_RESPONSE,
  };

  // If |custom_scheme| is true all requests will use a custom scheme.
  // If |unhandled| is true the final request (after any redirects) will be
  // unhandled, meaning that default handling is disabled and GetResourceHandler
  // returns null.
  BasicResponseTest(TestMode mode, bool custom_scheme, bool unhandled)
      : mode_(mode), custom_scheme_(custom_scheme), unhandled_(unhandled) {}

  void RunTest() override {
    CreateBrowser(GetStartupURL());
    SetTestTimeout();
  }

  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    EXPECT_UI_THREAD();
    TestHandler::OnAfterCreated(browser);

    if (mode_ == ABORT_AFTER_CREATED) {
      SetSignalCompletionWhenAllBrowsersClose(false);
      CloseBrowser(browser, false);
    }
  }

  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
    EXPECT_UI_THREAD();
    TestHandler::OnBeforeClose(browser);

    if (IsAborted()) {
      DestroyTest();
    }
  }

  bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                      CefRefPtr<CefFrame> frame,
                      CefRefPtr<CefRequest> request,
                      bool user_gesture,
                      bool is_redirect) override {
    EXPECT_UI_THREAD();
    if (browser_id_ == 0) {
      // This is the first callback that provides a browser ID.
      browser_id_ = browser->GetIdentifier();
      EXPECT_GT(browser_id_, 0);
    } else {
      EXPECT_EQ(browser_id_, browser->GetIdentifier());
    }
    EXPECT_TRUE(frame->IsMain());

    if (IsChromeRuntimeEnabled()) {
      // With the Chrome runtime this is true on initial navigation via
      // chrome::AddTabAt() and also true for clicked links.
      EXPECT_TRUE(user_gesture);
    } else {
      EXPECT_FALSE(user_gesture);
    }
    if (on_before_browse_ct_ == 0 || mode_ == RESTART_RESOURCE_RESPONSE) {
      EXPECT_FALSE(is_redirect) << on_before_browse_ct_;
    } else {
      EXPECT_TRUE(is_redirect) << on_before_browse_ct_;
    }

    on_before_browse_ct_++;

    VerifyState(kOnBeforeBrowse, request, nullptr);

    if (mode_ == ABORT_BEFORE_BROWSE) {
      SetSignalCompletionWhenAllBrowsersClose(false);
      CloseBrowser(browser, false);
    }

    return false;
  }

  CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request,
      bool is_navigation,
      bool is_download,
      const CefString& request_initiator,
      bool& disable_default_handling) override {
    EXPECT_IO_THREAD();
    EXPECT_EQ(browser_id_, browser->GetIdentifier());
    EXPECT_TRUE(frame->IsMain());

    if (request_id_ == 0U) {
      // This is the first callback that provides a request ID.
      request_id_ = request->GetIdentifier();
      EXPECT_GT(request_id_, 0U);
    }

    VerifyState(kGetResourceRequestHandler, request, nullptr);

    EXPECT_TRUE(is_navigation);
    EXPECT_FALSE(is_download);
    EXPECT_STREQ("null", request_initiator.ToString().c_str());

    // Check expected default value.
    if (custom_scheme_) {
      // There is no default handling for custom schemes.
      EXPECT_TRUE(disable_default_handling);
    } else {
      EXPECT_FALSE(disable_default_handling);
      // If |unhandled_| is true then we don't want default handling of requests
      // (e.g. attempts to resolve over the network).
      disable_default_handling = unhandled_;
    }

    get_resource_request_handler_ct_++;

    return this;
  }

  CefRefPtr<CefCookieAccessFilter> GetCookieAccessFilter(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request) override {
    EXPECT_IO_THREAD();
    EXPECT_EQ(browser_id_, browser->GetIdentifier());
    EXPECT_TRUE(frame->IsMain());

    VerifyState(kGetCookieAccessFilter, request, nullptr);

    get_cookie_access_filter_ct_++;

    return nullptr;
  }

  cef_return_value_t OnBeforeResourceLoad(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request,
      CefRefPtr<CefCallback> callback) override {
    EXPECT_IO_THREAD();
    if (IsChromeRuntimeEnabled() && request->GetResourceType() == RT_FAVICON) {
      // Ignore favicon requests.
      return RV_CANCEL;
    }

    EXPECT_EQ(browser_id_, browser->GetIdentifier());
    EXPECT_TRUE(frame->IsMain());

    VerifyState(kOnBeforeResourceLoad, request, nullptr);

    on_before_resource_load_ct_++;

    if (mode_ == INCOMPLETE_BEFORE_RESOURCE_LOAD) {
      incomplete_callback_ = callback;

      // Close the browser asynchronously to complete the test.
      CloseBrowserAsync();
      return RV_CONTINUE_ASYNC;
    }

    if (mode_ == MODIFY_BEFORE_RESOURCE_LOAD) {
      // Expect this data in the request for future callbacks.
      SetCustomHeader(request);
    } else if (mode_ == REDIRECT_BEFORE_RESOURCE_LOAD) {
      // Redirect to this URL.
      request->SetURL(GetURL(RESULT_HTML));
    }

    // Other continuation modes are tested by
    // ResourceRequestHandlerTest.BeforeResourceLoad*.
    return RV_CONTINUE;
  }

  CefRefPtr<CefResourceHandler> GetResourceHandler(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request) override {
    EXPECT_IO_THREAD();
    EXPECT_EQ(browser_id_, browser->GetIdentifier());
    EXPECT_TRUE(frame->IsMain());

    VerifyState(kGetResourceHandler, request, nullptr);

    get_resource_handler_ct_++;

    if (IsIncompleteRequestHandler()) {
      // Close the browser asynchronously to complete the test.
      CloseBrowserAsync();
      return GetIncompleteResource();
    }

    const std::string& url = request->GetURL();
    if (url == GetURL(RESULT_HTML) && mode_ == RESTART_RESOURCE_RESPONSE) {
      if (get_resource_handler_ct_ == 1) {
        // First request that will be restarted after response.
        return GetOKResource();
      } else {
        // Restarted request.
        if (unhandled_)
          return nullptr;
        return GetOKResource();
      }
    } else if (url == GetURL(RESULT_HTML)) {
      if (unhandled_)
        return nullptr;
      return GetOKResource();
    } else if (url == GetURL(REDIRECT_HTML) &&
               mode_ == REDIRECT_RESOURCE_RESPONSE) {
      if (get_resource_handler_ct_ == 1) {
        // First request that will be redirected after response.
        return GetOKResource();
      } else {
        // Redirected request.
        if (unhandled_)
          return nullptr;
        return GetOKResource();
      }
    } else if (url == GetURL(REDIRECT_HTML) || url == GetURL(REDIRECT2_HTML)) {
      std::string redirect_url;
      if (mode_ == REDIRECT_REQUEST_HANDLER ||
          mode_ == REDIRECT_RESOURCE_RESPONSE) {
        EXPECT_STREQ(GetURL(REDIRECT_HTML), url.c_str());
        redirect_url = GetURL(RESULT_HTML);
      } else if (mode_ == REDIRECT_RESOURCE_REDIRECT) {
        EXPECT_STREQ(GetURL(REDIRECT2_HTML), url.c_str());
        redirect_url = GetURL(REDIRECT_HTML);
      } else {
        NOTREACHED();
      }

      return GetRedirectResource(redirect_url);
    } else {
      NOTREACHED();
      return nullptr;
    }
  }

  void OnResourceRedirect(CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefFrame> frame,
                          CefRefPtr<CefRequest> request,
                          CefRefPtr<CefResponse> response,
                          CefString& new_url) override {
    EXPECT_IO_THREAD();
    EXPECT_EQ(browser_id_, browser->GetIdentifier());
    EXPECT_TRUE(frame->IsMain());

    VerifyState(kOnResourceRedirect, request, response);

    if (mode_ == REDIRECT_REQUEST_HANDLER ||
        mode_ == REDIRECT_RESOURCE_RESPONSE) {
      // The URL redirected to from GetResourceHandler or OnResourceResponse.
      EXPECT_STREQ(GetURL(RESULT_HTML), new_url.ToString().c_str());
    } else if (mode_ == REDIRECT_RESOURCE_REDIRECT) {
      if (on_resource_redirect_ct_ == 0) {
        // The URL redirected to from GetResourceHandler.
        EXPECT_STREQ(GetURL(REDIRECT_HTML), new_url.ToString().c_str());
        // Redirect again.
        new_url = GetURL(RESULT_HTML);
      } else {
        NOTREACHED();
      }
    }

    on_resource_redirect_ct_++;
  }

  bool OnResourceResponse(CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefFrame> frame,
                          CefRefPtr<CefRequest> request,
                          CefRefPtr<CefResponse> response) override {
    EXPECT_IO_THREAD();
    EXPECT_EQ(browser_id_, browser->GetIdentifier());
    EXPECT_TRUE(frame->IsMain());

    VerifyState(kOnResourceResponse, request, response);

    on_resource_response_ct_++;

    if (on_resource_response_ct_ == 1) {
      if (mode_ == REDIRECT_RESOURCE_RESPONSE) {
        // Redirect the request to this URL.
        request->SetURL(GetURL(RESULT_HTML));
        return true;
      } else if (mode_ == RESTART_RESOURCE_RESPONSE) {
        // Restart the request loading this data.
        SetCustomHeader(request);
        return true;
      }
    }

    return false;
  }

  CefRefPtr<CefResponseFilter> GetResourceResponseFilter(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request,
      CefRefPtr<CefResponse> response) override {
    EXPECT_IO_THREAD();
    EXPECT_EQ(browser_id_, browser->GetIdentifier());
    EXPECT_TRUE(frame->IsMain());

    VerifyState(kGetResourceResponseFilter, request, response);

    get_resource_response_filter_ct_++;

    return nullptr;
  }

  void OnResourceLoadComplete(CefRefPtr<CefBrowser> browser,
                              CefRefPtr<CefFrame> frame,
                              CefRefPtr<CefRequest> request,
                              CefRefPtr<CefResponse> response,
                              URLRequestStatus status,
                              int64 received_content_length) override {
    EXPECT_IO_THREAD();

    if (IsChromeRuntimeEnabled() && request->GetResourceType() == RT_FAVICON) {
      // Ignore favicon requests.
      return;
    }

    EXPECT_EQ(browser_id_, browser->GetIdentifier());
    EXPECT_TRUE(frame->IsMain());

    VerifyState(kOnResourceLoadComplete, request, response);

    if (unhandled_ || IsIncomplete() || (IsAborted() && status == UR_FAILED)) {
      EXPECT_EQ(UR_FAILED, status);
      EXPECT_EQ(0, received_content_length);
    } else {
      EXPECT_EQ(UR_SUCCESS, status);
      EXPECT_EQ(static_cast<int64>(GetResponseBody().length()),
                received_content_length);
    }

    on_resource_load_complete_ct_++;

    if (IsIncomplete()) {
      MaybeDestroyTest(false);
    }
  }

  void OnProtocolExecution(CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefFrame> frame,
                           CefRefPtr<CefRequest> request,
                           bool& allow_os_execution) override {
    EXPECT_IO_THREAD();
    EXPECT_EQ(browser_id_, browser->GetIdentifier());
    EXPECT_TRUE(frame->IsMain());

    EXPECT_TRUE(custom_scheme_);
    EXPECT_TRUE(unhandled_);

    // Check expected default value.
    EXPECT_FALSE(allow_os_execution);

    VerifyState(kOnProtocolExecution, request, nullptr);
    on_protocol_execution_ct_++;
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    EXPECT_UI_THREAD();
    EXPECT_EQ(browser_id_, browser->GetIdentifier());
    EXPECT_TRUE(frame->IsMain());

    if (unhandled_)
      EXPECT_EQ(httpStatusCode, 0);
    else
      EXPECT_EQ(httpStatusCode, 200);

    on_load_end_ct_++;

    TestHandler::OnLoadEnd(browser, frame, httpStatusCode);
    DestroyTest();
  }

  void DestroyTest() override {
    if (mode_ == RESTART_RESOURCE_RESPONSE) {
      EXPECT_EQ(1, on_before_browse_ct_);
      EXPECT_EQ(2, get_resource_request_handler_ct_);
      EXPECT_EQ(2, get_cookie_access_filter_ct_);
      EXPECT_EQ(2, on_before_resource_load_ct_);
      EXPECT_EQ(2, get_resource_handler_ct_);
      EXPECT_EQ(0, on_resource_redirect_ct_);
      // Unhandled requests won't see a call to GetResourceResponseFilter or
      // OnResourceResponse. In this case we're restarting from inside
      // OnResourceResponse.
      if (unhandled_) {
        EXPECT_EQ(0, get_resource_response_filter_ct_);
        EXPECT_EQ(1, on_resource_response_ct_);
      } else {
        EXPECT_EQ(1, get_resource_response_filter_ct_);
        EXPECT_EQ(2, on_resource_response_ct_);
      }
    } else if (IsLoad()) {
      EXPECT_EQ(1, on_before_browse_ct_);
      EXPECT_EQ(1, get_resource_request_handler_ct_);
      EXPECT_EQ(1, get_cookie_access_filter_ct_);
      EXPECT_EQ(1, on_before_resource_load_ct_);
      EXPECT_EQ(1, get_resource_handler_ct_);
      EXPECT_EQ(0, on_resource_redirect_ct_);

      // Unhandled requests won't see a call to GetResourceResponseFilter
      // or OnResourceResponse.
      if (unhandled_) {
        EXPECT_EQ(0, get_resource_response_filter_ct_);
        EXPECT_EQ(0, on_resource_response_ct_);
      } else {
        EXPECT_EQ(1, get_resource_response_filter_ct_);
        EXPECT_EQ(1, on_resource_response_ct_);
      }
    } else if (IsRedirect()) {
      EXPECT_EQ(2, on_before_browse_ct_);
      EXPECT_EQ(2, get_resource_request_handler_ct_);
      EXPECT_EQ(2, get_cookie_access_filter_ct_);
      EXPECT_EQ(2, on_before_resource_load_ct_);
      if (mode_ == REDIRECT_BEFORE_RESOURCE_LOAD) {
        EXPECT_EQ(1, get_resource_handler_ct_);
      } else {
        EXPECT_EQ(2, get_resource_handler_ct_);
      }
      EXPECT_EQ(1, on_resource_redirect_ct_);

      // Unhandled requests won't see a call to GetResourceResponseFilter.
      if (unhandled_) {
        EXPECT_EQ(0, get_resource_response_filter_ct_);
      } else {
        EXPECT_EQ(1, get_resource_response_filter_ct_);
      }

      // Unhandled requests won't see a call to OnResourceResponse.
      if (mode_ == REDIRECT_RESOURCE_RESPONSE) {
        // In this case we're redirecting from inside OnResourceResponse.
        if (unhandled_) {
          EXPECT_EQ(1, on_resource_response_ct_);
        } else {
          EXPECT_EQ(2, on_resource_response_ct_);
        }
      } else {
        if (unhandled_) {
          EXPECT_EQ(0, on_resource_response_ct_);
        } else {
          EXPECT_EQ(1, on_resource_response_ct_);
        }
      }
    } else if (IsIncomplete()) {
      EXPECT_EQ(1, on_before_browse_ct_);
      EXPECT_EQ(1, get_resource_request_handler_ct_);
      EXPECT_EQ(1, get_cookie_access_filter_ct_);
      EXPECT_EQ(1, on_before_resource_load_ct_);

      if (IsIncompleteRequestHandler()) {
        EXPECT_EQ(1, get_resource_handler_ct_);
      } else {
        EXPECT_EQ(0, get_resource_handler_ct_);
      }

      EXPECT_EQ(0, on_resource_redirect_ct_);

      if (mode_ == INCOMPLETE_REQUEST_HANDLER_READ) {
        EXPECT_EQ(1, get_resource_response_filter_ct_);
        EXPECT_EQ(1, on_resource_response_ct_);
      } else {
        EXPECT_EQ(0, get_resource_response_filter_ct_);
        EXPECT_EQ(0, on_resource_response_ct_);
      }
    } else if (IsAborted()) {
      EXPECT_EQ(1, on_before_browse_ct_);
      // The callbacks executed may vary based on timing.
      EXPECT_NEAR(0, get_resource_request_handler_ct_, 1);
      EXPECT_NEAR(0, get_cookie_access_filter_ct_, 1);
      EXPECT_NEAR(0, on_before_resource_load_ct_, 1);
      EXPECT_NEAR(0, get_resource_handler_ct_, 1);
      EXPECT_NEAR(0, get_resource_response_filter_ct_, 1);
      EXPECT_NEAR(0, on_resource_response_ct_, 1);
      EXPECT_EQ(0, on_resource_redirect_ct_);
    } else {
      NOTREACHED();
    }

    if (IsAborted()) {
      // The callbacks executed may vary based on timing.
      EXPECT_NEAR(0, on_load_end_ct_, 1);
      EXPECT_NEAR(resource_handler_created_ct_, resource_handler_destroyed_ct_,
                  1);
      EXPECT_NEAR(0, on_resource_load_complete_ct_, 1);
    } else {
      EXPECT_EQ(resource_handler_created_ct_, resource_handler_destroyed_ct_);
      EXPECT_EQ(1, on_resource_load_complete_ct_);
    }

    if (IsIncomplete()) {
      EXPECT_EQ(0, on_load_end_ct_);
    } else if (!IsAborted()) {
      EXPECT_EQ(1, on_load_end_ct_);
    }

    if (custom_scheme_ && unhandled_ && !(IsIncomplete() || IsAborted())) {
      EXPECT_EQ(1, on_protocol_execution_ct_);
    } else if (IsAborted()) {
      // The callbacks executed may vary based on timing.
      EXPECT_NEAR(0, on_protocol_execution_ct_, 1);
    } else {
      EXPECT_EQ(0, on_protocol_execution_ct_);
    }

    TestHandler::DestroyTest();

    if (!SignalCompletionWhenAllBrowsersClose()) {
      // Complete asynchronously so the call stack has a chance to unwind.
      CefPostTask(TID_UI,
                  base::BindOnce(&BasicResponseTest::TestComplete, this));
    }
  }

 private:
  enum TestUrl {
    RESULT_HTML,
    REDIRECT_HTML,
    REDIRECT2_HTML,
  };

  const char* GetURL(TestUrl url) const {
    if (custom_scheme_) {
      if (url == RESULT_HTML)
        return "rrhcustom://test.com/result.html";
      if (url == REDIRECT_HTML)
        return "rrhcustom://test.com/redirect.html";
      if (url == REDIRECT2_HTML)
        return "rrhcustom://test.com/redirect2.html";
    } else {
      if (url == RESULT_HTML)
        return "http://test.com/result.html";
      if (url == REDIRECT_HTML)
        return "http://test.com/redirect.html";
      if (url == REDIRECT2_HTML)
        return "http://test.com/redirect2.html";
    }

    NOTREACHED();
    return "";
  }

  const char* GetStartupURL() const {
    if (IsLoad() || IsIncomplete() || IsAborted()) {
      return GetURL(RESULT_HTML);
    } else if (mode_ == REDIRECT_RESOURCE_REDIRECT) {
      return GetURL(REDIRECT2_HTML);
    } else if (IsRedirect()) {
      return GetURL(REDIRECT_HTML);
    }

    NOTREACHED();
    return "";
  }

  std::string GetResponseBody() const {
    return "<html><body>Response</body></html>";
  }
  std::string GetRedirectBody() const {
    return "<html><body>Redirect</body></html>";
  }

  base::OnceClosure GetResourceDestroyCallback() {
    resource_handler_created_ct_++;
    return base::BindOnce(&BasicResponseTest::MaybeDestroyTest, this, true);
  }

  bool GetCallbackResourceHandlerMode(CallbackResourceHandler::Mode& mode) {
    switch (mode_) {
      case IMMEDIATE_REQUEST_HANDLER_OPEN:
        mode = CallbackResourceHandler::IMMEDIATE_OPEN;
        return true;
      case IMMEDIATE_REQUEST_HANDLER_READ:
        mode = CallbackResourceHandler::IMMEDIATE_READ;
        return true;
      case IMMEDIATE_REQUEST_HANDLER_ALL:
        mode = CallbackResourceHandler::IMMEDIATE_ALL;
        return true;
      case DELAYED_REQUEST_HANDLER_OPEN:
        mode = CallbackResourceHandler::DELAYED_OPEN;
        return true;
      case DELAYED_REQUEST_HANDLER_READ:
        mode = CallbackResourceHandler::DELAYED_READ;
        return true;
      case DELAYED_REQUEST_HANDLER_ALL:
        mode = CallbackResourceHandler::DELAYED_ALL;
        return true;
      default:
        break;
    }
    return false;
  }

  CefRefPtr<CefResourceHandler> GetResource(int status_code,
                                            const CefString& status_text,
                                            const CefString& mime_type,
                                            CefResponse::HeaderMap header_map,
                                            const std::string& body) {
    CefRefPtr<CefStreamReader> stream = CefStreamReader::CreateForData(
        const_cast<char*>(body.c_str()), body.size());

    CallbackResourceHandler::Mode handler_mode;
    if (GetCallbackResourceHandlerMode(handler_mode)) {
      return new CallbackResourceHandler(handler_mode, status_code, status_text,
                                         mime_type, header_map, stream,
                                         GetResourceDestroyCallback());
    }

    return new NormalResourceHandler(status_code, status_text, mime_type,
                                     header_map, stream,
                                     GetResourceDestroyCallback());
  }

  CefRefPtr<CefResourceHandler> GetOKResource() {
    return GetResource(200, "OK", "text/html", CefResponse::HeaderMap(),
                       GetResponseBody());
  }

  CefRefPtr<CefResourceHandler> GetRedirectResource(
      const std::string& redirect_url) {
    CefResponse::HeaderMap headerMap;
    headerMap.insert(std::make_pair("Location", redirect_url));

    return GetResource(307, "Temporary Redirect", "text/html", headerMap,
                       GetRedirectBody());
  }

  CefRefPtr<CefResourceHandler> GetIncompleteResource() {
    if (TestOldResourceAPI()) {
      return new IncompleteResourceHandlerOld(
          mode_ == INCOMPLETE_REQUEST_HANDLER_OPEN
              ? IncompleteResourceHandlerOld::BLOCK_PROCESS_REQUEST
              : IncompleteResourceHandlerOld::BLOCK_READ_RESPONSE,
          "text/html", GetResourceDestroyCallback());
    }

    return new IncompleteResourceHandler(
        mode_ == INCOMPLETE_REQUEST_HANDLER_OPEN
            ? IncompleteResourceHandler::BLOCK_OPEN
            : IncompleteResourceHandler::BLOCK_READ,
        "text/html", GetResourceDestroyCallback());
  }

  bool IsLoad() const {
    return mode_ == LOAD || mode_ == MODIFY_BEFORE_RESOURCE_LOAD ||
           mode_ == RESTART_RESOURCE_RESPONSE ||
           mode_ == IMMEDIATE_REQUEST_HANDLER_OPEN ||
           mode_ == IMMEDIATE_REQUEST_HANDLER_READ ||
           mode_ == IMMEDIATE_REQUEST_HANDLER_ALL ||
           mode_ == DELAYED_REQUEST_HANDLER_OPEN ||
           mode_ == DELAYED_REQUEST_HANDLER_READ ||
           mode_ == DELAYED_REQUEST_HANDLER_ALL;
  }

  bool IsIncompleteRequestHandler() const {
    return mode_ == INCOMPLETE_REQUEST_HANDLER_OPEN ||
           mode_ == INCOMPLETE_REQUEST_HANDLER_READ;
  }

  bool IsIncomplete() const {
    return mode_ == INCOMPLETE_BEFORE_RESOURCE_LOAD ||
           IsIncompleteRequestHandler();
  }

  bool IsAborted() const {
    return mode_ == ABORT_AFTER_CREATED || mode_ == ABORT_BEFORE_BROWSE;
  }

  bool IsRedirect() const {
    return mode_ == REDIRECT_BEFORE_RESOURCE_LOAD ||
           mode_ == REDIRECT_REQUEST_HANDLER ||
           mode_ == REDIRECT_RESOURCE_REDIRECT ||
           mode_ == REDIRECT_RESOURCE_RESPONSE;
  }

  static void SetCustomHeader(CefRefPtr<CefRequest> request) {
    EXPECT_FALSE(request->IsReadOnly());
    request->SetHeaderByName("X-Custom-Header", "value", false);
  }

  static std::string GetCustomHeader(CefRefPtr<CefRequest> request) {
    return request->GetHeaderByName("X-Custom-Header");
  }

  // Resource-related callbacks.
  enum Callback {
    kOnBeforeBrowse,
    kGetResourceRequestHandler,
    kGetCookieAccessFilter,
    kOnBeforeResourceLoad,
    kGetResourceHandler,
    kOnResourceRedirect,
    kOnResourceResponse,
    kGetResourceResponseFilter,
    kOnResourceLoadComplete,
    kOnProtocolExecution,
  };

  bool ShouldHaveResponse(Callback callback) const {
    return callback >= kOnResourceRedirect &&
           callback <= kOnResourceLoadComplete;
  }

  bool ShouldHaveWritableRequest(Callback callback) const {
    return callback == kOnBeforeResourceLoad || callback == kOnResourceResponse;
  }

  void VerifyState(Callback callback,
                   CefRefPtr<CefRequest> request,
                   CefRefPtr<CefResponse> response) const {
    EXPECT_TRUE(request) << callback;

    if (ShouldHaveResponse(callback)) {
      EXPECT_TRUE(response) << callback;
      EXPECT_TRUE(response->IsReadOnly()) << callback;
    } else {
      EXPECT_FALSE(response) << callback;
    }

    if (ShouldHaveWritableRequest(callback)) {
      EXPECT_FALSE(request->IsReadOnly()) << callback;
    } else {
      EXPECT_TRUE(request->IsReadOnly()) << callback;
    }

    if (callback == kOnBeforeBrowse) {
      // Browser-side navigation no longer exposes the actual request
      // information.
      EXPECT_EQ(0U, request->GetIdentifier()) << callback;
    } else {
      // All resource-related callbacks share the same request ID.
      EXPECT_EQ(request_id_, request->GetIdentifier()) << callback;
    }

    if (IsLoad() || IsIncomplete() || IsAborted()) {
      EXPECT_STREQ("GET", request->GetMethod().ToString().c_str()) << callback;
      EXPECT_STREQ(GetURL(RESULT_HTML), request->GetURL().ToString().c_str())
          << callback;

      // Expect the header for all callbacks following the callback that
      // initially sets it.
      const std::string& custom_header = GetCustomHeader(request);
      if ((mode_ == RESTART_RESOURCE_RESPONSE &&
           on_resource_response_ct_ > 0) ||
          (mode_ == MODIFY_BEFORE_RESOURCE_LOAD &&
           on_before_resource_load_ct_ > 0)) {
        EXPECT_STREQ("value", custom_header.c_str()) << callback;
      } else {
        EXPECT_STREQ("", custom_header.c_str()) << callback;
      }

      if (response)
        VerifyOKResponse(callback, response);
    } else if (IsRedirect()) {
      EXPECT_STREQ("GET", request->GetMethod().ToString().c_str()) << callback;
      if (on_before_browse_ct_ == 1) {
        // Before the redirect.
        EXPECT_STREQ(GetStartupURL(), request->GetURL().ToString().c_str())
            << callback;
      } else if (on_before_browse_ct_ == 2) {
        // After the redirect.
        EXPECT_STREQ(GetURL(RESULT_HTML), request->GetURL().ToString().c_str())
            << callback;
      } else {
        NOTREACHED() << callback;
      }

      if (response) {
        if (callback == kOnResourceRedirect) {
          // Before the redirect.
          VerifyRedirectResponse(callback, response);
        } else {
          // After the redirect.
          VerifyOKResponse(callback, response);
        }
      }
    } else {
      NOTREACHED() << callback;
    }
  }

  void VerifyOKResponse(Callback callback,
                        CefRefPtr<CefResponse> response) const {
    const auto error_code = response->GetError();

    // True for the first response in cases where we're redirecting/restarting
    // from inside OnResourceResponse (e.g. the first response always succeeds).
    const bool override_unhandled = unhandled_ &&
                                    (mode_ == REDIRECT_RESOURCE_RESPONSE ||
                                     mode_ == RESTART_RESOURCE_RESPONSE) &&
                                    get_resource_handler_ct_ == 1;

    // True for tests where the request will be incomplete and never receive a
    // response.
    const bool incomplete_unhandled =
        (mode_ == INCOMPLETE_BEFORE_RESOURCE_LOAD ||
         mode_ == INCOMPLETE_REQUEST_HANDLER_OPEN ||
         (IsAborted() && !custom_scheme_ && error_code != ERR_NONE));

    if ((unhandled_ && !override_unhandled) || incomplete_unhandled) {
      EXPECT_TRUE(ERR_ABORTED == error_code ||
                  ERR_UNKNOWN_URL_SCHEME == error_code)
          << callback << error_code;
      EXPECT_EQ(0, response->GetStatus()) << callback;
      EXPECT_STREQ("", response->GetStatusText().ToString().c_str())
          << callback;
      EXPECT_STREQ("", response->GetURL().ToString().c_str()) << callback;
      EXPECT_STREQ("", response->GetMimeType().ToString().c_str()) << callback;
      EXPECT_STREQ("", response->GetCharset().ToString().c_str()) << callback;
    } else {
      if ((mode_ == INCOMPLETE_REQUEST_HANDLER_READ || IsAborted()) &&
          callback == kOnResourceLoadComplete &&
          response->GetError() != ERR_NONE) {
        // We got a response, but we also got aborted.
        EXPECT_EQ(ERR_ABORTED, response->GetError()) << callback;
      } else {
        EXPECT_EQ(ERR_NONE, response->GetError()) << callback;
      }
      EXPECT_EQ(200, response->GetStatus()) << callback;
      EXPECT_STREQ("OK", response->GetStatusText().ToString().c_str())
          << callback;
      EXPECT_STREQ("", response->GetURL().ToString().c_str()) << callback;
      EXPECT_STREQ("text/html", response->GetMimeType().ToString().c_str())
          << callback;
      EXPECT_STREQ("", response->GetCharset().ToString().c_str()) << callback;
    }
  }

  void VerifyRedirectResponse(Callback callback,
                              CefRefPtr<CefResponse> response) const {
    EXPECT_EQ(ERR_NONE, response->GetError()) << callback;
    EXPECT_EQ(307, response->GetStatus()) << callback;
    const std::string& status_text = response->GetStatusText();
    EXPECT_TRUE(status_text == "Internal Redirect" ||
                status_text == "Temporary Redirect")
        << status_text << callback;
    EXPECT_STREQ("", response->GetURL().ToString().c_str()) << callback;
    EXPECT_STREQ("", response->GetMimeType().ToString().c_str()) << callback;
    EXPECT_STREQ("", response->GetCharset().ToString().c_str()) << callback;
  }

  void CloseBrowserAsync() {
    EXPECT_TRUE(IsIncomplete());
    SetSignalCompletionWhenAllBrowsersClose(false);
    CefPostDelayedTask(
        TID_UI, base::BindOnce(&TestHandler::CloseBrowser, GetBrowser(), false),
        100);
  }

  void MaybeDestroyTest(bool from_handler) {
    if (!CefCurrentlyOn(TID_UI)) {
      CefPostTask(TID_UI, base::BindOnce(&BasicResponseTest::MaybeDestroyTest,
                                         this, from_handler));
      return;
    }

    if (from_handler) {
      resource_handler_destroyed_ct_++;
    }

    bool destroy_test = false;
    if (IsIncomplete()) {
      // Destroy the test if we got OnResourceLoadComplete and either the
      // resource handler will never complete or it was destroyed.
      destroy_test =
          on_resource_load_complete_ct_ > 0 &&
          (!IsIncompleteRequestHandler() ||
           resource_handler_destroyed_ct_ == resource_handler_created_ct_);
    } else {
      // Destroy the test if we got OnLoadEnd and the expected number of
      // resource handlers were destroyed.
      destroy_test = on_load_end_ct_ > 0 && resource_handler_destroyed_ct_ ==
                                                resource_handler_created_ct_;
    }

    if (destroy_test) {
      DestroyTest();
    }
  }

  const TestMode mode_;
  const bool custom_scheme_;
  const bool unhandled_;

  int browser_id_ = 0;
  uint64 request_id_ = 0U;

  int resource_handler_created_ct_ = 0;

  int on_before_browse_ct_ = 0;
  int on_load_end_ct_ = 0;

  int get_resource_request_handler_ct_ = 0;
  int on_before_resource_load_ct_ = 0;
  int get_cookie_access_filter_ct_ = 0;
  int get_resource_handler_ct_ = 0;
  int on_resource_redirect_ct_ = 0;
  int on_resource_response_ct_ = 0;
  int get_resource_response_filter_ct_ = 0;
  int on_resource_load_complete_ct_ = 0;
  int on_protocol_execution_ct_ = 0;
  int resource_handler_destroyed_ct_ = 0;

  // Used with INCOMPLETE_BEFORE_RESOURCE_LOAD.
  CefRefPtr<CefCallback> incomplete_callback_;

  DISALLOW_COPY_AND_ASSIGN(BasicResponseTest);
  IMPLEMENT_REFCOUNTING(BasicResponseTest);
};

}  // namespace

#define BASIC_TEST(name, test_mode, custom, unhandled)            \
  TEST(ResourceRequestHandlerTest, Basic##name) {                 \
    CefRefPtr<BasicResponseTest> handler = new BasicResponseTest( \
        BasicResponseTest::test_mode, custom, unhandled);         \
    handler->ExecuteTest();                                       \
    ReleaseAndWaitForDestructor(handler);                         \
  }

#define BASIC_TEST_ALL_MODES(name, custom, unhandled)                          \
  BASIC_TEST(name##Load, LOAD, custom, unhandled)                              \
  BASIC_TEST(name##AbortAfterCreated, ABORT_AFTER_CREATED, custom, unhandled)  \
  BASIC_TEST(name##AbortBeforeBrowse, ABORT_BEFORE_BROWSE, custom, unhandled)  \
  BASIC_TEST(name##ModifyBeforeResourceLoad, MODIFY_BEFORE_RESOURCE_LOAD,      \
             custom, unhandled)                                                \
  BASIC_TEST(name##RedirectBeforeResourceLoad, REDIRECT_BEFORE_RESOURCE_LOAD,  \
             custom, unhandled)                                                \
  BASIC_TEST(name##RedirectRequestHandler, REDIRECT_REQUEST_HANDLER, custom,   \
             unhandled)                                                        \
  BASIC_TEST(name##RedirectResourceRedirect, REDIRECT_RESOURCE_REDIRECT,       \
             custom, unhandled)                                                \
  BASIC_TEST(name##RedirectResourceResponse, REDIRECT_RESOURCE_RESPONSE,       \
             custom, unhandled)                                                \
  BASIC_TEST(name##RestartResourceResponse, RESTART_RESOURCE_RESPONSE, custom, \
             unhandled)

// Tests only supported in handled mode.
#define BASIC_TEST_HANDLED_MODES(name, custom)                                \
  BASIC_TEST(name##ImmediateRequestHandlerOpen,                               \
             IMMEDIATE_REQUEST_HANDLER_OPEN, custom, false)                   \
  BASIC_TEST(name##ImmediateRequestHandlerRead,                               \
             IMMEDIATE_REQUEST_HANDLER_READ, custom, false)                   \
  BASIC_TEST(name##ImmediateRequestHandlerAll, IMMEDIATE_REQUEST_HANDLER_ALL, \
             custom, false)                                                   \
  BASIC_TEST(name##DelayedRequestHandlerOpen, DELAYED_REQUEST_HANDLER_OPEN,   \
             custom, false)                                                   \
  BASIC_TEST(name##DelayedRequestHandlerRead, DELAYED_REQUEST_HANDLER_READ,   \
             custom, false)                                                   \
  BASIC_TEST(name##DelayedRequestHandlerAll, DELAYED_REQUEST_HANDLER_ALL,     \
             custom, false)                                                   \
  BASIC_TEST(name##IncompleteBeforeResourceLoad,                              \
             INCOMPLETE_BEFORE_RESOURCE_LOAD, custom, false)                  \
  BASIC_TEST(name##IncompleteRequestHandlerOpen,                              \
             INCOMPLETE_REQUEST_HANDLER_OPEN, custom, false)                  \
  BASIC_TEST(name##IncompleteRequestHandlerRead,                              \
             INCOMPLETE_REQUEST_HANDLER_READ, custom, false)

BASIC_TEST_ALL_MODES(StandardHandled, false, false)
BASIC_TEST_ALL_MODES(StandardUnhandled, false, true)
BASIC_TEST_ALL_MODES(CustomHandled, true, false)
BASIC_TEST_ALL_MODES(CustomUnhandled, true, true)

BASIC_TEST_HANDLED_MODES(StandardHandled, false)
BASIC_TEST_HANDLED_MODES(CustomHandled, true)

namespace {

const char kSubresourceProcessMsg[] = "SubresourceMsg";

class SubresourceResponseTest : public RoutingTestHandler {
 public:
  enum TestMode {
    // Normal load, nothing fancy.
    LOAD,

    // Don't continue from OnBeforeResourceLoad, then close the browser to
    // verify destruction handling of in-progress requests.
    INCOMPLETE_BEFORE_RESOURCE_LOAD,

    // Modify the request (add headers) in OnBeforeResourceLoad.
    MODIFY_BEFORE_RESOURCE_LOAD,

    // Redirect the request (change the URL) in OnBeforeResourceLoad.
    REDIRECT_BEFORE_RESOURCE_LOAD,

    // Return a CefResourceHandler from GetResourceHandler that continues
    // immediately by using the callback object instead of the return value.
    IMMEDIATE_REQUEST_HANDLER_OPEN,
    IMMEDIATE_REQUEST_HANDLER_READ,
    IMMEDIATE_REQUEST_HANDLER_ALL,

    // Return a CefResourceHandler from GetResourceHandler that continues with
    // a delay by using the callback object.
    DELAYED_REQUEST_HANDLER_OPEN,
    DELAYED_REQUEST_HANDLER_READ,
    DELAYED_REQUEST_HANDLER_ALL,

    // Return a CefResourceHandler from GetResourceHandler that never completes,
    // then close the browser to verify destruction handling of in-progress
    // requests.
    INCOMPLETE_REQUEST_HANDLER_OPEN,
    INCOMPLETE_REQUEST_HANDLER_READ,

    // Redirect the request using a CefResourceHandler returned from
    // GetResourceHandler.
    REDIRECT_REQUEST_HANDLER,

    // Redirect the request (change the URL) an additional time in
    // OnResourceRedirect after using a CefResourceHandler returned from
    // GetResourceHandler for the first redirect.
    REDIRECT_RESOURCE_REDIRECT,

    // Redirect the request (change the URL) in OnResourceResponse.
    REDIRECT_RESOURCE_RESPONSE,

    // Restart the request (add headers) in OnResourceResponse.
    RESTART_RESOURCE_RESPONSE,
  };

  // If |custom_scheme| is true all requests will use a custom scheme.
  // If |unhandled| is true the final request (after any redirects) will be
  // unhandled, meaning that default handling is disabled and GetResourceHandler
  // returns null.
  // If |subframe| is true the resource will be loaded in an iframe.
  SubresourceResponseTest(TestMode mode,
                          bool custom_scheme,
                          bool unhandled,
                          bool subframe)
      : mode_(mode),
        custom_scheme_(custom_scheme),
        unhandled_(unhandled),
        subframe_(subframe) {}

  void RunTest() override {
    CreateBrowser(GetMainURL());
    SetTestTimeout();
  }

  bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                      CefRefPtr<CefFrame> frame,
                      CefRefPtr<CefRequest> request,
                      bool user_gesture,
                      bool is_redirect) override {
    EXPECT_UI_THREAD();
    if (browser_id_ == 0) {
      // This is the first callback that provides a browser ID.
      browser_id_ = browser->GetIdentifier();
      EXPECT_GT(browser_id_, 0);
    } else {
      EXPECT_EQ(browser_id_, browser->GetIdentifier());
    }

    const std::string& url = request->GetURL();
    if (IsMainURL(url)) {
      EXPECT_TRUE(frame->IsMain());
    } else if (IsSubURL(url)) {
      EXPECT_FALSE(frame->IsMain());
      EXPECT_TRUE(subframe_);
    } else {
      EXPECT_FALSE(true);  // Not reached.
    }

    if (IsChromeRuntimeEnabled() && IsMainURL(url)) {
      // With the Chrome runtime this is true on initial navigation via
      // chrome::AddTabAt() and also true for clicked links.
      EXPECT_TRUE(user_gesture);
    } else {
      EXPECT_FALSE(user_gesture);
    }

    EXPECT_FALSE(is_redirect);

    on_before_browse_ct_++;
    return false;
  }

  CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request,
      bool is_navigation,
      bool is_download,
      const CefString& request_initiator,
      bool& disable_default_handling) override {
    EXPECT_IO_THREAD();
    EXPECT_EQ(browser_id_, browser->GetIdentifier());

    const std::string& url = request->GetURL();
    if (IgnoreURL(url))
      return nullptr;

    const bool is_main_url = IsMainURL(url);
    const bool is_sub_url = IsSubURL(url);

    if (is_main_url) {
      EXPECT_TRUE(frame->IsMain());
    } else if (is_sub_url) {
      EXPECT_FALSE(frame->IsMain());
      EXPECT_TRUE(subframe_);
    }

    if (is_main_url || is_sub_url) {
      // Track the frame ID that we'll expect for resource callbacks.
      // Do this here instead of OnBeforeBrowse because OnBeforeBrowse may
      // return -4 (kInvalidFrameId) for the initial navigation.
      if (frame_id_ == 0) {
        if (subframe_) {
          if (is_sub_url)
            frame_id_ = frame->GetIdentifier();
        } else {
          frame_id_ = frame->GetIdentifier();
        }
      }
      return this;
    }

    VerifyFrame(kGetResourceRequestHandler, frame);

    if (request_id_ == 0U) {
      // This is the first callback that provides a request ID.
      request_id_ = request->GetIdentifier();
      EXPECT_GT(request_id_, 0U);
    }

    VerifyState(kGetResourceRequestHandler, request, nullptr);

    EXPECT_FALSE(is_navigation);
    EXPECT_FALSE(is_download);
    EXPECT_STREQ(GetOrigin(), request_initiator.ToString().c_str());

    // Check expected default value.
    if (custom_scheme_) {
      // There is no default handling for custom schemes.
      EXPECT_TRUE(disable_default_handling);
    } else {
      EXPECT_FALSE(disable_default_handling);
      // If |unhandled_| is true then we don't want default handling of requests
      // (e.g. attempts to resolve over the network).
      disable_default_handling = unhandled_;
    }

    get_resource_request_handler_ct_++;

    return this;
  }

  CefRefPtr<CefCookieAccessFilter> GetCookieAccessFilter(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request) override {
    EXPECT_IO_THREAD();
    EXPECT_EQ(browser_id_, browser->GetIdentifier());

    const std::string& url = request->GetURL();
    if (IgnoreURL(url))
      return nullptr;

    if (IsMainURL(url)) {
      EXPECT_TRUE(frame->IsMain());
      return nullptr;
    } else if (IsSubURL(url)) {
      EXPECT_FALSE(frame->IsMain());
      EXPECT_TRUE(subframe_);
      return nullptr;
    }

    VerifyFrame(kGetCookieAccessFilter, frame);

    VerifyState(kGetCookieAccessFilter, request, nullptr);

    get_cookie_access_filter_ct_++;

    return nullptr;
  }

  cef_return_value_t OnBeforeResourceLoad(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request,
      CefRefPtr<CefCallback> callback) override {
    EXPECT_IO_THREAD();

    if (IsChromeRuntimeEnabled() && request->GetResourceType() == RT_FAVICON) {
      // Ignore favicon requests.
      return RV_CANCEL;
    }

    EXPECT_EQ(browser_id_, browser->GetIdentifier());

    if (IsMainURL(request->GetURL())) {
      EXPECT_TRUE(frame->IsMain());
      return RV_CONTINUE;
    } else if (IsSubURL(request->GetURL())) {
      EXPECT_FALSE(frame->IsMain());
      EXPECT_TRUE(subframe_);
      return RV_CONTINUE;
    }

    VerifyFrame(kOnBeforeResourceLoad, frame);

    VerifyState(kOnBeforeResourceLoad, request, nullptr);

    on_before_resource_load_ct_++;

    if (mode_ == INCOMPLETE_BEFORE_RESOURCE_LOAD) {
      incomplete_callback_ = callback;

      // Close the browser asynchronously to complete the test.
      CloseBrowserAsync();
      return RV_CONTINUE_ASYNC;
    }

    if (mode_ == MODIFY_BEFORE_RESOURCE_LOAD) {
      // Expect this data in the request for future callbacks.
      SetCustomHeader(request);
    } else if (mode_ == REDIRECT_BEFORE_RESOURCE_LOAD) {
      // Redirect to this URL.
      request->SetURL(GetURL(RESULT_JS));
    }

    // Other continuation modes are tested by
    // ResourceRequestHandlerTest.BeforeResourceLoad*.
    return RV_CONTINUE;
  }

  CefRefPtr<CefResourceHandler> GetResourceHandler(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request) override {
    EXPECT_IO_THREAD();
    EXPECT_EQ(browser_id_, browser->GetIdentifier());

    if (IsMainURL(request->GetURL())) {
      EXPECT_TRUE(frame->IsMain());
      return GetMainResource();
    } else if (IsSubURL(request->GetURL())) {
      EXPECT_FALSE(frame->IsMain());
      EXPECT_TRUE(subframe_);
      return GetSubResource();
    }

    VerifyFrame(kGetResourceHandler, frame);

    VerifyState(kGetResourceHandler, request, nullptr);

    get_resource_handler_ct_++;

    if (IsIncompleteRequestHandler()) {
      // Close the browser asynchronously to complete the test.
      CloseBrowserAsync();
      return GetIncompleteResource();
    }

    const std::string& url = request->GetURL();
    if (url == GetURL(RESULT_JS) && mode_ == RESTART_RESOURCE_RESPONSE) {
      if (get_resource_handler_ct_ == 1) {
        // First request that will be restarted after response.
        return GetOKResource();
      } else {
        // Restarted request.
        if (unhandled_)
          return nullptr;
        return GetOKResource();
      }
    } else if (url == GetURL(RESULT_JS)) {
      if (unhandled_)
        return nullptr;
      return GetOKResource();
    } else if (url == GetURL(REDIRECT_JS) &&
               mode_ == REDIRECT_RESOURCE_RESPONSE) {
      if (get_resource_handler_ct_ == 1) {
        // First request that will be redirected after response.
        return GetOKResource();
      } else {
        // Redirected request.
        if (unhandled_)
          return nullptr;
        return GetOKResource();
      }
    } else if (url == GetURL(REDIRECT_JS) || url == GetURL(REDIRECT2_JS)) {
      std::string redirect_url;
      if (mode_ == REDIRECT_REQUEST_HANDLER ||
          mode_ == REDIRECT_RESOURCE_RESPONSE) {
        EXPECT_STREQ(GetURL(REDIRECT_JS), url.c_str());
        redirect_url = GetURL(RESULT_JS);
      } else if (mode_ == REDIRECT_RESOURCE_REDIRECT) {
        EXPECT_STREQ(GetURL(REDIRECT2_JS), url.c_str());
        redirect_url = GetURL(REDIRECT_JS);
      } else {
        NOTREACHED();
      }

      return GetRedirectResource(redirect_url);
    } else {
      NOTREACHED();
      return nullptr;
    }
  }

  void OnResourceRedirect(CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefFrame> frame,
                          CefRefPtr<CefRequest> request,
                          CefRefPtr<CefResponse> response,
                          CefString& new_url) override {
    EXPECT_IO_THREAD();
    EXPECT_EQ(browser_id_, browser->GetIdentifier());

    if (IsMainURL(request->GetURL()) || IsSubURL(request->GetURL())) {
      EXPECT_FALSE(true);  // Not reached.
      return;
    }

    VerifyFrame(kOnResourceRedirect, frame);

    VerifyState(kOnResourceRedirect, request, response);

    if (mode_ == REDIRECT_REQUEST_HANDLER ||
        mode_ == REDIRECT_RESOURCE_RESPONSE) {
      // The URL redirected to from GetResourceHandler or OnResourceResponse.
      EXPECT_STREQ(GetURL(RESULT_JS), new_url.ToString().c_str());
    } else if (mode_ == REDIRECT_RESOURCE_REDIRECT) {
      if (on_resource_redirect_ct_ == 0) {
        // The URL redirected to from GetResourceHandler.
        EXPECT_STREQ(GetURL(REDIRECT_JS), new_url.ToString().c_str());
        // Redirect again.
        new_url = GetURL(RESULT_JS);
      } else {
        NOTREACHED();
      }
    }

    on_resource_redirect_ct_++;
  }

  bool OnResourceResponse(CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefFrame> frame,
                          CefRefPtr<CefRequest> request,
                          CefRefPtr<CefResponse> response) override {
    EXPECT_IO_THREAD();
    EXPECT_EQ(browser_id_, browser->GetIdentifier());

    if (IsMainURL(request->GetURL())) {
      EXPECT_TRUE(frame->IsMain());
      return false;
    } else if (IsSubURL(request->GetURL())) {
      EXPECT_FALSE(frame->IsMain());
      EXPECT_TRUE(subframe_);
      return false;
    }

    VerifyFrame(kOnResourceResponse, frame);

    VerifyState(kOnResourceResponse, request, response);

    on_resource_response_ct_++;

    if (on_resource_response_ct_ == 1) {
      if (mode_ == REDIRECT_RESOURCE_RESPONSE) {
        // Redirect the request to this URL.
        request->SetURL(GetURL(RESULT_JS));
        return true;
      } else if (mode_ == RESTART_RESOURCE_RESPONSE) {
        // Restart the request loading this data.
        SetCustomHeader(request);
        return true;
      }
    }

    return false;
  }

  CefRefPtr<CefResponseFilter> GetResourceResponseFilter(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request,
      CefRefPtr<CefResponse> response) override {
    EXPECT_IO_THREAD();
    EXPECT_EQ(browser_id_, browser->GetIdentifier());

    if (IsMainURL(request->GetURL())) {
      EXPECT_TRUE(frame->IsMain());
      return nullptr;
    } else if (IsSubURL(request->GetURL())) {
      EXPECT_FALSE(frame->IsMain());
      EXPECT_TRUE(subframe_);
      return nullptr;
    }

    VerifyFrame(kGetResourceResponseFilter, frame);

    VerifyState(kGetResourceResponseFilter, request, response);

    get_resource_response_filter_ct_++;

    return nullptr;
  }

  void OnResourceLoadComplete(CefRefPtr<CefBrowser> browser,
                              CefRefPtr<CefFrame> frame,
                              CefRefPtr<CefRequest> request,
                              CefRefPtr<CefResponse> response,
                              URLRequestStatus status,
                              int64 received_content_length) override {
    EXPECT_IO_THREAD();

    if (IsChromeRuntimeEnabled() && request->GetResourceType() == RT_FAVICON) {
      // Ignore favicon requests.
      return;
    }

    EXPECT_EQ(browser_id_, browser->GetIdentifier());

    if (IsMainURL(request->GetURL())) {
      EXPECT_TRUE(frame->IsMain());
      EXPECT_EQ(UR_SUCCESS, status);
      EXPECT_EQ(static_cast<int64>(GetMainResponseBody().length()),
                received_content_length);
      return;
    } else if (IsSubURL(request->GetURL())) {
      EXPECT_FALSE(frame->IsMain());
      EXPECT_EQ(UR_SUCCESS, status);
      EXPECT_EQ(static_cast<int64>(GetSubResponseBody().length()),
                received_content_length);
      EXPECT_TRUE(subframe_);
      return;
    }

    VerifyFrame(kOnResourceLoadComplete, frame);

    VerifyState(kOnResourceLoadComplete, request, response);

    if (unhandled_ || IsIncomplete()) {
      EXPECT_EQ(UR_FAILED, status);
      EXPECT_EQ(0, received_content_length);
    } else {
      EXPECT_EQ(UR_SUCCESS, status);
      EXPECT_EQ(static_cast<int64>(GetResponseBody().length()),
                received_content_length);
    }

    on_resource_load_complete_ct_++;

    if (IsIncomplete()) {
      MaybeDestroyTest(false);
    }
  }

  void OnProtocolExecution(CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefFrame> frame,
                           CefRefPtr<CefRequest> request,
                           bool& allow_os_execution) override {
    EXPECT_IO_THREAD();
    EXPECT_EQ(browser_id_, browser->GetIdentifier());

    if (IsMainURL(request->GetURL()) || IsSubURL(request->GetURL())) {
      EXPECT_FALSE(true);  // Not reached.
      return;
    }

    VerifyFrame(kOnProtocolExecution, frame);

    EXPECT_TRUE(custom_scheme_);
    EXPECT_TRUE(unhandled_);

    // Check expected default value.
    EXPECT_FALSE(allow_os_execution);

    VerifyState(kOnProtocolExecution, request, nullptr);
    on_protocol_execution_ct_++;
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    EXPECT_UI_THREAD();
    EXPECT_EQ(browser_id_, browser->GetIdentifier());

    EXPECT_EQ(httpStatusCode, 200);

    on_load_end_ct_++;

    TestHandler::OnLoadEnd(browser, frame, httpStatusCode);
    MaybeDestroyTest(false);
  }

  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64 query_id,
               const CefString& request,
               bool persistent,
               CefRefPtr<Callback> callback) override {
    EXPECT_UI_THREAD();
    EXPECT_EQ(browser_id_, browser->GetIdentifier());

    EXPECT_STREQ(kSubresourceProcessMsg, request.ToString().c_str());

    VerifyFrame(kOnQuery, frame);

    callback->Success("");

    on_query_ct_++;
    MaybeDestroyTest(false);

    return true;
  }

  void DestroyTest() override {
    // Only called for the main and/or sub frame load.
    if (subframe_) {
      EXPECT_EQ(2, on_before_browse_ct_);
    } else {
      EXPECT_EQ(1, on_before_browse_ct_);
    }

    if (mode_ == RESTART_RESOURCE_RESPONSE) {
      EXPECT_EQ(2, get_resource_request_handler_ct_);
      EXPECT_EQ(2, get_cookie_access_filter_ct_);
      EXPECT_EQ(2, on_before_resource_load_ct_);
      EXPECT_EQ(2, get_resource_handler_ct_);
      EXPECT_EQ(0, on_resource_redirect_ct_);
      // Unhandled requests won't see a call to GetResourceResponseFilter or
      // OnResourceResponse. In this case we're restarting from inside
      // OnResourceResponse.
      if (unhandled_) {
        EXPECT_EQ(0, get_resource_response_filter_ct_);
        EXPECT_EQ(1, on_resource_response_ct_);
      } else {
        EXPECT_EQ(1, get_resource_response_filter_ct_);
        EXPECT_EQ(2, on_resource_response_ct_);
      }
    } else if (IsLoad()) {
      EXPECT_EQ(1, get_resource_request_handler_ct_);
      EXPECT_EQ(1, get_cookie_access_filter_ct_);
      EXPECT_EQ(1, on_before_resource_load_ct_);
      EXPECT_EQ(1, get_resource_handler_ct_);
      EXPECT_EQ(0, on_resource_redirect_ct_);
      // Unhandled requests won't see a call to GetResourceResponseFilter or
      // OnResourceResponse.
      if (unhandled_) {
        EXPECT_EQ(0, get_resource_response_filter_ct_);
        EXPECT_EQ(0, on_resource_response_ct_);
      } else {
        EXPECT_EQ(1, get_resource_response_filter_ct_);
        EXPECT_EQ(1, on_resource_response_ct_);
      }
    } else if (IsRedirect()) {
      EXPECT_EQ(2, get_resource_request_handler_ct_);
      EXPECT_EQ(2, get_cookie_access_filter_ct_);
      EXPECT_EQ(2, on_before_resource_load_ct_);
      if (mode_ == REDIRECT_BEFORE_RESOURCE_LOAD) {
        EXPECT_EQ(1, get_resource_handler_ct_);
      } else {
        EXPECT_EQ(2, get_resource_handler_ct_);
      }
      EXPECT_EQ(1, on_resource_redirect_ct_);

      // Unhandled requests won't see a call to GetResourceResponseFilter.
      if (unhandled_)
        EXPECT_EQ(0, get_resource_response_filter_ct_);
      else
        EXPECT_EQ(1, get_resource_response_filter_ct_);

      // Unhandled requests won't see a call to OnResourceResponse.
      if (mode_ == REDIRECT_RESOURCE_RESPONSE) {
        // In this case we're redirecting from inside OnResourceResponse.
        if (unhandled_)
          EXPECT_EQ(1, on_resource_response_ct_);
        else
          EXPECT_EQ(2, on_resource_response_ct_);
      } else {
        if (unhandled_)
          EXPECT_EQ(0, on_resource_response_ct_);
        else
          EXPECT_EQ(1, on_resource_response_ct_);
      }
    } else if (IsIncomplete()) {
      EXPECT_EQ(1, get_resource_request_handler_ct_);
      EXPECT_EQ(1, get_cookie_access_filter_ct_);
      EXPECT_EQ(1, on_before_resource_load_ct_);

      if (IsIncompleteRequestHandler()) {
        EXPECT_EQ(1, get_resource_handler_ct_);
      } else {
        EXPECT_EQ(0, get_resource_handler_ct_);
      }

      EXPECT_EQ(0, on_resource_redirect_ct_);

      if (mode_ == INCOMPLETE_REQUEST_HANDLER_READ) {
        EXPECT_EQ(1, get_resource_response_filter_ct_);
        EXPECT_EQ(1, on_resource_response_ct_);
      } else {
        EXPECT_EQ(0, get_resource_response_filter_ct_);
        EXPECT_EQ(0, on_resource_response_ct_);
      }
    } else {
      NOTREACHED();
    }

    EXPECT_EQ(resource_handler_created_ct_, resource_handler_destroyed_ct_);
    EXPECT_EQ(1, on_resource_load_complete_ct_);

    // Only called for the main and/or sub frame load.
    if (IsIncomplete()) {
      EXPECT_EQ(0, on_load_end_ct_);
    } else {
      if (subframe_) {
        EXPECT_EQ(2, on_load_end_ct_);
      } else {
        EXPECT_EQ(1, on_load_end_ct_);
      }
    }

    if (unhandled_ || IsIncomplete()) {
      EXPECT_EQ(0, on_query_ct_);
    } else {
      EXPECT_EQ(1, on_query_ct_);
    }

    if (custom_scheme_ && unhandled_ && !IsIncomplete()) {
      EXPECT_EQ(1, on_protocol_execution_ct_);
    } else {
      EXPECT_EQ(0, on_protocol_execution_ct_);
    }

    TestHandler::DestroyTest();

    if (!SignalCompletionWhenAllBrowsersClose()) {
      // Complete asynchronously so the call stack has a chance to unwind.
      CefPostTask(TID_UI,
                  base::BindOnce(&SubresourceResponseTest::TestComplete, this));
    }
  }

 private:
  const char* GetMainURL() const {
    if (custom_scheme_) {
      return "rrhcustom://test.com/main.html";
    } else {
      return "http://test.com/main.html";
    }
  }

  const char* GetSubURL() const {
    if (custom_scheme_) {
      return "rrhcustom://test.com/subframe.html";
    } else {
      return "http://test.com/subframe.html";
    }
  }

  const char* GetOrigin() const {
    if (custom_scheme_) {
      return "rrhcustom://test.com";
    } else {
      return "http://test.com";
    }
  }

  bool IsMainURL(const std::string& url) const { return url == GetMainURL(); }
  bool IsSubURL(const std::string& url) const { return url == GetSubURL(); }

  enum TestUrl {
    RESULT_JS,
    REDIRECT_JS,
    REDIRECT2_JS,
  };

  const char* GetURL(TestUrl url) const {
    if (custom_scheme_) {
      if (url == RESULT_JS)
        return "rrhcustom://test.com/result.js";
      if (url == REDIRECT_JS)
        return "rrhcustom://test.com/redirect.js";
      if (url == REDIRECT2_JS)
        return "rrhcustom://test.com/redirect2.js";
    } else {
      if (url == RESULT_JS)
        return "http://test.com/result.js";
      if (url == REDIRECT_JS)
        return "http://test.com/redirect.js";
      if (url == REDIRECT2_JS)
        return "http://test.com/redirect2.js";
    }

    NOTREACHED();
    return "";
  }

  const char* GetStartupURL() const {
    if (IsLoad() || IsIncomplete()) {
      return GetURL(RESULT_JS);
    } else if (mode_ == REDIRECT_RESOURCE_REDIRECT) {
      return GetURL(REDIRECT2_JS);
    } else if (IsRedirect()) {
      return GetURL(REDIRECT_JS);
    }

    NOTREACHED();
    return "";
  }

  std::string GetMainResponseBody() const {
    std::stringstream html;
    html << "<html><head>";

    if (subframe_) {
      const std::string& url = GetSubURL();
      html << "<iframe src=\"" << url << "\"></iframe>";
    } else {
      const std::string& url = GetStartupURL();
      html << "<script type=\"text/javascript\" src=\"" << url
           << "\"></script>";
    }

    html << "</head><body><p>Main</p></body></html>";
    return html.str();
  }

  std::string GetSubResponseBody() const {
    DCHECK(subframe_);

    std::stringstream html;
    html << "<html><head>";

    const std::string& url = GetStartupURL();
    html << "<script type=\"text/javascript\" src=\"" << url << "\"></script>";

    html << "</head><body><p>Sub</p></body></html>";
    return html.str();
  }

  std::string GetResponseBody() const {
    return "window.testQuery({request:'" + std::string(kSubresourceProcessMsg) +
           "'});";
  }
  std::string GetRedirectBody() const {
    return "<html><body>Redirect</body></html>";
  }

  base::OnceClosure GetResourceDestroyCallback() {
    resource_handler_created_ct_++;
    return base::BindOnce(&SubresourceResponseTest::MaybeDestroyTest, this,
                          true);
  }

  bool GetCallbackResourceHandlerMode(CallbackResourceHandler::Mode& mode) {
    switch (mode_) {
      case IMMEDIATE_REQUEST_HANDLER_OPEN:
        mode = CallbackResourceHandler::IMMEDIATE_OPEN;
        return true;
      case IMMEDIATE_REQUEST_HANDLER_READ:
        mode = CallbackResourceHandler::IMMEDIATE_READ;
        return true;
      case IMMEDIATE_REQUEST_HANDLER_ALL:
        mode = CallbackResourceHandler::IMMEDIATE_ALL;
        return true;
      case DELAYED_REQUEST_HANDLER_OPEN:
        mode = CallbackResourceHandler::DELAYED_OPEN;
        return true;
      case DELAYED_REQUEST_HANDLER_READ:
        mode = CallbackResourceHandler::DELAYED_READ;
        return true;
      case DELAYED_REQUEST_HANDLER_ALL:
        mode = CallbackResourceHandler::DELAYED_ALL;
        return true;
      default:
        break;
    }
    return false;
  }

  CefRefPtr<CefResourceHandler> GetResource(int status_code,
                                            const CefString& status_text,
                                            const CefString& mime_type,
                                            CefResponse::HeaderMap header_map,
                                            const std::string& body) {
    CefRefPtr<CefStreamReader> stream = CefStreamReader::CreateForData(
        const_cast<char*>(body.c_str()), body.size());

    CallbackResourceHandler::Mode handler_mode;
    if (GetCallbackResourceHandlerMode(handler_mode)) {
      return new CallbackResourceHandler(handler_mode, status_code, status_text,
                                         mime_type, header_map, stream,
                                         GetResourceDestroyCallback());
    }

    return new NormalResourceHandler(status_code, status_text, mime_type,
                                     header_map, stream,
                                     GetResourceDestroyCallback());
  }

  CefRefPtr<CefResourceHandler> GetMainResource() {
    return GetResource(200, "OK", "text/html", CefResponse::HeaderMap(),
                       GetMainResponseBody());
  }

  CefRefPtr<CefResourceHandler> GetSubResource() {
    return GetResource(200, "OK", "text/html", CefResponse::HeaderMap(),
                       GetSubResponseBody());
  }

  CefRefPtr<CefResourceHandler> GetOKResource() {
    return GetResource(200, "OK", "text/javascript", CefResponse::HeaderMap(),
                       GetResponseBody());
  }

  CefRefPtr<CefResourceHandler> GetRedirectResource(
      const std::string& redirect_url) {
    CefResponse::HeaderMap headerMap;
    headerMap.insert(std::make_pair("Location", redirect_url));

    return GetResource(307, "Temporary Redirect", "text/javascript", headerMap,
                       GetRedirectBody());
  }

  CefRefPtr<CefResourceHandler> GetIncompleteResource() {
    if (TestOldResourceAPI()) {
      return new IncompleteResourceHandlerOld(
          mode_ == INCOMPLETE_REQUEST_HANDLER_OPEN
              ? IncompleteResourceHandlerOld::BLOCK_PROCESS_REQUEST
              : IncompleteResourceHandlerOld::BLOCK_READ_RESPONSE,
          "text/javascript", GetResourceDestroyCallback());
    }

    return new IncompleteResourceHandler(
        mode_ == INCOMPLETE_REQUEST_HANDLER_OPEN
            ? IncompleteResourceHandler::BLOCK_OPEN
            : IncompleteResourceHandler::BLOCK_READ,
        "text/javascript", GetResourceDestroyCallback());
  }

  bool IsLoad() const {
    return mode_ == LOAD || mode_ == MODIFY_BEFORE_RESOURCE_LOAD ||
           mode_ == RESTART_RESOURCE_RESPONSE ||
           mode_ == IMMEDIATE_REQUEST_HANDLER_OPEN ||
           mode_ == IMMEDIATE_REQUEST_HANDLER_READ ||
           mode_ == IMMEDIATE_REQUEST_HANDLER_ALL ||
           mode_ == DELAYED_REQUEST_HANDLER_OPEN ||
           mode_ == DELAYED_REQUEST_HANDLER_READ ||
           mode_ == DELAYED_REQUEST_HANDLER_ALL;
  }

  bool IsIncompleteRequestHandler() const {
    return mode_ == INCOMPLETE_REQUEST_HANDLER_OPEN ||
           mode_ == INCOMPLETE_REQUEST_HANDLER_READ;
  }

  bool IsIncomplete() const {
    return mode_ == INCOMPLETE_BEFORE_RESOURCE_LOAD ||
           IsIncompleteRequestHandler();
  }

  bool IsRedirect() const {
    return mode_ == REDIRECT_BEFORE_RESOURCE_LOAD ||
           mode_ == REDIRECT_REQUEST_HANDLER ||
           mode_ == REDIRECT_RESOURCE_REDIRECT ||
           mode_ == REDIRECT_RESOURCE_RESPONSE;
  }

  static void SetCustomHeader(CefRefPtr<CefRequest> request) {
    EXPECT_FALSE(request->IsReadOnly());
    request->SetHeaderByName("X-Custom-Header", "value", false);
  }

  static std::string GetCustomHeader(CefRefPtr<CefRequest> request) {
    return request->GetHeaderByName("X-Custom-Header");
  }

  // Resource-related callbacks.
  enum Callback {
    kGetResourceRequestHandler,
    kGetCookieAccessFilter,
    kOnBeforeResourceLoad,
    kGetResourceHandler,
    kOnResourceRedirect,
    kOnResourceResponse,
    kGetResourceResponseFilter,
    kOnResourceLoadComplete,
    kOnProtocolExecution,
    kOnQuery,
  };

  bool ShouldHaveResponse(Callback callback) const {
    return callback >= kOnResourceRedirect &&
           callback <= kOnResourceLoadComplete;
  }

  bool ShouldHaveWritableRequest(Callback callback) const {
    return callback == kOnBeforeResourceLoad || callback == kOnResourceResponse;
  }

  void VerifyFrame(Callback callback, CefRefPtr<CefFrame> frame) const {
    EXPECT_TRUE(frame);

    if (subframe_)
      EXPECT_FALSE(frame->IsMain()) << callback;
    else
      EXPECT_TRUE(frame->IsMain()) << callback;

    EXPECT_EQ(frame_id_, frame->GetIdentifier()) << callback;
  }

  void VerifyState(Callback callback,
                   CefRefPtr<CefRequest> request,
                   CefRefPtr<CefResponse> response) const {
    EXPECT_TRUE(request) << callback;

    if (ShouldHaveResponse(callback)) {
      EXPECT_TRUE(response) << callback;
      EXPECT_TRUE(response->IsReadOnly()) << callback;
    } else {
      EXPECT_FALSE(response) << callback;
    }

    if (ShouldHaveWritableRequest(callback)) {
      EXPECT_FALSE(request->IsReadOnly()) << callback;
    } else {
      EXPECT_TRUE(request->IsReadOnly()) << callback;
    }

    // All resource-related callbacks share the same request ID.
    EXPECT_EQ(request_id_, request->GetIdentifier()) << callback;

    if (IsLoad() || IsIncomplete()) {
      EXPECT_STREQ("GET", request->GetMethod().ToString().c_str()) << callback;
      EXPECT_STREQ(GetURL(RESULT_JS), request->GetURL().ToString().c_str())
          << callback;

      // Expect the header for all callbacks following the callback that
      // initially sets it.
      const std::string& custom_header = GetCustomHeader(request);
      if ((mode_ == RESTART_RESOURCE_RESPONSE &&
           on_resource_response_ct_ > 0) ||
          (mode_ == MODIFY_BEFORE_RESOURCE_LOAD &&
           on_before_resource_load_ct_ > 0)) {
        EXPECT_STREQ("value", custom_header.c_str()) << callback;
      } else {
        EXPECT_STREQ("", custom_header.c_str()) << callback;
      }

      if (response)
        VerifyOKResponse(callback, response);
    } else if (IsRedirect()) {
      EXPECT_STREQ("GET", request->GetMethod().ToString().c_str()) << callback;
      // Subresource loads don't get OnBeforeBrowse calls, so this check is a
      // bit less exact then with main resource loads.
      if (on_resource_redirect_ct_ == 0) {
        // Before the redirect.
        EXPECT_STREQ(GetStartupURL(), request->GetURL().ToString().c_str())
            << callback;
      } else {
        // After the redirect.
        EXPECT_STREQ(GetURL(RESULT_JS), request->GetURL().ToString().c_str())
            << callback;
      }

      if (response) {
        if (callback == kOnResourceRedirect) {
          // Before the redirect.
          VerifyRedirectResponse(callback, response);
        } else {
          // After the redirect.
          VerifyOKResponse(callback, response);
        }
      }
    } else {
      NOTREACHED() << callback;
    }
  }

  void VerifyOKResponse(Callback callback,
                        CefRefPtr<CefResponse> response) const {
    // True for the first response in cases where we're redirecting/restarting
    // from inside OnResourceResponse (e.g. the first response always succeeds).
    const bool override_unhandled = unhandled_ &&
                                    (mode_ == REDIRECT_RESOURCE_RESPONSE ||
                                     mode_ == RESTART_RESOURCE_RESPONSE) &&
                                    get_resource_handler_ct_ == 1;

    // True for tests where the request will be incomplete and never receive a
    // response.
    const bool incomplete_unhandled =
        (mode_ == INCOMPLETE_BEFORE_RESOURCE_LOAD ||
         mode_ == INCOMPLETE_REQUEST_HANDLER_OPEN);

    if ((unhandled_ && !override_unhandled) || incomplete_unhandled) {
      if (incomplete_unhandled) {
        EXPECT_EQ(ERR_ABORTED, response->GetError()) << callback;
      } else {
        EXPECT_EQ(ERR_UNKNOWN_URL_SCHEME, response->GetError()) << callback;
      }
      EXPECT_EQ(0, response->GetStatus()) << callback;
      EXPECT_STREQ("", response->GetStatusText().ToString().c_str())
          << callback;
      EXPECT_STREQ("", response->GetURL().ToString().c_str()) << callback;
      EXPECT_STREQ("", response->GetMimeType().ToString().c_str()) << callback;
      EXPECT_STREQ("", response->GetCharset().ToString().c_str()) << callback;
    } else {
      if (mode_ == INCOMPLETE_REQUEST_HANDLER_READ &&
          callback == kOnResourceLoadComplete) {
        // We got a response, but we also got aborted.
        EXPECT_EQ(ERR_ABORTED, response->GetError()) << callback;
      } else {
        EXPECT_EQ(ERR_NONE, response->GetError()) << callback;
      }
      EXPECT_EQ(200, response->GetStatus()) << callback;
      EXPECT_STREQ("OK", response->GetStatusText().ToString().c_str())
          << callback;
      EXPECT_STREQ("", response->GetURL().ToString().c_str()) << callback;
      EXPECT_STREQ("text/javascript",
                   response->GetMimeType().ToString().c_str())
          << callback;
      EXPECT_STREQ("", response->GetCharset().ToString().c_str()) << callback;
    }
  }

  void VerifyRedirectResponse(Callback callback,
                              CefRefPtr<CefResponse> response) const {
    EXPECT_EQ(ERR_NONE, response->GetError()) << callback;
    EXPECT_EQ(307, response->GetStatus()) << callback;
    const std::string& status_text = response->GetStatusText();
    EXPECT_TRUE(status_text == "Internal Redirect" ||
                status_text == "Temporary Redirect")
        << status_text << callback;
    EXPECT_STREQ("", response->GetURL().ToString().c_str()) << callback;
    EXPECT_STREQ("", response->GetMimeType().ToString().c_str()) << callback;
    EXPECT_STREQ("", response->GetCharset().ToString().c_str()) << callback;
  }

  void CloseBrowserAsync() {
    EXPECT_TRUE(IsIncomplete());
    SetSignalCompletionWhenAllBrowsersClose(false);
    CefPostDelayedTask(
        TID_UI, base::BindOnce(&TestHandler::CloseBrowser, GetBrowser(), false),
        100);
  }

  void MaybeDestroyTest(bool from_handler) {
    if (!CefCurrentlyOn(TID_UI)) {
      CefPostTask(TID_UI,
                  base::BindOnce(&SubresourceResponseTest::MaybeDestroyTest,
                                 this, from_handler));
      return;
    }

    if (from_handler) {
      resource_handler_destroyed_ct_++;
    }

    bool destroy_test = false;
    if (IsIncomplete()) {
      // Destroy the test if we got OnResourceLoadComplete and either the
      // resource handler will never complete or it was destroyed.
      destroy_test =
          on_resource_load_complete_ct_ > 0 &&
          (!IsIncompleteRequestHandler() ||
           resource_handler_destroyed_ct_ == resource_handler_created_ct_);
    } else {
      // Destroy the test if we got the expected number of OnLoadEnd and
      // OnQuery, and the expected number of resource handlers were destroyed.
      destroy_test =
          on_load_end_ct_ > (subframe_ ? 1 : 0) &&
          (on_query_ct_ > 0 || unhandled_) &&
          resource_handler_destroyed_ct_ == resource_handler_created_ct_;
    }

    if (destroy_test) {
      DestroyTest();
    }
  }

  const TestMode mode_;
  const bool custom_scheme_;
  const bool unhandled_;
  const bool subframe_;

  int browser_id_ = 0;
  int64 frame_id_ = 0;
  uint64 request_id_ = 0U;

  int resource_handler_created_ct_ = 0;

  int on_before_browse_ct_ = 0;
  int on_load_end_ct_ = 0;
  int on_query_ct_ = 0;

  int get_resource_request_handler_ct_ = 0;
  int get_cookie_access_filter_ct_ = 0;
  int on_before_resource_load_ct_ = 0;
  int get_resource_handler_ct_ = 0;
  int on_resource_redirect_ct_ = 0;
  int on_resource_response_ct_ = 0;
  int get_resource_response_filter_ct_ = 0;
  int on_resource_load_complete_ct_ = 0;
  int on_protocol_execution_ct_ = 0;
  int resource_handler_destroyed_ct_ = 0;

  // Used with INCOMPLETE_BEFORE_RESOURCE_LOAD.
  CefRefPtr<CefCallback> incomplete_callback_;

  DISALLOW_COPY_AND_ASSIGN(SubresourceResponseTest);
  IMPLEMENT_REFCOUNTING(SubresourceResponseTest);
};

}  // namespace

#define SUBRESOURCE_TEST(name, test_mode, custom, unhandled, subframe)        \
  TEST(ResourceRequestHandlerTest, Subresource##name) {                       \
    CefRefPtr<SubresourceResponseTest> handler = new SubresourceResponseTest( \
        SubresourceResponseTest::test_mode, custom, unhandled, subframe);     \
    handler->ExecuteTest();                                                   \
    ReleaseAndWaitForDestructor(handler);                                     \
  }

#define SUBRESOURCE_TEST_ALL_MODES(name, custom, unhandled, subframe)          \
  SUBRESOURCE_TEST(name##Load, LOAD, custom, unhandled, subframe)              \
  SUBRESOURCE_TEST(name##ModifyBeforeResourceLoad,                             \
                   MODIFY_BEFORE_RESOURCE_LOAD, custom, unhandled, subframe)   \
  SUBRESOURCE_TEST(name##RedirectBeforeResourceLoad,                           \
                   REDIRECT_BEFORE_RESOURCE_LOAD, custom, unhandled, subframe) \
  SUBRESOURCE_TEST(name##RedirectRequestHandler, REDIRECT_REQUEST_HANDLER,     \
                   custom, unhandled, subframe)                                \
  SUBRESOURCE_TEST(name##RedirectResourceRedirect, REDIRECT_RESOURCE_REDIRECT, \
                   custom, unhandled, subframe)                                \
  SUBRESOURCE_TEST(name##RedirectResourceResponse, REDIRECT_RESOURCE_RESPONSE, \
                   custom, unhandled, subframe)                                \
  SUBRESOURCE_TEST(name##RestartResourceResponse, RESTART_RESOURCE_RESPONSE,   \
                   custom, unhandled, subframe)

// Tests only supported in handled mode.
#define SUBRESOURCE_TEST_HANDLED_MODES(name, custom, subframe)               \
  SUBRESOURCE_TEST(name##ImmediateRequestHandlerOpen,                        \
                   IMMEDIATE_REQUEST_HANDLER_OPEN, custom, false, subframe)  \
  SUBRESOURCE_TEST(name##ImmediateRequestHandlerRead,                        \
                   IMMEDIATE_REQUEST_HANDLER_READ, custom, false, subframe)  \
  SUBRESOURCE_TEST(name##ImmediateRequestHandlerAll,                         \
                   IMMEDIATE_REQUEST_HANDLER_ALL, custom, false, subframe)   \
  SUBRESOURCE_TEST(name##DelayedRequestHandlerOpen,                          \
                   DELAYED_REQUEST_HANDLER_OPEN, custom, false, subframe)    \
  SUBRESOURCE_TEST(name##DelayedRequestHandlerRead,                          \
                   DELAYED_REQUEST_HANDLER_READ, custom, false, subframe)    \
  SUBRESOURCE_TEST(name##DelayedRequestHandlerAll,                           \
                   DELAYED_REQUEST_HANDLER_ALL, custom, false, subframe)     \
  SUBRESOURCE_TEST(name##IncompleteBeforeResourceLoad,                       \
                   INCOMPLETE_BEFORE_RESOURCE_LOAD, custom, false, subframe) \
  SUBRESOURCE_TEST(name##IncompleteRequestHandlerOpen,                       \
                   INCOMPLETE_REQUEST_HANDLER_OPEN, custom, false, subframe) \
  SUBRESOURCE_TEST(name##IncompleteRequestHandlerRead,                       \
                   INCOMPLETE_REQUEST_HANDLER_READ, custom, false, subframe)

SUBRESOURCE_TEST_ALL_MODES(StandardHandledMainFrame, false, false, false)
SUBRESOURCE_TEST_ALL_MODES(StandardUnhandledMainFrame, false, true, false)
SUBRESOURCE_TEST_ALL_MODES(CustomHandledMainFrame, true, false, false)
SUBRESOURCE_TEST_ALL_MODES(CustomUnhandledMainFrame, true, true, false)

SUBRESOURCE_TEST_ALL_MODES(StandardHandledSubFrame, false, false, true)
SUBRESOURCE_TEST_ALL_MODES(StandardUnhandledSubFrame, false, true, true)
SUBRESOURCE_TEST_ALL_MODES(CustomHandledSubFrame, true, false, true)
SUBRESOURCE_TEST_ALL_MODES(CustomUnhandledSubFrame, true, true, true)

SUBRESOURCE_TEST_HANDLED_MODES(StandardHandledMainFrame, false, false)
SUBRESOURCE_TEST_HANDLED_MODES(CustomHandledMainFrame, true, false)

SUBRESOURCE_TEST_HANDLED_MODES(StandardHandledSubFrame, false, true)
SUBRESOURCE_TEST_HANDLED_MODES(CustomHandledSubFrame, true, true)

namespace {

const char kResourceTestHtml[] = "http://test.com/resource.html";

class RedirectResponseTest : public TestHandler {
 public:
  enum TestMode {
    URL,
    HEADER,
    POST,
  };

  RedirectResponseTest(TestMode mode, bool via_request_context_handler)
      : via_request_context_handler_(via_request_context_handler) {
    if (mode == URL)
      resource_test_.reset(new UrlResourceTest);
    else if (mode == HEADER)
      resource_test_.reset(new HeaderResourceTest);
    else
      resource_test_.reset(new PostResourceTest);
  }

  void RunTest() override {
    AddResource(kResourceTestHtml, GetHtml(), "text/html");

    resource_request_handler_ = new ResourceRequestHandler(this);

    CefRefPtr<CefRequestContext> request_context =
        CefRequestContext::GetGlobalContext();
    if (via_request_context_handler_) {
      CefRefPtr<CefRequestContextHandler> request_context_handler =
          new RequestContextHandler(resource_request_handler_.get());
      CefRequestContextSettings settings;
      request_context = CefRequestContext::CreateContext(
          request_context, request_context_handler);
    }

    CreateBrowser(kResourceTestHtml, request_context);
    SetTestTimeout();
  }

  CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request,
      bool is_navigation,
      bool is_download,
      const CefString& request_initiator,
      bool& disable_default_handling) override {
    if (via_request_context_handler_) {
      // Use the handler returned by RequestContextHandler.
      return nullptr;
    }
    return resource_request_handler_.get();
  }

  bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                      CefRefPtr<CefFrame> frame,
                      CefRefPtr<CefRequest> request,
                      bool user_gesture,
                      bool is_redirect) override {
    EXPECT_UI_THREAD();
    EXPECT_EQ(0, browser_id_);
    browser_id_ = browser->GetIdentifier();
    EXPECT_GT(browser_id_, 0);

    // This method is only called for the main resource.
    EXPECT_STREQ(kResourceTestHtml, request->GetURL().ToString().c_str());

    // Browser-side navigation no longer exposes the actual request information.
    EXPECT_EQ(0U, request->GetIdentifier());

    return false;
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    EXPECT_UI_THREAD();
    EXPECT_EQ(browser_id_, browser->GetIdentifier());

    TestHandler::OnLoadEnd(browser, frame, httpStatusCode);
    DestroyTest();
  }

  void DestroyTest() override {
    resource_test_->CheckExpected();
    resource_test_.reset(nullptr);

    TestHandler::DestroyTest();
  }

 private:
  std::string GetHtml() const {
    std::stringstream html;
    html << "<html><head>";

    const std::string& url = resource_test_->start_url();
    html << "<script type=\"text/javascript\" src=\"" << url << "\"></script>";

    html << "</head><body><p>Main</p></body></html>";
    return html.str();
  }

  class ResourceTest {
   public:
    ResourceTest(const std::string& start_url,
                 size_t expected_resource_response_ct = 2U,
                 size_t expected_before_resource_load_ct = 1U,
                 size_t expected_resource_redirect_ct = 0U,
                 size_t expected_resource_load_complete_ct = 1U)
        : start_url_(start_url),
          expected_resource_response_ct_(expected_resource_response_ct),
          expected_before_resource_load_ct_(expected_before_resource_load_ct),
          expected_resource_redirect_ct_(expected_resource_redirect_ct),
          expected_resource_load_complete_ct_(
              expected_resource_load_complete_ct) {}
    virtual ~ResourceTest() {}

    const std::string& start_url() const { return start_url_; }

    virtual bool OnBeforeResourceLoad(CefRefPtr<CefBrowser> browser,
                                      CefRefPtr<CefFrame> frame,
                                      CefRefPtr<CefRequest> request) {
      before_resource_load_ct_++;
      return false;
    }

    virtual CefRefPtr<CefResourceHandler> GetResourceHandler(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefRequest> request) {
      get_resource_handler_ct_++;

      const std::string& js_content = "<!-- -->";

      CefRefPtr<CefStreamReader> stream = CefStreamReader::CreateForData(
          const_cast<char*>(js_content.c_str()), js_content.size());

      return new CefStreamResourceHandler(200, "OK", "text/javascript",
                                          CefResponse::HeaderMap(), stream);
    }

    virtual void OnResourceRedirect(CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefFrame> frame,
                                    CefRefPtr<CefRequest> request,
                                    CefString& new_url) {
      resource_redirect_ct_++;
    }

    bool OnResourceResponse(CefRefPtr<CefBrowser> browser,
                            CefRefPtr<CefFrame> frame,
                            CefRefPtr<CefRequest> request,
                            CefRefPtr<CefResponse> response) {
      EXPECT_TRUE(CheckUrl(request->GetURL()));

      // Verify the response returned by GetResourceHandler.
      EXPECT_EQ(200, response->GetStatus());
      EXPECT_STREQ("OK", response->GetStatusText().ToString().c_str());
      EXPECT_STREQ("text/javascript",
                   response->GetMimeType().ToString().c_str());

      if (resource_response_ct_++ == 0U) {
        // Always redirect at least one time.
        OnResourceReceived(browser, frame, request, response);
        return true;
      }

      OnRetryReceived(browser, frame, request, response);
      return (resource_response_ct_ < expected_resource_response_ct_);
    }

    CefRefPtr<CefResponseFilter> GetResourceResponseFilter(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefRequest> request,
        CefRefPtr<CefResponse> response) {
      get_resource_response_filter_ct_++;
      return nullptr;
    }

    void OnResourceLoadComplete(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefRefPtr<CefRequest> request,
                                CefRefPtr<CefResponse> response,
                                URLRequestStatus status,
                                int64 received_content_length) {
      EXPECT_TRUE(CheckUrl(request->GetURL()));

      // Verify the response returned by GetResourceHandler.
      EXPECT_EQ(200, response->GetStatus());
      EXPECT_STREQ("OK", response->GetStatusText().ToString().c_str());
      EXPECT_STREQ("text/javascript",
                   response->GetMimeType().ToString().c_str());

      resource_load_complete_ct_++;
    }

    virtual bool CheckUrl(const std::string& url) const {
      return (url == start_url_);
    }

    virtual void CheckExpected() {
      EXPECT_TRUE(got_resource_);
      EXPECT_TRUE(got_resource_retry_);

      EXPECT_EQ(expected_resource_response_ct_, resource_response_ct_);
      EXPECT_EQ(expected_resource_response_ct_, get_resource_handler_ct_);
      EXPECT_EQ(expected_resource_load_complete_ct_,
                get_resource_response_filter_ct_);
      EXPECT_EQ(expected_before_resource_load_ct_, before_resource_load_ct_);
      EXPECT_EQ(expected_resource_redirect_ct_, resource_redirect_ct_);
      EXPECT_EQ(expected_resource_load_complete_ct_,
                resource_load_complete_ct_);
    }

   protected:
    virtual void OnResourceReceived(CefRefPtr<CefBrowser> browser,
                                    CefRefPtr<CefFrame> frame,
                                    CefRefPtr<CefRequest> request,
                                    CefRefPtr<CefResponse> response) {
      got_resource_.yes();
    }

    virtual void OnRetryReceived(CefRefPtr<CefBrowser> browser,
                                 CefRefPtr<CefFrame> frame,
                                 CefRefPtr<CefRequest> request,
                                 CefRefPtr<CefResponse> response) {
      got_resource_retry_.yes();
    }

   private:
    std::string start_url_;

    size_t resource_response_ct_ = 0U;
    size_t expected_resource_response_ct_;
    size_t before_resource_load_ct_ = 0U;
    size_t expected_before_resource_load_ct_;
    size_t get_resource_handler_ct_ = 0U;
    size_t resource_redirect_ct_ = 0U;
    size_t expected_resource_redirect_ct_;
    size_t get_resource_response_filter_ct_ = 0U;
    size_t resource_load_complete_ct_ = 0U;
    size_t expected_resource_load_complete_ct_;

    TrackCallback got_resource_;
    TrackCallback got_resource_retry_;
  };

  class UrlResourceTest : public ResourceTest {
   public:
    // With NetworkService we don't get an additional (unnecessary) redirect
    // callback.
    UrlResourceTest()
        : ResourceTest("http://test.com/start_url.js", 2U, 2U, 1U) {
      redirect_url_ = "http://test.com/redirect_url.js";
    }

    bool CheckUrl(const std::string& url) const override {
      if (url == redirect_url_)
        return true;

      return ResourceTest::CheckUrl(url);
    }

    void OnResourceRedirect(CefRefPtr<CefBrowser> browser,
                            CefRefPtr<CefFrame> frame,
                            CefRefPtr<CefRequest> request,
                            CefString& new_url) override {
      ResourceTest::OnResourceRedirect(browser, frame, request, new_url);
      const std::string& old_url = request->GetURL();
      EXPECT_STREQ(start_url().c_str(), old_url.c_str());
      EXPECT_STREQ(redirect_url_.c_str(), new_url.ToString().c_str());
    }

   private:
    void OnResourceReceived(CefRefPtr<CefBrowser> browser,
                            CefRefPtr<CefFrame> frame,
                            CefRefPtr<CefRequest> request,
                            CefRefPtr<CefResponse> response) override {
      ResourceTest::OnResourceReceived(browser, frame, request, response);
      request->SetURL(redirect_url_);
    }

    void OnRetryReceived(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         CefRefPtr<CefRequest> request,
                         CefRefPtr<CefResponse> response) override {
      ResourceTest::OnRetryReceived(browser, frame, request, response);
      const std::string& new_url = request->GetURL();
      EXPECT_STREQ(redirect_url_.c_str(), new_url.c_str());
    }

    std::string redirect_url_;
  };

  class HeaderResourceTest : public ResourceTest {
   public:
    // With NetworkService we restart the request, so we get another call to
    // OnBeforeResourceLoad.
    HeaderResourceTest()
        : ResourceTest("http://test.com/start_header.js", 2U, 2U) {
      expected_headers_.insert(std::make_pair("Test-Key1", "Value1"));
      expected_headers_.insert(std::make_pair("Test-Key2", "Value2"));
    }

   private:
    void OnResourceReceived(CefRefPtr<CefBrowser> browser,
                            CefRefPtr<CefFrame> frame,
                            CefRefPtr<CefRequest> request,
                            CefRefPtr<CefResponse> response) override {
      ResourceTest::OnResourceReceived(browser, frame, request, response);
      request->SetHeaderMap(expected_headers_);
    }

    void OnRetryReceived(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         CefRefPtr<CefRequest> request,
                         CefRefPtr<CefResponse> response) override {
      ResourceTest::OnRetryReceived(browser, frame, request, response);
      CefRequest::HeaderMap actual_headers;
      request->GetHeaderMap(actual_headers);
      TestMapEqual(expected_headers_, actual_headers, true);
    }

    CefRequest::HeaderMap expected_headers_;
  };

  class PostResourceTest : public ResourceTest {
   public:
    // With NetworkService we restart the request, so we get another call to
    // OnBeforeResourceLoad.
    PostResourceTest() : ResourceTest("http://test.com/start_post.js", 2U, 2U) {
      CefRefPtr<CefPostDataElement> elem = CefPostDataElement::Create();
      const std::string data("Test Post Data");
      elem->SetToBytes(data.size(), data.c_str());

      expected_post_ = CefPostData::Create();
      expected_post_->AddElement(elem);
    }

   private:
    void OnResourceReceived(CefRefPtr<CefBrowser> browser,
                            CefRefPtr<CefFrame> frame,
                            CefRefPtr<CefRequest> request,
                            CefRefPtr<CefResponse> response) override {
      ResourceTest::OnResourceReceived(browser, frame, request, response);
      request->SetPostData(expected_post_);
    }

    void OnRetryReceived(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         CefRefPtr<CefRequest> request,
                         CefRefPtr<CefResponse> response) override {
      ResourceTest::OnRetryReceived(browser, frame, request, response);
      CefRefPtr<CefPostData> actual_post = request->GetPostData();
      TestPostDataEqual(expected_post_, actual_post);
    }

    CefRefPtr<CefPostData> expected_post_;
  };

  class RequestContextHandler : public CefRequestContextHandler {
   public:
    explicit RequestContextHandler(
        CefRefPtr<CefResourceRequestHandler> resource_request_handler)
        : resource_request_handler_(resource_request_handler) {}

    CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefRequest> request,
        bool is_navigation,
        bool is_download,
        const CefString& request_initiator,
        bool& disable_default_handling) override {
      return resource_request_handler_;
    }

   private:
    CefRefPtr<CefResourceRequestHandler> resource_request_handler_;

    IMPLEMENT_REFCOUNTING(RequestContextHandler);
    DISALLOW_COPY_AND_ASSIGN(RequestContextHandler);
  };

  class ResourceRequestHandler : public CefResourceRequestHandler {
   public:
    explicit ResourceRequestHandler(RedirectResponseTest* test) : test_(test) {}

    cef_return_value_t OnBeforeResourceLoad(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefRequest> request,
        CefRefPtr<CefCallback> callback) override {
      EXPECT_IO_THREAD();

      if (IsChromeRuntimeEnabled() &&
          request->GetResourceType() == RT_FAVICON) {
        // Ignore favicon requests.
        return RV_CANCEL;
      }

      EXPECT_EQ(test_->browser_id_, browser->GetIdentifier());

      if (request->GetURL() == kResourceTestHtml) {
        // All loads of the main resource should keep the same request id.
        EXPECT_EQ(0U, main_request_id_);
        main_request_id_ = request->GetIdentifier();
        EXPECT_GT(main_request_id_, 0U);
        return RV_CONTINUE;
      }

      // All redirects of the sub-resource should keep the same request id.
      if (sub_request_id_ == 0U) {
        sub_request_id_ = request->GetIdentifier();
        EXPECT_GT(sub_request_id_, 0U);
      } else {
        EXPECT_EQ(sub_request_id_, request->GetIdentifier());
      }

      return test_->resource_test_->OnBeforeResourceLoad(browser, frame,
                                                         request)
                 ? RV_CANCEL
                 : RV_CONTINUE;
    }

    CefRefPtr<CefResourceHandler> GetResourceHandler(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefRequest> request) override {
      EXPECT_IO_THREAD();
      EXPECT_EQ(test_->browser_id_, browser->GetIdentifier());

      if (request->GetURL() == kResourceTestHtml) {
        EXPECT_EQ(main_request_id_, request->GetIdentifier());
        return test_->GetResourceHandler(browser, frame, request);
      }

      EXPECT_EQ(sub_request_id_, request->GetIdentifier());
      return test_->resource_test_->GetResourceHandler(browser, frame, request);
    }

    void OnResourceRedirect(CefRefPtr<CefBrowser> browser,
                            CefRefPtr<CefFrame> frame,
                            CefRefPtr<CefRequest> request,
                            CefRefPtr<CefResponse> response,
                            CefString& new_url) override {
      EXPECT_IO_THREAD();
      EXPECT_EQ(test_->browser_id_, browser->GetIdentifier());
      EXPECT_EQ(sub_request_id_, request->GetIdentifier());

      test_->resource_test_->OnResourceRedirect(browser, frame, request,
                                                new_url);
    }

    bool OnResourceResponse(CefRefPtr<CefBrowser> browser,
                            CefRefPtr<CefFrame> frame,
                            CefRefPtr<CefRequest> request,
                            CefRefPtr<CefResponse> response) override {
      EXPECT_IO_THREAD();
      EXPECT_TRUE(browser.get());
      EXPECT_EQ(test_->browser_id_, browser->GetIdentifier());

      EXPECT_TRUE(frame.get());
      EXPECT_TRUE(frame->IsMain());

      if (request->GetURL() == kResourceTestHtml) {
        EXPECT_EQ(main_request_id_, request->GetIdentifier());
        return false;
      }

      EXPECT_EQ(sub_request_id_, request->GetIdentifier());
      return test_->resource_test_->OnResourceResponse(browser, frame, request,
                                                       response);
    }

    CefRefPtr<CefResponseFilter> GetResourceResponseFilter(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefRequest> request,
        CefRefPtr<CefResponse> response) override {
      EXPECT_IO_THREAD();
      EXPECT_TRUE(browser.get());
      EXPECT_EQ(test_->browser_id_, browser->GetIdentifier());

      EXPECT_TRUE(frame.get());
      EXPECT_TRUE(frame->IsMain());

      if (request->GetURL() == kResourceTestHtml) {
        EXPECT_EQ(main_request_id_, request->GetIdentifier());
        return nullptr;
      }

      return test_->resource_test_->GetResourceResponseFilter(
          browser, frame, request, response);
    }

    void OnResourceLoadComplete(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefRefPtr<CefRequest> request,
                                CefRefPtr<CefResponse> response,
                                URLRequestStatus status,
                                int64 received_content_length) override {
      EXPECT_IO_THREAD();

      if (IsChromeRuntimeEnabled() &&
          request->GetResourceType() == RT_FAVICON) {
        // Ignore favicon requests.
        return;
      }

      EXPECT_TRUE(browser.get());
      EXPECT_EQ(test_->browser_id_, browser->GetIdentifier());

      EXPECT_TRUE(frame.get());
      EXPECT_TRUE(frame->IsMain());

      if (request->GetURL() == kResourceTestHtml) {
        EXPECT_EQ(main_request_id_, request->GetIdentifier());
        return;
      }

      EXPECT_EQ(sub_request_id_, request->GetIdentifier());
      test_->resource_test_->OnResourceLoadComplete(
          browser, frame, request, response, status, received_content_length);
    }

   private:
    RedirectResponseTest* const test_;

    uint64 main_request_id_ = 0U;
    uint64 sub_request_id_ = 0U;

    IMPLEMENT_REFCOUNTING(ResourceRequestHandler);
    DISALLOW_COPY_AND_ASSIGN(ResourceRequestHandler);
  };

  const bool via_request_context_handler_;

  int browser_id_ = 0;
  std::unique_ptr<ResourceTest> resource_test_;
  CefRefPtr<ResourceRequestHandler> resource_request_handler_;

  IMPLEMENT_REFCOUNTING(RedirectResponseTest);
};

}  // namespace

// Verify redirect with client handler.
TEST(ResourceRequestHandlerTest, RedirectURLViaClient) {
  CefRefPtr<RedirectResponseTest> handler =
      new RedirectResponseTest(RedirectResponseTest::URL, false);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Verify redirect + modified headers with client handler.
TEST(ResourceRequestHandlerTest, RedirectHeaderViaClient) {
  CefRefPtr<RedirectResponseTest> handler =
      new RedirectResponseTest(RedirectResponseTest::HEADER, false);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Verify redirect + modified post data with client handler.
TEST(ResourceRequestHandlerTest, RedirectPostViaClient) {
  CefRefPtr<RedirectResponseTest> handler =
      new RedirectResponseTest(RedirectResponseTest::POST, false);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Verify redirect with context handler.
TEST(ResourceRequestHandlerTest, RedirectURLViaContext) {
  CefRefPtr<RedirectResponseTest> handler =
      new RedirectResponseTest(RedirectResponseTest::URL, true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Verify redirect + modified headers with context handler.
TEST(ResourceRequestHandlerTest, RedirectHeaderViaContext) {
  CefRefPtr<RedirectResponseTest> handler =
      new RedirectResponseTest(RedirectResponseTest::HEADER, true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Verify redirect + modified post data with context handler.
TEST(ResourceRequestHandlerTest, RedirectPostViaContext) {
  CefRefPtr<RedirectResponseTest> handler =
      new RedirectResponseTest(RedirectResponseTest::POST, true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

namespace {

const char kResourceTestHtml2[] = "http://test.com/resource2.html";

class BeforeResourceLoadTest : public TestHandler {
 public:
  enum TestMode {
    CANCEL,
    CANCEL_ASYNC,
    CANCEL_NAV,
    CONTINUE,
    CONTINUE_ASYNC,
  };

  explicit BeforeResourceLoadTest(TestMode mode) : test_mode_(mode) {}

  void RunTest() override {
    AddResource(kResourceTestHtml, "<html><body>Test</body></html>",
                "text/html");
    AddResource(kResourceTestHtml2, "<html><body>Test2</body></html>",
                "text/html");
    CreateBrowser(kResourceTestHtml);
    SetTestTimeout();
  }

  cef_return_value_t OnBeforeResourceLoad(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request,
      CefRefPtr<CefCallback> callback) override {
    EXPECT_IO_THREAD();

    if (IsChromeRuntimeEnabled() && request->GetResourceType() == RT_FAVICON) {
      // Ignore favicon requests.
      return RV_CANCEL;
    }

    // Allow the 2nd navigation to continue.
    const std::string& url = request->GetURL();
    if (url == kResourceTestHtml2) {
      got_before_resource_load2_.yes();
      EXPECT_EQ(CANCEL_NAV, test_mode_);
      return RV_CONTINUE;
    }

    EXPECT_FALSE(got_before_resource_load_);
    got_before_resource_load_.yes();

    if (test_mode_ == CANCEL) {
      // Cancel immediately.
      return RV_CANCEL;
    } else if (test_mode_ == CONTINUE) {
      // Continue immediately.
      return RV_CONTINUE;
    } else {
      if (test_mode_ == CANCEL_NAV) {
        // Cancel the request by navigating to a new URL.
        browser->GetMainFrame()->LoadURL(kResourceTestHtml2);
      } else if (test_mode_ == CONTINUE_ASYNC) {
        // Continue asynchronously.
        CefPostTask(TID_UI,
                    base::BindOnce(&CefCallback::Continue, callback.get()));
      } else {
        // Cancel asynchronously.
        CefPostTask(TID_UI,
                    base::BindOnce(&CefCallback::Cancel, callback.get()));
      }
      return RV_CONTINUE_ASYNC;
    }
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    EXPECT_UI_THREAD();

    EXPECT_FALSE(got_load_end_);
    got_load_end_.yes();

    const std::string& url = frame->GetURL();
    if (test_mode_ == CANCEL_NAV)
      EXPECT_STREQ(kResourceTestHtml2, url.data());
    else
      EXPECT_STREQ(kResourceTestHtml, url.data());

    TestHandler::OnLoadEnd(browser, frame, httpStatusCode);
    DestroyTest();
  }

  void OnLoadError(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   ErrorCode errorCode,
                   const CefString& errorText,
                   const CefString& failedUrl) override {
    EXPECT_UI_THREAD();

    EXPECT_FALSE(got_load_error_);
    got_load_error_.yes();

    const std::string& url = failedUrl;
    EXPECT_STREQ(kResourceTestHtml, url.data());

    TestHandler::OnLoadError(browser, frame, errorCode, errorText, failedUrl);
    if (test_mode_ != CANCEL_NAV)
      DestroyTest();
  }

  void DestroyTest() override {
    EXPECT_TRUE(got_before_resource_load_);

    if (test_mode_ == CANCEL_NAV)
      EXPECT_TRUE(got_before_resource_load2_);
    else
      EXPECT_FALSE(got_before_resource_load2_);

    if (test_mode_ == CONTINUE || test_mode_ == CONTINUE_ASYNC) {
      EXPECT_TRUE(got_load_end_);
      EXPECT_FALSE(got_load_error_);
    } else if (test_mode_ == CANCEL || test_mode_ == CANCEL_ASYNC) {
      EXPECT_FALSE(got_load_end_);
      EXPECT_TRUE(got_load_error_);
    }

    TestHandler::DestroyTest();
  }

 private:
  const TestMode test_mode_;

  TrackCallback got_before_resource_load_;
  TrackCallback got_before_resource_load2_;
  TrackCallback got_load_end_;
  TrackCallback got_load_error_;

  IMPLEMENT_REFCOUNTING(BeforeResourceLoadTest);
};

}  // namespace

TEST(ResourceRequestHandlerTest, BeforeResourceLoadCancel) {
  CefRefPtr<BeforeResourceLoadTest> handler =
      new BeforeResourceLoadTest(BeforeResourceLoadTest::CANCEL);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(ResourceRequestHandlerTest, BeforeResourceLoadCancelAsync) {
  CefRefPtr<BeforeResourceLoadTest> handler =
      new BeforeResourceLoadTest(BeforeResourceLoadTest::CANCEL_ASYNC);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(ResourceRequestHandlerTest, BeforeResourceLoadCancelNav) {
  CefRefPtr<BeforeResourceLoadTest> handler =
      new BeforeResourceLoadTest(BeforeResourceLoadTest::CANCEL_NAV);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(ResourceRequestHandlerTest, BeforeResourceLoadContinue) {
  CefRefPtr<BeforeResourceLoadTest> handler =
      new BeforeResourceLoadTest(BeforeResourceLoadTest::CONTINUE);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(ResourceRequestHandlerTest, BeforeResourceLoadContinueAsync) {
  CefRefPtr<BeforeResourceLoadTest> handler =
      new BeforeResourceLoadTest(BeforeResourceLoadTest::CONTINUE_ASYNC);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

namespace {

// For response filtering we need to test:
// - Passing through content unchanged.
// - Not reading all of the input buffer.
// - Needing more input and getting it.
// - Needing more input and not getting it.
// - Filter error.

const char kResponseFilterTestUrl[] = "http://tests.com/response_filter.html";

size_t GetResponseBufferSize() {
  // Match the default |capacity_num_bytes| value from
  // mojo::Core::CreateDataPipe.
  return 64 * 1024;  // 64kb
}

const char kInputHeader[] = "<html><head></head><body>";
const char kInputFooter[] = "</body></html>";

// Repeat |content| the minimum number of times necessary to satisfy
// |desired_min_size|. If |calculated_repeat_ct| is non-nullptr it will be set
// to the number of times that |content| was repeated.
std::string CreateInput(const std::string& content,
                        size_t desired_min_size,
                        size_t* calculated_repeat_ct = nullptr) {
  const size_t header_footer_size =
      sizeof(kInputHeader) + sizeof(kInputFooter) - 2;
  EXPECT_GE(desired_min_size, header_footer_size + content.size());
  desired_min_size -= header_footer_size;

  size_t repeat_ct =
      static_cast<size_t>(std::ceil(static_cast<double>(desired_min_size) /
                                    static_cast<double>(content.size())));
  if (calculated_repeat_ct)
    *calculated_repeat_ct = repeat_ct;

  std::string result;
  result.reserve(header_footer_size + (content.size() * repeat_ct));

  result = kInputHeader;
  while (repeat_ct--)
    result += content;
  result += kInputFooter;

  return result;
}

std::string CreateOutput(const std::string& content, size_t repeat_ct) {
  const size_t header_footer_size =
      sizeof(kInputHeader) + sizeof(kInputFooter) - 2;

  std::string result;
  result.reserve(header_footer_size + (content.size() * repeat_ct));

  result = kInputHeader;
  while (repeat_ct--)
    result += content;
  result += kInputFooter;

  return result;
}

// Base class for test filters.
class ResponseFilterTestBase : public CefResponseFilter {
 public:
  ResponseFilterTestBase() : filter_count_(0U) {}

  bool InitFilter() override {
    EXPECT_FALSE(got_init_filter_);
    got_init_filter_.yes();
    return true;
  }

  FilterStatus Filter(void* data_in,
                      size_t data_in_size,
                      size_t& data_in_read,
                      void* data_out,
                      size_t data_out_size,
                      size_t& data_out_written) override {
    if (data_in_size == 0U)
      EXPECT_FALSE(data_in);
    else
      EXPECT_TRUE(data_in);
    EXPECT_EQ(data_in_read, 0U);
    EXPECT_TRUE(data_out);
    EXPECT_GT(data_out_size, 0U);
    EXPECT_EQ(data_out_written, 0U);
    filter_count_++;
    return RESPONSE_FILTER_ERROR;
  }

  // Returns the input that will be fed into the filter.
  virtual std::string GetInput() = 0;

  // Verify the output from the filter.
  virtual void VerifyOutput(cef_urlrequest_status_t status,
                            int64 received_content_length,
                            const std::string& received_content) {
    EXPECT_TRUE(got_init_filter_);
    EXPECT_GT(filter_count_, 0U);
  }

  virtual void VerifyStatusCode(int httpStatusCode) const {
    EXPECT_TRUE(httpStatusCode == 0 || httpStatusCode == 200) << httpStatusCode;
  }

 protected:
  TrackCallback got_init_filter_;
  size_t filter_count_;

  IMPLEMENT_REFCOUNTING(ResponseFilterTestBase);
};

// Pass through the contents unchanged.
class ResponseFilterPassThru : public ResponseFilterTestBase {
 public:
  explicit ResponseFilterPassThru(bool limit_read) : limit_read_(limit_read) {}

  FilterStatus Filter(void* data_in,
                      size_t data_in_size,
                      size_t& data_in_read,
                      void* data_out,
                      size_t data_out_size,
                      size_t& data_out_written) override {
    ResponseFilterTestBase::Filter(data_in, data_in_size, data_in_read,
                                   data_out, data_out_size, data_out_written);

    if (limit_read_) {
      // Read at most 1k bytes.
      data_in_read = std::min(data_in_size, static_cast<size_t>(1024U));
    } else {
      // Read all available bytes.
      data_in_read = data_in_size;
    }

    data_out_written = std::min(data_in_read, data_out_size);
    memcpy(data_out, data_in, data_out_written);

    return RESPONSE_FILTER_DONE;
  }

  std::string GetInput() override {
    input_ = CreateInput("FOOBAR ", GetResponseBufferSize() * 2U + 1);
    return input_;
  }

  void VerifyOutput(cef_urlrequest_status_t status,
                    int64 received_content_length,
                    const std::string& received_content) override {
    ResponseFilterTestBase::VerifyOutput(status, received_content_length,
                                         received_content);

    if (limit_read_)
      // Expected to read 2 full buffers of GetResponseBufferSize() at 1kb
      // increments and one partial buffer.
      EXPECT_EQ(2U * (GetResponseBufferSize() / 1024) + 1U, filter_count_);
    else {
      // Expected to read 2 full buffers of GetResponseBufferSize() and one
      // partial buffer.
      EXPECT_EQ(3U, filter_count_);
    }
    EXPECT_STREQ(input_.c_str(), received_content.c_str());

    // Input size and content size should match.
    EXPECT_EQ(input_.size(), static_cast<size_t>(received_content_length));
    EXPECT_EQ(input_.size(), received_content.size());
  }

 private:
  std::string input_;
  bool limit_read_;
};

const char kFindString[] = "REPLACE_THIS_STRING";
const char kReplaceString[] = "This is the replaced string!";

// Helper for passing params to Write().
#define WRITE_PARAMS data_out_ptr, data_out_size, data_out_written

// Replace all instances of |kFindString| with |kReplaceString|.
// This implementation is similar to the example in
// tests/shared/response_filter_test.cc.
class ResponseFilterNeedMore : public ResponseFilterTestBase {
 public:
  ResponseFilterNeedMore()
      : find_match_offset_(0U),
        replace_overflow_size_(0U),
        input_size_(0U),
        repeat_ct_(0U) {}

  FilterStatus Filter(void* data_in,
                      size_t data_in_size,
                      size_t& data_in_read,
                      void* data_out,
                      size_t data_out_size,
                      size_t& data_out_written) override {
    ResponseFilterTestBase::Filter(data_in, data_in_size, data_in_read,
                                   data_out, data_out_size, data_out_written);

    // All data will be read.
    data_in_read = data_in_size;

    const size_t find_size = sizeof(kFindString) - 1;

    const char* data_in_ptr = static_cast<char*>(data_in);
    char* data_out_ptr = static_cast<char*>(data_out);

    // Reset the overflow.
    std::string old_overflow;
    if (!overflow_.empty()) {
      old_overflow = overflow_;
      overflow_.clear();
    }

    const size_t likely_out_size =
        data_in_size + replace_overflow_size_ + old_overflow.size();
    if (data_out_size < likely_out_size) {
      // We'll likely need to use the overflow buffer. Size it appropriately.
      overflow_.reserve(likely_out_size - data_out_size);
    }

    if (!old_overflow.empty()) {
      // Write the overflow from last time.
      Write(old_overflow.c_str(), old_overflow.size(), WRITE_PARAMS);
    }

    // Evaluate each character in the input buffer. Track how many characters in
    // a row match kFindString. If kFindString is completely matched then write
    // kReplaceString. Otherwise, write the input characters as-is.
    for (size_t i = 0U; i < data_in_size; ++i) {
      if (data_in_ptr[i] == kFindString[find_match_offset_]) {
        // Matched the next character in the find string.
        if (++find_match_offset_ == find_size) {
          // Complete match of the find string. Write the replace string.
          Write(kReplaceString, sizeof(kReplaceString) - 1, WRITE_PARAMS);

          // Start over looking for a match.
          find_match_offset_ = 0;
        }
        continue;
      }

      // Character did not match the find string.
      if (find_match_offset_ > 0) {
        // Write the portion of the find string that has matched so far.
        Write(kFindString, find_match_offset_, WRITE_PARAMS);

        // Start over looking for a match.
        find_match_offset_ = 0;
      }

      // Write the current character.
      Write(&data_in_ptr[i], 1, WRITE_PARAMS);
    }

    // If a match is currently in-progress and input was provided then we need
    // more data. Otherwise, we're done.
    return find_match_offset_ > 0 && data_in_size > 0
               ? RESPONSE_FILTER_NEED_MORE_DATA
               : RESPONSE_FILTER_DONE;
  }

  std::string GetInput() override {
    const std::string& input =
        CreateInput(std::string(kFindString) + " ",
                    GetResponseBufferSize() * 2U + 1, &repeat_ct_);
    input_size_ = input.size();

    const size_t find_size = sizeof(kFindString) - 1;
    const size_t replace_size = sizeof(kReplaceString) - 1;

    // Determine a reasonable amount of space for find/replace overflow.
    if (replace_size > find_size)
      replace_overflow_size_ = (replace_size - find_size) * repeat_ct_;

    return input;
  }

  void VerifyOutput(cef_urlrequest_status_t status,
                    int64 received_content_length,
                    const std::string& received_content) override {
    ResponseFilterTestBase::VerifyOutput(status, received_content_length,
                                         received_content);

    const std::string& output =
        CreateOutput(std::string(kReplaceString) + " ", repeat_ct_);
    EXPECT_STREQ(output.c_str(), received_content.c_str());

    // Pre-filter content length should be the original input size.
    EXPECT_EQ(input_size_, static_cast<size_t>(received_content_length));

    // Filtered content length should be the output size.
    EXPECT_EQ(output.size(), received_content.size());

    // Expected to read 2 full buffers of GetResponseBufferSize() and one
    // partial buffer, and then one additional call to drain the overflow.
    EXPECT_EQ(4U, filter_count_);
  }

 private:
  inline void Write(const char* str,
                    size_t str_size,
                    char*& data_out_ptr,
                    size_t data_out_size,
                    size_t& data_out_written) {
    // Number of bytes remaining in the output buffer.
    const size_t remaining_space = data_out_size - data_out_written;
    // Maximum number of bytes we can write into the output buffer.
    const size_t max_write = std::min(str_size, remaining_space);

    // Write the maximum portion that fits in the output buffer.
    if (max_write == 1) {
      // Small optimization for single character writes.
      *data_out_ptr = str[0];
      data_out_ptr += 1;
      data_out_written += 1;
    } else if (max_write > 1) {
      memcpy(data_out_ptr, str, max_write);
      data_out_ptr += max_write;
      data_out_written += max_write;
    }

    if (max_write < str_size) {
      // Need to write more bytes than will fit in the output buffer. Store the
      // remainder in the overflow buffer.
      overflow_ += std::string(str + max_write, str_size - max_write);
    }
  }

  // The portion of the find string that is currently matching.
  size_t find_match_offset_;

  // The likely amount of overflow.
  size_t replace_overflow_size_;

  // Overflow from the output buffer.
  std::string overflow_;

  // The original input size.
  size_t input_size_;

  // The number of times the find string was repeated.
  size_t repeat_ct_;
};

// Return a filter error.
class ResponseFilterError : public ResponseFilterTestBase {
 public:
  ResponseFilterError() {}

  FilterStatus Filter(void* data_in,
                      size_t data_in_size,
                      size_t& data_in_read,
                      void* data_out,
                      size_t data_out_size,
                      size_t& data_out_written) override {
    ResponseFilterTestBase::Filter(data_in, data_in_size, data_in_read,
                                   data_out, data_out_size, data_out_written);

    return RESPONSE_FILTER_ERROR;
  }

  std::string GetInput() override {
    return kInputHeader + std::string("ERROR") + kInputFooter;
  }

  void VerifyOutput(cef_urlrequest_status_t status,
                    int64 received_content_length,
                    const std::string& received_content) override {
    ResponseFilterTestBase::VerifyOutput(status, received_content_length,
                                         received_content);

    EXPECT_EQ(UR_FAILED, status);

    // Expect empty content.
    EXPECT_STREQ("", received_content.c_str());
    EXPECT_EQ(0U, received_content_length);

    // Expect to only be called one time.
    EXPECT_EQ(filter_count_, 1U);
  }

  void VerifyStatusCode(int httpStatusCode) const override {
    EXPECT_EQ(ERR_CONTENT_DECODING_FAILED, httpStatusCode);
  }
};

class ResponseFilterTestHandler : public TestHandler {
 public:
  explicit ResponseFilterTestHandler(
      CefRefPtr<ResponseFilterTestBase> response_filter)
      : response_filter_(response_filter) {}

  void RunTest() override {
    const std::string& resource = response_filter_->GetInput();
    AddResource(kResponseFilterTestUrl, resource, "text/html");

    // Create the browser.
    CreateBrowser(kResponseFilterTestUrl);

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  CefRefPtr<CefResponseFilter> GetResourceResponseFilter(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request,
      CefRefPtr<CefResponse> response) override {
    EXPECT_IO_THREAD();

    DCHECK(!got_resource_response_filter_);
    got_resource_response_filter_.yes();
    return response_filter_;
  }

  void OnResourceLoadComplete(CefRefPtr<CefBrowser> browser,
                              CefRefPtr<CefFrame> frame,
                              CefRefPtr<CefRequest> request,
                              CefRefPtr<CefResponse> response,
                              URLRequestStatus status,
                              int64 received_content_length) override {
    EXPECT_IO_THREAD();

    if (IsChromeRuntimeEnabled() && request->GetResourceType() == RT_FAVICON) {
      // Ignore favicon requests.
      return;
    }

    DCHECK(!got_resource_load_complete_);
    got_resource_load_complete_.yes();

    status_ = status;
    received_content_length_ = received_content_length;
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    DCHECK(!got_load_end_);
    got_load_end_.yes();

    response_filter_->VerifyStatusCode(httpStatusCode);

    GetOutputContent(frame);
  }

 private:
  // Retrieve the output content using a StringVisitor. This effectively
  // serializes the DOM from the renderer process so any comparison to the
  // filter output is somewhat error-prone.
  void GetOutputContent(CefRefPtr<CefFrame> frame) {
    class StringVisitor : public CefStringVisitor {
     public:
      using VisitorCallback =
          base::OnceCallback<void(const std::string& /*received_content*/)>;

      explicit StringVisitor(VisitorCallback callback)
          : callback_(std::move(callback)) {}

      void Visit(const CefString& string) override {
        std::move(callback_).Run(string);
      }

     private:
      VisitorCallback callback_;

      IMPLEMENT_REFCOUNTING(StringVisitor);
    };

    frame->GetSource(new StringVisitor(
        base::BindOnce(&ResponseFilterTestHandler::VerifyOutput, this)));
  }

  void VerifyOutput(const std::string& received_content) {
    response_filter_->VerifyOutput(status_, received_content_length_,
                                   received_content);
    response_filter_ = nullptr;

    DestroyTest();
  }

  void DestroyTest() override {
    EXPECT_TRUE(got_resource_response_filter_);
    EXPECT_TRUE(got_resource_load_complete_);
    EXPECT_TRUE(got_load_end_);

    TestHandler::DestroyTest();
  }

  CefRefPtr<ResponseFilterTestBase> response_filter_;

  TrackCallback got_resource_response_filter_;
  TrackCallback got_resource_load_complete_;
  TrackCallback got_load_end_;

  URLRequestStatus status_;
  int64 received_content_length_;

  IMPLEMENT_REFCOUNTING(ResponseFilterTestHandler);
};

}  // namespace

// Pass through contents unchanged. Read all available input.
TEST(ResourceRequestHandlerTest, FilterPassThruReadAll) {
  CefRefPtr<ResponseFilterTestHandler> handler =
      new ResponseFilterTestHandler(new ResponseFilterPassThru(false));
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Pass through contents unchanged. Read limited input.
TEST(ResourceRequestHandlerTest, FilterPassThruReadLimited) {
  CefRefPtr<ResponseFilterTestHandler> handler =
      new ResponseFilterTestHandler(new ResponseFilterPassThru(true));
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Find/replace contents such that we occasionally need more data.
TEST(ResourceRequestHandlerTest, FilterNeedMore) {
  CefRefPtr<ResponseFilterTestHandler> handler =
      new ResponseFilterTestHandler(new ResponseFilterNeedMore());
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Error during filtering.
TEST(ResourceRequestHandlerTest, FilterError) {
  CefRefPtr<ResponseFilterTestHandler> handler =
      new ResponseFilterTestHandler(new ResponseFilterError());
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Entry point for registering custom schemes.
// Called from client_app_delegates.cc.
void RegisterResourceRequestHandlerCustomSchemes(
    CefRawPtr<CefSchemeRegistrar> registrar) {
  // Add a custom standard scheme.
  registrar->AddCustomScheme(
      "rrhcustom", CEF_SCHEME_OPTION_STANDARD | CEF_SCHEME_OPTION_CORS_ENABLED);
}
