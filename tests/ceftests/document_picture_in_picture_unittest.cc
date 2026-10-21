// Copyright 2026 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file.

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "include/base/cef_callback.h"
#include "include/cef_command_line.h"
#include "include/cef_devtools_message_observer.h"
#include "include/cef_image.h"
#include "include/cef_parser.h"
#include "include/views/cef_browser_view_delegate.h"
#include "include/views/cef_display.h"
#include "include/views/cef_window_delegate.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/test_handler.h"
#include "tests/ceftests/test_util.h"
#include "tests/gtest/include/gtest/gtest.h"
#include "tests/shared/browser/client_app_browser.h"

namespace {

constexpr char kUrl[] = "https://tests/pip-options.html";
constexpr char kFeature[] = "DocumentPictureInPictureCefOptions";

class PipBrowserTest : public client::ClientAppBrowser::Delegate {
 public:
  void OnBeforeCommandLineProcessing(
      CefRefPtr<client::ClientAppBrowser> app,
      CefRefPtr<CefCommandLine> command_line) override {
    auto features =
        command_line->GetSwitchValue("enable-blink-features").ToString();
    if (!features.empty()) {
      features += ',';
    }
    features += kFeature;
    command_line->AppendSwitchWithValue("enable-blink-features", features);
  }

 private:
  IMPLEMENT_REFCOUNTING(PipBrowserTest);
};

enum class TestMode {
  kPosition,
  kResizableDefault,
  kResizableTrue,
  kResizableFalse,
  kMoveAllowed,
  kMoveDenied,
  kFramed,
  kFrameless,
  kActivationAllowed,
  kActivationRequired,
  kTransparentDefault,
  kTransparentFalse,
  kTransparentTrue,
  kReopen,
};

struct Settings {
  TestMode mode;
  bool frameless = false;
  bool allow_move = false;
  bool allow_without_activation = false;
};

class PipTestHandler : public TestHandler {
 public:
  explicit PipTestHandler(Settings settings)
      : settings_(settings), frameless_(settings.frameless) {}

  void RunTest() override {
    AddResource(kUrl, "<!doctype html><title>PiP options</title>", "text/html");
    CefPostTask(TID_UI, base::BindOnce(&PipTestHandler::CreateOpener, this));
    SetTestTimeout(15000);
  }

  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
    TestHandler::OnAfterCreated(browser);
    if (!opener_) {
      opener_ = browser;
    } else {
      EXPECT_FALSE(pip_);
      pip_ = browser;
      ++pip_created_;
    }
    registrations_.emplace_back(
        browser->GetIdentifier(),
        browser->GetHost()->AddDevToolsMessageObserver(new Observer(this)));
  }

