// Copyright 2016 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/base/cef_bind.h"
#include "include/cef_task.h"
#include "include/cef_thread.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/ceftests/test_handler.h"
#include "tests/gtest/include/gtest/gtest.h"
#include "tests/shared/browser/client_app_browser.h"
#include "tests/shared/renderer/client_app_renderer.h"

using client::ClientAppBrowser;
using client::ClientAppRenderer;

namespace {

// Base class for creating and testing threads.
class ThreadTest : public base::RefCountedThreadSafe<ThreadTest> {
 public:
  ThreadTest() {}
  virtual ~ThreadTest() {}

  // Create the test thread. Should only be called one time.
  void CreateTestThread() {
    EXPECT_TRUE(!thread_.get());

    owner_task_runner_ = CefTaskRunner::GetForCurrentThread();
    EXPECT_TRUE(owner_task_runner_.get());
    EXPECT_TRUE(owner_task_runner_->BelongsToCurrentThread());

    thread_ = CefThread::CreateThread("test_thread");
    EXPECT_TRUE(thread_.get());
    EXPECT_TRUE(thread_->IsRunning());

    thread_id_ = thread_->GetPlatformThreadId();
    EXPECT_NE(thread_id_, kInvalidPlatformThreadId);

    thread_task_runner_ = thread_->GetTaskRunner();
    EXPECT_TRUE(thread_task_runner_.get());

    AssertOwnerThread();
  }

  // Destroy the test thread. Should only be called one time.
  void DestroyTestThread() {
    EXPECT_TRUE(thread_.get());
    AssertOwnerThread();

    EXPECT_TRUE(thread_->IsRunning());
    thread_->Stop();
    EXPECT_FALSE(thread_->IsRunning());

    AssertOwnerThread();

    thread_ = nullptr;
  }

  // Execute |test_task| on the test thread. After execution |callback| will be
  // posted to |callback_task_runner|.
  void PostOnTestThreadAndCallback(
      const base::Closure& test_task,
      CefRefPtr<CefTaskRunner> callback_task_runner,
      const base::Closure& callback) {
    EXPECT_TRUE(thread_.get());
    thread_task_runner_->PostTask(CefCreateClosureTask(
        base::Bind(&ThreadTest::ExecuteOnTestThread, this, test_task,
                   callback_task_runner, callback)));
  }

  CefRefPtr<CefTaskRunner> owner_task_runner() const {
    return owner_task_runner_;
  }
  CefRefPtr<CefTaskRunner> thread_task_runner() const {
    return thread_task_runner_;
  }

  // Assert that we're running on the owner thread.
  void AssertOwnerThread() {
    EXPECT_TRUE(owner_task_runner_->BelongsToCurrentThread());
    EXPECT_FALSE(thread_task_runner_->BelongsToCurrentThread());
    EXPECT_TRUE(thread_task_runner_->IsSame(thread_->GetTaskRunner()));
    EXPECT_EQ(thread_id_, thread_->GetPlatformThreadId());
  }

  // Assert that we're running on the test thread.
  void AssertTestThread() {
    EXPECT_FALSE(owner_task_runner_->BelongsToCurrentThread());
    EXPECT_TRUE(thread_task_runner_->BelongsToCurrentThread());
    EXPECT_TRUE(thread_task_runner_->IsSame(thread_->GetTaskRunner()));
    EXPECT_EQ(thread_id_, thread_->GetPlatformThreadId());
  }

 private:
  // Helper for PostOnTestThreadAndCallback().
  void ExecuteOnTestThread(const base::Closure& test_task,
                           CefRefPtr<CefTaskRunner> callback_task_runner,
                           const base::Closure& callback) {
    AssertTestThread();

    test_task.Run();

    callback_task_runner->PostTask(CefCreateClosureTask(callback));
  }

  CefRefPtr<CefTaskRunner> owner_task_runner_;

  CefRefPtr<CefThread> thread_;
  cef_platform_thread_id_t thread_id_;
  CefRefPtr<CefTaskRunner> thread_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(ThreadTest);
};

}  // namespace

// Test thread creation and destruction without any task execution.
TEST(ThreadTest, Create) {
  scoped_refptr<ThreadTest> thread_test = new ThreadTest();
  thread_test->CreateTestThread();
  thread_test->DestroyTestThread();
  thread_test = nullptr;
}


namespace {

// Simple implementation of ThreadTest that creates a thread, executes tasks
// on the thread, then destroys the thread after all tasks have completed.
class SimpleThreadTest : public ThreadTest {
 public:
  SimpleThreadTest(size_t expected_task_count,
                   const base::Closure& task_callback,
                   const base::Closure& done_callback)
    : expected_task_count_(expected_task_count),
      task_callback_(task_callback),
      done_callback_(done_callback),
      got_task_count_(0U),
      got_done_count_(0U) {}

