// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/base/cef_callback.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/test_handler.h"
#include "tests/ceftests/test_util.h"
#include "tests/gtest/include/gtest/gtest.h"

namespace {

const char* kTestUrl = "https://tests/DialogTestHandler";

class DialogTestHandler : public TestHandler {
 public:
  struct TestConfig {
    explicit TestConfig(FileDialogMode dialog_mode)
        : mode(dialog_mode),
          title("Test Title"),
          default_file_name("Test File Name") {
      accept_filters.push_back("image/*");
      // We're handling the dialog before MIME type expansion.
      accept_extensions.push_back(CefString());
      accept_filters.push_back(".js");
      accept_extensions.push_back(".js");
      accept_filters.push_back(".css");
      accept_extensions.push_back(".css");
    }

    FileDialogMode mode;
    CefString title;
    CefString default_file_name;
    std::vector<CefString> accept_filters;
    std::vector<CefString> accept_extensions;

    bool skip_first_callback = false;

    // True if the callback should execute asynchronously.
    bool callback_async = false;
    // True if the callback should cancel.
    bool callback_cancel = false;
    // Resulting paths if not cancelled.
    std::vector<CefString> callback_paths;
  };

  class Callback : public CefRunFileDialogCallback {
   public:
    explicit Callback(DialogTestHandler* handler) : handler_(handler) {}

    void OnFileDialogDismissed(
        const std::vector<CefString>& file_paths) override {
      handler_->got_onfiledialogdismissed_.yes();

      if (handler_->config_.callback_cancel) {
        EXPECT_TRUE(file_paths.empty());
      } else {
        TestStringVectorEqual(handler_->config_.callback_paths, file_paths);
      }

      handler_->DestroyTest();
      handler_ = nullptr;
    }

   private:
    DialogTestHandler* handler_;

    IMPLEMENT_REFCOUNTING(Callback);
  };

  explicit DialogTestHandler(const TestConfig& config) : config_(config) {}

  void RunTest() override {
    AddResource(kTestUrl, "<html><body>TEST</body></html>", "text/html");

    // Create the browser
    CreateBrowser(kTestUrl);

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    browser->GetHost()->RunFileDialog(
        config_.mode, config_.title, config_.default_file_name,
        config_.accept_filters, new Callback(this));
  }

  void ExecuteCallback(CefRefPtr<CefFileDialogCallback> callback) {
    if (config_.callback_cancel) {
      callback->Cancel();
    } else {
      callback->Continue(config_.callback_paths);
    }
  }

  // CefDialogHandler
  bool OnFileDialog(CefRefPtr<CefBrowser> browser,
                    FileDialogMode mode,
                    const CefString& title,
                    const CefString& default_file_name,
                    const std::vector<CefString>& accept_filters,
                    const std::vector<CefString>& accept_extensions,
                    const std::vector<CefString>& accept_descriptions,
                    CefRefPtr<CefFileDialogCallback> callback) override {
    got_onfiledialog_ct_++;

    std::string url = browser->GetMainFrame()->GetURL();
    EXPECT_STREQ(kTestUrl, url.c_str());

    EXPECT_EQ(config_.mode, mode);
    EXPECT_STREQ(config_.title.ToString().c_str(), title.ToString().c_str());

    EXPECT_EQ(accept_filters.size(), accept_extensions.size());
    EXPECT_EQ(accept_filters.size(), accept_descriptions.size());
    TestStringVectorEqual(config_.accept_filters, accept_filters);

    if (got_onfiledialog_ct_ == 1U) {
      // On the 2nd+ call this will be set to the last opened path value
      // (possibly leftover from a different test).
      EXPECT_STREQ(config_.default_file_name.ToString().c_str(),
                   default_file_name.ToString().c_str());

      TestStringVectorEqual(config_.accept_extensions, accept_extensions);

      // All descriptions should be empty.
      for (auto& desc : accept_descriptions) {
        EXPECT_TRUE(desc.empty());
      }

      if (config_.skip_first_callback) {
        return false;
      }
    } else if (got_onfiledialog_ct_ == 2U) {
      // All MIME types should be resolved to file extensions.
      // A description should be provided for MIME types only.
      for (size_t i = 0; i < accept_filters.size(); ++i) {
        const std::string filter = accept_filters[i];
        const std::string ext = accept_extensions[i];
        const std::string desc = accept_descriptions[i];
        if (filter == "image/*") {
          EXPECT_TRUE(ext.find(".png") > 0);
          EXPECT_TRUE(ext.find(".jpg") > 0);
          EXPECT_FALSE(desc.empty());
        } else {
          // Single file extensions should match.
          EXPECT_STREQ(filter.data(), ext.data());
          EXPECT_TRUE(desc.empty());
        }
      }
    } else {
      NOTREACHED();
    }

    if (config_.callback_async) {
      CefPostTask(TID_UI, base::BindOnce(&DialogTestHandler::ExecuteCallback,
                                         this, callback));
    } else {
      ExecuteCallback(callback);
    }

    return true;
  }