  void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
    for (auto& [id, registration] : registrations_) {
      if (id == browser->GetIdentifier()) {
        registration = nullptr;
      }
    }
    const bool was_pip = pip_ && browser->IsSame(pip_);
    if (was_pip) {
      pip_ = nullptr;
    }
    if (opener_ && browser->IsSame(opener_)) {
      opener_ = nullptr;
    }
    // Keep the opener alive until the last PiP browser has closed. The base
    // handler waits for both the opener's Browser and test-managed Window.
    TestHandler::OnBeforeClose(browser);
    if (was_pip && !stopping_ && after_close_) {
      GetUIThreadHelper()->PostTask(std::move(after_close_));
    }
  }

  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool is_loading,
                            bool can_go_back,
                            bool can_go_forward) override {
    if (!is_loading && opener_ && browser->IsSame(opener_) && !started_) {
      started_ = true;
      Start();
    }
  }

  void DestroyTest() override {
    stopping_ = true;
    pending_.Reset();
    after_close_.Reset();
    registrations_.clear();
    TestHandler::DestroyTest();
  }

 private:
  using ResultCallback =
      base::OnceCallback<void(CefRefPtr<CefDictionaryValue>)>;

  class Observer : public CefDevToolsMessageObserver {
   public:
    explicit Observer(PipTestHandler* handler) : handler_(handler) {}

    void OnDevToolsMethodResult(CefRefPtr<CefBrowser> browser,
                                int message_id,
                                bool success,
                                const void* result,
                                size_t result_size) override {
      handler_->OnResult(browser, message_id, success, result, result_size);
    }

   private:
    PipTestHandler* const handler_;
    IMPLEMENT_REFCOUNTING(Observer);
  };

  class ViewDelegate : public CefBrowserViewDelegate {
   public:
    explicit ViewDelegate(PipTestHandler* handler) : handler_(handler) {}

    bool UseFramelessWindowForPictureInPicture(
        CefRefPtr<CefBrowserView> browser_view) override {
      return handler_->frameless_;
    }

#if CEF_API_ADDED(13601)
    bool AllowMoveForPictureInPicture(
        CefRefPtr<CefBrowserView> browser_view) override {
      return handler_->settings_.allow_move;
    }
#endif

#if CEF_API_ADDED(14400)
    bool AllowPictureInPictureWithoutUserActivation(
        CefRefPtr<CefBrowserView> browser_view) override {
      return handler_->settings_.allow_without_activation;
    }
#endif

    void OnBrowserDestroyed(CefRefPtr<CefBrowserView> browser_view,
                            CefRefPtr<CefBrowser> browser) override {
      if (auto window = browser_view->GetWindow()) {
        window->Close();
      }
    }

   private:
    PipTestHandler* const handler_;
    IMPLEMENT_REFCOUNTING(ViewDelegate);
  };

  class WindowDelegate : public CefWindowDelegate {
   public:
    WindowDelegate(PipTestHandler* handler, CefRefPtr<CefBrowserView> view)
        : handler_(handler), view_(view) {}

    void OnWindowCreated(CefRefPtr<CefWindow> window) override {
      window->CenterWindow(CefSize(800, 600));
      window->AddChildView(view_);
      window->Show();
      browser_id_ = view_->GetBrowser()->GetIdentifier();
      handler_->OnWindowCreated(browser_id_);
    }

    void OnWindowDestroyed(CefRefPtr<CefWindow> window) override {
      view_ = nullptr;
      handler_->OnWindowDestroyed(browser_id_);
    }

    bool CanClose(CefRefPtr<CefWindow> window) override {
      auto browser = view_->GetBrowser();
      return !browser || browser->GetHost()->TryCloseBrowser();
    }

   private:
    PipTestHandler* const handler_;
    CefRefPtr<CefBrowserView> view_;
    int browser_id_ = 0;
    IMPLEMENT_REFCOUNTING(WindowDelegate);
  };

  void CreateOpener() {
    SetUseViews(true);
    SetUseAlloyStyle(false, false);
    // Deliberately create an isolated browser: the delegate configuration and
    // popup lifetime are specific to this test and must not enter the pool.
    auto context =
        CefRequestContext::CreateContext(CefRequestContextSettings(), nullptr);
    auto view = CefBrowserView::CreateBrowserView(
        this, kUrl, CefBrowserSettings(), nullptr, context,
        new ViewDelegate(this));
    CefWindow::CreateTopLevelWindow(new WindowDelegate(this, view));
  }

  void Execute(CefRefPtr<CefBrowser> browser,
               const char* method,
               CefRefPtr<CefDictionaryValue> params,
               ResultCallback callback) {
    if (stopping_) {
      return;
    }
    EXPECT_FALSE(pending_);
    pending_ = std::move(callback);
    pending_method_ = method;
    pending_browser_id_ = browser->GetIdentifier();
    pending_id_ = ++next_id_;
    const int id = pending_id_;
    EXPECT_EQ(id,
              browser->GetHost()->ExecuteDevToolsMethod(id, method, params));
  }

  void OnResult(CefRefPtr<CefBrowser> browser,
                int id,
                bool success,
                const void* result,
                size_t size) {
    if (stopping_ || id != pending_id_ ||
        browser->GetIdentifier() != pending_browser_id_) {
      return;
    }
    auto value = CefParseJSON(result, size, JSON_PARSER_RFC);
    if (!success || !value || value->GetType() != VTYPE_DICTIONARY) {
      ADD_FAILURE() << "CDP method " << pending_method_ << " (" << id
                    << ") failed: "
                    << (result ? std::string(static_cast<const char*>(result),
                                             size)
                               : "<no result>");
      DestroyTest();
      return;
    }
    auto dict = value->GetDictionary();
    if (dict->HasKey("exceptionDetails")) {
      ADD_FAILURE() << CefWriteJSON(value, JSON_WRITER_DEFAULT).ToString();
      DestroyTest();
      return;
    }
    auto callback = std::move(pending_);
    GetUIThreadHelper()->PostTask(base::BindOnce(std::move(callback), dict));
  }

  void Eval(CefRefPtr<CefBrowser> browser,
            const std::string& expression,
            ResultCallback callback,
            bool user_gesture = false) {
    auto params = CefDictionaryValue::Create();
    params->SetString("expression", expression);
    params->SetBool("awaitPromise", true);
    params->SetBool("returnByValue", true);
    params->SetBool("userGesture", user_gesture);
    Execute(browser, "Runtime.evaluate", params, std::move(callback));
  }

  void Start() {
    if (settings_.mode == TestMode::kActivationRequired ||
        settings_.mode == TestMode::kActivationAllowed) {
      // Popup permission and the PiP activation requirement are independent.
      // Allow popups in both cases so only the delegate value controls whether
      // opening without activation succeeds. This context is test-specific.
      opener_->GetHost()->GetRequestContext()->SetContentSetting(
          kUrl, kUrl, CEF_CONTENT_SETTING_TYPE_POPUPS,
          CEF_CONTENT_SETTING_VALUE_ALLOW);
      Eval(opener_, R"JS(
        (async () => {
          if (navigator.userActivation.isActive)
            throw new Error('unexpected user activation');
          try {
            window.pip = await documentPictureInPicture.requestWindow();
            return 'opened';
          } catch (error) { return error.name; }
        })()
      )JS",
           base::BindOnce(&PipTestHandler::CheckActivation, this));
      return;
    }

    std::string options = "{}";
    switch (settings_.mode) {
      case TestMode::kPosition: {
        const auto area = CefDisplay::GetPrimaryDisplay()->GetWorkArea();
        requested_position_ = CefPoint(area.x + 60, area.y + 60);
        options = "{position:{x:" + std::to_string(requested_position_.x) +
                  ",y:" + std::to_string(requested_position_.y) + "}}";
        break;
      }
      case TestMode::kResizableDefault:
      case TestMode::kTransparentDefault:
        options.clear();
        break;
      case TestMode::kResizableTrue:
        options = "{resizable:true}";
        break;
      case TestMode::kResizableFalse:
        options = "{resizable:false}";
        break;
      case TestMode::kTransparentFalse:
        options = "{transparent:false}";
        break;
      case TestMode::kTransparentTrue:
      case TestMode::kReopen:
        options = "{transparent:true}";
        break;
      default:
        break;
    }
    Open(options);
  }

  void Open(const std::string& options) {
    Eval(opener_,
         "(async () => { window.pip = await "
         "documentPictureInPicture.requestWindow({width:400,height:300,"
         "preferInitialWindowPlacement:true" +
             (options.empty() ? "" : ",cefOptions:" + options) +
             "}); return true; })()",
         base::BindOnce(&PipTestHandler::OnOpened, this), true);
  }

  void CheckActivation(CefRefPtr<CefDictionaryValue> result) {
    auto remote = result->GetDictionary("result");
    ASSERT_TRUE(remote);
    const bool allowed = settings_.mode == TestMode::kActivationAllowed;
    EXPECT_EQ(allowed ? "opened" : "NotAllowedError",
              remote->GetString("value").ToString());
    EXPECT_EQ(allowed ? 1 : 0, pip_created_);
    EXPECT_EQ(allowed, !!pip_);
    DestroyTest();
  }

  void OnOpened(CefRefPtr<CefDictionaryValue> result) {
    ASSERT_TRUE(pip_);
    EXPECT_EQ(round_ + 1, pip_created_);
    // Let the new window produce frames before inspecting its initial native
    // state and renderer geometry. No fixed-duration sleeps are required.
    Eval(pip_,
         "new Promise(r => requestAnimationFrame(() => "
         "requestAnimationFrame(() => r(true))))",
         base::BindOnce(&PipTestHandler::OnPipReady, this));
  }

  void OnPipReady(CefRefPtr<CefDictionaryValue> result) {
    switch (settings_.mode) {
      case TestMode::kPosition:
        Execute(pip_, "Browser.getWindowForTarget", nullptr,
                base::BindOnce(&PipTestHandler::CheckPosition, this));
        break;
      case TestMode::kResizableDefault:
      case TestMode::kResizableTrue:
      case TestMode::kResizableFalse:
        CheckResizable(settings_.mode != TestMode::kResizableFalse);
        DestroyTest();
        break;
      case TestMode::kMoveAllowed:
      case TestMode::kMoveDenied:
        CheckMovement();
        break;
      case TestMode::kFramed:
      case TestMode::kFrameless:
        Eval(pip_,
             "({width:outerWidth-innerWidth,"
             "height:outerHeight-innerHeight})",
             base::BindOnce(&PipTestHandler::CheckFrame, this));
        break;
      default:
        StartScreenshots();
        break;
    }
  }

  void CheckPosition(CefRefPtr<CefDictionaryValue> result) {
    auto bounds = result->GetDictionary("bounds");
    ASSERT_TRUE(bounds);
    EXPECT_EQ(requested_position_.x, bounds->GetInt("left"));
    EXPECT_EQ(requested_position_.y, bounds->GetInt("top"));
    DestroyTest();
  }

  void StartScreenshots() {
    pip_id_ = pip_->GetIdentifier();
    pip_handle_ = pip_->GetHost()->GetWindowHandle();
    CaptureBackground();
  }

  void CheckResizable(bool expected) {
#if defined(OS_MAC) || defined(OS_WIN)
    const auto resizable = IsNativeTestBrowserResizable(pip_);
    ASSERT_TRUE(resizable.has_value());
    EXPECT_EQ(expected, *resizable);
#endif
  }

  void CheckFrame(CefRefPtr<CefDictionaryValue> result) {
    auto remote = result->GetDictionary("result");
    ASSERT_TRUE(remote);
    auto value = remote->GetDictionary("value");
    ASSERT_TRUE(value);
    if (round_ == 0) {
      EXPECT_GT(value->GetInt("height"), 2);
      if (settings_.mode == TestMode::kFrameless) {
        framed_height_ = value->GetInt("height");
        framed_width_ = value->GetInt("width");
        after_close_ = base::BindOnce(&PipTestHandler::ReopenFrameless, this);
        pip_->GetHost()->CloseBrowser(true);
        return;
      }
    } else {
      // Compare against the framed window on the same platform, retaining
      // native resize borders while removing Chromium's top controls.
      EXPECT_LT(value->GetInt("height"), framed_height_);
      EXPECT_NEAR(value->GetInt("width"), framed_width_, 1);
    }
    DestroyTest();
  }

  void ReopenFrameless() {
    ++round_;
    frameless_ = true;
    Open("{}");
  }

  void CheckMovement() {
    Eval(pip_, R"JS(
      (async () => {
        if (navigator.userActivation.isActive)
          throw new Error('unexpected user activation');
        const original = {x:screenX,y:screenY,w:outerWidth,h:outerHeight};
        const errors = [];
        const wait = async (dx, dy, dw, dh) => {
          const deadline = performance.now() + 3000;
          while (Math.abs(screenX - original.x - dx) > 1 ||
                 Math.abs(screenY - original.y - dy) > 1 ||
                 Math.abs(outerWidth - original.w - dw) > 1 ||
                 Math.abs(outerHeight - original.h - dh) > 1) {
            if (performance.now() > deadline) throw new Error('move/resize timed out');
            await new Promise(r => requestAnimationFrame(r));
          }
        };
        // Move inward and shrink to avoid the work-area edges and PiP maximum.
        window.moveTo(original.x - 20, original.y - 20);
        try { window.resizeTo(original.w - 20, original.h - 20); }
        catch (error) { errors.push(error.name); }
        if (errors.length === 0) await wait(-20, -20, -20, -20);
        window.moveBy(10, 10);
        try { window.resizeBy(10, 10); }
        catch (error) { errors.push(error.name); }
        if (errors.length === 0) {
          await wait(-10, -10, -10, -10);
        } else {
          await new Promise(r => requestAnimationFrame(() => requestAnimationFrame(r)));
        }
        return {errors:errors.join(','), dx:screenX-original.x,dy:screenY-original.y,
                dw:outerWidth-original.w,dh:outerHeight-original.h};
      })()
    )JS",
         base::BindOnce(&PipTestHandler::OnMoved, this));
  }

  void OnMoved(CefRefPtr<CefDictionaryValue> result) {
    auto remote = result->GetDictionary("result");
    ASSERT_TRUE(remote);
    auto value = remote->GetDictionary("value");
    ASSERT_TRUE(value);
    const bool allowed = settings_.allow_move;
    EXPECT_EQ(allowed ? "" : "NotAllowedError,NotAllowedError",
              value->GetString("errors").ToString());
    for (const auto* key : {"dx", "dy", "dw", "dh"}) {
      EXPECT_NEAR(allowed ? -10 : 0, value->GetInt(key), allowed ? 1 : 0)
          << key;
    }
    DestroyTest();
  }

  void CaptureBackground() {
    // A black/white marker proves that each screenshot includes this update
    // without depending on the display color profile's mapping of RGB colors.
    // Sample a separate empty region for alpha; never force transparency with
    // Emulation.setDefaultBackgroundColorOverride, which would mask
    // regressions.
    const auto color = step_ == 1 ? "black" : "transparent";
    const auto marker = step_ == 1 ? "white" : "black";
    Eval(pip_,
         "(async () => { "
         "document.documentElement.style.background='transparent';"
         "document.body.style.cssText='margin:0;background:" +
             std::string(color) +
             "';document.body.innerHTML='<div style=\"position:fixed;left:0;"
             "top:0;width:16px;height:16px;background:" +
             marker +
             "\"></div>';await new Promise(r => requestAnimationFrame(() => "
             "requestAnimationFrame(r)));return true;})()",
         base::BindOnce(&PipTestHandler::TakeScreenshot, this));
  }

  void TakeScreenshot(CefRefPtr<CefDictionaryValue> result) {
    auto params = CefDictionaryValue::Create();
    params->SetString("format", "png");
    params->SetBool("fromSurface", true);
    auto clip = CefDictionaryValue::Create();
    clip->SetDouble("x", 0);
    clip->SetDouble("y", 0);
    clip->SetDouble("width", 64);
    clip->SetDouble("height", 64);
    clip->SetDouble("scale", 1);
    params->SetDictionary("clip", clip);
    Execute(pip_, "Page.captureScreenshot", params,
            base::BindOnce(&PipTestHandler::CheckScreenshot, this));
  }

  void CheckScreenshot(CefRefPtr<CefDictionaryValue> result) {
    auto png = CefBase64Decode(result->GetString("data"));
    ASSERT_TRUE(png);
    std::vector<uint8_t> bytes(png->GetSize());
    ASSERT_EQ(bytes.size(), png->GetData(bytes.data(), bytes.size(), 0));
    auto image = CefImage::CreateImage();
    ASSERT_TRUE(image->AddPNG(1, bytes.data(), bytes.size()));
    int width = 0;
    int height = 0;
    auto bitmap =
        image->GetAsBitmap(1, CEF_COLOR_TYPE_RGBA_8888,
                           CEF_ALPHA_TYPE_PREMULTIPLIED, width, height);
    ASSERT_TRUE(bitmap);
    ASSERT_GT(width, 0);
    ASSERT_GT(height, 0);
    // Clip dimensions are CSS pixels; the bitmap may use a device scale > 1.
    std::array<uint8_t, 4> pixel{};
    const auto read = [&](int x, int y) {
      const size_t offset = (static_cast<size_t>(y) * width + x) * 4;
      EXPECT_EQ(pixel.size(),
                bitmap->GetData(pixel.data(), pixel.size(), offset));
    };
    read(width / 8, height / 8);
    EXPECT_EQ((step_ == 1 ? (std::array<uint8_t, 4>{255, 255, 255, 255})
                          : (std::array<uint8_t, 4>{0, 0, 0, 255})),
              pixel);
    if (step_ == 1) {
      read(width / 2, height / 2);
      EXPECT_EQ((std::array<uint8_t, 4>{0, 0, 0, 255}), pixel);
    }
    bool transparent = settings_.mode == TestMode::kTransparentTrue ||
                       (settings_.mode == TestMode::kReopen && round_ != 1);
#if !defined(OS_MAC) && !defined(OS_WIN)
    transparent = false;
#endif
    // Check a region, not a single pixel, away from text, borders and marker.
    for (int y = height / 2; y < 3 * height / 4; ++y) {
      for (int x = width / 2; x < 3 * width / 4; ++x) {
        read(x, y);
        ASSERT_EQ(transparent && step_ != 1 ? 0 : 255, pixel[3])
            << "round=" << round_ << " step=" << step_ << " pixel=" << x << ','
            << y;
      }
    }
    EXPECT_EQ(pip_id_, pip_->GetIdentifier());
    EXPECT_EQ(pip_handle_, pip_->GetHost()->GetWindowHandle());
    EXPECT_EQ(round_ + 1, pip_created_);
    if (++step_ < 3) {
      CaptureBackground();
    } else if (settings_.mode == TestMode::kReopen && round_ < 2) {
      after_close_ = base::BindOnce(&PipTestHandler::Reopen, this);
      pip_->GetHost()->CloseBrowser(true);
    } else {
      DestroyTest();
    }
  }

  void Reopen() {
    ++round_;
    step_ = 0;
    Open(round_ == 1 ? "{transparent:false}" : "{transparent:true}");
  }

  const Settings settings_;
  CefRefPtr<CefBrowser> opener_;
  CefRefPtr<CefBrowser> pip_;
  std::vector<std::pair<int, CefRefPtr<CefRegistration>>> registrations_;
  ResultCallback pending_;
  std::string pending_method_;
  base::OnceClosure after_close_;
  CefPoint requested_position_;
  CefWindowHandle pip_handle_ = kNullWindowHandle;
  int pip_id_ = 0;
  int pip_created_ = 0;
  int round_ = 0;
  int step_ = 0;
  int next_id_ = 0;
  int pending_id_ = 0;
  int pending_browser_id_ = 0;
  int framed_height_ = 0;
  int framed_width_ = 0;
  bool frameless_;
  bool started_ = false;
  bool stopping_ = false;

  IMPLEMENT_REFCOUNTING(PipTestHandler);
};

