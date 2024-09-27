// Copyright 2022 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include <string>
#include <vector>

#include "include/base/cef_bind.h"
#include "include/cef_parser.h"
#include "include/cef_permission_handler.h"
#include "include/cef_request_context_handler.h"
#include "include/test/cef_test_helpers.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_stream_resource_handler.h"
#include "tests/ceftests/test_handler.h"
#include "tests/ceftests/test_suite.h"
#include "tests/ceftests/test_util.h"
#include "tests/gtest/include/gtest/gtest.h"
#include "tests/shared/browser/client_app_browser.h"

namespace {

// Media access requires HTTPS.
const char kMediaUrl[] = "https://media-access-test/media.html";
const char kMediaOrigin[] = "https://media-access-test/";

constexpr char kMediaNavUrl[] = "https://media-access-test/nav.html";

// Browser-side app delegate.
class MediaAccessBrowserTest : public client::ClientAppBrowser::Delegate,
                               public CefPermissionHandler {
 public:
  MediaAccessBrowserTest() = default;

  void OnBeforeCommandLineProcessing(
      CefRefPtr<client::ClientAppBrowser> app,
      CefRefPtr<CefCommandLine> command_line) override {
    // We might run tests on systems that don't have media device,
    // so just use fake devices.
    command_line->AppendSwitch("use-fake-device-for-media-stream");
  }

 private:
  IMPLEMENT_REFCOUNTING(MediaAccessBrowserTest);
};

class TestSetup {
 public:
  TestSetup() = default;

  // CONFIGURATION

  // True if a user gesture is required for the getDisplayMedia call.
  bool needs_user_gesture = false;

  // Deny the prompt by returning false in OnRequestMediaAccessPermission.
  bool deny_implicitly = false;

  // Deny the prompt by returning true in OnRequestMediaAccessPermission but
  // then never calling CefMediaAccessCallback::Continue.
  bool deny_with_navigation = false;

  // Don't synchronously execute the callback in OnRequestMediaAccessPermission.
  bool continue_async = false;

  // RESULTS

  // Method callbacks.
  TrackCallback got_request;
  TrackCallback got_change;

  // JS success state.
  TrackCallback got_js_success;
  TrackCallback got_audio;
  TrackCallback got_video;

  // JS error state.
  TrackCallback got_js_error;
  std::string js_error_str;

  // JS timeout state.
  TrackCallback got_js_timeout;
};

class MediaAccessTestHandler : public TestHandler, public CefPermissionHandler {
 public:
  MediaAccessTestHandler(TestSetup* tr, uint32_t request, uint32_t response)
      : test_setup_(tr), request_(request), response_(response) {}

  cef_return_value_t OnBeforeResourceLoad(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request,
      CefRefPtr<CefCallback> callback) override {
    std::string newUrl = request->GetURL();
    if (newUrl.find("tests/exit") != std::string::npos) {
      if (newUrl.find("SUCCESS") != std::string::npos) {
        EXPECT_FALSE(test_setup_->got_js_success);
        test_setup_->got_js_success.yes();

        auto dict = ParseURLData(newUrl);
        if (dict->GetBool("got_video_track")) {
          test_setup_->got_video.yes();
        }
        if (dict->GetBool("got_audio_track")) {
          test_setup_->got_audio.yes();
        }
      } else if (newUrl.find("ERROR") != std::string::npos) {
        EXPECT_FALSE(test_setup_->got_js_error);
        test_setup_->got_js_error.yes();

        auto dict = ParseURLData(newUrl);
        test_setup_->js_error_str = dict->GetString("error_str");
      } else if (newUrl.find("TIMEOUT") != std::string::npos) {
        EXPECT_FALSE(test_setup_->got_js_timeout);
        test_setup_->got_js_timeout.yes();
      }
      DestroyTest();
      return RV_CANCEL;
    }

    return RV_CONTINUE;
  }

