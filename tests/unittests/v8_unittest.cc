// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <sstream>
#include "include/cef_runnable.h"
#include "include/cef_task.h"
#include "include/cef_v8.h"
#include "tests/cefclient/client_app.h"
#include "tests/unittests/test_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

// How to add a new test:
// 1. Add a new value to the V8TestMode enumeration.
// 2. Add a method that implements the test in V8RendererTest.
// 3. Add a case for the new enumeration value in V8RendererTest::RunTest.
// 4. Add a line for the test in the "Define the tests" section at the bottom of
//    the file.

namespace {

// Unique values for V8 tests.
const char kV8TestUrl[] = "http://tests/V8Test.Test";
const char kV8BindingTestUrl[] = "http://tests/V8Test.BindingTest";
const char kV8ContextParentTestUrl[] = "http://tests/V8Test.ContextParentTest";
const char kV8ContextChildTestUrl[] = "http://tests/V8Test.ContextChildTest";
const char kV8NavTestUrl[] = "http://tests/V8Test.NavTest";
const char kV8OnUncaughtExceptionTestUrl[] =
    "http://tests/V8Test.OnUncaughtException";
const char kV8TestMsg[] = "V8Test.Test";
const char kV8TestCmdArg[] = "v8-test";
const char kV8DevToolsURLMsg[] = "V8Test.DevToolsURL";
const char kV8DevToolsLoadHookMsg[] = "V8Test.DevToolsLoadHook";

enum V8TestMode {
  V8TEST_NONE = 0,
  V8TEST_NULL_CREATE,
  V8TEST_BOOL_CREATE,
  V8TEST_INT_CREATE,
  V8TEST_UINT_CREATE,
  V8TEST_DOUBLE_CREATE,
  V8TEST_DATE_CREATE,
  V8TEST_STRING_CREATE,
  V8TEST_ARRAY_CREATE,
  V8TEST_ARRAY_VALUE,
  V8TEST_OBJECT_CREATE,
  V8TEST_OBJECT_USERDATA,
  V8TEST_OBJECT_ACCESSOR,
  V8TEST_OBJECT_ACCESSOR_EXCEPTION,
  V8TEST_OBJECT_ACCESSOR_FAIL,
  V8TEST_OBJECT_ACCESSOR_READONLY,
  V8TEST_OBJECT_VALUE,
  V8TEST_OBJECT_VALUE_READONLY,
  V8TEST_OBJECT_VALUE_ENUM,
  V8TEST_OBJECT_VALUE_DONTENUM,
  V8TEST_OBJECT_VALUE_DELETE,
  V8TEST_OBJECT_VALUE_DONTDELETE,
  V8TEST_OBJECT_VALUE_EMPTYKEY,
  V8TEST_FUNCTION_CREATE,
  V8TEST_FUNCTION_HANDLER,
  V8TEST_FUNCTION_HANDLER_EXCEPTION,
  V8TEST_FUNCTION_HANDLER_FAIL,
  V8TEST_FUNCTION_HANDLER_NO_OBJECT,
  V8TEST_FUNCTION_HANDLER_WITH_CONTEXT,
  V8TEST_CONTEXT_EVAL,
  V8TEST_CONTEXT_EVAL_EXCEPTION,
  V8TEST_CONTEXT_ENTERED,
  V8TEST_CONTEXT_INVALID,
  V8TEST_BINDING,
  V8TEST_STACK_TRACE,
  V8TEST_EXTENSION,
  V8TEST_ON_UNCAUGHT_EXCEPTION,
  V8TEST_ON_UNCAUGHT_EXCEPTION_DEV_TOOLS,
};

// Set to the current test being run in the browser process. Will always be
// V8TEST_NONE in the render process.
V8TestMode g_current_test_mode = V8TEST_NONE;

// Browser side.
class V8BrowserTest : public ClientApp::BrowserDelegate {
 public:
  V8BrowserTest() {}

  virtual void OnBeforeChildProcessLaunch(
      CefRefPtr<ClientApp> app,
      CefRefPtr<CefCommandLine> command_line) OVERRIDE {
    CefString process_type = command_line->GetSwitchValue("type");
    if (process_type == "renderer") {
      // Add the current test mode to the render process command line arguments.
      char buff[33];
      sprintf(buff, "%d", g_current_test_mode);
      command_line->AppendSwitchWithValue(kV8TestCmdArg, buff);
    }
  }

 private:
  IMPLEMENT_REFCOUNTING(V8BrowserTest);
};


// Renderer side.
class V8RendererTest : public ClientApp::RenderDelegate {
 public:
  V8RendererTest()
      : test_mode_(V8TEST_NONE),
        devtools_url_("") {
  }

  // Run a test when the process message is received from the browser.
  void RunTest() {
    switch (test_mode_) {
      case V8TEST_NULL_CREATE:
        RunNullCreateTest();
        break;
      case V8TEST_BOOL_CREATE:
        RunBoolCreateTest();
        break;
      case V8TEST_INT_CREATE:
        RunIntCreateTest();
        break;
      case V8TEST_UINT_CREATE:
        RunUIntCreateTest();
        break;
      case V8TEST_DOUBLE_CREATE:
        RunDoubleCreateTest();
        break;
      case V8TEST_DATE_CREATE:
        RunDateCreateTest();
        break;
      case V8TEST_STRING_CREATE:
        RunStringCreateTest();
        break;
      case V8TEST_ARRAY_CREATE:
        RunArrayCreateTest();
        break;
      case V8TEST_ARRAY_VALUE:
        RunArrayValueTest();
        break;
      case V8TEST_OBJECT_CREATE:
        RunObjectCreateTest();
        break;
      case V8TEST_OBJECT_USERDATA:
        RunObjectUserDataTest();
        break;
      case V8TEST_OBJECT_ACCESSOR:
        RunObjectAccessorTest();
        break;
      case V8TEST_OBJECT_ACCESSOR_EXCEPTION:
        RunObjectAccessorExceptionTest();
        break;
      case V8TEST_OBJECT_ACCESSOR_FAIL:
        RunObjectAccessorFailTest();
        break;
      case V8TEST_OBJECT_ACCESSOR_READONLY:
        RunObjectAccessorReadOnlyTest();
        break;
      case V8TEST_OBJECT_VALUE:
        RunObjectValueTest();
        break;
      case V8TEST_OBJECT_VALUE_READONLY:
        RunObjectValueReadOnlyTest();
        break;
      case V8TEST_OBJECT_VALUE_ENUM:
        RunObjectValueEnumTest();
        break;
      case V8TEST_OBJECT_VALUE_DONTENUM:
        RunObjectValueDontEnumTest();
        break;
      case V8TEST_OBJECT_VALUE_DELETE:
        RunObjectValueDeleteTest();
        break;
      case V8TEST_OBJECT_VALUE_DONTDELETE:
        RunObjectValueDontDeleteTest();
        break;
      case V8TEST_OBJECT_VALUE_EMPTYKEY:
        RunObjectValueEmptyKeyTest();
        break;
      case V8TEST_FUNCTION_CREATE:
        RunFunctionCreateTest();
        break;
      case V8TEST_FUNCTION_HANDLER:
        RunFunctionHandlerTest();
        break;
      case V8TEST_FUNCTION_HANDLER_EXCEPTION:
        RunFunctionHandlerExceptionTest();
        break;
      case V8TEST_FUNCTION_HANDLER_FAIL:
        RunFunctionHandlerFailTest();
        break;
      case V8TEST_FUNCTION_HANDLER_NO_OBJECT:
        RunFunctionHandlerNoObjectTest();
        break;
      case V8TEST_FUNCTION_HANDLER_WITH_CONTEXT:
        RunFunctionHandlerWithContextTest();
        break;
      case V8TEST_CONTEXT_EVAL:
        RunContextEvalTest();
        break;
      case V8TEST_CONTEXT_EVAL_EXCEPTION:
        RunContextEvalExceptionTest();
        break;
      case V8TEST_CONTEXT_ENTERED:
        RunContextEnteredTest();
        break;
      case V8TEST_CONTEXT_INVALID:
        // The test is triggered when the context is released.
        browser_->GetMainFrame()->LoadURL(kV8NavTestUrl);
        break;
      case V8TEST_BINDING:
        RunBindingTest();
        break;
      case V8TEST_STACK_TRACE:
        RunStackTraceTest();
        break;
      case V8TEST_ON_UNCAUGHT_EXCEPTION:
        RunOnUncaughtExceptionTest();
        break;
      case V8TEST_ON_UNCAUGHT_EXCEPTION_DEV_TOOLS:
        RunOnUncaughtExceptionDevToolsTest();
        break;
      default:
        // Was a startup test.
        EXPECT_TRUE(startup_test_success_);
        DestroyTest();
        break;
    }
  }

  // Run a test on render process startup.
  void RunStartupTest() {
    switch (test_mode_) {
      case V8TEST_EXTENSION:
        RunExtensionTest();
        break;
      default:
        break;
    }
  }

  void RunNullCreateTest() {
    CefRefPtr<CefV8Value> value = CefV8Value::CreateNull();
    EXPECT_TRUE(value.get());
    EXPECT_TRUE(value->IsNull());

    EXPECT_FALSE(value->IsUndefined());
    EXPECT_FALSE(value->IsArray());
    EXPECT_FALSE(value->IsBool());
    EXPECT_FALSE(value->IsDate());
    EXPECT_FALSE(value->IsDouble());
    EXPECT_FALSE(value->IsFunction());
    EXPECT_FALSE(value->IsInt());
    EXPECT_FALSE(value->IsUInt());
    EXPECT_FALSE(value->IsObject());
    EXPECT_FALSE(value->IsString());

    DestroyTest();
  }