class DocumentPictureInPictureTest : public testing::Test {
 protected:
  void Run(Settings settings) {
    CefRefPtr<PipTestHandler> handler = new PipTestHandler(settings);
    handler->ExecuteTest();
    ReleaseAndWaitForDestructor(handler);
  }
};

TEST_F(DocumentPictureInPictureTest, InitialPosition) {
  if (IsRunningOnWayland()) {
    GTEST_SKIP()
        << "Wayland does not support client-controlled window positions";
  }
  Run({TestMode::kPosition});
}

TEST_F(DocumentPictureInPictureTest, ResizableDefault) {
#if !defined(OS_MAC) && !defined(OS_WIN)
  GTEST_SKIP() << "Native resize capability query requires macOS or Windows";
#endif
  Run({TestMode::kResizableDefault});
}

TEST_F(DocumentPictureInPictureTest, ResizableTrue) {
#if !defined(OS_MAC) && !defined(OS_WIN)
  GTEST_SKIP() << "Native resize capability query requires macOS or Windows";
#endif
  Run({TestMode::kResizableTrue});
}

TEST_F(DocumentPictureInPictureTest, ResizableFalse) {
#if !defined(OS_MAC) && !defined(OS_WIN)
  GTEST_SKIP() << "Native resize capability query requires macOS or Windows";
#endif
  Run({TestMode::kResizableFalse});
}

