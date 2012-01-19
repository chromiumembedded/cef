// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef_runnable.h"
#include "include/cef_v8.h"
#include "tests/unittests/test_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Base class for V8 unit tests.
class V8TestHandler : public TestHandler {
 public:
  explicit V8TestHandler(const std::string& code) {
    html_ = "<html><head><script language=\"JavaScript\">" +
            code +
            "</script></head><body></body></html>";
  }

  virtual void RunTest() OVERRIDE {
    const std::string url = "http://tests/run.html";
    AddResource(url, html_, "text/html");
    CreateBrowser(url);
  }

  virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         int httpStatusCode) OVERRIDE {
    if (!browser->IsPopup() && frame->IsMain()) {
      frame->ExecuteJavaScript("gc();", "", 0);
      DestroyTest();
    }
  }

 private:
  std::string html_;
};

// Base class for V8 unit test handlers.
class V8TestV8Handler : public CefV8Handler {
  IMPLEMENT_REFCOUNTING(V8TestV8Handler);
};

}   // namespace

TEST(V8Test, ExternalMemoryAllocation) {
  class Test : public V8TestV8Handler {
   public:
    static std::string GetExtensionCode() {
      std::string code =
          "function createObject() {"
          "  native function createObject();"
          "  return createObject();"
          "}"
          "function checkSize(object) {"
          "  native function checkSize();"
          "  return checkSize(object);"
          "};";
      return code;
    }

    static std::string GetTestCode() {
      return "checkSize(createObject());";
    }

    Test()
        : object_created_(false),
          size_checked_(false) {
    }

    virtual bool Execute(const CefString& name,
                         CefRefPtr<CefV8Value> object,
                         const CefV8ValueList& arguments,
                         CefRefPtr<CefV8Value>& retval,
                         CefString& exception) {
      static const int kTestSize = 999999999;
      if (name == "createObject") {
        retval = CefV8Value::CreateObject(
            CefRefPtr<CefBase>(), CefRefPtr<CefV8Accessor>());
        object_created_ =
            (retval->AdjustExternallyAllocatedMemory(kTestSize) == kTestSize);
        return true;
      } else if (name == "checkSize") {
        size_checked_ =
            arguments[0]->GetExternallyAllocatedMemory() == kTestSize;
        return true;
      }
      return false;
    }

    bool object_created_;
    bool size_checked_;
  };

  Test* test = new Test();
  CefRegisterExtension("v8/externalMemory", test->GetExtensionCode(), test);

  V8TestHandler* test_handler = new V8TestHandler(test->GetTestCode());
  test_handler->ExecuteTest();

  ASSERT_TRUE(test->object_created_);
  ASSERT_TRUE(test->size_checked_);
}