  void RunBoolCreateTest() {
    CefRefPtr<CefV8Value> value = CefV8Value::CreateBool(true);
    EXPECT_TRUE(value.get());
    EXPECT_TRUE(value->IsBool());
    EXPECT_EQ(true, value->GetBoolValue());

    EXPECT_FALSE(value->IsUndefined());
    EXPECT_FALSE(value->IsArray());
    EXPECT_FALSE(value->IsDate());
    EXPECT_FALSE(value->IsDouble());
    EXPECT_FALSE(value->IsFunction());
    EXPECT_FALSE(value->IsInt());
    EXPECT_FALSE(value->IsUInt());
    EXPECT_FALSE(value->IsNull());
    EXPECT_FALSE(value->IsObject());
    EXPECT_FALSE(value->IsString());

    DestroyTest();
  }

  void RunIntCreateTest() {
    CefRefPtr<CefV8Value> value = CefV8Value::CreateInt(12);
    EXPECT_TRUE(value.get());
    EXPECT_TRUE(value->IsInt());
    EXPECT_TRUE(value->IsUInt());
    EXPECT_TRUE(value->IsDouble());
    EXPECT_EQ(12, value->GetIntValue());
    EXPECT_EQ((uint32)12, value->GetUIntValue());
    EXPECT_EQ(12, value->GetDoubleValue());

    EXPECT_FALSE(value->IsUndefined());
    EXPECT_FALSE(value->IsArray());
    EXPECT_FALSE(value->IsBool());
    EXPECT_FALSE(value->IsDate());
    EXPECT_FALSE(value->IsFunction());
    EXPECT_FALSE(value->IsNull());
    EXPECT_FALSE(value->IsObject());
    EXPECT_FALSE(value->IsString());

    DestroyTest();
  }

  void RunUIntCreateTest() {
    CefRefPtr<CefV8Value> value = CefV8Value::CreateUInt(12);
    EXPECT_TRUE(value.get());
    EXPECT_TRUE(value->IsInt());
    EXPECT_TRUE(value->IsUInt());
    EXPECT_TRUE(value->IsDouble());
    EXPECT_EQ(12, value->GetIntValue());
    EXPECT_EQ((uint32)12, value->GetUIntValue());
    EXPECT_EQ(12, value->GetDoubleValue());

    EXPECT_FALSE(value->IsUndefined());
    EXPECT_FALSE(value->IsArray());
    EXPECT_FALSE(value->IsBool());
    EXPECT_FALSE(value->IsDate());
    EXPECT_FALSE(value->IsFunction());
    EXPECT_FALSE(value->IsNull());
    EXPECT_FALSE(value->IsObject());
    EXPECT_FALSE(value->IsString());

    DestroyTest();
  }

  void RunDoubleCreateTest() {
    CefRefPtr<CefV8Value> value = CefV8Value::CreateDouble(12.1223);
    EXPECT_TRUE(value.get());
    EXPECT_TRUE(value->IsDouble());
    EXPECT_EQ(12.1223, value->GetDoubleValue());

    EXPECT_FALSE(value->IsUndefined());
    EXPECT_FALSE(value->IsArray());
    EXPECT_FALSE(value->IsBool());
    EXPECT_FALSE(value->IsDate());
    EXPECT_FALSE(value->IsFunction());
    EXPECT_FALSE(value->IsInt());
    EXPECT_FALSE(value->IsUInt());
    EXPECT_FALSE(value->IsNull());
    EXPECT_FALSE(value->IsObject());
    EXPECT_FALSE(value->IsString());

    DestroyTest();
  }

  void RunDateCreateTest() {
    CefRefPtr<CefV8Context> context = GetContext();

    CefTime date;
    date.year = 2200;
    date.month = 4;
#if !defined(OS_MACOSX)
    date.day_of_week = 5;
#endif
    date.day_of_month = 11;
    date.hour = 20;
    date.minute = 15;
    date.second = 42;

    // Enter the V8 context.
    EXPECT_TRUE(context->Enter());

    CefRefPtr<CefV8Value> value = CefV8Value::CreateDate(date);
    EXPECT_TRUE(value.get());
    EXPECT_TRUE(value->IsDate());
    EXPECT_TRUE(value->IsObject());
    EXPECT_EQ(date.GetTimeT(), value->GetDateValue().GetTimeT());

    // Exit the V8 context.
    EXPECT_TRUE(context->Exit());

    EXPECT_FALSE(value->IsUndefined());
    EXPECT_FALSE(value->IsArray());
    EXPECT_FALSE(value->IsBool());
    EXPECT_FALSE(value->IsDouble());
    EXPECT_FALSE(value->IsFunction());
    EXPECT_FALSE(value->IsInt());
    EXPECT_FALSE(value->IsUInt());
    EXPECT_FALSE(value->IsNull());
    EXPECT_FALSE(value->IsString());

    DestroyTest();
  }

  void RunStringCreateTest() {
    CefRefPtr<CefV8Value> value = CefV8Value::CreateString("My string");
    EXPECT_TRUE(value.get());
    EXPECT_TRUE(value->IsString());
    EXPECT_STREQ("My string", value->GetStringValue().ToString().c_str());

    EXPECT_FALSE(value->IsUndefined());
    EXPECT_FALSE(value->IsArray());
    EXPECT_FALSE(value->IsBool());
    EXPECT_FALSE(value->IsDate());
    EXPECT_FALSE(value->IsDouble());
    EXPECT_FALSE(value->IsFunction());
    EXPECT_FALSE(value->IsInt());
    EXPECT_FALSE(value->IsUInt());
    EXPECT_FALSE(value->IsNull());
    EXPECT_FALSE(value->IsObject());

    DestroyTest();
  }

  void RunArrayCreateTest() {
    CefRefPtr<CefV8Context> context = GetContext();

    // Enter the V8 context.
    EXPECT_TRUE(context->Enter());

    CefRefPtr<CefV8Value> value = CefV8Value::CreateArray(2);
    EXPECT_TRUE(value.get());
    EXPECT_TRUE(value->IsArray());
    EXPECT_TRUE(value->IsObject());
    EXPECT_EQ(2, value->GetArrayLength());
    EXPECT_FALSE(value->HasValue(0));
    EXPECT_FALSE(value->HasValue(1));

    // Exit the V8 context.
    EXPECT_TRUE(context->Exit());

    EXPECT_FALSE(value->IsUndefined());
    EXPECT_FALSE(value->IsBool());
    EXPECT_FALSE(value->IsDate());
    EXPECT_FALSE(value->IsDouble());
    EXPECT_FALSE(value->IsFunction());
    EXPECT_FALSE(value->IsInt());
    EXPECT_FALSE(value->IsUInt());
    EXPECT_FALSE(value->IsNull());
    EXPECT_FALSE(value->IsString());

    DestroyTest();
  }

  void RunArrayValueTest() {
    CefRefPtr<CefV8Context> context = GetContext();

    // Enter the V8 context.
    EXPECT_TRUE(context->Enter());

    CefRefPtr<CefV8Value> value = CefV8Value::CreateArray(0);
    EXPECT_TRUE(value.get());
    EXPECT_TRUE(value->IsArray());
    EXPECT_EQ(0, value->GetArrayLength());

    // Test addng values.
    EXPECT_FALSE(value->HasValue(0));
    EXPECT_FALSE(value->HasValue(1));

    EXPECT_TRUE(value->SetValue(0, CefV8Value::CreateInt(10)));
    EXPECT_FALSE(value->HasException());
    EXPECT_TRUE(value->HasValue(0));
    EXPECT_FALSE(value->HasValue(1));

    EXPECT_TRUE(value->GetValue(0)->IsInt());
    EXPECT_EQ(10, value->GetValue(0)->GetIntValue());
    EXPECT_FALSE(value->HasException());
    EXPECT_EQ(1, value->GetArrayLength());

    EXPECT_TRUE(value->SetValue(1, CefV8Value::CreateInt(43)));
    EXPECT_FALSE(value->HasException());
    EXPECT_TRUE(value->HasValue(0));
    EXPECT_TRUE(value->HasValue(1));

    EXPECT_TRUE(value->GetValue(1)->IsInt());
    EXPECT_EQ(43, value->GetValue(1)->GetIntValue());
    EXPECT_FALSE(value->HasException());
    EXPECT_EQ(2, value->GetArrayLength());

    EXPECT_TRUE(value->DeleteValue(0));
    EXPECT_FALSE(value->HasValue(0));
    EXPECT_TRUE(value->HasValue(1));
    EXPECT_EQ(2, value->GetArrayLength());

    EXPECT_TRUE(value->DeleteValue(1));
    EXPECT_FALSE(value->HasValue(0));
    EXPECT_FALSE(value->HasValue(1));
    EXPECT_EQ(2, value->GetArrayLength());

    // Exit the V8 context.
    EXPECT_TRUE(context->Exit());

    DestroyTest();
  }

  void RunObjectCreateTest() {
    CefRefPtr<CefV8Context> context = GetContext();

    // Enter the V8 context.
    EXPECT_TRUE(context->Enter());

    CefRefPtr<CefV8Value> value = CefV8Value::CreateObject(NULL);

    // Exit the V8 context.
    EXPECT_TRUE(context->Exit());

    EXPECT_TRUE(value.get());
    EXPECT_TRUE(value->IsObject());
    EXPECT_FALSE(value->GetUserData().get());

    EXPECT_FALSE(value->IsUndefined());
    EXPECT_FALSE(value->IsArray());
    EXPECT_FALSE(value->IsBool());
    EXPECT_FALSE(value->IsDate());
    EXPECT_FALSE(value->IsDouble());
    EXPECT_FALSE(value->IsFunction());
    EXPECT_FALSE(value->IsInt());
    EXPECT_FALSE(value->IsUInt());
    EXPECT_FALSE(value->IsNull());
    EXPECT_FALSE(value->IsString());

    DestroyTest();
  }

