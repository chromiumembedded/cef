// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <algorithm>

#include "include/base/cef_callback.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_scoped_temp_dir.h"
#include "tests/ceftests/test_handler.h"
#include "tests/ceftests/test_util.h"
#include "tests/gtest/include/gtest/gtest.h"
#include "tests/shared/browser/file_util.h"

namespace {

const char kPrintHtmlUrl[] = "https://tests/print.html";
const char kTestFileName[] = "print.pdf";
#if defined(OS_WIN)
const char kTestFileNameInvalid[] = "print.pdf?";
#endif  // defined(OS_WIN)
const char kTestHtml[] =
    "<!DOCTYPE html>\n"
    "<html lang=\"en\">\n"
    "<head>\n"
    "    <meta charset=\"UTF-8\">\n"
    "    <meta name=\"viewport\" content=\"width=device-width, "
    "initial-scale=1.0\">\n"
    "    <title>Print Test</title>\n"
    "    <style>\n"
    "        body {\n"
    "            font-family: Arial, sans-serif;\n"
    "            margin: 20px;\n"
    "            padding: 20px;\n"
    "            border: 1px solid #ccc;\n"
    "        }\n"
    "        h1 {\n"
    "            color: #333;\n"
    "        }\n"
    "        p {\n"
    "            font-size: 16px;\n"
    "            line-height: 1.5;\n"
    "        }\n"
    "        @media print {\n"
    "            body {\n"
    "                margin: 0;\n"
    "                padding: 0;\n"
    "                border: none;\n"
    "            }\n"
    "            h1 {\n"
    "                color: black;\n"
    "            }\n"
    "            p {\n"
    "                color: black;\n"
    "            }\n"
    "        }\n"
    "    </style>\n"
    "</head>\n"
    "<body>\n"
    "    <h1>Print Test Document</h1>\n"
    "    <p>This is a simple test document to check printing "
    "functionality.</p>\n"
    "</body>\n"
    "</html>";

// Browser-side test handler.
class PrintToPdfTestHandler : public TestHandler, public CefPdfPrintCallback {
 public:
  PrintToPdfTestHandler(const std::string& url, bool expect_ok)
      : url_(url), expect_ok_(expect_ok) {}

  virtual std::string GetTestFileName() { return kTestFileName; }

  void RunTest() override {
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());

    test_path_ =
        client::file_util::JoinPath(temp_dir_.GetPath(), GetTestFileName());

    // Add the resource.
    AddResource(url_, kTestHtml, "text/html");

    // Create the browser.
    CreateBrowser(url_);

    // Time out the test after a reasonable period of time.
    SetTestTimeout(5000);
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    const std::string& url = frame->GetURL();
    if (url == "about:blank" || url.find("chrome-extension://") == 0) {
      return;
    }

    if (url == kPrintHtmlUrl) {
      EXPECT_FALSE(got_on_load_end_html_);
      got_on_load_end_html_.yes();
    } else {
      NOTREACHED() << "url=" << url;
    }

    PrintToPDF(browser);
  }

  void OnPdfPrintFinished(const CefString& path, bool ok) override {
    EXPECT_EQ(expect_ok_, ok);

    if (ok) {
      EXPECT_EQ(test_path_, path);
    }

    EXPECT_FALSE(got_on_pdf_print_finished_);
    got_on_pdf_print_finished_.yes();

    CompleteTest();
  }

  void VerifyResultsOnFileThread() {
    EXPECT_TRUE(CefCurrentlyOn(TID_FILE_USER_VISIBLE));

    // Verify the file contents.
    std::string contents;
    bool ok = client::file_util::ReadFileToString(test_path_, &contents);
    EXPECT_EQ(ok, expect_ok_);
    if (expect_ok_) {
      EXPECT_GT(contents.size(), 1024U);
      // https://en.wikipedia.org/wiki/PDF
      // The file starts with a header containing a magic number (as a readable
      // string) and the version of the format, for example %PDF-1.7
      if (contents.size() >= 4U) {
        EXPECT_EQ(contents[0], '%');
        EXPECT_EQ(contents[1], 'P');
        EXPECT_EQ(contents[2], 'D');
        EXPECT_EQ(contents[3], 'F');
      }
    }

    EXPECT_TRUE(temp_dir_.Delete());
    EXPECT_TRUE(temp_dir_.IsEmpty());

    CefPostTask(TID_UI,
                base::BindOnce(&PrintToPdfTestHandler::CompleteTest, this));
  }

  void CompleteTest() {
    if (!verified_results_ && !temp_dir_.IsEmpty()) {
      verified_results_ = true;
      CefPostTask(TID_FILE_USER_VISIBLE,
                  base::BindOnce(
                      &PrintToPdfTestHandler::VerifyResultsOnFileThread, this));
      return;
    }

    DestroyTest();
  }

