# CEF Test Instructions for Claude Code

You are assisting with creating and running CEF unit tests from the `//cef/tests/ceftests` target.

## Quick Start

When the user asks you to work with ceftests, they typically want to:

- **Run tests** - Execute existing unit tests to verify functionality
- **Create new tests** - Write new unit tests for CEF features
- **Debug test failures** - Investigate and fix failing tests

## Understanding ceftests

**ceftests** is CEF's automated unit test suite built on Google Test (gtest):

- **Framework**: Google Test for test structure and assertions
- **Coverage**: Tests most CEF APIs across all platforms
- **Modes**: Chrome style, Alloy style, Views framework, headless
- **Location**: `//cef/tests/ceftests/`
- **Build target**: `ceftests`

**Key infrastructure files:**

- `test_handler.h/cc` - Base class for browser-based tests
- `test_suite.h/cc` - Test suite initialization
- `run_all_unittests.cc` - Main entry point
- `routing_test_handler.h/cc` - Base for message routing tests
- `test_server.h/cc` - HTTP test server
- `track_callback.h` - Callback execution tracking

## CRITICAL: Working Directory Awareness

**BEFORE running ANY command with `cd` or relative paths:**

1. ✓ **Check current directory:** `pwd`
2. ✓ **Determine the correct path** from your current location
3. ✓ **Execute the command** with the correct path

**Never assume your current directory. Always verify first.**

## Running Tests

### Step 1: Build ceftests

**Determine the build output directory:**

The user will typically provide the output directory, or you can identify it:

- **Common directories:** `out/Debug_GN_x64` (Windows/Linux), `out/Debug_GN_arm64` (Mac ARM)
- **Check existing builds:** `ls out/` to see available build directories
- **Look for .gn args:** The directory usually contains an `args.gn` file

```bash
# From chromium/src directory:
autoninja -C out/Debug_GN_x64 ceftests

# With error tolerance
autoninja -k 0 -C out/Debug_GN_x64 ceftests
```

**Replace `out/Debug_GN_x64` with the actual output directory for your platform.**

**If .gn files changed, regenerate build files first:**

```bash
# From chromium/src directory:
# Windows: cef\create_debug.bat
# Linux/Mac: ./cef/create_debug.sh
```

### Step 2: Run Tests

**Platform-specific paths:**

```bash
# Windows
out\Debug_GN_x64\ceftests.exe

# Linux
out/Debug_GN_x64/ceftests

# macOS (must use .app bundle path)
out/Debug_GN_arm64/ceftests.app/Contents/MacOS/ceftests
```

**Common options:**

```bash
# Run specific test suite
ceftests --gtest_filter="NavigationTest.*"

# Run specific test
ceftests --gtest_filter="NavigationTest.LoadURL"

# Exclude tests
ceftests --gtest_filter="-AudioOutputTest.*"

# List all tests
ceftests --gtest_list_tests

# Platform-specific
xvfb-run ceftests --no-sandbox          # Linux headless
```

**CEF execution modes (IMPORTANT):**

```bash
# Recommended: Use Views framework for most tests
ceftests --use-views

# Test both Chrome style and Alloy style
ceftests --use-views --gtest_filter="NavigationTest.*"
ceftests --use-views --use-alloy-style --gtest_filter="NavigationTest.*"

# Most tests should pass in both modes
```

**Testing new tests:**

When creating new tests, verify they work in both Chrome and Alloy styles:

```bash
# Chrome style (default)
ceftests --use-views --gtest_filter="MyNewTest.*"

# Alloy style
ceftests --use-views --use-alloy-style --gtest_filter="MyNewTest.*"
```

### Step 3: Interpret Results

**Success indicators:**

- All tests show `[       OK ]`
- Final summary: `[  PASSED  ]` for all tests
- Exit code 0
- No "leaked TestHandler objects"

**Failure indicators:**

- `[  FAILED  ]` status
- Assertion failures (`EXPECT_*`, `ASSERT_*`)
- Timeout (60s default)
- Crash or hang

## Creating New Tests

### Test Type 1: Simple Unit Tests

For utilities and simple APIs without browser integration:

```cpp
#include "tests/gtest/include/gtest/gtest.h"
#include "include/cef_parser.h"

TEST(MyFeatureTest, BasicFunctionality) {
  CefString input("test_value");
  CefString result = CefURIEncode(input, false);
  EXPECT_STREQ("test_value", result.ToString().c_str());
}
```

**Common assertions:**

```cpp
EXPECT_TRUE(condition);
EXPECT_FALSE(condition);
EXPECT_EQ(expected, actual);
EXPECT_STREQ("expected", actual);
EXPECT_NE(val1, val2);
EXPECT_LT(val1, val2);  // less than
EXPECT_GT(val1, val2);  // greater than
ASSERT_TRUE(critical);  // stops test on failure
ADD_FAILURE();          // manually fail test

// Add custom error details with <<
EXPECT_EQ(expected, actual) << "Custom error message";
EXPECT_TRUE(condition) << "Failed for value: " << value;
ADD_FAILURE() << "Detailed failure reason: " << details;
```

### Test Type 2: Browser-Based Tests

For features requiring browser instances, extend `TestHandler`. Use `AddResource()` to provide mock content - tests must be self-contained and not rely on external URLs.

```cpp
#include "tests/ceftests/test_handler.h"

namespace {

const char kTestUrl[] = "https://tests.test/page.html";
const char kTestContent[] = "<html><body>Test Page</body></html>";

class MyTestHandler : public TestHandler {
 public:
  MyTestHandler() = default;

  MyTestHandler(const MyTestHandler&) = delete;
  MyTestHandler& operator=(const MyTestHandler&) = delete;

  void RunTest() override {
    // Add mock resource before creating browser
    AddResource(kTestUrl, kTestContent, "text/html");
    CreateBrowser(kTestUrl);

    // Set timeout (starts the clock). Use default unless test needs longer.
    SetTestTimeout();
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    // Verify
    EXPECT_EQ(200, httpStatusCode);
    EXPECT_STREQ(kTestUrl, frame->GetURL().ToString().c_str());

    // Always call to complete test
    DestroyTest();
  }

  IMPLEMENT_REFCOUNTING(MyTestHandler);
};

}  // namespace

TEST(MyFeatureTest, BrowserTest) {
  CefRefPtr<MyTestHandler> handler = new MyTestHandler();
  handler->ExecuteTest();
  EXPECT_TRUE(handler->succeeded());
}
```