  void RunObjectUserDataTest() {
    CefRefPtr<CefV8Context> context = GetContext();

    class UserData : public CefBase {
     public:
      explicit UserData(int value) : value_(value) {}
      int value_;
      IMPLEMENT_REFCOUNTING(UserData);
    };

    // Enter the V8 context.
    EXPECT_TRUE(context->Enter());

    CefRefPtr<CefV8Value> value = CefV8Value::CreateObject(NULL);
    EXPECT_TRUE(value.get());

    EXPECT_TRUE(value->SetUserData(new UserData(10)));

    CefRefPtr<CefBase> user_data = value->GetUserData();
    EXPECT_TRUE(user_data.get());
    UserData* user_data_impl = static_cast<UserData*>(user_data.get());
    EXPECT_EQ(10, user_data_impl->value_);

    // Exit the V8 context.
    EXPECT_TRUE(context->Exit());

    DestroyTest();
  }

  void RunObjectAccessorTest() {
    CefRefPtr<CefV8Context> context = GetContext();

    static const char* kName = "val";
    static const int kValue = 20;

    class Accessor : public CefV8Accessor {
     public:
      Accessor() : value_(0) {}
      virtual bool Get(const CefString& name,
                       const CefRefPtr<CefV8Value> object,
                       CefRefPtr<CefV8Value>& retval,
                       CefString& exception) OVERRIDE {
        EXPECT_STREQ(kName, name.ToString().c_str());

        EXPECT_TRUE(object.get());
        EXPECT_TRUE(object->IsSame(object_));

        EXPECT_FALSE(retval.get());
        EXPECT_TRUE(exception.empty());

        got_get_.yes();
        retval = CefV8Value::CreateInt(value_);
        EXPECT_EQ(kValue, retval->GetIntValue());
        return true;
      }

      virtual bool Set(const CefString& name,
                       const CefRefPtr<CefV8Value> object,
                       const CefRefPtr<CefV8Value> value,
                       CefString& exception) OVERRIDE {
        EXPECT_STREQ(kName, name.ToString().c_str());

        EXPECT_TRUE(object.get());
        EXPECT_TRUE(object->IsSame(object_));

        EXPECT_TRUE(value.get());
        EXPECT_TRUE(exception.empty());

        got_set_.yes();
        value_ = value->GetIntValue();
        EXPECT_EQ(kValue, value_);
        return true;
      }

      CefRefPtr<CefV8Value> object_;
      int value_;
      TrackCallback got_get_;
      TrackCallback got_set_;

      IMPLEMENT_REFCOUNTING(Accessor);
    };

    // Enter the V8 context.
    EXPECT_TRUE(context->Enter());

    Accessor* accessor = new Accessor;
    CefRefPtr<CefV8Accessor> accessorPtr(accessor);

    CefRefPtr<CefV8Value> object = CefV8Value::CreateObject(accessor);
    EXPECT_TRUE(object.get());
    accessor->object_ = object;

    EXPECT_FALSE(object->HasValue(kName));

    EXPECT_TRUE(object->SetValue(kName, V8_ACCESS_CONTROL_DEFAULT,
        V8_PROPERTY_ATTRIBUTE_NONE));
    EXPECT_FALSE(object->HasException());
    EXPECT_TRUE(object->HasValue(kName));

    EXPECT_TRUE(object->SetValue(kName, CefV8Value::CreateInt(kValue),
        V8_PROPERTY_ATTRIBUTE_NONE));
    EXPECT_FALSE(object->HasException());
    EXPECT_TRUE(accessor->got_set_);
    EXPECT_EQ(kValue, accessor->value_);

    CefRefPtr<CefV8Value> val = object->GetValue(kName);
    EXPECT_FALSE(object->HasException());
    EXPECT_TRUE(val.get());
    EXPECT_TRUE(accessor->got_get_);
    EXPECT_TRUE(val->IsInt());
    EXPECT_EQ(kValue, val->GetIntValue());

    accessor->object_ = NULL;

    // Exit the V8 context.
    EXPECT_TRUE(context->Exit());

    DestroyTest();
  }

  void RunObjectAccessorExceptionTest() {
    CefRefPtr<CefV8Context> context = GetContext();

    static const char* kName = "val";
    static const char* kGetException = "My get exception";
    static const char* kSetException = "My set exception";
    static const char* kGetExceptionMsg = "Uncaught Error: My get exception";
    static const char* kSetExceptionMsg = "Uncaught Error: My set exception";

    class Accessor : public CefV8Accessor {
     public:
      Accessor() {}
      virtual bool Get(const CefString& name,
                       const CefRefPtr<CefV8Value> object,
                       CefRefPtr<CefV8Value>& retval,
                       CefString& exception) OVERRIDE {
        got_get_.yes();
        exception = kGetException;
        return true;
      }

      virtual bool Set(const CefString& name,
                       const CefRefPtr<CefV8Value> object,
                       const CefRefPtr<CefV8Value> value,
                       CefString& exception) OVERRIDE {
        got_set_.yes();
        exception = kSetException;
        return true;
      }

      TrackCallback got_get_;
      TrackCallback got_set_;

      IMPLEMENT_REFCOUNTING(Accessor);
    };

    // Enter the V8 context.
    EXPECT_TRUE(context->Enter());

    CefRefPtr<CefV8Exception> exception;
    Accessor* accessor = new Accessor;
    CefRefPtr<CefV8Accessor> accessorPtr(accessor);

    CefRefPtr<CefV8Value> object = CefV8Value::CreateObject(accessor);
    EXPECT_TRUE(object.get());

    EXPECT_FALSE(object->HasValue(kName));

    EXPECT_TRUE(object->SetValue(kName, V8_ACCESS_CONTROL_DEFAULT,
        V8_PROPERTY_ATTRIBUTE_NONE));
    EXPECT_FALSE(object->HasException());
    EXPECT_TRUE(object->HasValue(kName));

    EXPECT_FALSE(object->SetValue(kName, CefV8Value::CreateInt(1),
        V8_PROPERTY_ATTRIBUTE_NONE));
    EXPECT_TRUE(object->HasException());
    EXPECT_TRUE(accessor->got_set_);
    exception = object->GetException();
    EXPECT_TRUE(exception.get());
    EXPECT_STREQ(kSetExceptionMsg, exception->GetMessage().ToString().c_str());

    EXPECT_TRUE(object->ClearException());
    EXPECT_FALSE(object->HasException());

    CefRefPtr<CefV8Value> val = object->GetValue(kName);
    EXPECT_FALSE(val.get());
    EXPECT_TRUE(object->HasException());
    EXPECT_TRUE(accessor->got_get_);
    exception = object->GetException();
    EXPECT_TRUE(exception.get());
    EXPECT_STREQ(kGetExceptionMsg, exception->GetMessage().ToString().c_str());

    // Exit the V8 context.
    EXPECT_TRUE(context->Exit());

    DestroyTest();
  }

  void RunObjectAccessorFailTest() {
    CefRefPtr<CefV8Context> context = GetContext();

    static const char* kName = "val";

    class Accessor : public CefV8Accessor {
     public:
      Accessor() {}
      virtual bool Get(const CefString& name,
                       const CefRefPtr<CefV8Value> object,
                       CefRefPtr<CefV8Value>& retval,
                       CefString& exception) OVERRIDE {
        got_get_.yes();
        return false;
      }

      virtual bool Set(const CefString& name,
                       const CefRefPtr<CefV8Value> object,
                       const CefRefPtr<CefV8Value> value,
                       CefString& exception) OVERRIDE {
        got_set_.yes();
        return false;
      }

      TrackCallback got_get_;
      TrackCallback got_set_;

      IMPLEMENT_REFCOUNTING(Accessor);
    };

    // Enter the V8 context.
    EXPECT_TRUE(context->Enter());

    CefRefPtr<CefV8Exception> exception;
    Accessor* accessor = new Accessor;
    CefRefPtr<CefV8Accessor> accessorPtr(accessor);

    CefRefPtr<CefV8Value> object = CefV8Value::CreateObject(accessor);
    EXPECT_TRUE(object.get());

    EXPECT_FALSE(object->HasValue(kName));

    EXPECT_TRUE(object->SetValue(kName, V8_ACCESS_CONTROL_DEFAULT,
        V8_PROPERTY_ATTRIBUTE_NONE));
    EXPECT_FALSE(object->HasException());
    EXPECT_TRUE(object->HasValue(kName));

    EXPECT_TRUE(object->SetValue(kName, CefV8Value::CreateInt(1),
        V8_PROPERTY_ATTRIBUTE_NONE));
    EXPECT_FALSE(object->HasException());
    EXPECT_TRUE(accessor->got_set_);

    CefRefPtr<CefV8Value> val = object->GetValue(kName);
    EXPECT_TRUE(val.get());
    EXPECT_FALSE(object->HasException());
    EXPECT_TRUE(accessor->got_get_);
    EXPECT_TRUE(val->IsUndefined());

    // Exit the V8 context.
    EXPECT_TRUE(context->Exit());

    DestroyTest();
  }