  void RunTest() override {
    std::string page =
        "<html><head>"
        "<script>"
        "function onResult(val, data) {"
        " if(!data) {"
        "   data = {};"
        " }"
        " document.location = "
        "`https://tests/"
        "exit?result=${val}&data=${encodeURIComponent(JSON.stringify(data))}`;"
        "}"
        "function runTest() {";

    if (want_audio_device() || want_video_device()) {
      page += std::string("navigator.mediaDevices.getUserMedia({audio: ") +
              (want_audio_device() ? "true" : "false") +
              ", video: " + (want_video_device() ? "true" : "false") + "})";
    } else {
      page += std::string("navigator.mediaDevices.getDisplayMedia({audio: ") +
              (want_audio_desktop() ? "true" : "false") +
              ", video: " + (want_video_desktop() ? "true" : "false") + "})";
    }

    page +=
        ".then(function(stream) {"
        "  onResult(`SUCCESS`, {got_audio_track: "
        "stream.getAudioTracks().length "
        "> 0, got_video_track: stream.getVideoTracks().length > 0});"
        "})"
        ".catch(function(err) {"
        "  console.log(err.toString());"
        "  onResult(`ERROR`, {error_str: err.toString()});"
        "});"
        "}";

    if (test_setup_->deny_implicitly && !use_alloy_style_browser()) {
      // Default behavior with Chrome style is to show a UI prompt, so add
      // a timeout.
      page += "setTimeout(() => { onResult(`TIMEOUT`); }, 1000);";
    } else if (test_setup_->deny_with_navigation) {
      // Cancel the pending request by navigating.
      page += "setTimeout(() => { document.location = '" +
              std::string(kMediaNavUrl) + "'; }, 1000);";
    }

    page +=
        "</script>"
        "</head><body>";

    if (!test_setup_->needs_user_gesture) {
      page += "<script>runTest();</script>";
    }

    page += "MEDIA ACCESS TEST</body></html>";

    // Create the request context that will use an in-memory cache.
    CefRequestContextSettings settings;
    CefRefPtr<CefRequestContext> request_context =
        CefRequestContext::CreateContext(settings, nullptr);

    AddResource(kMediaUrl, page, "text/html");

    if (test_setup_->deny_with_navigation) {
      AddResource(kMediaNavUrl, "<html><body>Navigated</body></html>",
                  "text/html");
    }

    // Create the browser.
    CreateBrowser(kMediaUrl, request_context);

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  CefRefPtr<CefPermissionHandler> GetPermissionHandler() override {
    return this;
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    if (test_setup_->deny_with_navigation) {
      if (frame->GetURL().ToString() == kMediaNavUrl) {
        DestroyTest();
      }
    } else if (test_setup_->needs_user_gesture) {
      CefExecuteJavaScriptWithUserGestureForTests(frame, "runTest()");
    }
  }

  bool OnRequestMediaAccessPermission(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      const CefString& requesting_origin,
      uint32_t requested_permissions,
      CefRefPtr<CefMediaAccessCallback> callback) override {
    EXPECT_UI_THREAD();
    EXPECT_TRUE(frame->IsMain());

    EXPECT_EQ(requested_permissions, request_);
    EXPECT_STREQ(kMediaOrigin, requesting_origin.ToString().c_str());

    EXPECT_FALSE(test_setup_->got_request);
    test_setup_->got_request.yes();

    if (test_setup_->deny_implicitly) {
      return false;
    }

    if (test_setup_->deny_with_navigation) {
      // Handle the request, but never execute the callback.
      callback_ = callback;
      return true;
    }

    if (test_setup_->continue_async) {
      CefPostTask(TID_UI, base::BindOnce(&CefMediaAccessCallback::Continue,
                                         callback, response_));
    } else {
      callback->Continue(response_);
    }
    return true;
  }

  void OnMediaAccessChange(CefRefPtr<CefBrowser> browser,
                           bool has_video_access,
                           bool has_audio_access) override {
    EXPECT_UI_THREAD();
    EXPECT_EQ(got_video_device() || got_video_desktop(), has_video_access);
    EXPECT_EQ(got_audio_device() || got_audio_desktop(), has_audio_access);
    EXPECT_FALSE(test_setup_->got_change);
    test_setup_->got_change.yes();
  }

  void DestroyTest() override {
    callback_ = nullptr;

    const size_t js_outcome_ct = test_setup_->got_js_success +
                                 test_setup_->got_js_error +
                                 test_setup_->got_js_timeout;
    if (test_setup_->deny_with_navigation) {
      // Expect no JS outcome.
      EXPECT_EQ(0U, js_outcome_ct);
    } else {
      // Expect a single JS outcome.
      EXPECT_EQ(1U, js_outcome_ct);
    }

    TestHandler::DestroyTest();
  }

 private:
  bool want_audio_device() const {
    return request_ & CEF_MEDIA_PERMISSION_DEVICE_AUDIO_CAPTURE;
  }
  bool want_video_device() const {
    return request_ & CEF_MEDIA_PERMISSION_DEVICE_VIDEO_CAPTURE;
  }
  bool want_audio_desktop() const {
    return request_ & CEF_MEDIA_PERMISSION_DESKTOP_AUDIO_CAPTURE;
  }
  bool want_video_desktop() const {
    return request_ & CEF_MEDIA_PERMISSION_DESKTOP_VIDEO_CAPTURE;
  }

  bool got_audio_device() const {
    return response_ & CEF_MEDIA_PERMISSION_DEVICE_AUDIO_CAPTURE;
  }
  bool got_video_device() const {
    return response_ & CEF_MEDIA_PERMISSION_DEVICE_VIDEO_CAPTURE;
  }
  bool got_audio_desktop() const {
    return response_ & CEF_MEDIA_PERMISSION_DESKTOP_AUDIO_CAPTURE;
  }
  bool got_video_desktop() const {
    return response_ & CEF_MEDIA_PERMISSION_DESKTOP_VIDEO_CAPTURE;
  }

  CefRefPtr<CefDictionaryValue> ParseURLData(const std::string& url) {
    const std::string& find_str = "&data=";
    const std::string& data_string =
        url.substr(url.find(find_str) + std::string(find_str).length());
    const std::string& data_string_decoded = CefURIDecode(
        data_string, false,
        static_cast<cef_uri_unescape_rule_t>(
            UU_SPACES | UU_URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS));
    auto obj =
        CefParseJSON(data_string_decoded, JSON_PARSER_ALLOW_TRAILING_COMMAS);
    return obj->GetDictionary();
  }

  TestSetup* const test_setup_;
  const uint32_t request_;
  const uint32_t response_;

  CefRefPtr<CefMediaAccessCallback> callback_;

  IMPLEMENT_REFCOUNTING(MediaAccessTestHandler);
};
}  // namespace

// Capture device tests
TEST(MediaAccessTest, DeviceFailureWhenReturningFalse) {
  TestSetup test_setup;
  test_setup.deny_implicitly = true;

  CefRefPtr<MediaAccessTestHandler> handler =
      new MediaAccessTestHandler(&test_setup,
                                 CEF_MEDIA_PERMISSION_DEVICE_AUDIO_CAPTURE |
                                     CEF_MEDIA_PERMISSION_DEVICE_VIDEO_CAPTURE,
                                 CEF_MEDIA_PERMISSION_NONE);
  const bool use_alloy_style_browser = handler->use_alloy_style_browser();
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_setup.got_request);
  if (!use_alloy_style_browser) {
    // Chrome shows a UI prompt, so we time out.
    EXPECT_TRUE(test_setup.got_js_timeout);
  } else {
    EXPECT_TRUE(test_setup.got_js_error);
    EXPECT_STREQ("NotAllowedError: Permission denied",
                 test_setup.js_error_str.c_str());
  }
  EXPECT_FALSE(test_setup.got_change);
}

