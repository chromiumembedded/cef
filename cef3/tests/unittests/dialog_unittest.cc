// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef_runnable.h"
#include "tests/unittests/test_handler.h"
#include "tests/unittests/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char* kTestUrl = "http://tests/DialogTestHandler";

class DialogTestHandler : public TestHandler {
 public:
  struct TestConfig {
    explicit TestConfig(FileDialogMode dialog_mode)
      : mode(dialog_mode),
        title("Test Title"),
        default_file_name("Test File Name"),
        callback_async(false),
        callback_cancel(false) {
      accept_types.push_back("text/*");
      accept_types.push_back(".js");
      accept_types.push_back(".css");
    }

    FileDialogMode mode;
    CefString title;
    CefString default_file_name;
    std::vector<CefString> accept_types;

    bool callback_async;  // True if the callback should execute asynchronously.
    bool callback_cancel;  // True if the callback should cancel.
    std::vector<CefString> callback_paths;  // Resulting paths if not cancelled.
  };

  class Callback : public CefRunFileDialogCallback {
   public:
    explicit Callback(DialogTestHandler* handler)
        : handler_(handler) {
    }

    virtual void OnFileDialogDismissed(
        CefRefPtr<CefBrowserHost> browser_host,
        const std::vector<CefString>& file_paths) OVERRIDE {
      handler_->got_onfiledialogdismissed_.yes();

      std::string url = browser_host->GetBrowser()->GetMainFrame()->GetURL();
      EXPECT_STREQ(kTestUrl, url.c_str());

      if (handler_->config_.callback_cancel)
        EXPECT_TRUE(file_paths.empty());
      else
        TestStringVectorEqual(handler_->config_.callback_paths, file_paths);

      handler_->DestroyTest();
      handler_ = NULL;
    }

   private:
    DialogTestHandler* handler_;

    IMPLEMENT_REFCOUNTING(Callback);
  };

  explicit DialogTestHandler(const TestConfig& config)
      : config_(config) {
  }

  virtual void RunTest() OVERRIDE {
    AddResource(kTestUrl, "<html><body>TEST</body></html>", "text/html");

    // Create the browser
    CreateBrowser(kTestUrl);
  }

  virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         int httpStatusCode) OVERRIDE {
    browser->GetHost()->RunFileDialog(config_.mode,
                                      config_.title,
                                      config_.default_file_name,
                                      config_.accept_types,
                                      new Callback(this));
  }

  void ExecuteCallback(CefRefPtr<CefFileDialogCallback> callback) {
    if (config_.callback_cancel)
      callback->Cancel();
    else
      callback->Continue(config_.callback_paths);
  }

  // CefDialogHandler
  virtual bool OnFileDialog(
      CefRefPtr<CefBrowser> browser,
      FileDialogMode mode,
      const CefString& title,
      const CefString& default_file_name,
      const std::vector<CefString>& accept_types,
      CefRefPtr<CefFileDialogCallback> callback) OVERRIDE {
    got_onfiledialog_.yes();

    std::string url = browser->GetMainFrame()->GetURL();
    EXPECT_STREQ(kTestUrl, url.c_str());

    EXPECT_EQ(config_.mode, mode);
    EXPECT_STREQ(config_.title.ToString().c_str(), title.ToString().c_str());
    EXPECT_STREQ(config_.default_file_name.ToString().c_str(),
                 default_file_name.ToString().c_str());
    TestStringVectorEqual(config_.accept_types, accept_types);

    if (config_.callback_async) {
      CefPostTask(TID_UI,
          NewCefRunnableMethod(this, &DialogTestHandler::ExecuteCallback,
                               callback));
    } else {
      ExecuteCallback(callback);
    }

    return true;
  }

  virtual void DestroyTest() OVERRIDE {
    EXPECT_TRUE(got_onfiledialog_);
    EXPECT_TRUE(got_onfiledialogdismissed_);

    TestHandler::DestroyTest();
  }

  TestConfig config_;

  TrackCallback got_onfiledialog_;
  TrackCallback got_onfiledialogdismissed_;
};

}  // namespace

// Test with all parameters empty.
TEST(DialogTest, FileEmptyParams) {
  DialogTestHandler::TestConfig config(FILE_DIALOG_OPEN);
  config.title.clear();
  config.default_file_name.clear();
  config.accept_types.clear();
  config.callback_async = false;
  config.callback_cancel = false;

  CefRefPtr<DialogTestHandler> handler = new DialogTestHandler(config);
  handler->ExecuteTest();
}

