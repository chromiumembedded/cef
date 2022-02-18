// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/base/cef_callback.h"
#include "include/cef_pack_resources.h"
#include "include/cef_request_context_handler.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_stream_resource_handler.h"
#include "tests/ceftests/test_handler.h"
#include "tests/ceftests/test_util.h"
#include "tests/gtest/include/gtest/gtest.h"
#include "tests/shared/browser/client_app_browser.h"
#include "tests/shared/browser/resource_util.h"

namespace {

const char kPdfHtmlUrl[] = "http://tests/pdf.html";
const char kPdfDirectUrl[] = "http://tests/pdf.pdf";

// Delay waiting for iframe tests to load the PDF file.
#if defined(OS_LINUX)
const int64 kPdfLoadDelayMs = 7000;
#else
const int64 kPdfLoadDelayMs = 5000;
#endif

// Browser-side test handler.
class PdfViewerTestHandler : public TestHandler, public CefContextMenuHandler {
 public:
  enum Mode {
    // No specified context or handler (implicitly uses the global context).
    GLOBAL_DEFAULT,

    // Global context with no handler.
    GLOBAL_NO_HANDLER,

    // Custom context with no handler.
    CUSTOM_NO_HANDLER,
  };

  PdfViewerTestHandler(Mode mode, const std::string& url)
      : mode_(mode), url_(url) {}

  // Loading the PDF directly in the main frame instead of a sub-frame.
  bool HasDirectPdfLoad() const { return url_ == kPdfDirectUrl; }

  CefRefPtr<CefContextMenuHandler> GetContextMenuHandler() override {
    return this;
  }

  void RunTest() override {
    CefRefPtr<CefRequestContext> request_context;

    if (mode_ == GLOBAL_NO_HANDLER) {
      // Use the global request context.
      request_context = CefRequestContext::CreateContext(
          CefRequestContext::GetGlobalContext(), nullptr);
    } else if (mode_ == CUSTOM_NO_HANDLER) {
      // Create the request context that will use an in-memory cache.
      CefRequestContextSettings settings;
      request_context = CefRequestContext::CreateContext(settings, nullptr);
    }

    // Create the browser.
    CreateBrowser(url_, request_context);

    // Time out the test after a reasonable period of time.
    SetTestTimeout(5000 + kPdfLoadDelayMs);
  }

  CefRefPtr<CefResourceHandler> GetResourceHandler(
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefRefPtr<CefRequest> request) override {
    const std::string& url = request->GetURL();
    if (url == kPdfHtmlUrl) {
      CefRefPtr<CefStreamReader> stream =
          client::GetBinaryResourceReader("pdf.html");
      return new CefStreamResourceHandler("text/html", stream);
    } else if (url == kPdfDirectUrl) {
      CefRefPtr<CefStreamReader> stream =
          client::GetBinaryResourceReader("pdf.pdf");
      return new CefStreamResourceHandler("application/pdf", stream);
    }

    return nullptr;
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    bool is_pdf1 = false;
    const std::string& url = frame->GetURL();
    if (url == kPdfHtmlUrl) {
      if (!got_on_load_end_html_)
        got_on_load_end_html_.yes();
      else
        NOTREACHED();
    } else if (url == kPdfDirectUrl) {
      if (!got_on_load_end_pdf1_) {
        got_on_load_end_pdf1_.yes();
        is_pdf1 = true;
      } else if (!got_on_load_end_pdf2_) {
        got_on_load_end_pdf2_.yes();
      } else {
        NOTREACHED();
      }
    } else {
      NOTREACHED();
    }

    if (is_pdf1) {
      // The first PDF document has loaded.
      // TODO(chrome): Add support for custom context menus.
      if (IsChromeRuntimeEnabled() || got_context_menu_dismissed_) {
        // After context menu display. Destroy the test.
        CefPostDelayedTask(
            TID_UI, base::BindOnce(&PdfViewerTestHandler::DestroyTest, this),
            kPdfLoadDelayMs);
      } else {
        // Trigger the context menu.
        CefPostDelayedTask(
            TID_UI,
            base::BindOnce(&PdfViewerTestHandler::TriggerContextMenu, this,
                           frame->GetBrowser()),
            kPdfLoadDelayMs);
      }
    }
  }

