// Copyright (c) 2017 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFTESTS_EXTENSIONS_EXTENSION_TEST_HANDLER_H_
#define CEF_TESTS_CEFTESTS_EXTENSIONS_EXTENSION_TEST_HANDLER_H_
#pragma once

#include <vector>

#include "include/cef_extension_handler.h"
#include "include/cef_values.h"
#include "include/wrapper/cef_scoped_temp_dir.h"
#include "tests/ceftests/routing_test_handler.h"
#include "tests/gtest/include/gtest/gtest.h"

class ExtensionTestHandler : public RoutingTestHandler,
                             public CefExtensionHandler {
 public:
  // All tests must be able to run with all RequestContext combinations. See the
  // EXTENSION_TEST_GROUP_* macros below.
  enum RequestContextType {
    // If set create a custom context. Otherwise, use the global context.
    RC_TYPE_FLAG_CUSTOM = 1 << 0,

    // If set store data on disk. Otherwise, store data in memory.
    // Requires RC_TYPE_FLAG_CUSTOM.
    RC_TYPE_FLAG_ON_DISK = 1 << 1,

    // If set use a handler. Otherwise, don't.
    RC_TYPE_FLAG_WITH_HANDLER = 1 << 2,

    // If set load extensions with a different context that shares the same
    // storage but specifies a different handler.
    // Excludes RC_TYPE_FLAG_LOAD_WITHOUT_HANDLER.
    RC_TYPE_FLAG_LOAD_WITH_HANDLER = 1 << 3,

    // If set load extensions with a different context that shares the same
    // storage but doesn't specify a handler.
    // Requires RC_TYPE_FLAG_WITH_HANDLER.
    // Excludes RC_TYPE_FLAG_LOAD_WITH_HANDLER.
    RC_TYPE_FLAG_LOAD_WITHOUT_HANDLER = 1 << 4,
  };

  explicit ExtensionTestHandler(RequestContextType request_context_type);
  virtual ~ExtensionTestHandler();

  // TestHandler methods:
  void RunTest() override;
  void DestroyTest() override;
  void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;

  // CefExtensionHandler methods:
  void OnExtensionLoadFailed(cef_errorcode_t result) override;

  // CefMessageRouterBrowserSide::Handler methods:
  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64 query_id,
               const CefString& request,
               bool persistent,
               CefRefPtr<Callback> callback) override;

  CefRefPtr<CefRequestContext> request_context() const {
    return request_context_;
  }
  CefRefPtr<CefRequestContext> loader_request_context() const {
    return loader_request_context_;
  }

  bool request_context_is_custom() const {
    return !!(request_context_type_ & RC_TYPE_FLAG_CUSTOM);
  }
  bool request_context_on_disk() const {
    return !!(request_context_type_ & RC_TYPE_FLAG_ON_DISK);
  }
  bool request_context_with_handler() const {
    return !!(request_context_type_ & RC_TYPE_FLAG_WITH_HANDLER);
  }
  bool request_context_load_with_handler() const {
    return !!(request_context_type_ & RC_TYPE_FLAG_LOAD_WITH_HANDLER);
  }
  bool request_context_load_without_handler() const {
    return !!(request_context_type_ & RC_TYPE_FLAG_LOAD_WITHOUT_HANDLER);
  }
  bool request_context_same_loader() const {
    return !(request_context_load_with_handler() ||
             request_context_load_without_handler());
  }

 protected:
  // Returns the default extension manifest.
  typedef std::vector<std::string> ApiPermissionsList;
  static CefRefPtr<CefDictionaryValue> CreateDefaultManifest(
      const ApiPermissionsList& api_permissions);

  // Returns the JS code that, when executed, will deliver |message| to the
  // OnMessage callback.
  static std::string GetMessageJS(const std::string& message);

  // Run checks on the state of |extension| in |context|. If |has_access| is
  // true then |context| is expected to have access to |extension|. If
  // |is_loader| is true then |context| is expected to have loaded |extension|.
  static void VerifyExtensionInContext(CefRefPtr<CefExtension> extension,
                                       CefRefPtr<CefRequestContext> context,
                                       bool has_access,
                                       bool is_loader);

  // Helper for loading/unloading an extension.
  void LoadExtension(const std::string& extension_path,
                     CefRefPtr<CefDictionaryValue> manifest);
  void UnloadExtension(CefRefPtr<CefExtension> extension);

  // Release request contexts. This is normally called from DestroyTest().
  void ReleaseRequestContexts();

  void set_create_main_browser(bool val) { create_main_browser_ = val; }
  bool create_main_browser() const { return create_main_browser_; }

  // Called when its time to add resources for the main browser if
  // |create_main_browser_| is true.
  virtual void OnAddMainBrowserResources() {}
  // Called when its time to create the main browser if
  // |create_main_browser_| is true.
  virtual void OnCreateMainBrowser() {}

  // Called when its time to load extensions.
  virtual void OnLoadExtensions() = 0;

  // Called when |browser| receives |message|. Return true if the message is
  // handled. The JS code that sends messages is created by GetMessageJS().
  virtual bool OnMessage(CefRefPtr<CefBrowser> browser,
                         const std::string& message) = 0;

  // Called to perform verification on test destruction.
  virtual void OnDestroyTest() = 0;

 private:
  const RequestContextType request_context_type_;
  CefScopedTempDir request_context_temp_dir_;

  // Context used when creating browsers.
  CefRefPtr<CefRequestContext> request_context_;

  // Context used when loading extensions.
  CefRefPtr<CefRequestContext> loader_request_context_;

  // If true expect creation of a main browser. Default is true.
  bool create_main_browser_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionTestHandler);
};