  void RunObjectAccessorReadOnlyTest() {
    CefRefPtr<CefV8Context> context = GetContext();

    static const char* kName = "val";

    class Accessor : public CefV8Accessor {
     public:
      Accessor() {}
      virtual bool Get(const CefString& name,
                       const CefRefPtr<CefV8Value> object,
                       CefRefPtr<CefV8Value>& retval,
                       CefString& exception) OVERRIDE {
        got_get_.yes();
        return true;
      }

      virtual bool Set(const CefString& name,
                       const CefRefPtr<CefV8Value> object,
                       const CefRefPtr<CefV8Value> value,
                       CefString& exception) OVERRIDE {
        got_set_.yes();
        return true;
      }

      TrackCallback got_get_;
      TrackCallback got_set_;

      IMPLEMENT_REFCOUNTING(Accessor);
    };

    // Enter the V8 context.
    EXPECT_TRUE(context->Enter());

    CefRefPtr<CefV8Exception> exception;
    Accessor* accessor = new Accessor;
    CefRefPtr<CefV8Accessor> accessorPtr(accessor);

    CefRefPtr<CefV8Value> object = CefV8Value::CreateObject(accessor);
    EXPECT_TRUE(object.get());

    EXPECT_FALSE(object->HasValue(kName));

    EXPECT_TRUE(object->SetValue(kName, V8_ACCESS_CONTROL_DEFAULT,
        V8_PROPERTY_ATTRIBUTE_READONLY));
    EXPECT_FALSE(object->HasException());
    EXPECT_TRUE(object->HasValue(kName));

    EXPECT_TRUE(object->SetValue(kName, CefV8Value::CreateInt(1),
        V8_PROPERTY_ATTRIBUTE_NONE));
    EXPECT_FALSE(object->HasException());
    EXPECT_FALSE(accessor->got_set_);

    CefRefPtr<CefV8Value> val = object->GetValue(kName);
    EXPECT_TRUE(val.get());
    EXPECT_FALSE(object->HasException());
    EXPECT_TRUE(accessor->got_get_);
    EXPECT_TRUE(val->IsUndefined());

    // Exit the V8 context.
    EXPECT_TRUE(context->Exit());

    DestroyTest();
  }

  void RunObjectValueTest() {
    CefRefPtr<CefV8Context> context = GetContext();

    static const char* kName = "test_arg";
    static const int kVal1 = 13;
    static const int kVal2 = 65;

    // Enter the V8 context.
    EXPECT_TRUE(context->Enter());

    CefRefPtr<CefV8Value> object = context->GetGlobal();
    EXPECT_TRUE(object.get());

    object->SetValue(kName, CefV8Value::CreateInt(kVal1),
        V8_PROPERTY_ATTRIBUTE_NONE);

    std::stringstream test;
    test <<
        "if (window." << kName << " != " << kVal1 << ") throw 'Fail';\n" <<
        "window." << kName << " = " << kVal2 << ";";

    CefRefPtr<CefV8Value> retval;
    CefRefPtr<CefV8Exception> exception;

    EXPECT_TRUE(context->Eval(test.str(), retval, exception));
    if (exception.get())
      ADD_FAILURE() << exception->GetMessage().c_str();

    CefRefPtr<CefV8Value> newval = object->GetValue(kName);
    EXPECT_TRUE(newval.get());
    EXPECT_TRUE(newval->IsInt());
    EXPECT_EQ(kVal2, newval->GetIntValue());

    // Exit the V8 context.
    EXPECT_TRUE(context->Exit());

    DestroyTest();
  }

  void RunObjectValueReadOnlyTest() {
    CefRefPtr<CefV8Context> context = GetContext();

    static const char* kName = "test_arg";
    static const int kVal1 = 13;
    static const int kVal2 = 65;

    // Enter the V8 context.
    EXPECT_TRUE(context->Enter());

    CefRefPtr<CefV8Value> object = context->GetGlobal();
    EXPECT_TRUE(object.get());

    object->SetValue(kName, CefV8Value::CreateInt(kVal1),
        V8_PROPERTY_ATTRIBUTE_READONLY);

    std::stringstream test;
    test <<
        "if (window." << kName << " != " << kVal1 << ") throw 'Fail';\n" <<
        "window." << kName << " = " << kVal2 << ";";

    CefRefPtr<CefV8Value> retval;
    CefRefPtr<CefV8Exception> exception;

    EXPECT_TRUE(context->Eval(test.str(), retval, exception));
    if (exception.get())
      ADD_FAILURE() << exception->GetMessage().c_str();

    CefRefPtr<CefV8Value> newval = object->GetValue(kName);
    EXPECT_TRUE(newval.get());
    EXPECT_TRUE(newval->IsInt());
    EXPECT_EQ(kVal1, newval->GetIntValue());

    // Exit the V8 context.
    EXPECT_TRUE(context->Exit());

    DestroyTest();
  }

  void RunObjectValueEnumTest() {
    CefRefPtr<CefV8Context> context = GetContext();

    static const char* kObjName = "test_obj";
    static const char* kArgName = "test_arg";

    // Enter the V8 context.
    EXPECT_TRUE(context->Enter());

    CefRefPtr<CefV8Value> object = context->GetGlobal();
    EXPECT_TRUE(object.get());

    CefRefPtr<CefV8Value> obj1 = CefV8Value::CreateObject(NULL);
    object->SetValue(kObjName, obj1, V8_PROPERTY_ATTRIBUTE_NONE);

    obj1->SetValue(kArgName, CefV8Value::CreateInt(0),
        V8_PROPERTY_ATTRIBUTE_NONE);

    std::stringstream test;
    test <<
        "for (var i in window." << kObjName << ") {\n"
        "window." << kObjName << "[i]++;\n"
        "}";

    CefRefPtr<CefV8Value> retval;
    CefRefPtr<CefV8Exception> exception;

    EXPECT_TRUE(context->Eval(test.str(), retval, exception));
    if (exception.get())
      ADD_FAILURE() << exception->GetMessage().c_str();

    CefRefPtr<CefV8Value> newval = obj1->GetValue(kArgName);
    EXPECT_TRUE(newval.get());
    EXPECT_TRUE(newval->IsInt());
    EXPECT_EQ(1, newval->GetIntValue());

    // Exit the V8 context.
    EXPECT_TRUE(context->Exit());

    DestroyTest();
  }

  void RunObjectValueDontEnumTest() {
    CefRefPtr<CefV8Context> context = GetContext();

    static const char* kObjName = "test_obj";
    static const char* kArgName = "test_arg";

    // Enter the V8 context.
    EXPECT_TRUE(context->Enter());

    CefRefPtr<CefV8Value> object = context->GetGlobal();
    EXPECT_TRUE(object.get());

    CefRefPtr<CefV8Value> obj1 = CefV8Value::CreateObject(NULL);
    object->SetValue(kObjName, obj1, V8_PROPERTY_ATTRIBUTE_NONE);

    obj1->SetValue(kArgName, CefV8Value::CreateInt(0),
        V8_PROPERTY_ATTRIBUTE_DONTENUM);

    std::stringstream test;
    test <<
        "for (var i in window." << kObjName << ") {\n"
        "window." << kObjName << "[i]++;\n"
        "}";

    CefRefPtr<CefV8Value> retval;
    CefRefPtr<CefV8Exception> exception;

    EXPECT_TRUE(context->Eval(test.str(), retval, exception));
    if (exception.get())
      ADD_FAILURE() << exception->GetMessage().c_str();

    CefRefPtr<CefV8Value> newval = obj1->GetValue(kArgName);
    EXPECT_TRUE(newval.get());
    EXPECT_TRUE(newval->IsInt());
    EXPECT_EQ(0, newval->GetIntValue());

    // Exit the V8 context.
    EXPECT_TRUE(context->Exit());

    DestroyTest();
  }

  void RunObjectValueDeleteTest() {
    CefRefPtr<CefV8Context> context = GetContext();

    static const char* kName = "test_arg";
    static const int kVal1 = 13;
    static const int kVal2 = 65;

    // Enter the V8 context.
    EXPECT_TRUE(context->Enter());

    CefRefPtr<CefV8Value> object = context->GetGlobal();
    EXPECT_TRUE(object.get());

    object->SetValue(kName, CefV8Value::CreateInt(kVal1),
        V8_PROPERTY_ATTRIBUTE_NONE);

    std::stringstream test;
    test <<
        "if (window." << kName << " != " << kVal1 << ") throw 'Fail';\n" <<
        "window." << kName << " = " << kVal2 << ";\n"
        "delete window." << kName << ";";

    CefRefPtr<CefV8Value> retval;
    CefRefPtr<CefV8Exception> exception;

    EXPECT_TRUE(context->Eval(test.str(), retval, exception));
    if (exception.get())
      ADD_FAILURE() << exception->GetMessage().c_str();

    CefRefPtr<CefV8Value> newval = object->GetValue(kName);
    EXPECT_TRUE(newval.get());
    EXPECT_TRUE(newval->IsUndefined());
    EXPECT_FALSE(newval->IsInt());

    // Exit the V8 context.
    EXPECT_TRUE(context->Exit());

    DestroyTest();
  }

  void RunObjectValueDontDeleteTest() {
    CefRefPtr<CefV8Context> context = GetContext();

    static const char* kName = "test_arg";
    static const int kVal1 = 13;
    static const int kVal2 = 65;

    // Enter the V8 context.
    EXPECT_TRUE(context->Enter());

    CefRefPtr<CefV8Value> object = context->GetGlobal();
    EXPECT_TRUE(object.get());

    object->SetValue(kName, CefV8Value::CreateInt(kVal1),
        V8_PROPERTY_ATTRIBUTE_DONTDELETE);

    std::stringstream test;
    test <<
        "if (window." << kName << " != " << kVal1 << ") throw 'Fail';\n" <<
        "window." << kName << " = " << kVal2 << ";\n"
        "delete window." << kName << ";";

    CefRefPtr<CefV8Value> retval;
    CefRefPtr<CefV8Exception> exception;

    EXPECT_TRUE(context->Eval(test.str(), retval, exception));
    if (exception.get())
      ADD_FAILURE() << exception->GetMessage().c_str();

    CefRefPtr<CefV8Value> newval = object->GetValue(kName);
    EXPECT_TRUE(newval.get());
    EXPECT_TRUE(newval->IsInt());
    EXPECT_EQ(kVal2, newval->GetIntValue());

    // Exit the V8 context.
    EXPECT_TRUE(context->Exit());

    DestroyTest();
  }