**TestHandler key methods:**

```cpp
// Override these:
void RunTest() override;           // Start test - create browser
void OnLoadEnd(...) override;      // Page loaded
void OnLoadError(...) override;    // Load failed

// Call these:
CreateBrowser(url);                // Create browser
DestroyTest();                     // Complete test (REQUIRED)
AddResource(url, content, mime);   // Add mock resource
GetBrowser();                      // Get browser instance
SetTestTimeout();                  // Set timeout (default 5s), call at end of RunTest()
```

**Test timeout:**

- Call `SetTestTimeout()` at the end of `RunTest()` to prevent hung tests
- Uses default timeout (5000ms) unless you pass a custom value
- Disable at runtime with `--disable-test-timeout` flag

**Important:** If you need to test a CEF handler interface not yet implemented by `TestHandler`:

1. Check `test_handler.h` to see which interfaces `TestHandler` currently implements
2. If needed, add the new interface to `TestHandler` class declaration in `test_handler.h`
3. Implement the corresponding getter (e.g., `GetContextMenuHandler()`) in `test_handler.cc`
4. Then override the specific callbacks in your test handler

**URL conventions:**

- Use valid TLDs: `.test`, `.example`, `.localhost` for mock URLs
- Only use custom schemes (e.g., `custom://`) when explicitly testing scheme handlers

### Test Type 3: Message Router Tests

For JavaScript-C++ communication, extend `RoutingTestHandler`:

```cpp
#include "tests/ceftests/routing_test_handler.h"

namespace {

const char kTestUrl[] = "https://test.example/router.html";
const char kTestHtml[] =
    "<html><script>"
    "window.cefQuery({request: 'test_message', onSuccess: function(r) {"
    "  window.testResult = r;"
    "}});"
    "</script></html>";

class RouterTestHandler : public RoutingTestHandler {
 public:
  RouterTestHandler(const RouterTestHandler&) = delete;
  RouterTestHandler& operator=(const RouterTestHandler&) = delete;

  void RunTest() override {
    AddResource(kTestUrl, kTestHtml, "text/html");
    CreateBrowser(kTestUrl);
    SetTestTimeout();
  }

  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64_t query_id,
               const CefString& request,
               bool persistent,
               CefRefPtr<Callback> callback) override {
    if (request == "test_message") {
      callback->Success("response_from_cpp");
      got_query_.yes();
      MaybeDestroyTest();
      return true;
    }
    return false;
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    got_load_end_.yes();
    MaybeDestroyTest();
  }

  void MaybeDestroyTest() {
    // Wait for both page load and query to complete.
    // The query might execute before or after OnLoadEnd (race condition),
    // so we need to check both conditions before destroying.
    if (got_load_end_ && got_query_) {
      DestroyTest();
    }
  }

  void DestroyTest() override {
    // Verify both callbacks were called
    EXPECT_TRUE(got_load_end_);
    EXPECT_TRUE(got_query_);

    RoutingTestHandler::DestroyTest();
  }

 private:
  TrackCallback got_load_end_;
  TrackCallback got_query_;
  IMPLEMENT_REFCOUNTING(RouterTestHandler);
};

}  // namespace
```

### Test Type 4: TestServer-based Tests

For testing HTTP/HTTPS requests, CORS, redirects, custom headers, etc., extend both `TestHandler` AND `CefTestServerHandler`:

```cpp
#include "include/test/cef_test_server.h"
#include "tests/ceftests/test_handler.h"

namespace {

class ServerTestHandler : public TestHandler, public CefTestServerHandler {
 public:
  ServerTestHandler() = default;

  ServerTestHandler(const ServerTestHandler&) = delete;
  ServerTestHandler& operator=(const ServerTestHandler&) = delete;

  void RunTest() override {
    SetTestTimeout();
    CefPostTask(TID_UI,
                base::BindOnce(&ServerTestHandler::StartServer, this));
  }

  // CefTestServerHandler method
  bool OnTestServerRequest(
      CefRefPtr<CefTestServer> server,
      CefRefPtr<CefRequest> request,
      CefRefPtr<CefTestServerConnection> connection) override {
    // Handle the request on UI thread
    CefPostTask(TID_UI,
                base::BindOnce(&ServerTestHandler::HandleRequest, this,
                               request, connection));
    return true;
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    got_load_end_.yes();

    // Verify response
    EXPECT_EQ(200, httpStatusCode);

    // Check if we can stop server
    MaybeStopServer();
  }

  void DestroyTest() override {
    EXPECT_FALSE(server_);  // Server should be stopped
    EXPECT_TRUE(got_load_end_);
    EXPECT_TRUE(got_request_);

    TestHandler::DestroyTest();
  }

 private:
  void StartServer() {
    EXPECT_UI_THREAD();

    // Create and start server
    // port=0 means auto-assign port
    // https_server=false for HTTP, true for HTTPS
    server_ = CefTestServer::CreateAndStart(
        /*port=*/0, /*https_server=*/false,
        /*cert_type=*/CEF_TEST_CERT_OK_IP, this);

    server_origin_ = server_->GetOrigin();

    // Create browser pointing to server
    CreateBrowser(server_origin_ + "/test.html");
  }

  void HandleRequest(CefRefPtr<CefRequest> request,
                     CefRefPtr<CefTestServerConnection> connection) {
    EXPECT_UI_THREAD();
    got_request_.yes();

    const std::string response = "<html><body>Test Response</body></html>";
    connection->SendHttp200Response("text/html", response.c_str(),
                                    response.size());

    // Check if we can stop server
    MaybeStopServer();
  }

  void MaybeStopServer() {
    // Wait for both request and page load to complete.
    // The request might complete before or after OnLoadEnd (race condition).
    if (got_request_ && got_load_end_) {
      StopServer();
    }
  }

  void StopServer() {
    EXPECT_UI_THREAD();

    server_->Stop();
    server_ = nullptr;

    DestroyTest();
  }

  CefRefPtr<CefTestServer> server_;
  std::string server_origin_;

  TrackCallback got_request_;
  TrackCallback got_load_end_;

  IMPLEMENT_REFCOUNTING(ServerTestHandler);
};

}  // namespace

TEST(ServerTest, BasicRequest) {
  CefRefPtr<ServerTestHandler> handler = new ServerTestHandler();
  handler->ExecuteTest();
  ReleaseAndWaitForDestructor(handler);
}
```