  void DestroyTest() override {
    if (config_.skip_first_callback) {
      EXPECT_EQ(2U, got_onfiledialog_ct_);
    } else {
      EXPECT_EQ(1U, got_onfiledialog_ct_);
    }
    EXPECT_TRUE(got_onfiledialogdismissed_);

    TestHandler::DestroyTest();
  }

  const TestConfig& config_;

  size_t got_onfiledialog_ct_ = 0;
  TrackCallback got_onfiledialogdismissed_;

  IMPLEMENT_REFCOUNTING(DialogTestHandler);
};

}  // namespace

// Test with all parameters empty.
TEST(DialogTest, FileEmptyParams) {
  DialogTestHandler::TestConfig config(FILE_DIALOG_OPEN);
  config.title.clear();
  config.default_file_name.clear();
  config.accept_filters.clear();
  config.accept_extensions.clear();
  config.callback_async = false;
  config.callback_cancel = false;

  CefRefPtr<DialogTestHandler> handler = new DialogTestHandler(config);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(DialogTest, FileOpen) {
  DialogTestHandler::TestConfig config(FILE_DIALOG_OPEN);
  config.callback_async = false;
  config.callback_cancel = false;
  config.callback_paths.push_back("/path/to/file1.txt");

  CefRefPtr<DialogTestHandler> handler = new DialogTestHandler(config);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(DialogTest, FileOpenSkipFirstCallback) {
  DialogTestHandler::TestConfig config(FILE_DIALOG_OPEN);
  config.callback_async = false;
  config.callback_cancel = false;
  config.callback_paths.push_back("/path/to/file1.txt");
  config.skip_first_callback = true;

  CefRefPtr<DialogTestHandler> handler = new DialogTestHandler(config);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(DialogTest, FileOpenCancel) {
  DialogTestHandler::TestConfig config(FILE_DIALOG_OPEN);
  config.callback_async = false;
  config.callback_cancel = true;

  CefRefPtr<DialogTestHandler> handler = new DialogTestHandler(config);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(DialogTest, FileOpenCancelSkipFirstCallback) {
  DialogTestHandler::TestConfig config(FILE_DIALOG_OPEN);
  config.callback_async = false;
  config.callback_cancel = true;
  config.skip_first_callback = true;

  CefRefPtr<DialogTestHandler> handler = new DialogTestHandler(config);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(DialogTest, FileOpenAsync) {
  DialogTestHandler::TestConfig config(FILE_DIALOG_OPEN);
  config.callback_async = true;
  config.callback_cancel = false;
  config.callback_paths.push_back("/path/to/file1.txt");

  CefRefPtr<DialogTestHandler> handler = new DialogTestHandler(config);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(DialogTest, FileOpenAsyncSkipFirstCallback) {
  DialogTestHandler::TestConfig config(FILE_DIALOG_OPEN);
  config.callback_async = true;
  config.callback_cancel = false;
  config.callback_paths.push_back("/path/to/file1.txt");
  config.skip_first_callback = true;

  CefRefPtr<DialogTestHandler> handler = new DialogTestHandler(config);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(DialogTest, FileOpenAsyncCancel) {
  DialogTestHandler::TestConfig config(FILE_DIALOG_OPEN);
  config.callback_async = true;
  config.callback_cancel = true;

  CefRefPtr<DialogTestHandler> handler = new DialogTestHandler(config);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(DialogTest, FileOpenAsyncCancelSkipFirstCallback) {
  DialogTestHandler::TestConfig config(FILE_DIALOG_OPEN);
  config.callback_async = true;
  config.callback_cancel = true;
  config.skip_first_callback = true;

  CefRefPtr<DialogTestHandler> handler = new DialogTestHandler(config);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(DialogTest, FileOpenMultiple) {
  DialogTestHandler::TestConfig config(FILE_DIALOG_OPEN_MULTIPLE);
  config.callback_async = false;
  config.callback_cancel = false;
  config.callback_paths.push_back("/path/to/file1.txt");
  config.callback_paths.push_back("/path/to/file2.txt");

  CefRefPtr<DialogTestHandler> handler = new DialogTestHandler(config);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(DialogTest, FileOpenMultipleCancel) {
  DialogTestHandler::TestConfig config(FILE_DIALOG_OPEN_MULTIPLE);
  config.callback_async = false;
  config.callback_cancel = true;

  CefRefPtr<DialogTestHandler> handler = new DialogTestHandler(config);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(DialogTest, FileOpenMultipleAsync) {
  DialogTestHandler::TestConfig config(FILE_DIALOG_OPEN_MULTIPLE);
  config.callback_async = true;
  config.callback_cancel = false;
  config.callback_paths.push_back("/path/to/file1.txt");
  config.callback_paths.push_back("/path/to/file2.txt");

  CefRefPtr<DialogTestHandler> handler = new DialogTestHandler(config);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(DialogTest, FileOpenMultipleAsyncCancel) {
  DialogTestHandler::TestConfig config(FILE_DIALOG_OPEN_MULTIPLE);
  config.callback_async = false;
  config.callback_cancel = true;

  CefRefPtr<DialogTestHandler> handler = new DialogTestHandler(config);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(DialogTest, FileOpenFolder) {
  DialogTestHandler::TestConfig config(FILE_DIALOG_OPEN_FOLDER);
  config.callback_async = false;
  config.callback_cancel = false;
  config.callback_paths.push_back("/path/to/folder");

  CefRefPtr<DialogTestHandler> handler = new DialogTestHandler(config);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(DialogTest, FileOpenFolderCancel) {
  DialogTestHandler::TestConfig config(FILE_DIALOG_OPEN_FOLDER);
  config.callback_async = false;
  config.callback_cancel = true;

  CefRefPtr<DialogTestHandler> handler = new DialogTestHandler(config);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(DialogTest, FileOpenFolderAsync) {
  DialogTestHandler::TestConfig config(FILE_DIALOG_OPEN_FOLDER);
  config.callback_async = true;
  config.callback_cancel = false;
  config.callback_paths.push_back("/path/to/folder");

  CefRefPtr<DialogTestHandler> handler = new DialogTestHandler(config);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(DialogTest, FileOpenFolderAsyncCancel) {
  DialogTestHandler::TestConfig config(FILE_DIALOG_OPEN_FOLDER);
  config.callback_async = false;
  config.callback_cancel = true;

  CefRefPtr<DialogTestHandler> handler = new DialogTestHandler(config);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(DialogTest, FileSave) {
  DialogTestHandler::TestConfig config(FILE_DIALOG_SAVE);
  config.callback_async = false;
  config.callback_cancel = false;
  config.callback_paths.push_back("/path/to/file1.txt");

  CefRefPtr<DialogTestHandler> handler = new DialogTestHandler(config);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(DialogTest, FileSaveCancel) {
  DialogTestHandler::TestConfig config(FILE_DIALOG_SAVE);
  config.callback_async = false;
  config.callback_cancel = true;

  CefRefPtr<DialogTestHandler> handler = new DialogTestHandler(config);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(DialogTest, FileSaveAsync) {
  DialogTestHandler::TestConfig config(FILE_DIALOG_SAVE);
  config.callback_async = true;
  config.callback_cancel = false;
  config.callback_paths.push_back("/path/to/file1.txt");

  CefRefPtr<DialogTestHandler> handler = new DialogTestHandler(config);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

TEST(DialogTest, FileSaveAsyncCancel) {
  DialogTestHandler::TestConfig config(FILE_DIALOG_SAVE);
  config.callback_async = false;
  config.callback_cancel = true;

  CefRefPtr<DialogTestHandler> handler = new DialogTestHandler(config);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}