  void RunObjectValueEmptyKeyTest() {
    CefRefPtr<CefV8Context> context = GetContext();

    static const char* kName = "";
    static const int kVal = 13;

    // Enter the V8 context.
    EXPECT_TRUE(context->Enter());

    CefRefPtr<CefV8Value> object = context->GetGlobal();
    EXPECT_TRUE(object.get());

    EXPECT_FALSE(object->HasValue(kName));

    object->SetValue(kName, CefV8Value::CreateInt(kVal),
        V8_PROPERTY_ATTRIBUTE_NONE);
    EXPECT_TRUE(object->HasValue(kName));

    CefRefPtr<CefV8Value> newval = object->GetValue(kName);
    EXPECT_TRUE(newval.get());
    EXPECT_TRUE(newval->IsInt());
    EXPECT_EQ(kVal, newval->GetIntValue());

    EXPECT_TRUE(object->DeleteValue(kName));
    EXPECT_FALSE(object->HasValue(kName));

    // Exit the V8 context.
    EXPECT_TRUE(context->Exit());

    DestroyTest();
  }

  void RunFunctionCreateTest() {
    CefRefPtr<CefV8Context> context = GetContext();

    class Handler : public CefV8Handler {
     public:
      Handler() {}
      virtual bool Execute(const CefString& name,
                         CefRefPtr<CefV8Value> object,
                         const CefV8ValueList& arguments,
                         CefRefPtr<CefV8Value>& retval,
                         CefString& exception) OVERRIDE { return false; }
      IMPLEMENT_REFCOUNTING(Handler);
    };

    // Enter the V8 context.
    EXPECT_TRUE(context->Enter());

    CefRefPtr<CefV8Value> value = CefV8Value::CreateFunction("f", new Handler);

    // Exit the V8 context.
    EXPECT_TRUE(context->Exit());

    EXPECT_TRUE(value.get());
    EXPECT_TRUE(value->IsFunction());
    EXPECT_TRUE(value->IsObject());

    EXPECT_FALSE(value->IsUndefined());
    EXPECT_FALSE(value->IsArray());
    EXPECT_FALSE(value->IsBool());
    EXPECT_FALSE(value->IsDate());
    EXPECT_FALSE(value->IsDouble());
    EXPECT_FALSE(value->IsInt());
    EXPECT_FALSE(value->IsUInt());
    EXPECT_FALSE(value->IsNull());
    EXPECT_FALSE(value->IsString());

    DestroyTest();
  }

  void RunFunctionHandlerTest() {
    CefRefPtr<CefV8Context> context = GetContext();

    static const char* kFuncName = "myfunc";
    static const int kVal1 = 32;
    static const int kVal2 = 41;
    static const int kRetVal = 8;

    class Handler : public CefV8Handler {
     public:
      Handler() {}
      virtual bool Execute(const CefString& name,
                         CefRefPtr<CefV8Value> object,
                         const CefV8ValueList& arguments,
                         CefRefPtr<CefV8Value>& retval,
                         CefString& exception) OVERRIDE {
        EXPECT_STREQ(kFuncName, name.ToString().c_str());
        EXPECT_TRUE(object->IsSame(object_));

        EXPECT_EQ((size_t)2, arguments.size());
        EXPECT_TRUE(arguments[0]->IsInt());
        EXPECT_EQ(kVal1, arguments[0]->GetIntValue());
        EXPECT_TRUE(arguments[1]->IsInt());
        EXPECT_EQ(kVal2, arguments[1]->GetIntValue());

        EXPECT_TRUE(exception.empty());

        retval = CefV8Value::CreateInt(kRetVal);
        EXPECT_TRUE(retval.get());
        EXPECT_EQ(kRetVal, retval->GetIntValue());

        got_execute_.yes();
        return true;
      }

      CefRefPtr<CefV8Value> object_;
      TrackCallback got_execute_;

      IMPLEMENT_REFCOUNTING(Handler);
    };

    // Enter the V8 context.
    EXPECT_TRUE(context->Enter());

    Handler* handler = new Handler;
    CefRefPtr<CefV8Handler> handlerPtr(handler);

    CefRefPtr<CefV8Value> func =
        CefV8Value::CreateFunction(kFuncName, handler);
    EXPECT_TRUE(func.get());

    CefRefPtr<CefV8Value> obj = CefV8Value::CreateObject(NULL);
    EXPECT_TRUE(obj.get());
    handler->object_ = obj;

    CefV8ValueList args;
    args.push_back(CefV8Value::CreateInt(kVal1));
    args.push_back(CefV8Value::CreateInt(kVal2));

    CefRefPtr<CefV8Value> retval = func->ExecuteFunction(obj, args);
    EXPECT_TRUE(handler->got_execute_);
    EXPECT_TRUE(retval.get());
    EXPECT_FALSE(func->HasException());
    EXPECT_TRUE(retval->IsInt());
    EXPECT_EQ(kRetVal, retval->GetIntValue());

    handler->object_ = NULL;

    // Exit the V8 context.
    EXPECT_TRUE(context->Exit());

    DestroyTest();
  }

  void RunFunctionHandlerExceptionTest() {
    CefRefPtr<CefV8Context> context = GetContext();

    static const char* kException = "My error";
    static const char* kExceptionMsg = "Uncaught Error: My error";

    class Handler : public CefV8Handler {
     public:
      Handler() {}
      virtual bool Execute(const CefString& name,
                         CefRefPtr<CefV8Value> object,
                         const CefV8ValueList& arguments,
                         CefRefPtr<CefV8Value>& retval,
                         CefString& exception) OVERRIDE {
        exception = kException;
        got_execute_.yes();
        return true;
      }

      TrackCallback got_execute_;

      IMPLEMENT_REFCOUNTING(Handler);
    };

    // Enter the V8 context.
    EXPECT_TRUE(context->Enter());

    Handler* handler = new Handler;
    CefRefPtr<CefV8Handler> handlerPtr(handler);

    CefRefPtr<CefV8Value> func =
        CefV8Value::CreateFunction("myfunc", handler);
    EXPECT_TRUE(func.get());

    CefV8ValueList args;

    CefRefPtr<CefV8Value> retval = func->ExecuteFunction(NULL, args);
    EXPECT_TRUE(handler->got_execute_);
    EXPECT_FALSE(retval.get());
    EXPECT_TRUE(func->HasException());
    CefRefPtr<CefV8Exception> exception = func->GetException();
    EXPECT_TRUE(exception.get());
    EXPECT_STREQ(kExceptionMsg, exception->GetMessage().ToString().c_str());

    // Exit the V8 context.
    EXPECT_TRUE(context->Exit());

    DestroyTest();
  }

  void RunFunctionHandlerFailTest() {
    CefRefPtr<CefV8Context> context = GetContext();

    class Handler : public CefV8Handler {
     public:
      Handler() {}
      virtual bool Execute(const CefString& name,
                         CefRefPtr<CefV8Value> object,
                         const CefV8ValueList& arguments,
                         CefRefPtr<CefV8Value>& retval,
                         CefString& exception) OVERRIDE {
        got_execute_.yes();
        return false;
      }

      TrackCallback got_execute_;

      IMPLEMENT_REFCOUNTING(Handler);
    };

    // Enter the V8 context.
    EXPECT_TRUE(context->Enter());

    Handler* handler = new Handler;
    CefRefPtr<CefV8Handler> handlerPtr(handler);

    CefRefPtr<CefV8Value> func =
        CefV8Value::CreateFunction("myfunc", handler);
    EXPECT_TRUE(func.get());

    CefV8ValueList args;

    CefRefPtr<CefV8Value> retval = func->ExecuteFunction(NULL, args);
    EXPECT_TRUE(handler->got_execute_);
    EXPECT_TRUE(retval.get());
    EXPECT_FALSE(func->HasException());
    EXPECT_TRUE(retval->IsUndefined());

    // Exit the V8 context.
    EXPECT_TRUE(context->Exit());

    DestroyTest();
  }

  void RunFunctionHandlerNoObjectTest() {
    CefRefPtr<CefV8Context> context = GetContext();

    class Handler : public CefV8Handler {
     public:
      Handler() {}
      virtual bool Execute(const CefString& name,
                         CefRefPtr<CefV8Value> object,
                         const CefV8ValueList& arguments,
                         CefRefPtr<CefV8Value>& retval,
                         CefString& exception) OVERRIDE {
        EXPECT_TRUE(object.get());
        CefRefPtr<CefV8Context> context = CefV8Context::GetCurrentContext();
        EXPECT_TRUE(context.get());
        CefRefPtr<CefV8Value> global = context->GetGlobal();
        EXPECT_TRUE(global.get());
        EXPECT_TRUE(global->IsSame(object));

        got_execute_.yes();
        return true;
      }

      TrackCallback got_execute_;

      IMPLEMENT_REFCOUNTING(Handler);
    };

    // Enter the V8 context.
    EXPECT_TRUE(context->Enter());

    Handler* handler = new Handler;
    CefRefPtr<CefV8Handler> handlerPtr(handler);

    CefRefPtr<CefV8Value> func =
        CefV8Value::CreateFunction("myfunc", handler);
    EXPECT_TRUE(func.get());

    CefV8ValueList args;

    CefRefPtr<CefV8Value> retval = func->ExecuteFunction(NULL, args);
    EXPECT_TRUE(handler->got_execute_);
    EXPECT_TRUE(retval.get());
    EXPECT_FALSE(func->HasException());

    // Exit the V8 context.
    EXPECT_TRUE(context->Exit());

    DestroyTest();
  }