**Key points:**

- Extend both `TestHandler` AND `CefTestServerHandler`
- Use `CefTestServer::CreateAndStart()` to create the server
- Post to UI thread in `OnTestServerRequest()` - the callback may be called from a non-UI thread, but TestHandler operations require UI thread
- Use `connection->SendHttp200Response()` to send responses
- Use `MaybeStopServer()` pattern to wait for both request and load to complete
- Call `server_->Stop()` to stop server before destroying test
- For HTTPS: set `https_server=true` and specify `cert_type` (e.g., `CEF_TEST_CERT_OK_IP`)

### Test Type 5: Request Context Tests

For testing features that require isolated request contexts (cookies, cache, preferences), use the `RC_TEST` macros:

```cpp
#include "tests/ceftests/test_handler.h"
#include "tests/ceftests/test_util.h"

namespace {

enum TestMode {
  FIRST,
  SECOND,
};

class RequestContextTestHandler : public TestHandler {
 public:
  RequestContextTestHandler(TestMode test_mode,
                           TestRequestContextMode rc_mode,
                           const std::string& rc_cache_path)
      : test_mode_(test_mode),
        rc_mode_(rc_mode),
        rc_cache_path_(rc_cache_path) {}

  RequestContextTestHandler(const RequestContextTestHandler&) = delete;
  RequestContextTestHandler& operator=(const RequestContextTestHandler&) = delete;

  void RunTest() override {
    // Create request context asynchronously
    CreateTestRequestContext(rc_mode_, rc_cache_path_,
                            base::BindOnce(&RequestContextTestHandler::RunTestContinue,
                                          this));
  }

  void RunTestContinue(CefRefPtr<CefRequestContext> request_context) {
    request_context_ = request_context;

    // Use |test_mode_| and |request_context| for your test
    AddResource("https://tests.test/page.html", "<html><body>Test</body></html>",
                "text/html");

    // Create browser with custom request context
    CreateBrowser("https://tests.test/page.html", request_context_);
    SetTestTimeout();
  }

  void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                 CefRefPtr<CefFrame> frame,
                 int httpStatusCode) override {
    // Test logic using request_context_

    DestroyTest();
  }

 private:
  const TestMode test_mode_;
  const TestRequestContextMode rc_mode_;
  const std::string rc_cache_path_;
  CefRefPtr<CefRequestContext> request_context_;

  IMPLEMENT_REFCOUNTING(RequestContextTestHandler);
};

}  // namespace

// Define the test macro for this test
#define MY_TEST_GROUP(test_name, test_mode) \
  RC_TEST_GROUP_ALL(MyRequestContextTest, test_name, \
                    RequestContextTestHandler, test_mode)

// Create tests for both test modes
// This expands to multiple tests like:
//   MyRequestContextTest.FirstRCNone
//   MyRequestContextTest.FirstRCGlobal
//   MyRequestContextTest.FirstRCCustomInMemoryWithHandler
//   MyRequestContextTest.FirstRCCustomOnDiskWithHandler
//   MyRequestContextTest.SecondRCNone
//   ... etc
MY_TEST_GROUP(First, FIRST)
MY_TEST_GROUP(Second, SECOND)
```

**RC_TEST macro variants:**

- `RC_TEST_GROUP_ALL` - Tests all request context modes (in-memory + on-disk)
- `RC_TEST_GROUP_IN_MEMORY` - Tests only in-memory modes (no disk cache)
- `RC_TEST_GROUP_ON_DISK` - Tests only on-disk modes (with disk cache)

**Request context modes tested:**

- `TEST_RC_MODE_NONE` - No custom request context
- `TEST_RC_MODE_GLOBAL` - Global request context
- `TEST_RC_MODE_GLOBAL_WITH_HANDLER` - Global with handler
- `TEST_RC_MODE_CUSTOM_WITH_HANDLER` - Custom context (in-memory or on-disk)

### Test Type 6: Process-Level Handler Tests

For browser or renderer process-level features, use **ClientApp delegates**. These provide global process-level hooks (not per-browser instance like TestHandler).

**When to use:**

- Renderer process features (JavaScript execution, DOM manipulation, V8 bindings)
- Browser process initialization/configuration (preferences, schemes)
- Process message handling between browser and renderer
- Global lifecycle events (context initialization, process shutdown)

**ClientAppBrowser::Delegate** - Browser process hooks:

```cpp
#include "tests/shared/browser/client_app_browser.h"

namespace {

class MyBrowserDelegate : public client::ClientAppBrowser::Delegate {
 public:
  MyBrowserDelegate() = default;

  MyBrowserDelegate(const MyBrowserDelegate&) = delete;
  MyBrowserDelegate& operator=(const MyBrowserDelegate&) = delete;

  void OnBeforeCommandLineProcessing(
      CefRefPtr<client::ClientAppBrowser> app,
      CefRefPtr<CefCommandLine> command_line) override {
    // Called early in browser process startup
    // Configure browser behavior via command-line switches
    command_line->AppendSwitchWithValue("autoplay-policy",
                                        "no-user-gesture-required");
  }

  void OnContextInitialized(CefRefPtr<client::ClientAppBrowser> app) override {
    // Called after browser context is initialized
    // Configure global preferences for test requirements
    CefRefPtr<CefValue> value = CefValue::Create();
    value->SetBool(false);
    CefString error;
    bool result = CefRequestContext::GetGlobalContext()->SetPreference(
        "profile.mixed_forms_warnings", value, error);
    CHECK(result) << error.ToString();
  }

  // Other browser-side hooks available:
  // - OnRegisterCustomPreferences() - Register custom preferences
  // - OnAlreadyRunningAppRelaunch() - Handle app relaunch
  // - GetDefaultClient() - Provide default CefClient

 private:
  IMPLEMENT_REFCOUNTING(MyBrowserDelegate);
};

}  // namespace

// Registration function - called from client_app_delegates.cc
void CreateMyBrowserDelegate(client::ClientAppBrowser::DelegateSet& delegates) {
  delegates.insert(new MyBrowserDelegate);
}
```

