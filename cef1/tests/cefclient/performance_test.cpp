// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cefclient/performance_test.h"
#include "include/cef_v8.h"

#include <algorithm>
#include <string>

#include "cefclient/performance_test_setup.h"
#include "cefclient/resource_util.h"

namespace performance_test {

// Use more interations for a Release build.
#ifdef NDEBUG
const size_t kDefaultIterations = 100000;
#else
const size_t kDefaultIterations = 10000;
#endif

const char kTestUrl[] = "http://tests/performance";

namespace {

const char kGetPerfTests[] = "GetPerfTests";
const char kRunPerfTest[] = "RunPerfTest";

class V8Handler : public CefV8Handler {
 public:
  V8Handler() {
  }

  virtual bool Execute(const CefString& name,
                       CefRefPtr<CefV8Value> object,
                       const CefV8ValueList& arguments,
                       CefRefPtr<CefV8Value>& retval,
                       CefString& exception) {
    if (name == kRunPerfTest) {
      if (arguments.size() == 1 && arguments[0]->IsString()) {
        // Run the specified perf test.
        bool found = false;

        std::string test = arguments[0]->GetStringValue();
        for (size_t i = 0; i < kPerfTestsCount; ++i) {
          if (test == kPerfTests[i].name) {
            // Execute the test.
            int64 delta = kPerfTests[i].test(kPerfTests[i].iterations);

            retval = CefV8Value::CreateInt(delta);
            found = true;
            break;
          }
        }

        if (!found) {
          std::string msg = "Unknown test: ";
          msg.append(test);
          exception = msg;
        }
      } else {
        exception = "Invalid function parameters";
      }
    } else if (name == kGetPerfTests) {
      // Retrieve the list of perf tests.
      retval = CefV8Value::CreateArray(kPerfTestsCount);
      for (size_t i = 0; i < kPerfTestsCount; ++i) {
        CefRefPtr<CefV8Value> val = CefV8Value::CreateArray(2);
        val->SetValue(0, CefV8Value::CreateString(kPerfTests[i].name));
        val->SetValue(1, CefV8Value::CreateUInt(kPerfTests[i].iterations));
        retval->SetValue(i, val);
      }
    }

    return true;
  }

 private:
  IMPLEMENT_REFCOUNTING(V8Handler);
};

}  // namespace

void InitTest(CefRefPtr<CefBrowser> browser,
              CefRefPtr<CefFrame> frame,
              CefRefPtr<CefV8Value> object) {
  CefRefPtr<CefV8Handler> handler = new V8Handler();

  // Bind test functions.
  object->SetValue(kGetPerfTests,
      CefV8Value::CreateFunction(kGetPerfTests, handler),
          V8_PROPERTY_ATTRIBUTE_READONLY);
  object->SetValue(kRunPerfTest,
      CefV8Value::CreateFunction(kRunPerfTest, handler),
          V8_PROPERTY_ATTRIBUTE_READONLY);
}

void RunTest(CefRefPtr<CefBrowser> browser) {
  // Load the test URL.
  browser->GetMainFrame()->LoadURL(kTestUrl);
}

}  // namespace performance_test