  void RunFunctionHandlerWithContextTest() {
    CefRefPtr<CefV8Context> context = GetContext();

    class Handler : public CefV8Handler {
     public:
      Handler() {}
      virtual bool Execute(const CefString& name,
                         CefRefPtr<CefV8Value> object,
                         const CefV8ValueList& arguments,
                         CefRefPtr<CefV8Value>& retval,
                         CefString& exception) OVERRIDE {
        CefRefPtr<CefV8Context> context = CefV8Context::GetCurrentContext();
        EXPECT_TRUE(context.get());
        EXPECT_TRUE(context->IsSame(context_));
        got_execute_.yes();
        return true;
      }

      CefRefPtr<CefV8Context> context_;
      TrackCallback got_execute_;

      IMPLEMENT_REFCOUNTING(Handler);
    };

    // Enter the V8 context.
    EXPECT_TRUE(context->Enter());

    Handler* handler = new Handler;
    CefRefPtr<CefV8Handler> handlerPtr(handler);
    handler->context_ = context;

    CefRefPtr<CefV8Value> func =
        CefV8Value::CreateFunction("myfunc", handler);
    EXPECT_TRUE(func.get());

    // Exit the V8 context.
    EXPECT_TRUE(context->Exit());

    CefV8ValueList args;

    CefRefPtr<CefV8Value> retval =
        func->ExecuteFunctionWithContext(context, NULL, args);
    EXPECT_TRUE(handler->got_execute_);
    EXPECT_TRUE(retval.get());
    EXPECT_FALSE(func->HasException());

    handler->context_ = NULL;

    DestroyTest();
  }

  void RunContextEvalTest() {
    CefRefPtr<CefV8Context> context = GetContext();

    CefRefPtr<CefV8Value> retval;
    CefRefPtr<CefV8Exception> exception;

    EXPECT_TRUE(context->Eval("1+2", retval, exception));
    EXPECT_TRUE(retval.get());
    EXPECT_TRUE(retval->IsInt());
    EXPECT_EQ(3, retval->GetIntValue());
    EXPECT_FALSE(exception.get());

    DestroyTest();
  }

  void RunContextEvalExceptionTest() {
    CefRefPtr<CefV8Context> context = GetContext();

    CefRefPtr<CefV8Value> retval;
    CefRefPtr<CefV8Exception> exception;

    EXPECT_FALSE(context->Eval("1+foo", retval, exception));
    EXPECT_FALSE(retval.get());
    EXPECT_TRUE(exception.get());

    DestroyTest();
  }

  void RunContextEnteredTest() {
    CefRefPtr<CefV8Context> context = GetContext();

    CefRefPtr<CefV8Value> retval;
    CefRefPtr<CefV8Exception> exception;

    // Test value defined in OnContextCreated
    EXPECT_TRUE(context->Eval(
        "document.getElementById('f').contentWindow.v8_context_entered_test()",
        retval, exception));
    if (exception.get())
      ADD_FAILURE() << exception->GetMessage().c_str();

    EXPECT_TRUE(retval.get());
    EXPECT_TRUE(retval->IsInt());
    EXPECT_EQ(21, retval->GetIntValue());

    DestroyTest();
  }

  void RunBindingTest() {
    CefRefPtr<CefV8Context> context = GetContext();

    // Enter the V8 context.
    EXPECT_TRUE(context->Enter());

    CefRefPtr<CefV8Value> object = context->GetGlobal();
    EXPECT_TRUE(object.get());

    // Test value defined in OnContextCreated
    CefRefPtr<CefV8Value> value = object->GetValue("v8_binding_test");
    EXPECT_TRUE(value.get());
    EXPECT_TRUE(value->IsInt());
    EXPECT_EQ(12, value->GetIntValue());

    // Exit the V8 context.
    EXPECT_TRUE(context->Exit());

    DestroyTest();
  }

  void RunStackTraceTest() {
    CefRefPtr<CefV8Context> context = GetContext();

    static const char* kFuncName = "myfunc";

    class Handler : public CefV8Handler {
     public:
      Handler() {}
      virtual bool Execute(const CefString& name,
                         CefRefPtr<CefV8Value> object,
                         const CefV8ValueList& arguments,
                         CefRefPtr<CefV8Value>& retval,
                         CefString& exception) OVERRIDE {
        EXPECT_STREQ(kFuncName, name.ToString().c_str());

        stack_trace_ = CefV8StackTrace::GetCurrent(10);

        retval = CefV8Value::CreateInt(3);
        got_execute_.yes();
        return true;
      }

      TrackCallback got_execute_;
      CefRefPtr<CefV8StackTrace> stack_trace_;

      IMPLEMENT_REFCOUNTING(Handler);
    };

    // Enter the V8 context.
    EXPECT_TRUE(context->Enter());

    Handler* handler = new Handler;
    CefRefPtr<CefV8Handler> handlerPtr(handler);

    CefRefPtr<CefV8Value> func =
        CefV8Value::CreateFunction(kFuncName, handler);
    EXPECT_TRUE(func.get());
    CefRefPtr<CefV8Value> obj = context->GetGlobal();
    EXPECT_TRUE(obj.get());
    obj->SetValue(kFuncName, func, V8_PROPERTY_ATTRIBUTE_NONE);

    CefRefPtr<CefV8Value> retval;
    CefRefPtr<CefV8Exception> exception;

    EXPECT_TRUE(context->Eval(
        "function jsfunc() { return window.myfunc(); }\n"
        "jsfunc();",
        retval, exception));
    EXPECT_TRUE(retval.get());
    EXPECT_TRUE(retval->IsInt());
    EXPECT_EQ(3, retval->GetIntValue());
    EXPECT_FALSE(exception.get());

    EXPECT_TRUE(handler->stack_trace_.get());
    EXPECT_EQ(2, handler->stack_trace_->GetFrameCount());

    CefRefPtr<CefV8StackFrame> frame;

    frame = handler->stack_trace_->GetFrame(0);
    EXPECT_TRUE(frame->GetScriptName().empty());
    EXPECT_TRUE(frame->GetScriptNameOrSourceURL().empty());
    EXPECT_STREQ("jsfunc", frame->GetFunctionName().ToString().c_str());
    EXPECT_EQ(1, frame->GetLineNumber());
    EXPECT_EQ(35, frame->GetColumn());
    EXPECT_TRUE(frame.get());
    EXPECT_TRUE(frame->IsEval());
    EXPECT_FALSE(frame->IsConstructor());

    frame = handler->stack_trace_->GetFrame(1);
    EXPECT_TRUE(frame->GetScriptName().empty());
    EXPECT_TRUE(frame->GetScriptNameOrSourceURL().empty());
    EXPECT_TRUE(frame->GetFunctionName().empty());
    EXPECT_EQ(2, frame->GetLineNumber());
    EXPECT_EQ(1, frame->GetColumn());
    EXPECT_TRUE(frame.get());
    EXPECT_TRUE(frame->IsEval());
    EXPECT_FALSE(frame->IsConstructor());

    // Exit the V8 context.
    EXPECT_TRUE(context->Exit());

    DestroyTest();
  }

  void RunOnUncaughtExceptionTest() {
    test_context_ =
        browser_->GetMainFrame()->GetV8Context();
    browser_->GetMainFrame()->ExecuteJavaScript(
        "window.setTimeout(test, 0)", "about:blank", 0);
  }

  void RunOnUncaughtExceptionDevToolsTest() {
    EXPECT_FALSE(browser_->IsPopup());
    test_context_ =
        browser_->GetMainFrame()->GetV8Context();
    // Show DevTools.
    EXPECT_FALSE(devtools_url_.empty());
    browser_->GetMainFrame()->ExecuteJavaScript(
        "window.open('" + devtools_url_ + "');", "about:blank", 0);
  }

  void OnUncaughtException(CefRefPtr<ClientApp> app,
                           CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefFrame> frame,
                           CefRefPtr<CefV8Context> context,
                           CefRefPtr<CefV8Exception> exception,
                           CefRefPtr<CefV8StackTrace> stackTrace) OVERRIDE {
    if (test_mode_ == V8TEST_ON_UNCAUGHT_EXCEPTION ||
        test_mode_ == V8TEST_ON_UNCAUGHT_EXCEPTION_DEV_TOOLS) {
      EXPECT_TRUE(test_context_->IsSame(context));
      EXPECT_STREQ("Uncaught ReferenceError: asd is not defined",
        exception->GetMessage().ToString().c_str());
      std::ostringstream stackFormatted;
      for (int i = 0; i < stackTrace->GetFrameCount(); ++i) {
        stackFormatted << "at "
            << stackTrace->GetFrame(i)->GetFunctionName().ToString()
            << "() in " << stackTrace->GetFrame(i)->GetScriptName().ToString()
            << " on line " << stackTrace->GetFrame(i)->GetLineNumber() << "\n";
      }
      const char* stackFormattedShouldBe =
          "at test2() in http://tests/V8Test.OnUncaughtException on line 3\n"
          "at test() in http://tests/V8Test.OnUncaughtException on line 2\n";
      EXPECT_STREQ(stackFormattedShouldBe, stackFormatted.str().c_str());
      DestroyTest();
    }
  }