**ClientAppRenderer::Delegate** - Renderer process hooks:

```cpp
#include "tests/shared/renderer/client_app_renderer.h"

namespace {

class MyRendererDelegate : public client::ClientAppRenderer::Delegate {
 public:
  MyRendererDelegate() = default;

  MyRendererDelegate(const MyRendererDelegate&) = delete;
  MyRendererDelegate& operator=(const MyRendererDelegate&) = delete;

  bool OnProcessMessageReceived(
      CefRefPtr<client::ClientAppRenderer> app,
      CefRefPtr<CefBrowser> browser,
      CefRefPtr<CefFrame> frame,
      CefProcessId source_process,
      CefRefPtr<CefProcessMessage> message) override {
    if (message->GetName() == "MyTest.UniqueMessage") {
      // Handle process message from browser
      // Perform renderer-side operations
      return true;  // Message handled
    }
    return false;  // Message not handled
  }

  // Other renderer-side hooks available:
  // - OnWebKitInitialized() - V8/Blink initialization
  // - OnBrowserCreated() - Browser created in renderer
  // - OnBrowserDestroyed() - Browser destroyed in renderer
  // - OnContextCreated() - JavaScript context created
  // - OnContextReleased() - JavaScript context released
  // - OnUncaughtException() - Uncaught JavaScript exception
  // - OnFocusedNodeChanged() - DOM node focus changed
  // - GetLoadHandler() - Provide load handler

 private:
  IMPLEMENT_REFCOUNTING(MyRendererDelegate);
};

}  // namespace

// Registration function - called from client_app_delegates.cc
void CreateMyRendererDelegate(client::ClientAppRenderer::DelegateSet& delegates) {
  delegates.insert(new MyRendererDelegate);
}
```

**CRITICAL: Delegate Scope and Isolation**

Delegates are registered **globally** and called for **ALL tests** in the process, not just your specific test. This is fundamentally different from TestHandler which is scoped to a single test.

**To avoid interfering with other tests:**

1. **Use unique identifiers** - Message names, URLs, scheme names must be unique
    - ✅ Good: `"MyFeatureTest.ProcessMessage"`, `"myfeature://unique"`
    - ❌ Bad: `"test_message"`, `"custom://test"` (too generic, will conflict)

2. **Configure test mode via extra_info** - Best practice for renderer delegates:
    ```cpp
    // In TestHandler (browser process):
    void RunTest() override {
      CefRefPtr<CefDictionaryValue> extra_info = CefDictionaryValue::Create();
      extra_info->SetInt("my-test-mode", MY_SPECIFIC_TEST_MODE);

      AddResource(kTestUrl, kTestHtml, "text/html");
      CreateBrowser(kTestUrl, nullptr, extra_info);
      SetTestTimeout();
    }

    // In Renderer Delegate:
    void OnBrowserCreated(CefRefPtr<ClientAppRenderer> app,
                          CefRefPtr<CefBrowser> browser,
                          CefRefPtr<CefDictionaryValue> extra_info) override {
      // Check if this browser instance is for YOUR test
      if (extra_info && extra_info->HasKey("my-test-mode")) {
        test_mode_ = extra_info->GetInt("my-test-mode");
        // Configure test-specific behavior
      }
    }

    // In other delegate methods:
    void OnContextCreated(...) override {
      if (test_mode_ != MY_SPECIFIC_TEST_MODE) {
        return;  // Not our test
      }
      // Execute test-specific logic
    }
    ```

3. **Check conditions before executing** - Only execute logic when conditions match your test
    - Always check for unique identifiers (message names, URLs, test modes) before executing
    - Return `false` for unhandled events to let other delegates process them
    - Return `true` only when you've handled the event
    - See `OnProcessMessageReceived` example above for the pattern

**Registration in client_app_delegates.cc:**

After creating your delegate, register it in `cef/tests/ceftests/client_app_delegates.cc`:

```cpp
// Add extern declaration
void CreateBrowserDelegates(ClientAppBrowser::DelegateSet& delegates) {
  // ... existing delegates ...

  // Add your browser delegate
  extern void CreateMyBrowserDelegate(ClientAppBrowser::DelegateSet& delegates);
  CreateMyBrowserDelegate(delegates);
}

void CreateRenderDelegates(ClientAppRenderer::DelegateSet& delegates) {
  // ... existing delegates ...

  // Add your renderer delegate
  extern void CreateMyRendererDelegate(ClientAppRenderer::DelegateSet& delegates);
  CreateMyRendererDelegate(delegates);
}
```

**Combining with TestHandler:**

Often you'll use both - delegate for process-level hooks and TestHandler for test execution:

- **TestHandler** (browser process) creates the browser, sends messages, verifies results
- **Delegate** (renderer process) receives messages, executes renderer-side logic
- Communication via `CefProcessMessage` with unique message names
- Delegate checks message name before executing to avoid interfering with other tests

**Key differences from TestHandler:**

| Feature | ClientApp Delegate | TestHandler |
|---------|-------------------|-------------|
| Scope | Process-level (global) | Browser instance |
| Lifetime | Entire process | Single test |
| Called for | **ALL tests in process** | **Only the specific test** |
| Use case | Renderer hooks, global config | Browser testing, navigation |
| Registration | client_app_delegates.cc | No registration needed |
| Isolation | Must check unique identifiers | Automatically isolated |

**See:**

- `cef/tests/ceftests/cors_unittest.cc` - ClientAppBrowser::Delegate example (CorsBrowserTest)
- `cef/tests/ceftests/thread_unittest.cc` - ClientAppRenderer::Delegate example (RenderThreadRendererTest)
- `cef/tests/ceftests/v8_unittest.cc` - OnBrowserCreated with extra_info pattern for test mode configuration
- `cef/tests/ceftests/client_app_delegates.cc` - Registration mechanism

### Test Type 7: Views Framework Tests

For Views-specific UI testing (windows, controls, layout), use async UI tests with `CefWaitableEvent`. These do NOT use the `TestHandler::ExecuteTest()` pattern.

**Two approaches:**

1. **Simple views tests (buttons, textfields, panels)**: Use `TestWindowDelegate::RunTest()` helper
2. **BrowserView tests**: Create custom handler and delegate (see pattern below)