  void TriggerContextMenu(CefRefPtr<CefBrowser> browser) {
    CefMouseEvent mouse_event;

    if (HasDirectPdfLoad()) {
      // Somewhere in the main PDF viewing area (avoid left preview bar).
      mouse_event.x = 400;
      mouse_event.y = 200;
    } else {
      // Somewhere in the first PDF viewing area.
      mouse_event.x = 100;
      mouse_event.y = 100;
    }

    // Send right-click mouse down and mouse up to tigger context menu.
    browser->GetHost()->SendMouseClickEvent(mouse_event, MBT_RIGHT, false, 1);
    browser->GetHost()->SendMouseClickEvent(mouse_event, MBT_RIGHT, true, 1);
  }

  bool RunContextMenu(CefRefPtr<CefBrowser> browser,
                      CefRefPtr<CefFrame> frame,
                      CefRefPtr<CefContextMenuParams> params,
                      CefRefPtr<CefMenuModel> model,
                      CefRefPtr<CefRunContextMenuCallback> callback) override {
    EXPECT_FALSE(got_run_context_menu_);
    got_run_context_menu_.yes();

    // Do nothing with the context menu.
    callback->Cancel();

    return true;
  }

  void OnContextMenuDismissed(CefRefPtr<CefBrowser> browser,
                              CefRefPtr<CefFrame> frame) override {
    EXPECT_FALSE(got_context_menu_dismissed_);
    got_context_menu_dismissed_.yes();

    CefPostTask(TID_UI,
                base::BindOnce(&PdfViewerTestHandler::DestroyTest, this));
  }

  void DestroyTest() override {
    // TODO(chrome): Add support for custom context menus.
    if (!IsChromeRuntimeEnabled()) {
      EXPECT_TRUE(got_run_context_menu_);
      EXPECT_TRUE(got_context_menu_dismissed_);
    } else {
      EXPECT_FALSE(got_run_context_menu_);
      EXPECT_FALSE(got_context_menu_dismissed_);
    }

    if (url_ == kPdfHtmlUrl) {
      // The HTML file will load the PDF twice in iframes.
      EXPECT_TRUE(got_on_load_end_html_);
      EXPECT_TRUE(got_on_load_end_pdf1_);
      EXPECT_TRUE(got_on_load_end_pdf2_);
    } else if (url_ == kPdfDirectUrl) {
      // Load the PDF file directly.
      EXPECT_FALSE(got_on_load_end_html_);
      EXPECT_TRUE(got_on_load_end_pdf1_);
      EXPECT_FALSE(got_on_load_end_pdf2_);
    } else {
      NOTREACHED();
    }

    TestHandler::DestroyTest();
  }

  const Mode mode_;
  const std::string url_;

  TrackCallback got_on_load_end_html_;
  TrackCallback got_on_load_end_pdf1_;
  TrackCallback got_on_load_end_pdf2_;
  TrackCallback got_run_context_menu_;
  TrackCallback got_context_menu_dismissed_;

  IMPLEMENT_REFCOUNTING(PdfViewerTestHandler);
};

}  // namespace

#define RUN_TEST(name, type, url)                                  \
  TEST(PdfViewerTest, name) {                                      \
    CefRefPtr<PdfViewerTestHandler> handler =                      \
        new PdfViewerTestHandler(PdfViewerTestHandler::type, url); \
    handler->ExecuteTest();                                        \
    ReleaseAndWaitForDestructor(handler);                          \
  }

RUN_TEST(GlobalDefaultPdfDirect, GLOBAL_DEFAULT, kPdfDirectUrl)
RUN_TEST(GlobalDefaultPdfHtml, GLOBAL_DEFAULT, kPdfHtmlUrl)

RUN_TEST(GlobalNoHandlerPdfDirect, GLOBAL_NO_HANDLER, kPdfDirectUrl)
RUN_TEST(GlobalNoHandlerPdfHtml, GLOBAL_NO_HANDLER, kPdfHtmlUrl)

RUN_TEST(CustomNoHandlerPdfDirect, CUSTOM_NO_HANDLER, kPdfDirectUrl)
RUN_TEST(CustomNoHandlerPdfHtml, CUSTOM_NO_HANDLER, kPdfHtmlUrl)