TEST_F(DocumentPictureInPictureTest, MoveAndResizeAllowedWithoutActivation) {
  if (IsRunningOnWayland()) {
    GTEST_SKIP()
        << "Wayland does not support client-controlled window positions";
  }
  Run({TestMode::kMoveAllowed, false, true});
}

TEST_F(DocumentPictureInPictureTest, MoveAndResizeDeniedWithoutActivation) {
  Run({TestMode::kMoveDenied});
}

TEST_F(DocumentPictureInPictureTest, Frameless) {
  Run({TestMode::kFrameless});
}

TEST_F(DocumentPictureInPictureTest, FramedDefault) {
  Run({TestMode::kFramed});
}

TEST_F(DocumentPictureInPictureTest, OpenWithoutActivationAllowed) {
  Run({TestMode::kActivationAllowed, false, false, true});
}

TEST_F(DocumentPictureInPictureTest, OpenWithoutActivationDenied) {
  Run({TestMode::kActivationRequired});
}

TEST_F(DocumentPictureInPictureTest, TransparencyDefault) {
  Run({TestMode::kTransparentDefault, true});
}

TEST_F(DocumentPictureInPictureTest, TransparencyFalse) {
  Run({TestMode::kTransparentFalse, true});
}

TEST_F(DocumentPictureInPictureTest, TransparencyTrue) {
  Run({TestMode::kTransparentTrue, true});
}

TEST_F(DocumentPictureInPictureTest, TransparencyReopenIsolation) {
  Run({TestMode::kReopen, true});
}

}  // namespace

// Called from client_app_delegates.cc.
void CreateDocumentPictureInPictureBrowserTests(
    client::ClientAppBrowser::DelegateSet& delegates) {
  delegates.insert(new PipBrowserTest);
}