// Helper for implementing an extension test.
#define EXTENSION_TEST(name, test_class, rc_type)                        \
  TEST(ExtensionTest, name) {                                            \
    CefRefPtr<test_class> handler = new test_class(                      \
        static_cast<ExtensionTestHandler::RequestContextType>(rc_type)); \
    handler->ExecuteTest();                                              \
    ReleaseAndWaitForDestructor(handler);                                \
  }

// Helper for implementing extension tests that include all RequestContext
// combinations. When two or more extension tests significantly overlap in
// tested functionality the first test should use the ALL macro and the others
// should use the MINIMAL macro.
#define EXTENSION_TEST_GROUP_ALL(name, test_class)                             \
  EXTENSION_TEST(name##RCGlobal, test_class, 0)                                \
  EXTENSION_TEST(name##RCGlobalLoadWithHandler, test_class,                    \
                 ExtensionTestHandler::RC_TYPE_FLAG_LOAD_WITH_HANDLER)         \
  EXTENSION_TEST(name##RCGlobalWithHandler, test_class,                        \
                 ExtensionTestHandler::RC_TYPE_FLAG_WITH_HANDLER)              \
  EXTENSION_TEST(name##RCGlobalWithHandlerLoadWithHandler, test_class,         \
                 ExtensionTestHandler::RC_TYPE_FLAG_WITH_HANDLER |             \
                     ExtensionTestHandler::RC_TYPE_FLAG_LOAD_WITH_HANDLER)     \
  EXTENSION_TEST(name##RCGlobalWithHandlerLoadWithoutHandler, test_class,      \
                 ExtensionTestHandler::RC_TYPE_FLAG_WITH_HANDLER |             \
                     ExtensionTestHandler::RC_TYPE_FLAG_LOAD_WITHOUT_HANDLER)  \
  EXTENSION_TEST(name##RCCustomInMemory, test_class,                           \
                 ExtensionTestHandler::RC_TYPE_FLAG_CUSTOM)                    \
  EXTENSION_TEST(name##RCCustomInMemoryLoadWithHandler, test_class,            \
                 ExtensionTestHandler::RC_TYPE_FLAG_CUSTOM |                   \
                     ExtensionTestHandler::RC_TYPE_FLAG_LOAD_WITH_HANDLER)     \
  EXTENSION_TEST(name##RCCustomInMemoryWithHandler, test_class,                \
                 ExtensionTestHandler::RC_TYPE_FLAG_CUSTOM |                   \
                     ExtensionTestHandler::RC_TYPE_FLAG_WITH_HANDLER)          \
  EXTENSION_TEST(name##RCCustomInMemoryWithHandlerLoadWithHandler, test_class, \
                 ExtensionTestHandler::RC_TYPE_FLAG_CUSTOM |                   \
                     ExtensionTestHandler::RC_TYPE_FLAG_WITH_HANDLER |         \
                     ExtensionTestHandler::RC_TYPE_FLAG_LOAD_WITH_HANDLER)     \
  EXTENSION_TEST(name##RCCustomInMemoryWithHandlerLoadWithoutHandler,          \
                 test_class,                                                   \
                 ExtensionTestHandler::RC_TYPE_FLAG_CUSTOM |                   \
                     ExtensionTestHandler::RC_TYPE_FLAG_WITH_HANDLER |         \
                     ExtensionTestHandler::RC_TYPE_FLAG_LOAD_WITHOUT_HANDLER)  \
  EXTENSION_TEST(name##RCCustomOnDisk, test_class,                             \
                 ExtensionTestHandler::RC_TYPE_FLAG_CUSTOM |                   \
                     ExtensionTestHandler::RC_TYPE_FLAG_ON_DISK)               \
  EXTENSION_TEST(name##RCCustomOnDiskLoadWithHandler, test_class,              \
                 ExtensionTestHandler::RC_TYPE_FLAG_CUSTOM |                   \
                     ExtensionTestHandler::RC_TYPE_FLAG_ON_DISK |              \
                     ExtensionTestHandler::RC_TYPE_FLAG_LOAD_WITH_HANDLER)     \
  EXTENSION_TEST(name##RCCustomOnDiskWithHandler, test_class,                  \
                 ExtensionTestHandler::RC_TYPE_FLAG_CUSTOM |                   \
                     ExtensionTestHandler::RC_TYPE_FLAG_ON_DISK |              \
                     ExtensionTestHandler::RC_TYPE_FLAG_WITH_HANDLER)          \
  EXTENSION_TEST(name##RCCustomOnDiskWithHandlerLoadWithHandler, test_class,   \
                 ExtensionTestHandler::RC_TYPE_FLAG_CUSTOM |                   \
                     ExtensionTestHandler::RC_TYPE_FLAG_ON_DISK |              \
                     ExtensionTestHandler::RC_TYPE_FLAG_WITH_HANDLER |         \
                     ExtensionTestHandler::RC_TYPE_FLAG_LOAD_WITH_HANDLER)     \
  EXTENSION_TEST(name##RCCustomOnDiskWithHandlerLoadWithoutHandler,            \
                 test_class,                                                   \
                 ExtensionTestHandler::RC_TYPE_FLAG_CUSTOM |                   \
                     ExtensionTestHandler::RC_TYPE_FLAG_ON_DISK |              \
                     ExtensionTestHandler::RC_TYPE_FLAG_WITH_HANDLER |         \
                     ExtensionTestHandler::RC_TYPE_FLAG_LOAD_WITHOUT_HANDLER)

#define EXTENSION_TEST_GROUP_MINIMAL_GLOBAL(name, test_class) \
  EXTENSION_TEST(name##RCGlobal, test_class, 0)               \
  EXTENSION_TEST(name##RCGlobalWithHandler, test_class,       \
                 ExtensionTestHandler::RC_TYPE_FLAG_WITH_HANDLER)

#define EXTENSION_TEST_GROUP_MINIMAL_CUSTOM(name, test_class)   \
  EXTENSION_TEST(name##RCCustomInMemory, test_class,            \
                 ExtensionTestHandler::RC_TYPE_FLAG_CUSTOM)     \
  EXTENSION_TEST(name##RCCustomInMemoryWithHandler, test_class, \
                 ExtensionTestHandler::RC_TYPE_FLAG_CUSTOM |    \
                     ExtensionTestHandler::RC_TYPE_FLAG_WITH_HANDLER)

// Helper for implementing extension tests that include a minimal set of
// RequestContext combinations. This mostly just verifies that the test runs
// and doesn't leak state information in the context.
#define EXTENSION_TEST_GROUP_MINIMAL(name, test_class)  \
  EXTENSION_TEST_GROUP_MINIMAL_GLOBAL(name, test_class) \
  EXTENSION_TEST_GROUP_MINIMAL_CUSTOM(name, test_class)

#endif  // CEF_TESTS_CEFTESTS_EXTENSIONS_EXTENSION_TEST_HANDLER_H_