  // Test execution of a native function when the extension is loaded.
  void RunExtensionTest() {
    std::string code = "native function v8_extension_test();"
                       "v8_extension_test();";

    class Handler : public CefV8Handler {
     public:
      Handler(TrackCallback* callback)
          : callback_(callback) {
      }

      virtual bool Execute(const CefString& name,
                           CefRefPtr<CefV8Value> object,
                           const CefV8ValueList& arguments,
                           CefRefPtr<CefV8Value>& retval,
                           CefString& exception) OVERRIDE {
        EXPECT_STREQ("v8_extension_test", name.ToString().c_str());
        callback_->yes();
        return true;
      }

      TrackCallback* callback_;

      IMPLEMENT_REFCOUNTING(Handler);
    };

    CefRegisterExtension("v8/test-extension", code,
                         new Handler(&startup_test_success_));
  }

  virtual void OnWebKitInitialized(CefRefPtr<ClientApp> app) {
    test_mode_ = g_current_test_mode;
    if (test_mode_ == V8TEST_NONE) {
      // Retrieve the test mode from the command line.
      CefRefPtr<CefCommandLine> command_line =
          CefCommandLine::GetGlobalCommandLine();
      CefString value = command_line->GetSwitchValue(kV8TestCmdArg);
      if (!value.empty())
        test_mode_ = static_cast<V8TestMode>(atoi(value.ToString().c_str()));
    }
    if (test_mode_ > V8TEST_NONE)
      RunStartupTest();
  }

  virtual void OnContextCreated(CefRefPtr<ClientApp> app,
                                CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefRefPtr<CefV8Context> context) OVERRIDE {
    if (test_mode_ == V8TEST_ON_UNCAUGHT_EXCEPTION_DEV_TOOLS) {
      if (browser_.get() == NULL) {
        app_ = app;
        browser_ = browser;
      }
      // The test is run from OnProcessMessageReceived(), after the message
      // and the devtools url has been received from OnLoadEnd().
      return;
    }

    app_ = app;
    browser_ = browser;

    std::string url = frame->GetURL();
    if (url == kV8ContextChildTestUrl) {
      // For V8TEST_CONTEXT_ENTERED
      class Handler : public CefV8Handler {
       public:
        Handler() {}
        virtual bool Execute(const CefString& name,
                           CefRefPtr<CefV8Value> object,
                           const CefV8ValueList& arguments,
                           CefRefPtr<CefV8Value>& retval,
                           CefString& exception) OVERRIDE {
          // context for the sub-frame
          CefRefPtr<CefV8Context> context = CefV8Context::GetCurrentContext();
          EXPECT_TRUE(context.get());

          // entered context should be the same as the main frame context
          CefRefPtr<CefV8Context> entered = CefV8Context::GetEnteredContext();
          EXPECT_TRUE(entered.get());
          EXPECT_TRUE(entered->IsSame(context_));

          context_ = NULL;
          retval = CefV8Value::CreateInt(21);
          return true;
        }

        CefRefPtr<CefV8Context> context_;
        IMPLEMENT_REFCOUNTING(Handler);
      };

      Handler* handler = new Handler;
      CefRefPtr<CefV8Handler> handlerPtr(handler);

      // main frame context
      handler->context_ = GetContext();

      // Function that will be called from the parent frame context.
      CefRefPtr<CefV8Value> func =
          CefV8Value::CreateFunction("v8_context_entered_test", handler);
      EXPECT_TRUE(func.get());

      CefRefPtr<CefV8Value> object = context->GetGlobal();
      EXPECT_TRUE(object.get());
      EXPECT_TRUE(object->SetValue("v8_context_entered_test", func,
          V8_PROPERTY_ATTRIBUTE_NONE));
    } else if (url == kV8ContextParentTestUrl) {
      // For V8TEST_CONTEXT_ENTERED. Do nothing.
      return;
    } else if (url == kV8BindingTestUrl) {
      // For V8TEST_BINDING
      CefRefPtr<CefV8Value> object = context->GetGlobal();
      EXPECT_TRUE(object.get());
      EXPECT_TRUE(object->SetValue("v8_binding_test",
          CefV8Value::CreateInt(12),
          V8_PROPERTY_ATTRIBUTE_NONE));
    }

    if (test_mode_ > V8TEST_NONE && url != kV8NavTestUrl) {
      // Run the test asynchronously.
      CefPostTask(TID_RENDERER,
                  NewCefRunnableMethod(this, &V8RendererTest::RunTest));
    }
  }

  virtual void OnContextReleased(CefRefPtr<ClientApp> app,
                                 CefRefPtr<CefBrowser> browser,
                                 CefRefPtr<CefFrame> frame,
                                 CefRefPtr<CefV8Context> context) OVERRIDE {
    if (test_mode_ == V8TEST_CONTEXT_INVALID &&
        frame->GetURL().ToString() != kV8NavTestUrl) {
      test_context_ =
          browser_->GetMainFrame()->GetV8Context();
      test_object_ = CefV8Value::CreateArray(10);
      CefPostTask(TID_RENDERER,
          NewCefRunnableMethod(this, &V8RendererTest::DestroyTest));
    }
  }

  virtual void OnBrowserDestroyed(CefRefPtr<ClientApp> app,
                                  CefRefPtr<CefBrowser> browser) OVERRIDE {
    if (test_mode_ == V8TEST_ON_UNCAUGHT_EXCEPTION_DEV_TOOLS) {
      if (browser->IsPopup()) {
        // After window destruction there is still a call to
        // ScriptController::setCaptureCallStackForUncaughtExceptions(0),
        // for which we have to wait.
        CefPostDelayedTask(TID_RENDERER, NewCefRunnableMethod(
            this, &V8RendererTest::DevToolsClosed), 1000);
      }
    }
  }

  virtual bool OnProcessMessageReceived(CefRefPtr<ClientApp> app,
                                        CefRefPtr<CefBrowser> browser,
                                        CefProcessId source_process,
                                        CefRefPtr<CefProcessMessage> message)
                                        OVERRIDE {
    if (test_mode_ == V8TEST_ON_UNCAUGHT_EXCEPTION_DEV_TOOLS) {
      EXPECT_TRUE(browser.get());
      EXPECT_EQ(PID_BROWSER, source_process);
      EXPECT_TRUE(message.get());
      EXPECT_TRUE(message->IsReadOnly());

      if (message->GetName() == kV8DevToolsURLMsg) {
        EXPECT_FALSE(browser->IsPopup());
        devtools_url_ = message->GetArgumentList()->GetString(0);
        EXPECT_FALSE(devtools_url_.empty());
        if (!TestFailed()) {
          CefPostTask(TID_RENDERER,
              NewCefRunnableMethod(this, &V8RendererTest::RunTest));
        }
      } else if (message->GetName() == kV8DevToolsLoadHookMsg) {
        EXPECT_TRUE(browser->IsPopup());
        DevToolsLoadHook(browser);
      } else {
        EXPECT_TRUE(false) << "not reached";
        return false;
      }
      return true;
    }
    return false;
  }

  void DevToolsLoadHook(CefRefPtr<CefBrowser> browser) {
    EXPECT_TRUE(browser->IsPopup());
    CefRefPtr<CefV8Context> context = browser->GetMainFrame()->GetV8Context();
    static const char* kFuncName = "DevToolsLoaded";

    class Handler : public CefV8Handler {
     public:
      Handler() {}
      virtual bool Execute(const CefString& name,
                           CefRefPtr<CefV8Value> object,
                           const CefV8ValueList& arguments,
                           CefRefPtr<CefV8Value>& retval,
                           CefString& exception) OVERRIDE {
          EXPECT_STREQ(kFuncName, name.ToString().c_str());
          if (name == kFuncName) {
            EXPECT_TRUE(exception.empty());
            retval = CefV8Value::CreateNull();
            EXPECT_TRUE(retval.get());
            renderer_test_->DevToolsLoaded(browser_);
            return true;
          }
          return false;
      }
      CefRefPtr<V8RendererTest> renderer_test_;
      CefRefPtr<CefBrowser> browser_;
      IMPLEMENT_REFCOUNTING(Handler);
    };

    EXPECT_TRUE(context->Enter());
    Handler* handler = new Handler;
    handler->renderer_test_ = this;
    handler->browser_ = browser;
    CefRefPtr<CefV8Handler> handlerPtr(handler);
    CefRefPtr<CefV8Value> func =
      CefV8Value::CreateFunction(kFuncName, handler);
    EXPECT_TRUE(func.get());
    EXPECT_TRUE(context->GetGlobal()->SetValue(
        kFuncName, func, V8_PROPERTY_ATTRIBUTE_NONE));
    EXPECT_TRUE(context->Exit());

    // Call DevToolsLoaded() when DevTools window completed loading.
    std::string jsCode = "(function(){"
        "  var oldLoadCompleted = InspectorFrontendAPI.loadCompleted;"
        "  if (InspectorFrontendAPI._isLoaded) {"
        "      window.DevToolsLoaded();"
        "  } else {"
        "    InspectorFrontendAPI.loadCompleted = function(){"
        "      oldLoadCompleted.call(InspectorFrontendAPI);"
        "      window.DevToolsLoaded();"
        "    };"
        "  }"
        "})();";

    CefRefPtr<CefV8Value> retval;
    CefRefPtr<CefV8Exception> exception;
    EXPECT_TRUE(context->Eval(CefString(jsCode), retval, exception));
  }

  void DevToolsLoaded(CefRefPtr<CefBrowser> browser) {
    EXPECT_TRUE(browser->IsPopup());
    EXPECT_NE(browser->GetIdentifier(), browser_->GetIdentifier());
    CefRefPtr<CefV8Value> retval;
    CefRefPtr<CefV8Exception> exception;
    EXPECT_TRUE(browser->GetMainFrame()->GetV8Context()->Eval(
        "window.close()", retval, exception));
  }