TEST(MediaAccessTest, DeviceFailureWhenNoCallback) {
  TestSetup test_setup;
  test_setup.deny_with_navigation = true;

  CefRefPtr<MediaAccessTestHandler> handler =
      new MediaAccessTestHandler(&test_setup,
                                 CEF_MEDIA_PERMISSION_DEVICE_AUDIO_CAPTURE |
                                     CEF_MEDIA_PERMISSION_DEVICE_VIDEO_CAPTURE,
                                 CEF_MEDIA_PERMISSION_NONE);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  // No JS result.
  EXPECT_TRUE(test_setup.got_request);
  EXPECT_FALSE(test_setup.got_change);
}

TEST(MediaAccessTest, DeviceFailureWhenReturningNoPermission) {
  TestSetup test_setup;

  CefRefPtr<MediaAccessTestHandler> handler =
      new MediaAccessTestHandler(&test_setup,
                                 CEF_MEDIA_PERMISSION_DEVICE_AUDIO_CAPTURE |
                                     CEF_MEDIA_PERMISSION_DEVICE_VIDEO_CAPTURE,
                                 CEF_MEDIA_PERMISSION_NONE);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_setup.got_request);
  EXPECT_TRUE(test_setup.got_js_error);
  EXPECT_STREQ("NotAllowedError: Permission denied",
               test_setup.js_error_str.c_str());
  EXPECT_FALSE(test_setup.got_change);
}