#### Option 1: Using TestWindowDelegate Helper

For testing views/controls without browsers (buttons, textfields, layout, etc.):

```cpp
#include "tests/ceftests/views/test_window_delegate.h"

#define MY_VIEW_TEST_ASYNC(name) \
  UI_THREAD_TEST_ASYNC(MyViewTest, name)

namespace {

void RunMyTest(CefRefPtr<CefWindow> window) {
  // Create and test your views
  CefRefPtr<CefButton> button = CefButton::CreateButton(...);
  window->AddChildView(button);
  window->Layout();

  // Test assertions
  EXPECT_TRUE(button->IsVisible());

  // Window will be closed automatically
}

void MyTestImpl(CefRefPtr<CefWaitableEvent> event) {
  auto config = std::make_unique<TestWindowDelegate::Config>();
  config->on_window_created = base::BindOnce(RunMyTest);
  config->close_window = true;  // Auto-close after callback (default)
  config->frameless = false;     // Framed window (default)
  TestWindowDelegate::RunTest(event, std::move(config));
}

}  // namespace

// Test registration (outside namespace)
MY_VIEW_TEST_ASYNC(MyTest)
```

**Config options:**

- `on_window_created` - Called when window is created (run test logic here)
- `on_window_destroyed` - Called when window is destroyed
- `on_accelerator` - Handle keyboard shortcuts
- `on_key_event` - Handle key events
- `close_window` - Auto-close window after `on_window_created` (default: true)
- `frameless` - Create frameless window (default: false)
- `window_size` - Initial window size (default: 400x400)
- `initial_show_state` - Window show state (default: normal)

**When to use:** Button clicks, textfield input, panel layout, scroll views, etc.

**Reference:** `cef/tests/ceftests/views/button_unittest.cc`, `textfield_unittest.cc`

#### Option 2: Custom Handler and Delegate Pattern

For BrowserView tests or complex window management:

**Pattern:**

```cpp
#define MY_VIEW_TEST_ASYNC(name) \
  UI_THREAD_TEST_ASYNC(MyViewTest, name)

namespace {

class MyViewTestHandler : public TestHandler {
 public:
  using OnCompleteCallback = base::OnceCallback<void(bool success)>;

  explicit MyViewTestHandler(int expected_browser_count)
      : expected_browser_count_(expected_browser_count) {
    SetDestroyTestExpected(false);  // Not using ExecuteTest()
    SetUseViews(true);
    SetTestTimeout();
  }

  MyViewTestHandler(const MyViewTestHandler&) = delete;
  MyViewTestHandler& operator=(const MyViewTestHandler&) = delete;

  void RunTest() override {}  // Not used

  void SetOnComplete(OnCompleteCallback callback) {
    on_complete_ = std::move(callback);
  }

  void ClearCallbacks() { on_complete_.Reset(); }

  void DestroyTest() override {
    ADD_FAILURE() << "Test timeout";
    if (on_complete_) {
      std::move(on_complete_).Run(false);  // false = timeout
    }
  }

 private:
  // Call from internal test logic when complete (e.g., OnLoadEnd, OnButtonClick)
  void OnTestComplete() {
    if (on_complete_) {
      CefPostTask(TID_UI, base::BindOnce(std::move(on_complete_), true));
    }
  }

  const int expected_browser_count_;  // Use to track loaded browsers in OnLoadEnd
  OnCompleteCallback on_complete_;

  IMPLEMENT_REFCOUNTING(MyViewTestHandler);
};

// For BrowserView tests, also implement CefBrowserViewDelegate
class MyViewDelegate : public CefWindowDelegate,
                        public CefBrowserViewDelegate {
 public:
  MyViewDelegate(CefRefPtr<MyViewTestHandler> handler,
                 CefRefPtr<CefWaitableEvent> event,
                 int expected_browser_count)
      : handler_(handler),
        event_(event),
        expected_browser_count_(expected_browser_count) {}

  MyViewDelegate(const MyViewDelegate&) = delete;
  MyViewDelegate& operator=(const MyViewDelegate&) = delete;

  void OnWindowCreated(CefRefPtr<CefWindow> window) override {
    window_ = window;
    // Create and configure views/BrowserViews
    window_->Show();
  }

  bool CanClose(CefRefPtr<CefWindow> window) override {
    // IMPORTANT: Implement TryCloseBrowser() logic here
    // See browser_view_unittest.cc for the correct pattern
    // Must call TryCloseBrowser() and handle cleanup properly
    return true;
  }

  void OnWindowDestroyed(CefRefPtr<CefWindow> window) override {
    // Verify expected browsers were created and destroyed
    EXPECT_EQ(expected_browser_count_, browser_created_count_);
    EXPECT_EQ(expected_browser_count_, browser_destroyed_count_);

    handler_->ClearCallbacks();  // Break reference cycle
    event_->Signal();  // Release test
  }

  // CefBrowserViewDelegate methods (for BrowserView tests):
  void OnBrowserCreated(CefRefPtr<CefBrowserView> browser_view,
                        CefRefPtr<CefBrowser> browser) override {
    browser_created_count_++;
    // Notify TestHandler for accounting
    handler_->OnWindowCreated(browser->GetIdentifier());
  }

  void OnBrowserDestroyed(CefRefPtr<CefBrowserView> browser_view,
                          CefRefPtr<CefBrowser> browser) override {
    browser_destroyed_count_++;
    // Notify TestHandler for accounting
    handler_->OnWindowDestroyed(browser->GetIdentifier());

    // After each browser is destroyed, try closing the window.
    // CanClose will check if all remaining browsers are ready.
    if (window_) {
      window_->Close();
    }
  }

  CefRefPtr<CefWindow> window() const { return window_; }

 private:
  CefRefPtr<MyViewTestHandler> handler_;
  CefRefPtr<CefWaitableEvent> event_;
  CefRefPtr<CefWindow> window_;
  const int expected_browser_count_;
  int browser_created_count_ = 0;
  int browser_destroyed_count_ = 0;

  IMPLEMENT_REFCOUNTING(MyViewDelegate);
};

void MyTestImpl(CefRefPtr<CefWaitableEvent> event) {
  const int expected_browser_count = 1;
  CefRefPtr<MyViewTestHandler> handler =
      new MyViewTestHandler(expected_browser_count);
  CefRefPtr<MyViewDelegate> delegate =
      new MyViewDelegate(handler, event, expected_browser_count);

  // Completion callback: called when browsers finish loading (or on timeout)
  // Can perform additional test operations here before closing window
  // Example: Move BrowserView between child/overlay, simulate user input, etc.
  handler->SetOnComplete(base::BindOnce(
      [](CefRefPtr<MyViewDelegate> delegate, bool success) {
        // For simple tests, just close immediately
        delegate->window()->Close();

        // For complex tests, continue testing after load:
        // if (success) {
        //   PerformAdditionalTestSteps(delegate);
        // } else {
        //   delegate->window()->Close();
        // }
      },
      delegate));

  CefWindow::CreateTopLevelWindow(delegate);
}

}  // namespace

// Test registration (outside namespace)
MY_VIEW_TEST_ASYNC(MyTest)
```