TEST(DialogTest, FileOpen) {
  DialogTestHandler::TestConfig config(FILE_DIALOG_OPEN);
  config.callback_async = false;
  config.callback_cancel = false;
  config.callback_paths.push_back("/path/to/file1.txt");

  CefRefPtr<DialogTestHandler> handler = new DialogTestHandler(config);
  handler->ExecuteTest();
}

TEST(DialogTest, FileOpenCancel) {
  DialogTestHandler::TestConfig config(FILE_DIALOG_OPEN);
  config.callback_async = false;
  config.callback_cancel = true;

  CefRefPtr<DialogTestHandler> handler = new DialogTestHandler(config);
  handler->ExecuteTest();
}

TEST(DialogTest, FileOpenAsync) {
  DialogTestHandler::TestConfig config(FILE_DIALOG_OPEN);
  config.callback_async = true;
  config.callback_cancel = false;
  config.callback_paths.push_back("/path/to/file1.txt");

  CefRefPtr<DialogTestHandler> handler = new DialogTestHandler(config);
  handler->ExecuteTest();
}

TEST(DialogTest, FileOpenAsyncCancel) {
  DialogTestHandler::TestConfig config(FILE_DIALOG_OPEN);
  config.callback_async = false;
  config.callback_cancel = true;

  CefRefPtr<DialogTestHandler> handler = new DialogTestHandler(config);
  handler->ExecuteTest();
}

TEST(DialogTest, FileOpenMultiple) {
  DialogTestHandler::TestConfig config(FILE_DIALOG_OPEN_MULTIPLE);
  config.callback_async = false;
  config.callback_cancel = false;
  config.callback_paths.push_back("/path/to/file1.txt");
  config.callback_paths.push_back("/path/to/file2.txt");

  CefRefPtr<DialogTestHandler> handler = new DialogTestHandler(config);
  handler->ExecuteTest();
}

TEST(DialogTest, FileOpenMultipleCancel) {
  DialogTestHandler::TestConfig config(FILE_DIALOG_OPEN_MULTIPLE);
  config.callback_async = false;
  config.callback_cancel = true;

  CefRefPtr<DialogTestHandler> handler = new DialogTestHandler(config);
  handler->ExecuteTest();
}

TEST(DialogTest, FileOpenMultipleAsync) {
  DialogTestHandler::TestConfig config(FILE_DIALOG_OPEN_MULTIPLE);
  config.callback_async = true;
  config.callback_cancel = false;
  config.callback_paths.push_back("/path/to/file1.txt");
  config.callback_paths.push_back("/path/to/file2.txt");

  CefRefPtr<DialogTestHandler> handler = new DialogTestHandler(config);
  handler->ExecuteTest();
}

TEST(DialogTest, FileOpenMultipleAsyncCancel) {
  DialogTestHandler::TestConfig config(FILE_DIALOG_OPEN_MULTIPLE);
  config.callback_async = false;
  config.callback_cancel = true;

  CefRefPtr<DialogTestHandler> handler = new DialogTestHandler(config);
  handler->ExecuteTest();
}

TEST(DialogTest, FileSave) {
  DialogTestHandler::TestConfig config(FILE_DIALOG_SAVE);
  config.callback_async = false;
  config.callback_cancel = false;
  config.callback_paths.push_back("/path/to/file1.txt");

  CefRefPtr<DialogTestHandler> handler = new DialogTestHandler(config);
  handler->ExecuteTest();
}

TEST(DialogTest, FileSaveCancel) {
  DialogTestHandler::TestConfig config(FILE_DIALOG_SAVE);
  config.callback_async = false;
  config.callback_cancel = true;

  CefRefPtr<DialogTestHandler> handler = new DialogTestHandler(config);
  handler->ExecuteTest();
}

TEST(DialogTest, FileSaveAsync) {
  DialogTestHandler::TestConfig config(FILE_DIALOG_SAVE);
  config.callback_async = true;
  config.callback_cancel = false;
  config.callback_paths.push_back("/path/to/file1.txt");

  CefRefPtr<DialogTestHandler> handler = new DialogTestHandler(config);
  handler->ExecuteTest();
}

TEST(DialogTest, FileSaveAsyncCancel) {
  DialogTestHandler::TestConfig config(FILE_DIALOG_SAVE);
  config.callback_async = false;
  config.callback_cancel = true;

  CefRefPtr<DialogTestHandler> handler = new DialogTestHandler(config);
  handler->ExecuteTest();
}