TEST(MediaAccessTest, DeviceFailureWhenReturningNoPermissionAsync) {
  TestSetup test_setup;
  test_setup.continue_async = true;

  CefRefPtr<MediaAccessTestHandler> handler =
      new MediaAccessTestHandler(&test_setup,
                                 CEF_MEDIA_PERMISSION_DEVICE_AUDIO_CAPTURE |
                                     CEF_MEDIA_PERMISSION_DEVICE_VIDEO_CAPTURE,
                                 CEF_MEDIA_PERMISSION_NONE);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_setup.got_request);
  EXPECT_TRUE(test_setup.got_js_error);
  EXPECT_STREQ("NotAllowedError: Permission denied",
               test_setup.js_error_str.c_str());
  EXPECT_FALSE(test_setup.got_change);
}

TEST(MediaAccessTest, DeviceFailureWhenRequestingAudioButReturningVideo) {
  TestSetup test_setup;

  CefRefPtr<MediaAccessTestHandler> handler = new MediaAccessTestHandler(
      &test_setup, CEF_MEDIA_PERMISSION_DEVICE_AUDIO_CAPTURE,
      CEF_MEDIA_PERMISSION_DEVICE_VIDEO_CAPTURE);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_setup.got_request);
  EXPECT_TRUE(test_setup.got_js_error);
  EXPECT_STREQ("AbortError: Invalid state", test_setup.js_error_str.c_str());
  EXPECT_FALSE(test_setup.got_change);
}

TEST(MediaAccessTest, DeviceFailureWhenRequestingVideoButReturningAudio) {
  TestSetup test_setup;

  CefRefPtr<MediaAccessTestHandler> handler = new MediaAccessTestHandler(
      &test_setup, CEF_MEDIA_PERMISSION_DEVICE_VIDEO_CAPTURE,
      CEF_MEDIA_PERMISSION_DEVICE_AUDIO_CAPTURE);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_setup.got_request);
  EXPECT_TRUE(test_setup.got_js_error);
  EXPECT_STREQ("AbortError: Invalid state", test_setup.js_error_str.c_str());
  EXPECT_FALSE(test_setup.got_change);
}

TEST(MediaAccessTest, DevicePartialFailureReturningVideo) {
  TestSetup test_setup;

  CefRefPtr<MediaAccessTestHandler> handler =
      new MediaAccessTestHandler(&test_setup,
                                 CEF_MEDIA_PERMISSION_DEVICE_AUDIO_CAPTURE |
                                     CEF_MEDIA_PERMISSION_DEVICE_VIDEO_CAPTURE,
                                 CEF_MEDIA_PERMISSION_DEVICE_VIDEO_CAPTURE);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_setup.got_request);
  EXPECT_TRUE(test_setup.got_js_error);
  EXPECT_STREQ("AbortError: Invalid state", test_setup.js_error_str.c_str());
  EXPECT_FALSE(test_setup.got_change);
}