  void RunTest() {
    // Create the test thread.
    CreateTestThread();

    for (size_t i = 0U; i < expected_task_count_; ++i) {
      // Execute Task() on the test thread and then call Done() on this thread.
      PostOnTestThreadAndCallback(
          base::Bind(&SimpleThreadTest::Task, this), owner_task_runner(),
          base::Bind(&SimpleThreadTest::Done, this));
    }
  }

  void DestroyTest() {
    EXPECT_EQ(expected_task_count_, got_task_count_);
    EXPECT_EQ(expected_task_count_, got_done_count_);

    // Destroy the test thread.
    DestroyTestThread();
  }

 private:
  void Task() {
    AssertTestThread();
    got_task_count_++;
    if (!task_callback_.is_null())
      task_callback_.Run();
  }

  void Done() {
    AssertOwnerThread();
    if (++got_done_count_ == expected_task_count_ && !done_callback_.is_null())
      done_callback_.Run();
  }

  const size_t expected_task_count_;
  base::Closure task_callback_;
  base::Closure done_callback_;

  size_t got_task_count_;
  size_t got_done_count_;

  DISALLOW_COPY_AND_ASSIGN(SimpleThreadTest);
};


// Test creation/execution of threads in the browser process.

const char kBrowserThreadTestHtml[] = "http://test.com/browserthread.html";

// Browser side.
class BrowserThreadTestHandler : public TestHandler {
 public:
  explicit BrowserThreadTestHandler(CefThreadId owner_thread_id)
    : owner_thread_id_(owner_thread_id) {}

  void RunTest() override {
    AddResource(kBrowserThreadTestHtml, "<html><body>Test</body></html>",
                "text/html");

    CreateBrowser(kBrowserThreadTestHtml);

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  void RunThreadTestOnOwnerThread() {
    if (!CefCurrentlyOn(owner_thread_id_)) {
      // Run the test on the desired owner thread.
      CefPostTask(owner_thread_id_,
          base::Bind(&BrowserThreadTestHandler::RunThreadTestOnOwnerThread,
                     this));
      return;
    }

    EXPECT_FALSE(thread_test_.get());
    thread_test_ = new SimpleThreadTest(
        3, base::Closure(),
        base::Bind(&BrowserThreadTestHandler::DoneOnOwnerThread, this));
    thread_test_->RunTest();
  }

  void DoneOnOwnerThread() {
    // Let the call stack unwind before destroying |thread_test_|.
    CefPostTask(owner_thread_id_,
        base::Bind(&BrowserThreadTestHandler::DestroyTestOnOwnerThread, this));
  }

  void DestroyTestOnOwnerThread() {
    EXPECT_TRUE(CefCurrentlyOn(owner_thread_id_));

    EXPECT_TRUE(thread_test_.get());
    if (thread_test_) {
      thread_test_->DestroyTest();
      thread_test_ = nullptr;
    }

    got_test_done_.yes();

    // Call DestroyTest() on the UI thread.
    CefPostTask(TID_UI,
        base::Bind(&BrowserThreadTestHandler::DestroyTest, this));
  }

  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading,
                            bool canGoBack,
                            bool canGoForward) override {
    if (!isLoading)
      RunThreadTestOnOwnerThread();
  }

 private:
  void DestroyTest() override {
    EXPECT_FALSE(thread_test_.get());
    EXPECT_TRUE(got_test_done_);

    TestHandler::DestroyTest();
  }

  const CefThreadId owner_thread_id_;

  scoped_refptr<SimpleThreadTest> thread_test_;
  TrackCallback got_test_done_;

  IMPLEMENT_REFCOUNTING(BrowserThreadTestHandler);
  DISALLOW_COPY_AND_ASSIGN(BrowserThreadTestHandler);
};

}  // namespace