  void DestroyTest() override {
    EXPECT_TRUE(got_on_load_end_html_);
    EXPECT_TRUE(got_on_pdf_print_finished_);
    EXPECT_TRUE(verified_results_);

    if (temp_dir_.IsValid()) {
      ASSERT_TRUE(temp_dir_.Delete());
    }

    TestHandler::DestroyTest();
  }

  virtual void PrintToPDF(CefRefPtr<CefBrowser> browser) {
    CefPdfPrintSettings settings;
    browser->GetHost()->PrintToPDF(test_path_, settings, this);
  }

 protected:
  CefScopedTempDir temp_dir_;

  const std::string url_;
  CefString test_path_;
  const bool expect_ok_;
  bool verified_results_ = false;

  TrackCallback got_on_load_end_html_;
  TrackCallback got_on_pdf_print_finished_;

  IMPLEMENT_REFCOUNTING(PrintToPdfTestHandler);
};

class PrintToPdfInvalidMarginTestHandler : public PrintToPdfTestHandler {
 public:
  PrintToPdfInvalidMarginTestHandler(const std::string& url, bool expect_ok)
      : PrintToPdfTestHandler(url, expect_ok) {}

  virtual void PrintToPDF(CefRefPtr<CefBrowser> browser) override {
    CefPdfPrintSettings settings;
    settings.margin_left = -1;
    settings.margin_type = cef_pdf_print_margin_type_t::PDF_PRINT_MARGIN_CUSTOM;
    browser->GetHost()->PrintToPDF(test_path_, settings, this);
  }

 protected:
  IMPLEMENT_REFCOUNTING(PrintToPdfInvalidMarginTestHandler);
};

class PrintToPdfInvalidPaperDimTestHandler : public PrintToPdfTestHandler {
 public:
  PrintToPdfInvalidPaperDimTestHandler(const std::string& url, bool expect_ok)
      : PrintToPdfTestHandler(url, expect_ok) {}

  virtual void PrintToPDF(CefRefPtr<CefBrowser> browser) override {
    CefPdfPrintSettings settings;
    settings.paper_width = -1;
    settings.paper_height = -1;
    browser->GetHost()->PrintToPDF(test_path_, settings, this);
  }

 protected:
  IMPLEMENT_REFCOUNTING(PrintToPdfInvalidPaperDimTestHandler);
};

class PrintToPdfInvalidScaleTestHandler : public PrintToPdfTestHandler {
 public:
  PrintToPdfInvalidScaleTestHandler(const std::string& url, bool expect_ok)
      : PrintToPdfTestHandler(url, expect_ok) {}

  virtual void PrintToPDF(CefRefPtr<CefBrowser> browser) override {
    CefPdfPrintSettings settings;
    settings.scale = 999999999;
    browser->GetHost()->PrintToPDF(test_path_, settings, this);
  }

 protected:
  IMPLEMENT_REFCOUNTING(PrintToPdfInvalidScaleTestHandler);
};

#if defined(OS_WIN)
class PrintToPdfInvalidFileNameTestHandler : public PrintToPdfTestHandler {
 public:
  PrintToPdfInvalidFileNameTestHandler(const std::string& url, bool expect_ok)
      : PrintToPdfTestHandler(url, expect_ok) {}

  std::string GetTestFileName() override { return kTestFileNameInvalid; }

 protected:
  IMPLEMENT_REFCOUNTING(PrintToPdfInvalidFileNameTestHandler);
};
#endif  // defined(OS_WIN)

}  // namespace

TEST(PdfPrintTest, DefaultSettings) {
  CefRefPtr<PrintToPdfTestHandler> handler =
      new PrintToPdfTestHandler(kPrintHtmlUrl, true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(PdfPrintTest, InvalidMargin) {
  // Should still pass as settings are validated
  CefRefPtr<PrintToPdfInvalidMarginTestHandler> handler =
      new PrintToPdfInvalidMarginTestHandler(kPrintHtmlUrl, true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(PdfPrintTest, InvalidPageDim) {
  // Should still pass as settings are validated
  CefRefPtr<PrintToPdfInvalidPaperDimTestHandler> handler =
      new PrintToPdfInvalidPaperDimTestHandler(kPrintHtmlUrl, true);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(PdfPrintTest, InvalidScale) {
  CefRefPtr<PrintToPdfInvalidScaleTestHandler> handler =
      new PrintToPdfInvalidScaleTestHandler(kPrintHtmlUrl, false);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

#if defined(OS_WIN)
TEST(PdfPrintTest, InvalidFileName) {
  CefRefPtr<PrintToPdfInvalidFileNameTestHandler> handler =
      new PrintToPdfInvalidFileNameTestHandler(kPrintHtmlUrl, false);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}
#endif  // defined(OS_WIN)