TEST(MediaAccessTest, DevicePartialFailureReturningAudio) {
  TestSetup test_setup;

  CefRefPtr<MediaAccessTestHandler> handler =
      new MediaAccessTestHandler(&test_setup,
                                 CEF_MEDIA_PERMISSION_DEVICE_AUDIO_CAPTURE |
                                     CEF_MEDIA_PERMISSION_DEVICE_VIDEO_CAPTURE,
                                 CEF_MEDIA_PERMISSION_DEVICE_AUDIO_CAPTURE);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_setup.got_request);
  EXPECT_TRUE(test_setup.got_js_error);
  EXPECT_STREQ("AbortError: Invalid state", test_setup.js_error_str.c_str());
  EXPECT_FALSE(test_setup.got_change);
}

TEST(MediaAccessTest, DeviceFailureWhenReturningScreenCapture1) {
  TestSetup test_setup;

  CefRefPtr<MediaAccessTestHandler> handler =
      new MediaAccessTestHandler(&test_setup,
                                 CEF_MEDIA_PERMISSION_DEVICE_AUDIO_CAPTURE |
                                     CEF_MEDIA_PERMISSION_DEVICE_VIDEO_CAPTURE,
                                 CEF_MEDIA_PERMISSION_DESKTOP_AUDIO_CAPTURE);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_setup.got_request);
  EXPECT_TRUE(test_setup.got_js_error);
  EXPECT_STREQ("AbortError: Invalid state", test_setup.js_error_str.c_str());
  EXPECT_FALSE(test_setup.got_change);
}

TEST(MediaAccessTest, DeviceFailureWhenReturningScreenCapture2) {
  TestSetup test_setup;

  CefRefPtr<MediaAccessTestHandler> handler =
      new MediaAccessTestHandler(&test_setup,
                                 CEF_MEDIA_PERMISSION_DEVICE_AUDIO_CAPTURE |
                                     CEF_MEDIA_PERMISSION_DEVICE_VIDEO_CAPTURE,
                                 CEF_MEDIA_PERMISSION_DESKTOP_VIDEO_CAPTURE);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_setup.got_request);
  EXPECT_TRUE(test_setup.got_js_error);
  EXPECT_STREQ("AbortError: Invalid state", test_setup.js_error_str.c_str());
  EXPECT_FALSE(test_setup.got_change);
}

TEST(MediaAccessTest, DeviceFailureWhenReturningScreenCapture3) {
  TestSetup test_setup;

  CefRefPtr<MediaAccessTestHandler> handler = new MediaAccessTestHandler(
      &test_setup, CEF_MEDIA_PERMISSION_DEVICE_AUDIO_CAPTURE,
      CEF_MEDIA_PERMISSION_DESKTOP_VIDEO_CAPTURE);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_setup.got_request);
  EXPECT_TRUE(test_setup.got_js_error);
  EXPECT_STREQ("AbortError: Invalid state", test_setup.js_error_str.c_str());
  EXPECT_FALSE(test_setup.got_change);
}

TEST(MediaAccessTest, DeviceFailureWhenReturningScreenCapture4) {
  TestSetup test_setup;

  CefRefPtr<MediaAccessTestHandler> handler = new MediaAccessTestHandler(
      &test_setup, CEF_MEDIA_PERMISSION_DEVICE_AUDIO_CAPTURE,
      CEF_MEDIA_PERMISSION_DESKTOP_AUDIO_CAPTURE);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_setup.got_request);
  EXPECT_TRUE(test_setup.got_js_error);
  EXPECT_STREQ("AbortError: Invalid state", test_setup.js_error_str.c_str());
  EXPECT_FALSE(test_setup.got_change);
}

