// Copyright 2022 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include <string>
#include <vector>

#include "include/base/cef_bind.h"
#include "include/cef_parser.h"
#include "include/cef_permission_handler.h"
#include "include/cef_request_context_handler.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_stream_resource_handler.h"
#include "tests/ceftests/test_handler.h"
#include "tests/ceftests/test_suite.h"
#include "tests/gtest/include/gtest/gtest.h"
#include "tests/shared/browser/client_app_browser.h"

namespace {

// Media access requires HTTPS.
const char kMediaUrl[] = "https://media-access-test/media.html";

// Browser-side app delegate.
class MediaAccessBrowserTest : public client::ClientAppBrowser::Delegate,
                               public CefPermissionHandler {
 public:
  MediaAccessBrowserTest() {}

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
  TestSetup() {}

  bool deny_implicitly = false;
  bool continue_async = false;

  TrackCallback got_success;
  TrackCallback got_audio;
  TrackCallback got_video;
  TrackCallback got_change;
};

class MediaAccessTestHandler : public TestHandler, public CefPermissionHandler {
 public:
  MediaAccessTestHandler(TestSetup* tr, uint32 request, uint32 response)
      : test_setup_(tr), request_(request), response_(response) {}

  cef_return_value_t OnBeforeResourceLoad(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request,
      CefRefPtr<CefCallback> callback) override {
    std::string newUrl = request->GetURL();
    if (newUrl.find("tests/exit") != std::string::npos) {
      CefURLParts url_parts;
      CefParseURL(newUrl, url_parts);
      if (newUrl.find("SUCCESS") != std::string::npos) {
        test_setup_->got_success.yes();
        std::string data_string = newUrl.substr(newUrl.find("&data=") +
                                                std::string("&data=").length());
        std::string data_string_decoded = CefURIDecode(
            data_string, false,
            static_cast<cef_uri_unescape_rule_t>(
                UU_SPACES | UU_URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS));
        auto obj = CefParseJSON(data_string_decoded,
                                JSON_PARSER_ALLOW_TRAILING_COMMAS);
        CefRefPtr<CefDictionaryValue> data = obj->GetDictionary();
        const auto got_video = data->GetBool("got_video_track");
        const auto got_audio = data->GetBool("got_audio_track");
        if (got_video) {
          test_setup_->got_video.yes();
        }
        if (got_audio) {
          test_setup_->got_audio.yes();
        }
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
        "   data = { got_audio_track: false, got_video_track: false};"
        " }"
        " document.location = "
        "`http://tests/"
        "exit?result=${val}&data=${encodeURIComponent(JSON.stringify(data))}`;"
        "}";

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
        "onResult(`SUCCESS`, {got_audio_track: stream.getAudioTracks().length "
        "> 0, got_video_track: stream.getVideoTracks().length > 0});"
        "})"
        ".catch(function(err) {"
        "console.log(err);"
        "onResult(`FAILURE`);"
        "});"
        "</script>"
        "</head><body>MEDIA ACCESS TEST</body></html>";

    // Create the request context that will use an in-memory cache.
    CefRequestContextSettings settings;
    CefRefPtr<CefRequestContext> request_context =
        CefRequestContext::CreateContext(settings, nullptr);

    AddResource(kMediaUrl, page, "text/html");

    // Create the browser.
    CreateBrowser(kMediaUrl, request_context);

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  CefRefPtr<CefPermissionHandler> GetPermissionHandler() override {
    return this;
  }

  void CompleteTest() {
    if (!CefCurrentlyOn(TID_UI)) {
      CefPostTask(TID_UI,
                  base::BindOnce(&MediaAccessTestHandler::CompleteTest, this));
      return;
    }

    DestroyTest();
  }

  bool OnRequestMediaAccessPermission(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      const CefString& requesting_url,
      uint32 requested_permissions,
      CefRefPtr<CefMediaAccessCallback> callback) override {
    EXPECT_UI_THREAD();
    EXPECT_TRUE(frame->IsMain());

    if (test_setup_->deny_implicitly) {
      return false;
    }

    EXPECT_EQ(requested_permissions, request_);

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

  TestSetup* const test_setup_;
  const uint32 request_;
  const uint32 response_;

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
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_FALSE(test_setup.got_success);
  EXPECT_FALSE(test_setup.got_audio);
  EXPECT_FALSE(test_setup.got_video);
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

  EXPECT_FALSE(test_setup.got_success);
  EXPECT_FALSE(test_setup.got_audio);
  EXPECT_FALSE(test_setup.got_video);
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

  EXPECT_FALSE(test_setup.got_success);
  EXPECT_FALSE(test_setup.got_audio);
  EXPECT_FALSE(test_setup.got_video);
  EXPECT_FALSE(test_setup.got_change);
}

TEST(MediaAccessTest, DeviceFailureWhenRequestingAudioButReturningVideo) {
  TestSetup test_setup;

  CefRefPtr<MediaAccessTestHandler> handler = new MediaAccessTestHandler(
      &test_setup, CEF_MEDIA_PERMISSION_DEVICE_AUDIO_CAPTURE,
      CEF_MEDIA_PERMISSION_DEVICE_VIDEO_CAPTURE);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_FALSE(test_setup.got_success);
  EXPECT_FALSE(test_setup.got_audio);
  EXPECT_FALSE(test_setup.got_video);
  EXPECT_FALSE(test_setup.got_change);
}

TEST(MediaAccessTest, DeviceFailureWhenRequestingVideoButReturningAudio) {
  TestSetup test_setup;

  CefRefPtr<MediaAccessTestHandler> handler = new MediaAccessTestHandler(
      &test_setup, CEF_MEDIA_PERMISSION_DEVICE_VIDEO_CAPTURE,
      CEF_MEDIA_PERMISSION_DEVICE_AUDIO_CAPTURE);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_FALSE(test_setup.got_success);
  EXPECT_FALSE(test_setup.got_audio);
  EXPECT_FALSE(test_setup.got_video);
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

  EXPECT_FALSE(test_setup.got_success);
  EXPECT_FALSE(test_setup.got_audio);
  EXPECT_FALSE(test_setup.got_video);
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

  EXPECT_FALSE(test_setup.got_success);
  EXPECT_FALSE(test_setup.got_audio);
  EXPECT_FALSE(test_setup.got_video);
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

  EXPECT_FALSE(test_setup.got_success);
  EXPECT_FALSE(test_setup.got_audio);
  EXPECT_FALSE(test_setup.got_video);
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

  EXPECT_FALSE(test_setup.got_success);
  EXPECT_FALSE(test_setup.got_audio);
  EXPECT_FALSE(test_setup.got_video);
  EXPECT_FALSE(test_setup.got_change);
}

TEST(MediaAccessTest, DeviceFailureWhenReturningScreenCapture3) {
  TestSetup test_setup;

  CefRefPtr<MediaAccessTestHandler> handler = new MediaAccessTestHandler(
      &test_setup, CEF_MEDIA_PERMISSION_DEVICE_AUDIO_CAPTURE,
      CEF_MEDIA_PERMISSION_DESKTOP_VIDEO_CAPTURE);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_FALSE(test_setup.got_success);
  EXPECT_FALSE(test_setup.got_audio);
  EXPECT_FALSE(test_setup.got_video);
  EXPECT_FALSE(test_setup.got_change);
}

TEST(MediaAccessTest, DeviceFailureWhenReturningScreenCapture4) {
  TestSetup test_setup;

  CefRefPtr<MediaAccessTestHandler> handler = new MediaAccessTestHandler(
      &test_setup, CEF_MEDIA_PERMISSION_DEVICE_AUDIO_CAPTURE,
      CEF_MEDIA_PERMISSION_DESKTOP_AUDIO_CAPTURE);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_FALSE(test_setup.got_success);
  EXPECT_FALSE(test_setup.got_audio);
  EXPECT_FALSE(test_setup.got_video);
  EXPECT_FALSE(test_setup.got_change);
}

TEST(MediaAccessTest, DeviceFailureWhenReturningScreenCapture5) {
  TestSetup test_setup;

  CefRefPtr<MediaAccessTestHandler> handler = new MediaAccessTestHandler(
      &test_setup, CEF_MEDIA_PERMISSION_DEVICE_VIDEO_CAPTURE,
      CEF_MEDIA_PERMISSION_DESKTOP_VIDEO_CAPTURE);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_FALSE(test_setup.got_success);
  EXPECT_FALSE(test_setup.got_audio);
  EXPECT_FALSE(test_setup.got_video);
  EXPECT_FALSE(test_setup.got_change);
}

TEST(MediaAccessTest, DeviceFailureWhenReturningScreenCapture6) {
  TestSetup test_setup;

  CefRefPtr<MediaAccessTestHandler> handler = new MediaAccessTestHandler(
      &test_setup, CEF_MEDIA_PERMISSION_DEVICE_VIDEO_CAPTURE,
      CEF_MEDIA_PERMISSION_DESKTOP_AUDIO_CAPTURE);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_FALSE(test_setup.got_success);
  EXPECT_FALSE(test_setup.got_audio);
  EXPECT_FALSE(test_setup.got_video);
  EXPECT_FALSE(test_setup.got_change);
}

TEST(MediaAccessTest, DeviceSuccessAudioOnly) {
  TestSetup test_setup;

  CefRefPtr<MediaAccessTestHandler> handler = new MediaAccessTestHandler(
      &test_setup, CEF_MEDIA_PERMISSION_DEVICE_AUDIO_CAPTURE,
      CEF_MEDIA_PERMISSION_DEVICE_AUDIO_CAPTURE);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_setup.got_success);
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

  EXPECT_TRUE(test_setup.got_success);
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

  EXPECT_TRUE(test_setup.got_success);
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

  EXPECT_TRUE(test_setup.got_success);
  EXPECT_TRUE(test_setup.got_audio);
  EXPECT_TRUE(test_setup.got_video);
  EXPECT_TRUE(test_setup.got_change);
}

// Screen capture tests
TEST(MediaAccessTest, DesktopFailureWhenReturningNoPermission) {
  TestSetup test_setup;

  CefRefPtr<MediaAccessTestHandler> handler =
      new MediaAccessTestHandler(&test_setup,
                                 CEF_MEDIA_PERMISSION_DESKTOP_AUDIO_CAPTURE |
                                     CEF_MEDIA_PERMISSION_DESKTOP_VIDEO_CAPTURE,
                                 CEF_MEDIA_PERMISSION_NONE);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_FALSE(test_setup.got_success);
  EXPECT_FALSE(test_setup.got_audio);
  EXPECT_FALSE(test_setup.got_video);
  EXPECT_FALSE(test_setup.got_change);
}

TEST(MediaAccessTest, DesktopFailureWhenRequestingVideoButReturningAudio) {
  TestSetup test_setup;

  CefRefPtr<MediaAccessTestHandler> handler = new MediaAccessTestHandler(
      &test_setup, CEF_MEDIA_PERMISSION_DESKTOP_VIDEO_CAPTURE,
      CEF_MEDIA_PERMISSION_DESKTOP_AUDIO_CAPTURE);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_FALSE(test_setup.got_success);
  EXPECT_FALSE(test_setup.got_audio);
  EXPECT_FALSE(test_setup.got_video);
  EXPECT_FALSE(test_setup.got_change);
}

TEST(MediaAccessTest, DesktopPartialSuccessReturningVideo) {
  TestSetup test_setup;

  CefRefPtr<MediaAccessTestHandler> handler =
      new MediaAccessTestHandler(&test_setup,
                                 CEF_MEDIA_PERMISSION_DESKTOP_AUDIO_CAPTURE |
                                     CEF_MEDIA_PERMISSION_DESKTOP_VIDEO_CAPTURE,
                                 CEF_MEDIA_PERMISSION_DESKTOP_VIDEO_CAPTURE);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_TRUE(test_setup.got_success);
  EXPECT_FALSE(test_setup.got_audio);
  EXPECT_TRUE(test_setup.got_video);
  EXPECT_TRUE(test_setup.got_change);
}

TEST(MediaAccessTest, DesktopPartialFailureReturningAudio) {
  TestSetup test_setup;
  CefRefPtr<MediaAccessTestHandler> handler =
      new MediaAccessTestHandler(&test_setup,
                                 CEF_MEDIA_PERMISSION_DESKTOP_AUDIO_CAPTURE |
                                     CEF_MEDIA_PERMISSION_DESKTOP_VIDEO_CAPTURE,
                                 CEF_MEDIA_PERMISSION_DESKTOP_AUDIO_CAPTURE);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);

  EXPECT_FALSE(test_setup.got_success);
  EXPECT_FALSE(test_setup.got_audio);
  EXPECT_FALSE(test_setup.got_video);
  EXPECT_FALSE(test_setup.got_change);
}

// Entry point for creating media access browser test objects.
// Called from client_app_delegates.cc.
void CreateMediaAccessBrowserTests(
    client::ClientAppBrowser::DelegateSet& delegates) {
  delegates.insert(new MediaAccessBrowserTest);
}
