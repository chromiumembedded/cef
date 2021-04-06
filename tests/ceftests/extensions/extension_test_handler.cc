// Copyright (c) 2017 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/ceftests/extensions/extension_test_handler.h"

#include "include/cef_request_context_handler.h"
#include "tests/ceftests/test_suite.h"
#include "tests/ceftests/test_util.h"

ExtensionTestHandler::ExtensionTestHandler(
    RequestContextType request_context_type)
    : request_context_type_(request_context_type), create_main_browser_(true) {
  // Verify supported flag combinations.
  if (request_context_on_disk()) {
    EXPECT_TRUE(request_context_is_custom());
  }
  if (request_context_load_with_handler()) {
    EXPECT_FALSE(request_context_load_without_handler());
  }
  if (request_context_load_without_handler()) {
    EXPECT_TRUE(request_context_with_handler());
    EXPECT_FALSE(request_context_load_with_handler());
  }
}

ExtensionTestHandler::~ExtensionTestHandler() {
  if (!request_context_temp_dir_.IsEmpty()) {
    // Temporary directory will be deleted on shutdown.
    request_context_temp_dir_.Take();
  }
}

void ExtensionTestHandler::RunTest() {
  if (create_main_browser_)
    OnAddMainBrowserResources();

  CefRefPtr<CefRequestContextHandler> rc_handler;
  if (request_context_with_handler()) {
    class Handler : public CefRequestContextHandler {
     public:
      explicit Handler(ExtensionTestHandler* test_handler)
          : test_handler_(test_handler) {}

      void OnRequestContextInitialized(
          CefRefPtr<CefRequestContext> request_context) override {
        if (test_handler_->create_main_browser()) {
          // Load extensions after the RequestContext has been initialized by
          // creation of the main browser.
          test_handler_->OnLoadExtensions();
        }
      }

     private:
      ExtensionTestHandler* test_handler_;

      IMPLEMENT_REFCOUNTING(Handler);
    };
    rc_handler = new Handler(this);
  }

  if (request_context_is_custom()) {
    CefRequestContextSettings settings;

    if (request_context_on_disk()) {
      // Create a new temporary directory.
      EXPECT_TRUE(request_context_temp_dir_.CreateUniqueTempDirUnderPath(
          CefTestSuite::GetInstance()->root_cache_path()));
      CefString(&settings.cache_path) = request_context_temp_dir_.GetPath();
    }

    request_context_ = CefRequestContext::CreateContext(settings, rc_handler);
  } else {
    request_context_ = CefRequestContext::CreateContext(
        CefRequestContext::GetGlobalContext(), rc_handler);
  }

  if (request_context_load_with_handler()) {
    class Handler : public CefRequestContextHandler {
     public:
      Handler() {}

     private:
      IMPLEMENT_REFCOUNTING(Handler);
    };
    loader_request_context_ =
        CefRequestContext::CreateContext(request_context_, new Handler());
  } else if (request_context_load_without_handler()) {
    loader_request_context_ =
        CefRequestContext::CreateContext(request_context_, nullptr);
  } else {
    loader_request_context_ = request_context_;
  }

  if (create_main_browser_) {
    OnCreateMainBrowser();
  } else {
    // Creation of the extension browser will trigger initialization of the
    // RequestContext, so just load the extensions now.
    OnLoadExtensions();
  }

  // Time out the test after a reasonable period of time.
  SetTestTimeout();
}

void ExtensionTestHandler::DestroyTest() {
  OnDestroyTest();
  ReleaseRequestContexts();
  RoutingTestHandler::DestroyTest();
}

void ExtensionTestHandler::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  RoutingTestHandler::OnAfterCreated(browser);

  if (create_main_browser() && !request_context_with_handler() &&
      GetBrowserId() == browser->GetIdentifier()) {
    // When the RequestContext doesn't have a handler we won't get a
    // notification for RequestContext initialization. Instead use main browser
    // creation to indicate that the RequestContext has been initialized.
    OnLoadExtensions();
  }
}