TEST(MediaAccessTest, DeviceFailureWhenReturningScreenCapture5) {
  TestSetup test_setup;

  CefRefPtr<MediaAccessTestHandler> handler = new MediaAccessTestHandler(
      &test_setup, CEF_MEDIA_PERMISSION_DEVICE_VIDEO_CAPTURE,
      CEF_MEDIA_PERMISSION_DESKTOP_VIDEO_CAPTURE);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_setup.got_request);
  EXPECT_TRUE(test_setup.got_js_error);
  EXPECT_STREQ("AbortError: Invalid state", test_setup.js_error_str.c_str());
  EXPECT_FALSE(test_setup.got_change);
}

TEST(MediaAccessTest, DeviceFailureWhenReturningScreenCapture6) {
  TestSetup test_setup;

  CefRefPtr<MediaAccessTestHandler> handler = new MediaAccessTestHandler(
      &test_setup, CEF_MEDIA_PERMISSION_DEVICE_VIDEO_CAPTURE,
      CEF_MEDIA_PERMISSION_DESKTOP_AUDIO_CAPTURE);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_setup.got_request);
  EXPECT_TRUE(test_setup.got_js_error);
  EXPECT_STREQ("AbortError: Invalid state", test_setup.js_error_str.c_str());
  EXPECT_FALSE(test_setup.got_change);
}

TEST(MediaAccessTest, DeviceSuccessAudioOnly) {
  TestSetup test_setup;

  CefRefPtr<MediaAccessTestHandler> handler = new MediaAccessTestHandler(
      &test_setup, CEF_MEDIA_PERMISSION_DEVICE_AUDIO_CAPTURE,
      CEF_MEDIA_PERMISSION_DEVICE_AUDIO_CAPTURE);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_setup.got_request);
  EXPECT_TRUE(test_setup.got_js_success);
  EXPECT_TRUE(test_setup.got_audio);
  EXPECT_FALSE(test_setup.got_video);
  EXPECT_TRUE(test_setup.got_change);
}

TEST(MediaAccessTest, DeviceSuccessVideoOnly) {
  TestSetup test_setup;

  CefRefPtr<MediaAccessTestHandler> handler = new MediaAccessTestHandler(
      &test_setup, CEF_MEDIA_PERMISSION_DEVICE_VIDEO_CAPTURE,
      CEF_MEDIA_PERMISSION_DEVICE_VIDEO_CAPTURE);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_setup.got_request);
  EXPECT_TRUE(test_setup.got_js_success);
  EXPECT_FALSE(test_setup.got_audio);
  EXPECT_TRUE(test_setup.got_video);
  EXPECT_TRUE(test_setup.got_change);
}

TEST(MediaAccessTest, DeviceSuccessAudioVideo) {
  TestSetup test_setup;

  CefRefPtr<MediaAccessTestHandler> handler =
      new MediaAccessTestHandler(&test_setup,
                                 CEF_MEDIA_PERMISSION_DEVICE_VIDEO_CAPTURE |
                                     CEF_MEDIA_PERMISSION_DEVICE_AUDIO_CAPTURE,
                                 CEF_MEDIA_PERMISSION_DEVICE_VIDEO_CAPTURE |
                                     CEF_MEDIA_PERMISSION_DEVICE_AUDIO_CAPTURE);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_setup.got_request);
  EXPECT_TRUE(test_setup.got_js_success);
  EXPECT_TRUE(test_setup.got_audio);
  EXPECT_TRUE(test_setup.got_video);
  EXPECT_TRUE(test_setup.got_change);
}