**When to use:** BrowserView lifecycle, multiple browsers, overlay controllers, or tests requiring TestHandler functionality (OnLoadEnd, message routing, etc.)

**Key differences from TestHandler tests:**

- Call `SetDestroyTestExpected(false)` - not using `ExecuteTest()` pattern
- Use `OnceCallback<void(bool success)>` for completion (true = success, false = timeout)
- Signal `event` in `OnWindowDestroyed()` to release test
- Call `handler_->ClearCallbacks()` in `OnWindowDestroyed()` to break reference cycle (delegate → handler → callback → delegate)

**IMPORTANT:** Browser/window close lifecycle is complex. **Read `cef/tests/ceftests/views/browser_view_unittest.cc` thoroughly** to understand:

- `CanClose()` implementation with `TryCloseBrowser()`
- `OnBeforeClose()` handling and cleanup sequencing
- Proper ordering of browser close vs window close
- Reference counting and circular reference management
- The pattern for when browsers should be destroyed before closing the window

### Add Test File to Build

```python
# In cef/tests/ceftests/BUILD.gn

source_set("ceftests_source_set") {
  sources = [
    # ... existing files ...
    "my_new_feature_unittest.cc",  # Add your file in alphabetical order
  ]
}
```

**File naming:** `{feature}_unittest.cc`

**Important:** Add new files in alphabetical order within the `sources` list.

**File structure:**

```cpp
// Copyright header

#include "tests/ceftests/test_handler.h"
#include "tests/gtest/include/gtest/gtest.h"

namespace {

// Test handlers and helpers

}  // namespace

// Tests (outside namespace)
TEST(MyFeatureTest, FirstTest) { }
TEST(MyFeatureTest, SecondTest) { }
```

## Common Patterns

### Async Operations

Use async operations when testing multi-step processes, coordinating multiple callbacks, or simulating timing scenarios:

```cpp
class AsyncTestHandler : public TestHandler {
 public:
  AsyncTestHandler(const AsyncTestHandler&) = delete;
  AsyncTestHandler& operator=(const AsyncTestHandler&) = delete;

  void RunTest() override {
    CefPostTask(TID_UI, base::BindOnce(&AsyncTestHandler::Step1, this));
    SetTestTimeout();
  }

  void Step1() {
    operation_started_ = true;
    CefPostDelayedTask(TID_UI,
                       base::BindOnce(&AsyncTestHandler::Step2, this),
                       100);
  }

  void Step2() {
    EXPECT_TRUE(operation_started_);
    DestroyTest();
  }

 private:
  bool operation_started_ = false;

  IMPLEMENT_REFCOUNTING(AsyncTestHandler);
};
```

**When to use:**

- Testing sequences where operations must complete in order
- Allowing time for async CEF operations to complete (e.g., JavaScript execution)
- Coordinating multiple independent async callbacks

### Reliable Test Patterns

**Prefer event-driven testing over timing-dependent testing:**

```cpp
// BAD: Timing-dependent - may fail if operation takes longer than expected
void RunTest() override {
  CreateBrowser("https://tests.test/page.html");
  // Hope the page loads in 500ms...
  CefPostDelayedTask(TID_UI,
                     base::BindOnce(&MyTestHandler::CheckResult, this),
                     500);
  SetTestTimeout();
}

// GOOD: Event-driven - waits for actual callback
void OnLoadEnd(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int httpStatusCode) override {
  // React to the actual event
  CheckResult();
  DestroyTest();
}
```

**Use JavaScript queries to signal completion:**

```cpp
// JavaScript sends completion signal
const char kTestHtml[] =
  "<html><script>"
  "setTimeout(function() {"
  "  // Async operation completes"
  "  window.testQuery({request: 'operation_complete'});"
  "}, 0);"
  "</script></html>";

bool OnQuery(CefRefPtr<CefBrowser> browser,
             CefRefPtr<CefFrame> frame,
             int64_t query_id,
             const CefString& request,
             bool persistent,
             CefRefPtr<Callback> callback) override {
  if (request == "operation_complete") {
    got_completion_.yes();
    callback->Success("");
    DestroyTest();
    return true;
  }
  return false;
}
```

**Coordinate multiple async events with Maybe* pattern:**

```cpp
void MaybeCompleteTest() {
  // Wait for ALL expected events before completing
  if (got_load_end_ && got_query_ && got_resource_load_) {
    DestroyTest();
  }
}

void OnLoadEnd(...) override {
  got_load_end_.yes();
  MaybeCompleteTest();
}

bool OnQuery(...) override {
  got_query_.yes();
  MaybeCompleteTest();
  return true;
}
```

**Only use delays for UI timing (not logic coordination):**

```cpp
// Acceptable: UI needs time to render before click
window->Show();
CefPostDelayedTask(TID_UI, base::BindOnce(ClickButton, window, kButtonID),
                   100);  // Brief delay for UI rendering

// NOT acceptable: Waiting arbitrary time for logic
CefPostDelayedTask(TID_UI,
                   base::BindOnce(&MyTestHandler::CheckIfDone, this),
                   2000);  // Hope everything finishes in 2s?
```

**Key principles:**

- React to callbacks, don't guess timing
- Use `TrackCallback` to verify all expected events occurred
- Use JS queries (`window.testQuery`) to get signals from renderer
- Coordinate multiple events with Maybe* pattern, not delays
- Only use `CefPostDelayedTask` for UI event simulation, not test logic