  void DevToolsClosed() {
    browser_->GetMainFrame()->ExecuteJavaScript(
      "window.setTimeout(test, 0);", "about:blank", 0);
  }

 protected:
  // Return from the test.
  void DestroyTest() {
    if (test_mode_ == V8TEST_CONTEXT_INVALID) {
      // Verify that objects related to a particular context are not valid after
      // OnContextReleased is called for that context.
      EXPECT_FALSE(test_context_->IsValid());
      EXPECT_FALSE(test_object_->IsValid());
    }

    // Check if the test has failed.
    bool result = !TestFailed();

    // Return the result to the browser process.
    CefRefPtr<CefProcessMessage> return_msg =
        CefProcessMessage::Create(kV8TestMsg);
    EXPECT_TRUE(return_msg->GetArgumentList()->SetBool(0, result));
    EXPECT_TRUE(browser_->SendProcessMessage(PID_BROWSER, return_msg));

    app_ = NULL;
    browser_ = NULL;
    test_context_ = NULL;
    test_object_ = NULL;
    devtools_url_ = "";
  }

  // Return the V8 context.
  CefRefPtr<CefV8Context> GetContext() {
    CefRefPtr<CefV8Context> context = browser_->GetMainFrame()->GetV8Context();
    EXPECT_TRUE(context.get());
    return context;
  }

  CefRefPtr<ClientApp> app_;
  CefRefPtr<CefBrowser> browser_;
  V8TestMode test_mode_;
  std::string devtools_url_;

  CefRefPtr<CefV8Context> test_context_;
  CefRefPtr<CefV8Value> test_object_;

  // Used by startup tests to indicate success.
  TrackCallback startup_test_success_;

  IMPLEMENT_REFCOUNTING(V8RendererTest);
};

// Browser side.
class V8TestHandler : public TestHandler {
 public:
  V8TestHandler(V8TestMode test_mode, const char* test_url)
    : test_mode_(test_mode),
      test_url_(test_url) {
  }

  virtual void RunTest() OVERRIDE {
    // Nested script tag forces creation of the V8 context.
    if (test_mode_ == V8TEST_CONTEXT_ENTERED) {
      AddResource(kV8ContextParentTestUrl, "<html><body>"
          "<script>var i = 0;</script><iframe src=\"" +
          std::string(kV8ContextChildTestUrl) + "\" id=\"f\"></iframe></body>"
          "</html>", "text/html");
      AddResource(kV8ContextChildTestUrl, "<html><body>"
          "<script>var i = 0;</script>CHILD</body></html>",
          "text/html");
      CreateBrowser(kV8ContextParentTestUrl);
    } else if (test_mode_ == V8TEST_ON_UNCAUGHT_EXCEPTION ||
               test_mode_ == V8TEST_ON_UNCAUGHT_EXCEPTION_DEV_TOOLS) {
      AddResource(kV8OnUncaughtExceptionTestUrl, "<html><body>"
          "<h1>OnUncaughtException</h1>"
          "<script>\n"
          "function test(){ test2(); }\n"
          "function test2(){ asd(); }\n"
          "</script>\n"
          "</body></html>\n",
          "text/html");
      CreateBrowser(kV8OnUncaughtExceptionTestUrl);
    } else {
      EXPECT_TRUE(test_url_ != NULL);
      AddResource(test_url_, "<html><body>"
          "<script>var i = 0;</script>TEST</body></html>", "text/html");
      CreateBrowser(test_url_);
    }
  }

  virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         int httpStatusCode) OVERRIDE {
    if (test_mode_ == V8TEST_ON_UNCAUGHT_EXCEPTION_DEV_TOOLS) {
      if (browser->IsPopup()) {
        EXPECT_STREQ(
            GetBrowser()->GetHost()->GetDevToolsURL(true).ToString().c_str(),
            frame->GetURL().ToString().c_str());
        CefRefPtr<CefProcessMessage> return_msg =
            CefProcessMessage::Create(kV8DevToolsLoadHookMsg);
        EXPECT_TRUE(browser->SendProcessMessage(PID_RENDERER, return_msg));
      } else {
        // Send the DevTools url message only for the main browser.
        CefRefPtr<CefProcessMessage> return_msg =
            CefProcessMessage::Create(kV8DevToolsURLMsg);
        EXPECT_TRUE(return_msg->GetArgumentList()->SetString(0,
            browser->GetHost()->GetDevToolsURL(true)));
        EXPECT_TRUE(browser->SendProcessMessage(PID_RENDERER, return_msg));
      }
    }
  }

  virtual bool OnProcessMessageReceived(
      CefRefPtr<CefBrowser> browser,
      CefProcessId source_process,
      CefRefPtr<CefProcessMessage> message) OVERRIDE {
    EXPECT_TRUE(browser.get());
    EXPECT_EQ(PID_RENDERER, source_process);
    EXPECT_TRUE(message.get());
    EXPECT_TRUE(message->IsReadOnly());

    got_message_.yes();

    if (message->GetArgumentList()->GetBool(0))
      got_success_.yes();

    // Test is complete.
    DestroyTest();

    return true;
  }

  V8TestMode test_mode_;
  const char* test_url_;
  TrackCallback got_message_;
  TrackCallback got_success_;
};

}  // namespace


// Entry point for creating V8 browser test objects.
// Called from client_app_delegates.cc.
void CreateV8BrowserTests(ClientApp::BrowserDelegateSet& delegates) {
  delegates.insert(new V8BrowserTest);
}

// Entry point for creating V8 renderer test objects.
// Called from client_app_delegates.cc.
void CreateV8RendererTests(ClientApp::RenderDelegateSet& delegates) {
  delegates.insert(new V8RendererTest);
}


// Helpers for defining V8 tests.
#define V8_TEST_EX(name, test_mode, test_url) \
  TEST(V8Test, name) { \
    g_current_test_mode = test_mode; \
    CefRefPtr<V8TestHandler> handler = \
        new V8TestHandler(test_mode, test_url); \
    handler->ExecuteTest(); \
    EXPECT_TRUE(handler->got_message_); \
    EXPECT_TRUE(handler->got_success_); \
    g_current_test_mode = V8TEST_NONE; \
  }

#define V8_TEST(name, test_mode) \
  V8_TEST_EX(name, test_mode, kV8TestUrl)


// Define the tests.
V8_TEST(NullCreate, V8TEST_NULL_CREATE);
V8_TEST(BoolCreate, V8TEST_BOOL_CREATE);
V8_TEST(IntCreate, V8TEST_INT_CREATE);
V8_TEST(UIntCreate, V8TEST_UINT_CREATE);
V8_TEST(DoubleCreate, V8TEST_DOUBLE_CREATE);
V8_TEST(DateCreate, V8TEST_DATE_CREATE);
V8_TEST(StringCreate, V8TEST_STRING_CREATE);
V8_TEST(ArrayCreate, V8TEST_ARRAY_CREATE);
V8_TEST(ArrayValue, V8TEST_ARRAY_VALUE);
V8_TEST(ObjectCreate, V8TEST_OBJECT_CREATE);
V8_TEST(ObjectUserData, V8TEST_OBJECT_USERDATA);
V8_TEST(ObjectAccessor, V8TEST_OBJECT_ACCESSOR);
V8_TEST(ObjectAccessorException, V8TEST_OBJECT_ACCESSOR_EXCEPTION);
V8_TEST(ObjectAccessorFail, V8TEST_OBJECT_ACCESSOR_FAIL);
V8_TEST(ObjectAccessorReadOnly, V8TEST_OBJECT_ACCESSOR_READONLY);
V8_TEST(ObjectValue, V8TEST_OBJECT_VALUE);
V8_TEST(ObjectValueReadOnly, V8TEST_OBJECT_VALUE_READONLY);
V8_TEST(ObjectValueEnum, V8TEST_OBJECT_VALUE_ENUM);
V8_TEST(ObjectValueDontEnum, V8TEST_OBJECT_VALUE_DONTENUM);
V8_TEST(ObjectValueDelete, V8TEST_OBJECT_VALUE_DELETE);
V8_TEST(ObjectValueDontDelete, V8TEST_OBJECT_VALUE_DONTDELETE);
V8_TEST(ObjectValueEmptyKey, V8TEST_OBJECT_VALUE_EMPTYKEY);
V8_TEST(FunctionCreate, V8TEST_FUNCTION_CREATE);
V8_TEST(FunctionHandler, V8TEST_FUNCTION_HANDLER);
V8_TEST(FunctionHandlerException, V8TEST_FUNCTION_HANDLER_EXCEPTION);
V8_TEST(FunctionHandlerFail, V8TEST_FUNCTION_HANDLER_FAIL);
V8_TEST(FunctionHandlerNoObject, V8TEST_FUNCTION_HANDLER_NO_OBJECT);
V8_TEST(FunctionHandlerWithContext, V8TEST_FUNCTION_HANDLER_WITH_CONTEXT);
V8_TEST(ContextEval, V8TEST_CONTEXT_EVAL);
V8_TEST(ContextEvalException, V8TEST_CONTEXT_EVAL_EXCEPTION);
V8_TEST_EX(ContextEntered, V8TEST_CONTEXT_ENTERED, NULL);
V8_TEST(ContextInvalid, V8TEST_CONTEXT_INVALID);
V8_TEST_EX(Binding, V8TEST_BINDING, kV8BindingTestUrl);
V8_TEST(StackTrace, V8TEST_STACK_TRACE);
V8_TEST(OnUncaughtException, V8TEST_ON_UNCAUGHT_EXCEPTION);
V8_TEST(OnUncaughtExceptionDevTools, V8TEST_ON_UNCAUGHT_EXCEPTION_DEV_TOOLS);
V8_TEST(Extension, V8TEST_EXTENSION);