TEST(MediaAccessTest, DeviceSuccessAudioVideoAsync) {
  TestSetup test_setup;
  test_setup.continue_async = true;

  CefRefPtr<MediaAccessTestHandler> handler =
      new MediaAccessTestHandler(&test_setup,
                                 CEF_MEDIA_PERMISSION_DEVICE_VIDEO_CAPTURE |
                                     CEF_MEDIA_PERMISSION_DEVICE_AUDIO_CAPTURE,
                                 CEF_MEDIA_PERMISSION_DEVICE_VIDEO_CAPTURE |
                                     CEF_MEDIA_PERMISSION_DEVICE_AUDIO_CAPTURE);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_setup.got_request);
  EXPECT_TRUE(test_setup.got_js_success);
  EXPECT_TRUE(test_setup.got_audio);
  EXPECT_TRUE(test_setup.got_video);
  EXPECT_TRUE(test_setup.got_change);
}

// Screen capture tests
TEST(MediaAccessTest, DesktopFailureWhenReturningNoPermission) {
  TestSetup test_setup;
  test_setup.needs_user_gesture = true;

  CefRefPtr<MediaAccessTestHandler> handler =
      new MediaAccessTestHandler(&test_setup,
                                 CEF_MEDIA_PERMISSION_DESKTOP_AUDIO_CAPTURE |
                                     CEF_MEDIA_PERMISSION_DESKTOP_VIDEO_CAPTURE,
                                 CEF_MEDIA_PERMISSION_NONE);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_setup.got_request);
  EXPECT_TRUE(test_setup.got_js_error);
  EXPECT_STREQ("NotAllowedError: Permission denied",
               test_setup.js_error_str.c_str());
  EXPECT_FALSE(test_setup.got_change);
}

TEST(MediaAccessTest, DesktopFailureWhenRequestingVideoButReturningAudio) {
  TestSetup test_setup;
  test_setup.needs_user_gesture = true;

  CefRefPtr<MediaAccessTestHandler> handler = new MediaAccessTestHandler(
      &test_setup, CEF_MEDIA_PERMISSION_DESKTOP_VIDEO_CAPTURE,
      CEF_MEDIA_PERMISSION_DESKTOP_AUDIO_CAPTURE);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_setup.got_request);
  EXPECT_TRUE(test_setup.got_js_error);
  EXPECT_STREQ("AbortError: Invalid state", test_setup.js_error_str.c_str());
  EXPECT_FALSE(test_setup.got_change);
}

TEST(MediaAccessTest, DesktopPartialSuccessReturningVideo) {
  TestSetup test_setup;
  test_setup.needs_user_gesture = true;

  CefRefPtr<MediaAccessTestHandler> handler =
      new MediaAccessTestHandler(&test_setup,
                                 CEF_MEDIA_PERMISSION_DESKTOP_AUDIO_CAPTURE |
                                     CEF_MEDIA_PERMISSION_DESKTOP_VIDEO_CAPTURE,
                                 CEF_MEDIA_PERMISSION_DESKTOP_VIDEO_CAPTURE);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_setup.got_request);
  EXPECT_TRUE(test_setup.got_js_success);
  EXPECT_FALSE(test_setup.got_audio);
  EXPECT_TRUE(test_setup.got_video);
  EXPECT_TRUE(test_setup.got_change);
}

TEST(MediaAccessTest, DesktopPartialFailureReturningAudio) {
  TestSetup test_setup;
  test_setup.needs_user_gesture = true;

  CefRefPtr<MediaAccessTestHandler> handler =
      new MediaAccessTestHandler(&test_setup,
                                 CEF_MEDIA_PERMISSION_DESKTOP_AUDIO_CAPTURE |
                                     CEF_MEDIA_PERMISSION_DESKTOP_VIDEO_CAPTURE,
                                 CEF_MEDIA_PERMISSION_DESKTOP_AUDIO_CAPTURE);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_setup.got_request);
  EXPECT_TRUE(test_setup.got_js_error);
  EXPECT_STREQ("AbortError: Invalid state", test_setup.js_error_str.c_str());
  EXPECT_FALSE(test_setup.got_change);
}

// Entry point for creating media access browser test objects.
// Called from client_app_delegates.cc.
void CreateMediaAccessBrowserTests(
    client::ClientAppBrowser::DelegateSet& delegates) {
  delegates.insert(new MediaAccessBrowserTest);
}