### Callback Tracking

```cpp
#include "tests/ceftests/track_callback.h"

class CallbackTestHandler : public TestHandler {
 public:
  CallbackTestHandler(const CallbackTestHandler&) = delete;
  CallbackTestHandler& operator=(const CallbackTestHandler&) = delete;

  void OnLoadStart(...) override {
    got_load_start_.yes();
  }

  void OnLoadEnd(...) override {
    got_load_end_.yes();
    DestroyTest();
  }

  void DestroyTest() override {
    // Verify all expected callbacks were called
    EXPECT_TRUE(got_load_start_);
    EXPECT_TRUE(got_load_end_);

    TestHandler::DestroyTest();
  }

 private:
  TrackCallback got_load_start_;
  TrackCallback got_load_end_;

  IMPLEMENT_REFCOUNTING(CallbackTestHandler);
};
```

### Temporary Directories

Use `CefScopedTempDir` for tests that need to write files or use disk cache:

```cpp
#include "include/wrapper/cef_scoped_temp_dir.h"

class TempDirTestHandler : public TestHandler {
 public:
  TempDirTestHandler(const TempDirTestHandler&) = delete;
  TempDirTestHandler& operator=(const TempDirTestHandler&) = delete;

  void RunTest() override {
    // Create temporary directory
    EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());

    // Get the path
    const std::string temp_path = temp_dir_.GetPath();

    // Use temp_path for downloads, cache, etc.
    CreateBrowser("https://tests.test/page.html");
    SetTestTimeout();
  }

  void OnLoadEnd(...) override {
    // File operations should be done on FILE thread
    CefPostTask(TID_FILE_USER_VISIBLE,
                base::BindOnce(&TempDirTestHandler::WriteFileOnFileThread, this));
  }

  void WriteFileOnFileThread() {
    EXPECT_TRUE(CefCurrentlyOn(TID_FILE_USER_VISIBLE));

    // Write a file to the temp directory
    const std::string file_path = temp_dir_.GetPath() + "/test.txt";
    // ... write file ...

    // Verify file and cleanup on FILE thread
    VerifyResultsOnFileThread();
  }

  void VerifyResultsOnFileThread() {
    EXPECT_TRUE(CefCurrentlyOn(TID_FILE_USER_VISIBLE));

    // Verify file was written
    // ... read and check file ...

    // Delete temp directory on FILE thread
    EXPECT_TRUE(temp_dir_.Delete());
    EXPECT_TRUE(temp_dir_.IsEmpty());

    // Go back to UI thread to destroy test
    CefPostTask(TID_UI,
                base::BindOnce(&TempDirTestHandler::DestroyTest, this));
  }

  void DestroyTest() override {
    if (!temp_dir_.IsEmpty()) {
      // DestroyTest() may be called for various reasons (timeout, failure, etc.)
      // Ensure temp_dir_ cleanup happens on FILE thread before destroying.
      CefPostTask(TID_FILE_USER_VISIBLE,
                  base::BindOnce(&TempDirTestHandler::VerifyResultsOnFileThread,
                                 this));
      return;
    }

    TestHandler::DestroyTest();
  }

 private:
  CefScopedTempDir temp_dir_;

  IMPLEMENT_REFCOUNTING(TempDirTestHandler);
};
```

**Use cases:**

- Download tests that need to write files
- Request context tests with disk cache (see Test Type 5)
- File I/O operations

**Important:**

- File operations (read/write) must be done on FILE thread (`TID_FILE_USER_VISIBLE`), not UI thread
- Call `temp_dir_.Delete()` on FILE thread in a `VerifyResultsOnFileThread()` method
- Verify deletion with `EXPECT_TRUE(temp_dir_.IsEmpty())`
- Post back to UI thread to call `DestroyTest()`
- In `DestroyTest()`, check `!temp_dir_.IsEmpty()` to ensure cleanup happens even if test fails or times out

### Sending Events

For tests that simulate user interaction, CEF provides event sending APIs.

**Browser-based tests** (use `SendMouseClickEvent` helper):

```cpp
#include "tests/ceftests/test_util.h"

// Helper function handles timing and event details
CefMouseEvent mouse_event;
mouse_event.x = 100;
mouse_event.y = 100;
SendMouseClickEvent(browser, mouse_event, MBT_LEFT);
```

**Views-based tests** (use window event APIs):

```cpp
// Mouse events - determine click point from view bounds
const CefRect& bounds = button->GetBoundsInScreen();
const CefPoint click_point(bounds.x + bounds.width / 2,
                          bounds.y + bounds.height / 2);

// Wait before sending to avoid rate limiting
CefPostDelayedTask(TID_UI, base::BindOnce([](CefRefPtr<CefWindow> window,
                                             CefPoint point) {
  window->SendMouseMove(point.x, point.y);
  window->SendMouseEvents(MBT_LEFT, true, true);  // press and release
}, window, click_point), 100);  // 100ms delay

// Keyboard events
window->SendKeyPress(VKEY_RETURN, EVENTFLAG_NONE);
window->SendKeyPress('A', EVENTFLAG_CONTROL_DOWN);  // Ctrl+A
```

**Use cases:**

- Testing button click handlers
- Simulating form input and submission
- Testing keyboard shortcuts
- Triggering downloads or file dialogs
- Testing drag and drop

**Important:**

- Add delays (100-200ms) before sending events to avoid rate limiting
- For Views tests, window must be visible (`window->Show()`)
- Calculate screen coordinates from `GetBoundsInScreen()` for accuracy
- Mouse events on Views require both `SendMouseMove()` and `SendMouseEvents()`
- **Focus and activation events can be flaky** - avoid tests that depend on `OnFocus()` or `OnWindowActivationChanged()` callbacks, especially on CI/CD systems or Mac where these events may not fire reliably

## Debugging Test Failures

### Common Issues and Fixes

**Issue: Test times out**

**Cause:** `DestroyTest()` never called, `SetTestTimeout()` not called, or waiting for event that never occurs

**Fix:**

```cpp
void RunTest() override {
  CreateBrowser(kTestUrl);

  // REQUIRED: Set timeout at end of RunTest()
  SetTestTimeout();  // Default 5000ms
}

void OnLoadEnd(...) override {
  // Ensure all code paths call DestroyTest()

  // Test logic

  DestroyTest();  // REQUIRED
}
```