// Test creation of new threads from the browser UI thread.
TEST(ThreadTest, CreateFromBrowserUIThread) {
  CefRefPtr<BrowserThreadTestHandler> handler =
      new BrowserThreadTestHandler(TID_UI);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Test creation of new threads from the browser IO thread.
TEST(ThreadTest, CreateFromBrowserIOThread) {
  CefRefPtr<BrowserThreadTestHandler> handler =
      new BrowserThreadTestHandler(TID_IO);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}

// Test creation of new threads from the browser FILE thread.
TEST(ThreadTest, CreateFromBrowserFILEThread) {
  CefRefPtr<BrowserThreadTestHandler> handler =
      new BrowserThreadTestHandler(TID_FILE);
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}


namespace {

// Test creation/execution of threads in the render process.

const char kRenderThreadTestHtml[] = "http://test.com/renderthread.html";
const char kRenderThreadTestMsg[] = "ThreadTest.RenderThreadTest";

// Browser side.
class RenderThreadTestHandler : public TestHandler {
 public:
  RenderThreadTestHandler() {}

  void RunTest() override {
    AddResource(kRenderThreadTestHtml, "<html><body>Test</body></html>",
                "text/html");

    CreateBrowser(kRenderThreadTestHtml);

    // Time out the test after a reasonable period of time.
    SetTestTimeout();
  }

  void OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                            bool isLoading,
                            bool canGoBack,
                            bool canGoForward) override {
    if (!isLoading) {
      // Return the test in the render process.
      CefRefPtr<CefProcessMessage> msg =
          CefProcessMessage::Create(kRenderThreadTestMsg);
      EXPECT_TRUE(browser->SendProcessMessage(PID_RENDERER, msg));
    }
  }

  bool OnProcessMessageReceived(
      CefRefPtr<CefBrowser> browser,
      CefProcessId source_process,
      CefRefPtr<CefProcessMessage> message) override {
    EXPECT_TRUE(browser.get());
    EXPECT_EQ(PID_RENDERER, source_process);
    EXPECT_TRUE(message.get());
    EXPECT_TRUE(message->IsReadOnly());

    const std::string& message_name = message->GetName();
    EXPECT_STREQ(kRenderThreadTestMsg, message_name.c_str());

    got_message_.yes();

    if (message->GetArgumentList()->GetBool(0))
      got_success_.yes();

    // Test is complete.
    DestroyTest();

    return true;
  }

 protected:
  void DestroyTest() override {
    EXPECT_TRUE(got_message_);
    EXPECT_TRUE(got_success_);

    TestHandler::DestroyTest();
  }

  TrackCallback got_message_;
  TrackCallback got_success_;

  IMPLEMENT_REFCOUNTING(RenderThreadTestHandler);
  DISALLOW_COPY_AND_ASSIGN(RenderThreadTestHandler);
};

// Renderer side.
class RenderThreadRendererTest : public ClientAppRenderer::Delegate {
 public:
  RenderThreadRendererTest() {}

  bool OnProcessMessageReceived(
      CefRefPtr<ClientAppRenderer> app,
      CefRefPtr<CefBrowser> browser,
      CefProcessId source_process,
      CefRefPtr<CefProcessMessage> message) override {
    if (message->GetName().ToString() == kRenderThreadTestMsg) {
      browser_ = browser;
      EXPECT_FALSE(thread_test_.get());
      thread_test_ = new SimpleThreadTest(
          3, base::Closure(),
          base::Bind(&RenderThreadRendererTest::Done, this));
      thread_test_->RunTest();
      return true;
    }

    // Message not handled.
    return false;
  }

 private:
  void Done() {
    // Let the call stack unwind before destroying |thread_test_|.
    CefPostTask(TID_RENDERER,
        base::Bind(&RenderThreadRendererTest::DestroyTest, this));
  }

  void DestroyTest() {
    EXPECT_TRUE(thread_test_.get());
    if (thread_test_) {
      thread_test_->DestroyTest();
      thread_test_ = nullptr;
    }

    // Check if the test has failed.
    bool result = !TestFailed();

    // Return the result to the browser process.
    CefRefPtr<CefProcessMessage> return_msg =
        CefProcessMessage::Create(kRenderThreadTestMsg);
    EXPECT_TRUE(return_msg->GetArgumentList()->SetBool(0, result));
    EXPECT_TRUE(browser_->SendProcessMessage(PID_BROWSER, return_msg));

    browser_ = nullptr;
  }

  CefRefPtr<CefBrowser> browser_;
  scoped_refptr<SimpleThreadTest> thread_test_;

  IMPLEMENT_REFCOUNTING(RenderThreadRendererTest);
  DISALLOW_COPY_AND_ASSIGN(RenderThreadRendererTest);
};

}  // namespace

TEST(ThreadTest, CreateFromRenderThread) {
  CefRefPtr<RenderThreadTestHandler> handler = new RenderThreadTestHandler();
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}


// Entry point for creating request handler renderer test objects.
// Called from client_app_delegates.cc.
void CreateThreadRendererTests(
    ClientAppRenderer::DelegateSet& delegates) {
  delegates.insert(new RenderThreadRendererTest);
}