void ExtensionTestHandler::OnExtensionLoadFailed(cef_errorcode_t result) {
  EXPECT_TRUE(CefCurrentlyOn(TID_UI));
  EXPECT_TRUE(false);  // Not reached.
}

// CefMessageRouterBrowserSide::Handler methods:
bool ExtensionTestHandler::OnQuery(CefRefPtr<CefBrowser> browser,
                                   CefRefPtr<CefFrame> frame,
                                   int64 query_id,
                                   const CefString& request,
                                   bool persistent,
                                   CefRefPtr<Callback> callback) {
  if (OnMessage(browser, request))
    return true;

  EXPECT_FALSE(true) << "Unexpected message: " << request.ToString();
  return false;
}

// static
CefRefPtr<CefDictionaryValue> ExtensionTestHandler::CreateDefaultManifest(
    const std::vector<std::string>& api_permissions) {
  CefRefPtr<CefDictionaryValue> manifest = CefDictionaryValue::Create();
  manifest->SetString("name", "An extension");
  manifest->SetString("description", "An extension description");
  manifest->SetString("version", "1.0");
  manifest->SetInt("manifest_version", 2);

  CefRefPtr<CefListValue> permissions = CefListValue::Create();
  permissions->SetSize(api_permissions.size() + 2);
  size_t idx = 0;
  for (; idx < api_permissions.size(); ++idx)
    permissions->SetString(idx, api_permissions[idx]);

  // Allow access to all http/https origins.
  permissions->SetString(idx++, "http://*/*");
  permissions->SetString(idx++, "https://*/*");

  manifest->SetList("permissions", permissions);

  return manifest;
}

// static
std::string ExtensionTestHandler::GetMessageJS(const std::string& message) {
  EXPECT_TRUE(!message.empty());
  return "window.testQuery({request:'" + message + "'});";
}

// static
void ExtensionTestHandler::VerifyExtensionInContext(
    CefRefPtr<CefExtension> extension,
    CefRefPtr<CefRequestContext> context,
    bool has_access,
    bool is_loader) {
  const CefString& extension_id = extension->GetIdentifier();
  EXPECT_FALSE(extension_id.empty());

  if (has_access) {
    EXPECT_TRUE(context->DidLoadExtension(extension_id));
    EXPECT_TRUE(context->HasExtension(extension_id));
  } else {
    EXPECT_FALSE(context->DidLoadExtension(extension_id));
    EXPECT_FALSE(context->HasExtension(extension_id));
  }

  CefRefPtr<CefExtension> extension2 = context->GetExtension(extension_id);
  if (has_access) {
    EXPECT_TRUE(extension2);
    EXPECT_TRUE(extension->IsSame(extension2));
    TestDictionaryEqual(extension->GetManifest(), extension2->GetManifest());
  } else {
    EXPECT_FALSE(extension2);
  }

  std::vector<CefString> extension_ids;
  EXPECT_TRUE(context->GetExtensions(extension_ids));

  // Should be our test extension and possibly the builtin PDF extension if it
  // has finished loading (our extension may load first if the call to
  // LoadExtension initializes the request context).
  bool has_extension = false;
  for (size_t i = 0; i < extension_ids.size(); ++i) {
    if (extension_ids[i] == extension_id) {
      has_extension = true;
      break;
    }
  }
  if (has_access) {
    EXPECT_TRUE(has_extension);
  } else {
    EXPECT_FALSE(has_extension);
  }
}

void ExtensionTestHandler::LoadExtension(
    const std::string& extension_path,
    CefRefPtr<CefDictionaryValue> manifest) {
  EXPECT_TRUE(!extension_path.empty());
  loader_request_context_->LoadExtension(extension_path, manifest, this);
}

void ExtensionTestHandler::UnloadExtension(CefRefPtr<CefExtension> extension) {
  EXPECT_TRUE(extension);
  extension->Unload();
  EXPECT_FALSE(extension->IsLoaded());
  EXPECT_FALSE(extension->GetLoaderContext());
}

void ExtensionTestHandler::ReleaseRequestContexts() {
  request_context_ = nullptr;
  loader_request_context_ = nullptr;
}