**For longer-running tests:**

```cpp
void RunTest() override {
  CreateBrowser(kTestUrl);

  // Increase timeout if test needs more time
  SetTestTimeout(10000);  // 10 seconds
}
```

**Disable timeout during debugging:**

```bash
ceftests --disable-test-timeout --gtest_filter="MyTest.*"
```

**Issue: Flaky test (inconsistent results)**

**Cause:** Race condition, timing-dependent logic, shared state

**Fix:**

```cpp
// Bad: Timing-dependent
CefPostDelayedTask(TID_UI, callback, 100);  // May be too short

// Good: Event-driven
void OnLoadEnd(...) override {
  VerifyAndComplete();  // React to actual event
}

// Bad: Shared global state
static int counter = 0;  // Shared across tests

// Good: Instance state
class MyTestHandler : public TestHandler {
  int instance_counter_ = 0;  // Unique per test
};
```

**Verify the fix:**

After fixing a flaky test, run it multiple times to verify stability. Choose repeat count based on test duration:

```bash
# Fast tests (<100ms): repeat 50 times
ceftests --gtest_filter="StringTest.UTF8" --gtest_repeat=50

# Medium tests (~1s): repeat 10 times
ceftests --gtest_filter="NavigationTest.LoadURL" --gtest_repeat=10

# Slow tests (>5s): repeat 3 times
ceftests --gtest_filter="DownloadTest.LargeFile" --gtest_repeat=3

# If any run fails, the test is still flaky
```

**Issue: Memory leak warnings**

**Cause:** `DestroyTest()` not called, circular references

**Fix:**

```cpp
// Ensure proper ref counting
IMPLEMENT_REFCOUNTING(MyTestHandler);

// Always call DestroyTest()
void OnLoadEnd(...) override {
  DestroyTest();  // REQUIRED - releases references
}
```

**Issue: Assertion failure**

**Example output:**

```
tests/ceftests/navigation_unittest.cc:45: Failure
Expected: 200
Actual: 404
```

**Fix:**

1. **Read the failing test** at the line shown in the error (e.g., line 45). Understand what the test expects and why it might be failing.

2. **Check the context** - examine what the test is doing:
    - What URL is being loaded?
    - What resource was added with `AddResource()`?
    - Is the expected value correct?
    - Did the test setup change?

3. **Add logging** if the cause isn't obvious:
    ```cpp
    void OnLoadEnd(...) override {
      LOG(INFO) << "Status: " << httpStatusCode;
      LOG(INFO) << "URL: " << frame->GetURL().ToString();
      EXPECT_EQ(200, httpStatusCode);
      DestroyTest();
    }
    ```

### Debugging Workflow

1. **Run specific test:**
    ```bash
    ceftests --gtest_filter="NavigationTest.LoadURL"
    ```

2. **Add logging:**
    ```cpp
    LOG(INFO) << "Debug info: " << value;
    ```

3. **Verify callbacks:**
    ```cpp
    TrackCallback got_callback_;
    // Later:
    if (!got_callback_) {
      LOG(ERROR) << "Callback never called!";
    }
    ```

4. **Rebuild and retest:**
    ```bash
    autoninja -C out/Debug_GN_x64 ceftests
    out/Debug_GN_x64/ceftests --gtest_filter="NavigationTest.LoadURL"
    ```

## Platform-Specific Notes

**See Step 2 for platform-specific executable paths.**

### macOS

- **Views support has limitations (Issue #3188)** - Some Views tests may be flaky
- **MUST use .app bundle path** - Direct binary path will not work

### Linux

- **Headless execution** - Use `xvfb-run` for CI/CD environments
- **Sandbox** - May need `--no-sandbox` depending on system configuration

## Test Execution Best Practices

**Test organization:**

- Group related tests in same file/suite
- Use descriptive names: `NavigationTest.LoadURLRedirect` not `Test1`
- Keep tests independent - don't rely on execution order
- **Never add global state** - use instance variables in test handlers instead
- Document complex test flows with comments

**Modifying existing tests:**

When modifying an existing test file:

- **Follow existing code style** - match the formatting, naming conventions, and patterns already used in that file
- **Follow existing test design** - use the same test handler patterns, helper functions, and assertion styles
- **Don't refactor unnecessarily** - make minimal changes needed for your modification
- Read other tests in the same file to understand the established patterns

**Running on CI/CD:**

```bash
# Exclude known flaky tests - see ChromiumUpdate.md for latest filters
ceftests --gtest_filter="-AudioOutputTest.*:MediaAccessTest.*"
```

## Communication Guidelines

**When running tests:**

```
✓ Built ceftests (4m 12s)

Running: NavigationTest.*

Results:
✓ PASSED: NavigationTest.LoadURL (234 ms)
✓ PASSED: NavigationTest.Redirect (156 ms)
✗ FAILED: NavigationTest.BackForward (timeout 60s)

Summary: 2/3 passed

Investigating failure...
```

**When creating tests:**

```
✓ Created: cef/tests/ceftests/my_feature_unittest.cc

Tests:
- MyFeatureTest.BasicFunctionality
- MyFeatureTest.EdgeCase

Updated BUILD.gn

Building and testing...
```

**When debugging:**

```
Investigating NavigationTest.BackForward:

Issue: Test timeout (60s)
Cause: DestroyTest() not called after back navigation
Fix: Added DestroyTest() call in navigation complete handler

Verification: Ran 5 times, all passed (avg 234ms)
```

## Success Criteria

Tests succeed when:

1. ✓ All tests complete without timeout
2. ✓ All assertions pass
3. ✓ No crashes
4. ✓ Exit code 0
5. ✓ No memory leaks

## References

- [ceftests README](../../tests/ceftests/README.md) - Test suite overview
- [CEF General Usage](https://bitbucket.org/chromiumembedded/cef/wiki/GeneralUsage.md) - API documentation
- [ChromiumUpdate.md](https://bitbucket.org/chromiumembedded/cef/wiki/ChromiumUpdate.md) - Test filters
- [Google Test Docs](https://google.github.io/googletest/) - gtest reference

---

**You are ready to create and run CEF tests. Work systematically and verify thoroughly.**
