// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.
//
// NOTE: DO NOT ADD NEW TESTS TO THIS FILE. USE v8_unittest.cc INSTEAD.
// http://code.google.com/p/chromiumembedded/issues/detail?id=480

#include "include/cef_runnable.h"
#include "include/cef_v8.h"
#include "tests/unittests/test_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

bool g_V8TestV8HandlerExecuteCalled;
bool g_V8TestV8HandlerExecute2Called;

class V8TestV8Handler : public CefV8Handler {
 public:
  explicit V8TestV8Handler(bool bindingTest) { binding_test_ = bindingTest; }

  virtual bool Execute(const CefString& name,
                       CefRefPtr<CefV8Value> object,
                       const CefV8ValueList& arguments,
                       CefRefPtr<CefV8Value>& retval,
                       CefString& exception) {
    TestExecute(name, object, arguments, retval, exception);
    return true;
  }

  void TestExecute(const CefString& name,
                   CefRefPtr<CefV8Value> object,
                   const CefV8ValueList& arguments,
                   CefRefPtr<CefV8Value>& retval,
                   CefString& exception) {
    if (name == "execute") {
      g_V8TestV8HandlerExecuteCalled = true;

      ASSERT_EQ((size_t)9, arguments.size());
      int argct = 0;

      // basic types
      ASSERT_TRUE(arguments[argct]->IsInt());
      ASSERT_EQ(5, arguments[argct]->GetIntValue());
      argct++;

      ASSERT_TRUE(arguments[argct]->IsDouble());
      ASSERT_EQ(6.543, arguments[argct]->GetDoubleValue());
      argct++;

      ASSERT_TRUE(arguments[argct]->IsBool());
      ASSERT_EQ(true, arguments[argct]->GetBoolValue());
      argct++;

      ASSERT_TRUE(arguments[argct]->IsDate());
      CefTime date = arguments[argct]->GetDateValue();
      ASSERT_EQ(date.year, 2010);
      ASSERT_EQ(date.month, 5);
      ASSERT_EQ(date.day_of_month, 3);
#if !defined(OS_MACOSX)
      ASSERT_EQ(date.day_of_week, 1);
#endif
      ASSERT_EQ(date.hour, 12);
      ASSERT_EQ(date.minute, 30);
      ASSERT_EQ(date.second, 10);
      ASSERT_NEAR(date.millisecond, 100, 1);
      argct++;

      ASSERT_TRUE(arguments[argct]->IsString());
      ASSERT_EQ(arguments[argct]->GetStringValue(), "test string");
      argct++;

      CefRefPtr<CefV8Value> value;

      // array
      ASSERT_TRUE(arguments[argct]->IsArray());
      ASSERT_EQ(4, arguments[argct]->GetArrayLength());
      {
        int subargct = 0;
        value = arguments[argct]->GetValue(subargct);
        ASSERT_TRUE(value.get() != NULL);
        ASSERT_TRUE(value->IsInt());
        ASSERT_EQ(7, value->GetIntValue());
        subargct++;

        value = arguments[argct]->GetValue(subargct);
        ASSERT_TRUE(value.get() != NULL);
        ASSERT_TRUE(value->IsDouble());
        ASSERT_EQ(5.432, value->GetDoubleValue());
        subargct++;

        value = arguments[argct]->GetValue(subargct);
        ASSERT_TRUE(value.get() != NULL);
        ASSERT_TRUE(value->IsBool());
        ASSERT_FALSE(value->GetBoolValue());
        subargct++;

        value = arguments[argct]->GetValue(subargct);
        ASSERT_TRUE(value.get() != NULL);
        ASSERT_TRUE(value->IsString());
        ASSERT_EQ(value->GetStringValue(), "another string");
        subargct++;
      }
      argct++;

      // object
      ASSERT_TRUE(arguments[argct]->IsObject());
      {
        value = arguments[argct]->GetValue("arg0");
        ASSERT_TRUE(value.get() != NULL);
        ASSERT_TRUE(value->IsInt());
        ASSERT_EQ(2, value->GetIntValue());

        value = arguments[argct]->GetValue("arg1");
        ASSERT_TRUE(value.get() != NULL);
        ASSERT_TRUE(value->IsDouble());
        ASSERT_EQ(3.433, value->GetDoubleValue());

        value = arguments[argct]->GetValue("arg2");
        ASSERT_TRUE(value.get() != NULL);
        ASSERT_TRUE(value->IsBool());
        ASSERT_EQ(true, value->GetBoolValue());

        value = arguments[argct]->GetValue("arg3");
        ASSERT_TRUE(value.get() != NULL);
        ASSERT_TRUE(value->IsString());
        ASSERT_EQ(value->GetStringValue(), "some string");
      }
      argct++;

      // function that returns a value
      ASSERT_TRUE(arguments[argct]->IsFunction());
      {
        CefV8ValueList args;
        args.push_back(CefV8Value::CreateInt(5));
        args.push_back(CefV8Value::CreateDouble(3.5));
        args.push_back(CefV8Value::CreateBool(true));
        args.push_back(CefV8Value::CreateString("10"));
        CefRefPtr<CefV8Value> rv;
        CefRefPtr<CefV8Exception> exception;
        ASSERT_TRUE(arguments[argct]->ExecuteFunction(
            arguments[argct], args, rv, exception, false));
        ASSERT_TRUE(rv.get() != NULL);
        ASSERT_TRUE(rv->IsDouble());
        ASSERT_EQ(19.5, rv->GetDoubleValue());
      }
      argct++;

      // function that throws an exception
      ASSERT_TRUE(arguments[argct]->IsFunction());
      {
        CefV8ValueList args;
        args.push_back(CefV8Value::CreateDouble(5));
        args.push_back(CefV8Value::CreateDouble(0));
        CefRefPtr<CefV8Value> rv;
        CefRefPtr<CefV8Exception> exception;
        ASSERT_TRUE(arguments[argct]->ExecuteFunction(
            arguments[argct], args, rv, exception, false));
        ASSERT_TRUE(exception.get());
        ASSERT_EQ(exception->GetMessage(), "Uncaught My Exception");
      }
      argct++;

      if (binding_test_) {
        // values
        value = object->GetValue("intVal");
        ASSERT_TRUE(value.get() != NULL);
        ASSERT_TRUE(value->IsInt());
        ASSERT_EQ(12, value->GetIntValue());

        value = object->GetValue("doubleVal");
        ASSERT_TRUE(value.get() != NULL);
        ASSERT_TRUE(value->IsDouble());
        ASSERT_EQ(5.432, value->GetDoubleValue());

        value = object->GetValue("boolVal");
        ASSERT_TRUE(value.get() != NULL);
        ASSERT_TRUE(value->IsBool());
        ASSERT_EQ(true, value->GetBoolValue());

        value = object->GetValue("stringVal");
        ASSERT_TRUE(value.get() != NULL);
        ASSERT_TRUE(value->IsString());
        ASSERT_EQ(value->GetStringValue(), "the string");

        value = object->GetValue("dateVal");
        ASSERT_TRUE(value.get() != NULL);
        ASSERT_TRUE(value->IsDate());
        CefTime date = value->GetDateValue();
        ASSERT_EQ(date.year, 2010);
        ASSERT_EQ(date.month, 5);
#if !defined(OS_MACOSX)
        ASSERT_EQ(date.day_of_week, 1);
#endif
        ASSERT_EQ(date.day_of_month, 3);
        ASSERT_EQ(date.hour, 12);
        ASSERT_EQ(date.minute, 30);
        ASSERT_EQ(date.second, 10);
        ASSERT_NEAR(date.millisecond, 100, 1);

        value = object->GetValue("arrayVal");
        ASSERT_TRUE(value.get() != NULL);
        ASSERT_TRUE(value->IsArray());
        {
          CefRefPtr<CefV8Value> value2;
          int subargct = 0;
          value2 = value->GetValue(subargct);
          ASSERT_TRUE(value2.get() != NULL);
          ASSERT_TRUE(value2->IsInt());
          ASSERT_EQ(4, value2->GetIntValue());
          subargct++;

          value2 = value->GetValue(subargct);
          ASSERT_TRUE(value2.get() != NULL);
          ASSERT_TRUE(value2->IsDouble());
          ASSERT_EQ(120.43, value2->GetDoubleValue());
          subargct++;

          value2 = value->GetValue(subargct);
          ASSERT_TRUE(value2.get() != NULL);
          ASSERT_TRUE(value2->IsBool());
          ASSERT_EQ(true, value2->GetBoolValue());
          subargct++;

          value2 = value->GetValue(subargct);
          ASSERT_TRUE(value2.get() != NULL);
          ASSERT_TRUE(value2->IsString());
          ASSERT_EQ(value2->GetStringValue(), "a string");
          subargct++;
        }
      }

      retval = CefV8Value::CreateInt(5);
    } else if (name == "execute2") {
      g_V8TestV8HandlerExecute2Called = true;

      // check the result of calling the "execute" function
      ASSERT_EQ((size_t)1, arguments.size());
      ASSERT_TRUE(arguments[0]->IsInt());
      ASSERT_EQ(5, arguments[0]->GetIntValue());
    }
  }

  bool binding_test_;

  IMPLEMENT_REFCOUNTING(V8TestV8Handler);
};

class V8TestHandler : public TestHandler {
 public:
  explicit V8TestHandler(bool bindingTest) { binding_test_ = bindingTest; }

  virtual void RunTest() OVERRIDE {
    std::string object;
    if (binding_test_) {
      // binding uses the window object
      object = "window.test";
    } else {
      // extension uses a global object
      object = "test";
    }

    std::stringstream testHtml;
    testHtml <<
      "<html><body>"
      "<script language=\"JavaScript\">"
      "function func(a,b,c,d) { return a+b+(c?1:0)+parseFloat(d); }"
      "function func2(a,b) { throw('My Exception'); }"
      << object << ".execute2("
      "  " << object << ".execute(5, 6.543, true,"
      "    new Date(Date.UTC(2010, 4, 3, 12, 30, 10, 100)), \"test string\","
      "    [7, 5.432, false, \"another string\"],"
      "    {arg0:2, arg1:3.433, arg2:true, arg3:\"some string\"}, func, func2)"
      ");"
      "</script>"
      "</body></html>";

    AddResource("http://tests/run.html", testHtml.str(), "text/html");
    CreateBrowser("http://tests/run.html");
  }

  virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         int httpStatusCode) OVERRIDE {
    if (!browser->IsPopup() && frame->IsMain())
      DestroyTest();
  }

  virtual void OnContextCreated(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefRefPtr<CefV8Context> context) OVERRIDE {
    if (binding_test_)
      TestHandleJSBinding(browser, frame, context->GetGlobal());
  }

  void TestHandleJSBinding(CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefFrame> frame,
                           CefRefPtr<CefV8Value> object) {
    // Create the new V8 object
    CefRefPtr<CefV8Value> testObj = CefV8Value::CreateObject(NULL, NULL);
    ASSERT_TRUE(testObj.get() != NULL);
    ASSERT_TRUE(object->SetValue("test", testObj, V8_PROPERTY_ATTRIBUTE_NONE));

    // Create an instance of V8ExecuteV8Handler
    CefRefPtr<CefV8Handler> testHandler(new V8TestV8Handler(true));
    ASSERT_TRUE(testHandler.get() != NULL);

    // Add the functions
    CefRefPtr<CefV8Value> testFunc;
    testFunc = CefV8Value::CreateFunction("execute", testHandler);
    ASSERT_TRUE(testFunc.get() != NULL);
    ASSERT_TRUE(testObj->SetValue("execute", testFunc,
        V8_PROPERTY_ATTRIBUTE_NONE));
    testFunc = CefV8Value::CreateFunction("execute2", testHandler);
    ASSERT_TRUE(testFunc.get() != NULL);
    ASSERT_TRUE(testObj->SetValue("execute2", testFunc,
        V8_PROPERTY_ATTRIBUTE_NONE));

    // Add the values
    ASSERT_TRUE(testObj->SetValue("intVal",
        CefV8Value::CreateInt(12), V8_PROPERTY_ATTRIBUTE_NONE));
    ASSERT_TRUE(testObj->SetValue("doubleVal",
        CefV8Value::CreateDouble(5.432), V8_PROPERTY_ATTRIBUTE_NONE));
    ASSERT_TRUE(testObj->SetValue("boolVal",
        CefV8Value::CreateBool(true), V8_PROPERTY_ATTRIBUTE_NONE));
    ASSERT_TRUE(testObj->SetValue("stringVal",
        CefV8Value::CreateString("the string"), V8_PROPERTY_ATTRIBUTE_NONE));

    cef_time_t date = {
        2010,
        5,
#if !defined(OS_MACOSX)
        1,
#endif
        3,
        12,
        30,
        10,
        100
    };
    ASSERT_TRUE(testObj->SetValue("dateVal", CefV8Value::CreateDate(date),
        V8_PROPERTY_ATTRIBUTE_NONE));

    CefRefPtr<CefV8Value> testArray(CefV8Value::CreateArray());
    ASSERT_TRUE(testArray.get() != NULL);
    ASSERT_TRUE(testObj->SetValue("arrayVal", testArray,
        V8_PROPERTY_ATTRIBUTE_NONE));
    ASSERT_TRUE(testArray->SetValue(0, CefV8Value::CreateInt(4)));
    ASSERT_TRUE(testArray->SetValue(1, CefV8Value::CreateDouble(120.43)));
    ASSERT_TRUE(testArray->SetValue(2, CefV8Value::CreateBool(true)));
    ASSERT_TRUE(testArray->SetValue(3, CefV8Value::CreateString("a string")));
  }

  bool binding_test_;
};

}  // namespace

// Verify window binding
TEST(V8Test, Binding) {
  g_V8TestV8HandlerExecuteCalled = false;
  g_V8TestV8HandlerExecute2Called = false;

  CefRefPtr<V8TestHandler> handler = new V8TestHandler(true);
  handler->ExecuteTest();

  ASSERT_TRUE(g_V8TestV8HandlerExecuteCalled);
  ASSERT_TRUE(g_V8TestV8HandlerExecute2Called);
}

// Verify extensions
TEST(V8Test, Extension) {
  g_V8TestV8HandlerExecuteCalled = false;
  g_V8TestV8HandlerExecute2Called = false;

  std::string extensionCode =
    "var test;"
    "if (!test)"
    "  test = {};"
    "(function() {"
    "  test.execute = function(a,b,c,d,e,f,g,h,i) {"
    "    native function execute();"
    "    return execute(a,b,c,d,e,f,g,h,i);"
    "  };"
    "  test.execute2 = function(a) {"
    "    native function execute2();"
    "    return execute2(a);"
    "  };"
    "})();";
  CefRegisterExtension("v8/test", extensionCode, new V8TestV8Handler(false));

  CefRefPtr<V8TestHandler> handler = new V8TestHandler(false);
  handler->ExecuteTest();

  ASSERT_TRUE(g_V8TestV8HandlerExecuteCalled);
  ASSERT_TRUE(g_V8TestV8HandlerExecute2Called);
}

namespace {

class TestNoNativeHandler : public TestHandler {
 public:
  class TestHandler : public CefV8Handler {
  public:
    explicit TestHandler(CefRefPtr<TestNoNativeHandler> test)
      : test_(test) {
    }

    virtual bool Execute(const CefString& name,
                         CefRefPtr<CefV8Value> object,
                         const CefV8ValueList& arguments,
                         CefRefPtr<CefV8Value>& retval,
                         CefString& exception) OVERRIDE {
      if (name == "result") {
        if (arguments.size() == 1 && arguments[0]->IsString()) {
          std::string value = arguments[0]->GetStringValue();
          if (value == "correct")
            test_->got_correct_.yes();
          else
            return false;
          return true;
        }
      }

      return false;
    }

    CefRefPtr<TestNoNativeHandler> test_;

    IMPLEMENT_REFCOUNTING(TestHandler);
  };

  TestNoNativeHandler() {
  }

  virtual void RunTest() OVERRIDE {
    std::string testHtml =
        "<html><body>\n"
        "<script language=\"JavaScript\">\n"
        "var result = test_nonative.add(1, 2);\n"
        "if (result == 3)\n"
        "  window.test.result('correct');\n"
        "</script>\n"
        "</body></html>";
    AddResource("http://tests/run.html", testHtml, "text/html");

    CreateBrowser("http://tests/run.html");
  }

  virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         int httpStatusCode) OVERRIDE {
    DestroyTest();
  }

  virtual void OnContextCreated(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefRefPtr<CefV8Context> context) OVERRIDE {
    // Retrieve the 'window' object.
    CefRefPtr<CefV8Value> object = context->GetGlobal();

    // Create the functions that will be used during the test.
    CefRefPtr<CefV8Value> obj = CefV8Value::CreateObject(NULL, NULL);
    CefRefPtr<CefV8Handler> handler = new TestHandler(this);
    obj->SetValue("result",
                  CefV8Value::CreateFunction("result", handler),
                  V8_PROPERTY_ATTRIBUTE_NONE);
    object->SetValue("test", obj, V8_PROPERTY_ATTRIBUTE_NONE);
  }

  TrackCallback got_correct_;
};

}  // namespace

// Verify extensions with no native functions
TEST(V8Test, ExtensionNoNative) {
  std::string extensionCode =
    "var test_nonative;"
    "if (!test_nonative)"
    "  test_nonative = {};"
    "(function() {"
    "  test_nonative.add = function(a, b) {"
    "    return a + b;"
    "  };"
    "})();";
  CefRegisterExtension("v8/test_nonative", extensionCode, NULL);

  CefRefPtr<TestNoNativeHandler> handler = new TestNoNativeHandler();
  handler->ExecuteTest();

  EXPECT_TRUE(handler->got_correct_);
}

namespace {

// Using a delegate so that the code below can remain inline.
class CefV8HandlerDelegate {
 public:
  virtual ~CefV8HandlerDelegate() {}
  virtual bool Execute(const CefString& name,
                       CefRefPtr<CefV8Value> object,
                       const CefV8ValueList& arguments,
                       CefRefPtr<CefV8Value>& retval,
                       CefString& exception) = 0;

  virtual bool Get(const CefString& name,
                   const CefRefPtr<CefV8Value> object,
                   CefRefPtr<CefV8Value>& retval,
                   CefString& exception) = 0;

  virtual bool Set(const CefString& name,
                   const CefRefPtr<CefV8Value> object,
                   const CefRefPtr<CefV8Value> value,
                   CefString& exception) = 0;
};

class DelegatingV8Handler : public CefV8Handler {
 public:
  explicit DelegatingV8Handler(CefV8HandlerDelegate *delegate)
    : delegate_(delegate) {}

  ~DelegatingV8Handler() {
  }

  bool Execute(const CefString& name,
               CefRefPtr<CefV8Value> object,
               const CefV8ValueList& arguments,
               CefRefPtr<CefV8Value>& retval,
               CefString& exception) OVERRIDE {
    return delegate_->Execute(name, object, arguments, retval, exception);
  }

 private:
  CefV8HandlerDelegate* delegate_;

  IMPLEMENT_REFCOUNTING(DelegatingV8Handler);
};

class DelegatingV8Accessor: public CefV8Accessor {
 public:
  explicit DelegatingV8Accessor(CefV8HandlerDelegate *delegate)
    : delegate_(delegate) { }

  bool Get(const CefString& name,
           const CefRefPtr<CefV8Value> object,
           CefRefPtr<CefV8Value>& retval,
           CefString& exception) OVERRIDE {
    return delegate_->Get(name, object, retval, exception);
  }

  bool Set(const CefString& name,
           const CefRefPtr<CefV8Value> object,
           const CefRefPtr<CefV8Value> value,
           CefString& exception) OVERRIDE {
    return delegate_->Set(name, object, value, exception);
  }

 private:
  CefV8HandlerDelegate* delegate_;

  IMPLEMENT_REFCOUNTING(DelegatingV8Accessor);
};

class TestContextHandler: public TestHandler, public CefV8HandlerDelegate {
 public:
  TestContextHandler() {}

  virtual void RunTest() OVERRIDE {
    // Test Flow:
    // load main.html.
    // 1. main.html calls hello("main", callIFrame) in the execute handler.
    //    The excute handler checks that "main" was called and saves
    //    the callIFrame function, context, and receiver object.
    // 2. iframe.html calls hello("iframe") in the execute handler.
    //    The execute handler checks that "iframe" was called. if both main
    //    and iframe were called, it calls CallIFrame()
    // 3. CallIFrame calls "callIFrame" in main.html
    // 4. which calls iframe.html "calledFromMain()".
    // 5. which calls "fromIFrame()" in execute handler.
    //    The execute handler checks that the entered and current urls are
    //    what we expect: "main.html" and "iframe.html", respectively
    // 6. It then posts a task to call AsyncTestContext
    //      you can validate the entered and current context are still the
    //      same here, but it is not checked by this test case.
    // 7. AsyncTestContext tests to make sure that no context is set at
    //    this point and loads "begin.html"
    // 8. begin.html calls "begin(func1, func2)" in the execute handler
    //    The execute handler posts a tasks to call both of those functions
    //    when no context is defined. Both should work with the specified
    //    context. AsyncTestException should run first, followed by
    //    AsyncTestNavigate() which calls the func2 to do a document.location
    //    based loading of "end.html".
    // 9. end.html calls "end()" in the execute handler.
    //    which concludes the test.

    y_ = 0;

    std::stringstream mainHtml;
    mainHtml <<
    "<html><body>"
    "<h1>Hello From Main Frame</h1>"
    "<script language=\"JavaScript\">"
    "aaa = function(){}; bbb = function(a){ a=1; };"
    "comp(false,{},{});\n"
    "comp(true,aaa,aaa);\n"
    "comp(true,bbb,bbb);\n"
    "comp(false,aaa,bbb);\n"
    "comp(false,{},bbb);\n"
    "comp(false,{},bbb);\n"
    "comp(true,0,0);\n"
    "comp(true,\"a\",\"a\");\n"
    "comp(false,\"a\",\"b\");\n"
    "try { point.x = -1; } catch(e) {  }\n"  // should not have any effect.
    "try { point.y = point.x;  theY = point.y; } catch(e) { point.y = 4321; }\n"
    // Test get and set exceptions.
    "try { exceptObj.makeException = 1; }"
    " catch(e) { gotSetException(e.toString()); }\n"
    "try { var x = exceptObj.makeException; }"
    " catch(e) { gotGetException(e.toString()); }\n"
    "hello(\"main\", callIFrame);"
    "function callIFrame() {"
    " var iframe = document.getElementById('iframe');"
    " iframe.contentWindow.calledFromMain();"
    "}"
    "</script>"
    "<iframe id=\"iframe\" src=\"http://tests/iframe.html\""
    " width=\"300\" height=\"300\">"
    "</iframe>"
    "</body></html>";

    AddResource("http://tests/main.html", mainHtml.str(), "text/html");

    std::stringstream iframeHtml;
    iframeHtml <<
    "<html><body>"
    "<h1>Hello From IFRAME</h1>"
    "<script language=\"JavaScript\">"
    "hello(\"iframe\");"
    "function calledFromMain() { fromIFrame(); }"
    "</script>"
    "</body></html>";

    AddResource("http://tests/iframe.html", iframeHtml.str(), "text/html");

    std::stringstream beginHtml;
    beginHtml <<
    "<html><body>"
    "<h1>V8 Context Test</h1>"
    "<script language=\"JavaScript\">"
    "function TestException() { throw('My Exception'); }"
    "function TestNavigate(a) { document.location = a.url; }"
    "begin(TestException, TestNavigate);"
    "</script>"
    "</body></html>";

    AddResource("http://tests/begin.html", beginHtml.str(), "text/html");

    std::stringstream endHtml;
    endHtml <<
    "<html><body>"
    "<h1>Navigation Succeeded!</h1>"
    "<script language=\"JavaScript\">"
    "end();"
    "</script>"
    "</body></html>";

    AddResource("http://tests/end.html", endHtml.str(), "text/html");

    CreateBrowser("http://tests/main.html");
  }

  virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         int httpStatusCode) OVERRIDE {
  }

  virtual void OnContextCreated(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefRefPtr<CefV8Context> context) OVERRIDE {
    // Retrieve the 'window' object.
    CefRefPtr<CefV8Value> object = context->GetGlobal();

    CefRefPtr<CefV8Context> cc = CefV8Context::GetCurrentContext();
    CefRefPtr<CefBrowser> currentBrowser = cc->GetBrowser();
    CefRefPtr<CefFrame> currentFrame = cc->GetFrame();
    CefString currentURL = currentFrame->GetURL();

    CefRefPtr<CefV8Context> ec = CefV8Context::GetEnteredContext();
    CefRefPtr<CefBrowser> enteredBrowser = ec->GetBrowser();
    CefRefPtr<CefFrame> enteredFrame = ec->GetFrame();
    CefString enteredURL = enteredFrame->GetURL();

    CefRefPtr<CefV8Handler> funcHandler(new DelegatingV8Handler(this));
    CefRefPtr<CefV8Value> helloFunc =
        CefV8Value::CreateFunction("hello", funcHandler);
    object->SetValue("hello", helloFunc, V8_PROPERTY_ATTRIBUTE_NONE);

    CefRefPtr<CefV8Value> fromIFrameFunc =
        CefV8Value::CreateFunction("fromIFrame", funcHandler);
    object->SetValue("fromIFrame", fromIFrameFunc, V8_PROPERTY_ATTRIBUTE_NONE);

    CefRefPtr<CefV8Value> goFunc =
        CefV8Value::CreateFunction("begin", funcHandler);
    object->SetValue("begin", goFunc, V8_PROPERTY_ATTRIBUTE_NONE);

    CefRefPtr<CefV8Value> doneFunc =
        CefV8Value::CreateFunction("end", funcHandler);
    object->SetValue("end", doneFunc, V8_PROPERTY_ATTRIBUTE_NONE);

    CefRefPtr<CefV8Value> compFunc =
        CefV8Value::CreateFunction("comp", funcHandler);
    object->SetValue("comp", compFunc, V8_PROPERTY_ATTRIBUTE_NONE);

    // Used for testing exceptions returned from accessors.
    CefRefPtr<CefV8Value> gotGetExceptionFunc =
        CefV8Value::CreateFunction("gotGetException", funcHandler);
    object->SetValue("gotGetException", gotGetExceptionFunc,
        V8_PROPERTY_ATTRIBUTE_NONE);
    CefRefPtr<CefV8Value> gotSetExceptionFunc =
        CefV8Value::CreateFunction("gotSetException", funcHandler);
    object->SetValue("gotSetException", gotSetExceptionFunc,
        V8_PROPERTY_ATTRIBUTE_NONE);

    // Create an object with accessor based properties:
    CefRefPtr<CefBase> blankBase;
    CefRefPtr<CefV8Accessor> accessor(new DelegatingV8Accessor(this));
    CefRefPtr<CefV8Value> point = CefV8Value::CreateObject(blankBase, accessor);

    point->SetValue("x", V8_ACCESS_CONTROL_DEFAULT,
        V8_PROPERTY_ATTRIBUTE_READONLY);
    point->SetValue("y", V8_ACCESS_CONTROL_DEFAULT,
        V8_PROPERTY_ATTRIBUTE_NONE);

    object->SetValue("point", point, V8_PROPERTY_ATTRIBUTE_NONE);

    // Create another object with accessor based properties:
    CefRefPtr<CefV8Value> exceptObj =
        CefV8Value::CreateObject(NULL, new DelegatingV8Accessor(this));

    exceptObj->SetValue("makeException", V8_ACCESS_CONTROL_DEFAULT,
        V8_PROPERTY_ATTRIBUTE_NONE);

    object->SetValue("exceptObj", exceptObj, V8_PROPERTY_ATTRIBUTE_NONE);
  }

  void CallIFrame() {
    CefV8ValueList args;
    CefRefPtr<CefV8Value> rv;
    CefRefPtr<CefV8Exception> exception;
    CefRefPtr<CefV8Value> empty;
    ASSERT_TRUE(funcIFrame_->ExecuteFunctionWithContext(contextIFrame_, empty,
                                                        args, rv, exception,
                                                        false));
  }

  void AsyncTestContext(CefRefPtr<CefV8Context> ec,
                        CefRefPtr<CefV8Context> cc) {
    // we should not be in a context in this call.
    CefRefPtr<CefV8Context> noContext = CefV8Context::GetCurrentContext();
    if (!noContext.get())
      got_no_context_.yes();

    CefRefPtr<CefBrowser> enteredBrowser = ec->GetBrowser();
    CefRefPtr<CefFrame> enteredFrame = ec->GetFrame();
    CefString enteredURL = enteredFrame->GetURL();
    CefString enteredName = enteredFrame->GetName();
    CefRefPtr<CefFrame> enteredMainFrame = enteredBrowser->GetMainFrame();
    CefString enteredMainURL = enteredMainFrame->GetURL();
    CefString enteredMainName = enteredMainFrame->GetName();

    CefRefPtr<CefBrowser> currentBrowser = cc->GetBrowser();
    CefRefPtr<CefFrame> currentFrame = cc->GetFrame();
    CefString currentURL = currentFrame->GetURL();
    CefString currentName = currentFrame->GetName();
    CefRefPtr<CefFrame> currentMainFrame = currentBrowser->GetMainFrame();
    CefString currentMainURL = currentMainFrame->GetURL();
    CefString currentMainName = currentMainFrame->GetName();

    CefRefPtr<CefBrowser> copyFromMainFrame =
        currentMainFrame->GetBrowser();

    currentMainFrame->LoadURL("http://tests/begin.html");
  }

  void AsyncTestException(CefRefPtr<CefV8Context> context,
                          CefRefPtr<CefV8Value> func) {
    CefV8ValueList args;
    CefRefPtr<CefV8Value> rv;
    CefRefPtr<CefV8Exception> exception;
    CefRefPtr<CefV8Value> empty;
    ASSERT_TRUE(func->ExecuteFunctionWithContext(context, empty, args, rv,
                                                 exception, false));
    if (exception.get() && exception->GetMessage() == "Uncaught My Exception")
      got_exception_.yes();
  }

  void AsyncTestNavigation(CefRefPtr<CefV8Context> context,
                           CefRefPtr<CefV8Value> func) {
    CefRefPtr<CefV8Exception> exception;
    CefV8ValueList args;
    CefRefPtr<CefV8Value> rv, obj, url;

    // Need to enter the context in order to create an Object,
    // Array, or Function. Simple types like String, Int,
    // Boolean, and Double don't require you to be in the
    // context before creating them.
    if ( context->Enter() ) {
      CefRefPtr<CefV8Value> global = context->GetGlobal();
      CefRefPtr<CefV8Value> anArray = CefV8Value::CreateArray();
      CefRefPtr<CefV8Handler> funcHandler(new DelegatingV8Handler(this));
      CefRefPtr<CefV8Value> foobarFunc =
          CefV8Value::CreateFunction("foobar", funcHandler);

      obj = CefV8Value::CreateObject(NULL, NULL);
      url = CefV8Value::CreateString("http://tests/end.html");

      obj->SetValue("url", url, V8_PROPERTY_ATTRIBUTE_NONE);
      obj->SetValue("foobar", foobarFunc, V8_PROPERTY_ATTRIBUTE_NONE);
      obj->SetValue("anArray", anArray, V8_PROPERTY_ATTRIBUTE_NONE);

      args.push_back(obj);

      ASSERT_TRUE(func->ExecuteFunctionWithContext(context, global, args, rv,
                                                   exception, false));
      if (!exception.get())
        got_navigation_.yes();

      context->Exit();
    }
  }

  bool Execute(const CefString& name,
               CefRefPtr<CefV8Value> object,
               const CefV8ValueList& arguments,
               CefRefPtr<CefV8Value>& retval,
               CefString& exception) OVERRIDE {
    CefRefPtr<CefV8Context> cc = CefV8Context::GetCurrentContext();
    CefRefPtr<CefV8Context> ec = CefV8Context::GetEnteredContext();

    CefRefPtr<CefBrowser> enteredBrowser = ec->GetBrowser();
    CefRefPtr<CefFrame> enteredFrame = ec->GetFrame();
    CefString enteredURL = enteredFrame->GetURL();
    CefString enteredName = enteredFrame->GetName();
    CefRefPtr<CefFrame> enteredMainFrame = enteredBrowser->GetMainFrame();
    CefString enteredMainURL = enteredMainFrame->GetURL();
    CefString enteredMainName = enteredMainFrame->GetName();

    CefRefPtr<CefBrowser> currentBrowser = cc->GetBrowser();
    CefRefPtr<CefFrame> currentFrame = cc->GetFrame();
    CefString currentURL = currentFrame->GetURL();
    CefString currentName = currentFrame->GetName();
    CefRefPtr<CefFrame> currentMainFrame = currentBrowser->GetMainFrame();
    CefString currentMainURL = currentMainFrame->GetURL();
    CefString currentMainName = currentMainFrame->GetName();

    if (name == "hello") {
      if (arguments.size() == 2 && arguments[0]->IsString() &&
         arguments[1]->IsFunction()) {
        CefString msg = arguments[0]->GetStringValue();
        if (msg == "main") {
          got_hello_main_.yes();
          contextIFrame_ = cc;
          funcIFrame_ = arguments[1];
        }
      } else if (arguments.size() == 1 && arguments[0]->IsString()) {
        CefString msg = arguments[0]->GetStringValue();
        if (msg == "iframe")
          got_hello_iframe_.yes();
      } else {
        return false;
      }

      if (got_hello_main_ && got_hello_iframe_ && funcIFrame_->IsFunction()) {
        // NB: At this point, enteredURL == http://tests/iframe.html which is
        // expected since the iframe made the call on its own. The unexpected
        // behavior is that in the call to fromIFrame (below) the enteredURL
        // == http://tests/main.html even though the iframe.html context was
        // entered first.
        //  -- Perhaps WebKit does something other than look at the bottom
        //     of stack for the entered context.
        if (enteredURL == "http://tests/iframe.html")
          got_iframe_as_entered_url_.yes();
        CallIFrame();
      }
      return true;
    } else if (name == "fromIFrame") {
      if (enteredURL == "http://tests/main.html")
        got_correct_entered_url_.yes();
      if (currentURL == "http://tests/iframe.html")
        got_correct_current_url_.yes();
      CefPostTask(TID_UI, NewCefRunnableMethod(this,
          &TestContextHandler::AsyncTestContext, ec, cc));
      return true;
    } else if (name == "begin") {
      if (arguments.size() == 2 && arguments[0]->IsFunction() &&
         arguments[1]->IsFunction()) {
        CefRefPtr<CefV8Value> funcException = arguments[0];
        CefRefPtr<CefV8Value> funcNavigate  = arguments[1];
        CefPostTask(TID_UI, NewCefRunnableMethod(this,
            &TestContextHandler::AsyncTestException, cc, funcException));
        CefPostTask(TID_UI, NewCefRunnableMethod(this,
            &TestContextHandler::AsyncTestNavigation, cc, funcNavigate));
        return true;
      }
    } else if (name == "comp") {
      if (arguments.size() == 3) {
        CefRefPtr<CefV8Value> expected = arguments[0];
        CefRefPtr<CefV8Value> one = arguments[1];
        CefRefPtr<CefV8Value> two = arguments[2];

        bool bExpected = expected->GetBoolValue();
        bool bOne2Two = one->IsSame(two);
        bool bTwo2One = two->IsSame(one);

        // IsSame should match the expected
        if ( bExpected != bOne2Two || bExpected != bTwo2One)
          got_bad_is_same_.yes();
      } else {
        got_bad_is_same_.yes();
      }
    } else if (name == "end") {
      got_testcomplete_.yes();
      DestroyTest();
      return true;
    } else if (name == "gotGetException") {
      if (arguments.size() == 1 &&
          arguments[0]->GetStringValue() == "Error: My Get Exception") {
        got_getexception_.yes();
      }
      return true;
    } else if (name == "gotSetException") {
      if (arguments.size() == 1 &&
          arguments[0]->GetStringValue() == "Error: My Set Exception") {
        got_setexception_.yes();
      }
      return true;
    }
    return false;
  }

  bool Get(const CefString& name,
           const CefRefPtr<CefV8Value> object,
           CefRefPtr<CefV8Value>& retval,
           CefString& exception) OVERRIDE {
    if (name == "x") {
      got_point_x_read_.yes();
      retval = CefV8Value::CreateInt(1234);
      return true;
    } else if (name == "y") {
      got_point_y_read_.yes();
      retval = CefV8Value::CreateInt(y_);
      return true;
    } else if (name == "makeException") {
      exception = "My Get Exception";
      return true;
    }
    return false;
  }

  bool Set(const CefString& name,
           const CefRefPtr<CefV8Value> object,
           const CefRefPtr<CefV8Value> value,
           CefString& exception) OVERRIDE {
    if (name == "y") {
      y_ = value->GetIntValue();
      if (y_ == 1234)
        got_point_y_write_.yes();
      return true;
    } else if (name == "makeException") {
      exception = "My Set Exception";
      return true;
    }
    return false;
  }

  // This function we will be called later to make it call into the
  // IFRAME, which then calls "fromIFrame" so that we can check the
  // entered vs current contexts are working as expected.
  CefRefPtr<CefV8Context> contextIFrame_;
  CefRefPtr<CefV8Value> funcIFrame_;

  TrackCallback got_point_x_read_;
  TrackCallback got_point_y_read_;
  TrackCallback got_point_y_write_;
  TrackCallback got_bad_is_same_;
  TrackCallback got_hello_main_;
  TrackCallback got_hello_iframe_;
  TrackCallback got_correct_entered_url_;
  TrackCallback got_correct_current_url_;
  TrackCallback got_iframe_as_entered_url_;
  TrackCallback got_no_context_;
  TrackCallback got_exception_;
  TrackCallback got_getexception_;
  TrackCallback got_setexception_;
  TrackCallback got_navigation_;
  TrackCallback got_testcomplete_;

  int y_;
};

}  // namespace

// Verify context works to allow async v8 callbacks
TEST(V8Test, Context) {
  CefRefPtr<TestContextHandler> handler = new TestContextHandler();
  handler->ExecuteTest();

  EXPECT_TRUE(handler->got_point_x_read_);
  EXPECT_TRUE(handler->got_point_y_read_);
  EXPECT_TRUE(handler->got_point_y_write_);
  EXPECT_FALSE(handler->got_bad_is_same_);
  EXPECT_TRUE(handler->got_hello_main_);
  EXPECT_TRUE(handler->got_hello_iframe_);
  EXPECT_TRUE(handler->got_no_context_);
  EXPECT_TRUE(handler->got_iframe_as_entered_url_);
  EXPECT_TRUE(handler->got_correct_entered_url_);
  EXPECT_TRUE(handler->got_correct_current_url_);
  EXPECT_TRUE(handler->got_exception_);
  EXPECT_TRUE(handler->got_getexception_);
  EXPECT_TRUE(handler->got_setexception_);
  EXPECT_TRUE(handler->got_navigation_);
  EXPECT_TRUE(handler->got_testcomplete_);
}

namespace {

class TestInternalHandler : public TestHandler {
 public:
  class UserData : public CefBase {
   public:
    explicit UserData(CefRefPtr<TestInternalHandler> test)
      : test_(test) {
    }

    void Test(const std::string& name) {
      if (name == "obj1-before") {
        if (test_->nav_ == 0)
          test_->got_userdata_obj1_before_test1_fail_.yes();
        else
          test_->got_userdata_obj1_before_test2_fail_.yes();
      } else if (name == "obj2-before") {
        if (test_->nav_ == 0)
          test_->got_userdata_obj2_before_test1_.yes();
        else
          test_->got_userdata_obj2_before_test2_fail_.yes();
      } else if (name == "obj1-after") {
        if (test_->nav_ == 0)
          test_->got_userdata_obj1_after_test1_fail_.yes();
        else
          test_->got_userdata_obj1_after_test2_fail_.yes();
      } else if (name == "obj2-after") {
        if (test_->nav_ == 0)
          test_->got_userdata_obj2_after_test1_.yes();
        else
          test_->got_userdata_obj2_after_test2_fail_.yes();
      }
    }

    CefRefPtr<TestInternalHandler> test_;

    IMPLEMENT_REFCOUNTING(UserData);
  };

  class Accessor : public CefV8Accessor {
   public:
    explicit Accessor(CefRefPtr<TestInternalHandler> test)
      : test_(test) {
    }

    virtual bool Get(const CefString& name,
                     const CefRefPtr<CefV8Value> object,
                     CefRefPtr<CefV8Value>& retval,
                     CefString& exception) OVERRIDE {
      if (test_->nav_ == 0)
        test_->got_accessor_get1_.yes();
      else
        test_->got_accessor_get2_fail_.yes();

      retval = CefV8Value::CreateString("default2");
      return true;
    }

    virtual bool Set(const CefString& name,
                     const CefRefPtr<CefV8Value> object,
                     const CefRefPtr<CefV8Value> value,
                     CefString& exception) OVERRIDE {
      if (test_->nav_ == 0)
        test_->got_accessor_set1_.yes();
      else
        test_->got_accessor_set2_fail_.yes();

      return true;
    }

    CefRefPtr<TestInternalHandler> test_;

    IMPLEMENT_REFCOUNTING(Accessor);
  };

  class Handler : public CefV8Handler {
   public:
    explicit Handler(CefRefPtr<TestInternalHandler> test)
      : test_(test),
        execute_ct_(0) {
    }

    virtual bool Execute(const CefString& name,
                         CefRefPtr<CefV8Value> object,
                         const CefV8ValueList& arguments,
                         CefRefPtr<CefV8Value>& retval,
                         CefString& exception) OVERRIDE {
      CefRefPtr<CefV8Handler> handler =
          object->GetValue("func")->GetFunctionHandler();

      if (execute_ct_ == 0) {
        if (handler.get() == this)
          test_->got_execute1_.yes();
        else
          test_->got_execute1_fail_.yes();
      } else {
        if (handler.get() == this)
          test_->got_execute2_.yes();
        else
          test_->got_execute2_fail_.yes();
      }

      execute_ct_++;

      return true;
    }

    CefRefPtr<TestInternalHandler> test_;
    int execute_ct_;

    IMPLEMENT_REFCOUNTING(Accessor);
  };

  class TestHandler : public CefV8Handler {
   public:
    explicit TestHandler(CefRefPtr<TestInternalHandler> test)
      : test_(test) {
    }

    virtual bool Execute(const CefString& name,
                         CefRefPtr<CefV8Value> object,
                         const CefV8ValueList& arguments,
                         CefRefPtr<CefV8Value>& retval,
                         CefString& exception) OVERRIDE {
      if (name == "store") {
        // Store a JSON value.
        if (arguments.size() == 2 && arguments[0]->IsString() &&
            arguments[1]->IsString()) {
          std::string name = arguments[0]->GetStringValue();
          std::string val = arguments[1]->GetStringValue();
          if (name == "obj1") {
            test_->obj1_json_ = val;
            if (val == "{\"value\":\"testval1\",\"value2\":\"default1\"}")
              test_->got_obj1_json_.yes();
          } else if (name == "obj2") {
            test_->obj2_json_ = val;
            if (val == "{\"value\":\"testval2\",\"value2\":\"default2\"}")
              test_->got_obj2_json_.yes();
          } else {
            return false;
          }
          retval = CefV8Value::CreateBool(true);
          return true;
        }
      } else if (name == "retrieve") {
        // Retrieve a JSON value.
        if (arguments.size() == 1 && arguments[0]->IsString()) {
          std::string name = arguments[0]->GetStringValue();
          std::string val;
          if (name == "obj1")
            val = test_->obj1_json_;
          else if (name == "obj2")
            val = test_->obj2_json_;
          if (!val.empty()) {
            retval = CefV8Value::CreateString(val);
            return true;
          }
        }
      } else if (name == "userdata") {
        if (arguments.size() == 2 && arguments[0]->IsString() &&
            arguments[1]->IsObject()) {
          std::string name = arguments[0]->GetStringValue();
          CefRefPtr<UserData> userData =
              reinterpret_cast<UserData*>(arguments[1]->GetUserData().get());
          if (!userData.get()) {
            // No UserData object.
            if (name == "obj1-before") {
              if (test_->nav_ == 0)
                test_->got_userdata_obj1_before_null1_.yes();
              else
                test_->got_userdata_obj1_before_null2_.yes();
            } else if (name == "obj2-before") {
              if (test_->nav_ == 0)
                test_->got_userdata_obj2_before_null1_fail_.yes();
              else
                test_->got_userdata_obj2_before_null2_.yes();
            } else if (name == "obj1-after") {
              if (test_->nav_ == 0)
                test_->got_userdata_obj1_after_null1_.yes();
              else
                test_->got_userdata_obj1_after_null2_.yes();
            } else if (name == "obj2-after") {
              if (test_->nav_ == 0)
                test_->got_userdata_obj2_after_null1_fail_.yes();
              else
                test_->got_userdata_obj2_after_null2_.yes();
            }
          } else {
            // Call the test function.
            userData->Test(name);
          }
          return true;
        }
      }  else if (name == "record") {
        if (arguments.size() == 1 && arguments[0]->IsString()) {
          std::string name = arguments[0]->GetStringValue();
          if (name == "userdata-obj1-set-succeed") {
            if (test_->nav_ == 0)
              test_->got_userdata_obj1_set_succeed1_.yes();
            else
              test_->got_userdata_obj1_set_succeed2_.yes();
          } else if (name == "userdata-obj1-set-except") {
            if (test_->nav_ == 0)
              test_->got_userdata_obj1_set_except1_fail_.yes();
            else
              test_->got_userdata_obj1_set_except2_fail_.yes();
          } else if (name == "userdata-obj2-set-succeed") {
            if (test_->nav_ == 0)
              test_->got_userdata_obj2_set_succeed1_.yes();
            else
              test_->got_userdata_obj2_set_succeed2_.yes();
          } else if (name == "userdata-obj2-set-except") {
            if (test_->nav_ == 0)
              test_->got_userdata_obj2_set_except1_fail_.yes();
            else
              test_->got_userdata_obj2_set_except2_fail_.yes();
          } else if (name == "func-set-succeed") {
            test_->got_func_set_succeed_.yes();
          } else if (name == "func-set-except") {
            test_->got_func_set_except_fail_.yes();
          }
          return true;
        }
      }

      return false;
    }

    CefRefPtr<TestInternalHandler> test_;

    IMPLEMENT_REFCOUNTING(TestHandler);
  };

  TestInternalHandler()
    : nav_(0) {
  }

  virtual void RunTest() OVERRIDE {
    std::string tests =
        // Test userdata retrieval.
        "window.test.userdata('obj1-before', window.obj1);\n"
        "window.test.userdata('obj2-before', window.obj2);\n"
        // Test accessors.
        "window.obj1.value2 = 'newval1';\n"
        "window.obj2.value2 = 'newval2';\n"
        "val1 = window.obj1.value2;\n"
        "val2 = window.obj2.value2;\n"
        // Test setting the hidden internal values.
        "try { window.obj1['Cef::UserData'] = 1;\n"
              "window.obj1['Cef::Accessor'] = 1;\n"
              "window.test.record('userdata-obj1-set-succeed'); }\n"
        "catch(e) { window.test.record('userdata-obj1-set-except'); }\n"
        "try { window.obj2['Cef::UserData'] = 1;\n"
              "window.obj2['Cef::Accessor'] = 1;\n"
              "window.test.record('userdata-obj2-set-succeed'); }\n"
        "catch(e) { window.test.record('userdata-obj2-set-except'); }\n"
        // Test userdata retrieval after messing with the internal values.
        "window.test.userdata('obj1-after', window.obj1);\n"
        "window.test.userdata('obj2-after', window.obj2);\n"
        // Test accessors after messing with the internal values.
        "window.obj1.value2 = 'newval1';\n"
        "window.obj2.value2 = 'newval2';\n"
        "val1 = window.obj1.value2;\n"
        "val2 = window.obj2.value2;\n";

    std::stringstream testHtml;

    testHtml <<
        "<html><body>\n"
        "<script language=\"JavaScript\">\n"
        // Serialize the bound values.
        "window.test.store('obj1', JSON.stringify(window.obj1));\n"
        "window.test.store('obj2', JSON.stringify(window.obj2));\n"
        // Run the tests.
        << tests.c_str() <<
        // Test function call.
        "window.func();\n"
        // Test setting the hidden internal values.
        "try { window.func['Cef::Handler'] = 1;\n"
              "window.test.record('func-set-succeed'); }\n"
        "catch(e) { window.test.record('func-set-except'); }\n"
        // Test function call after messing with internal values.
        "window.func();\n"
        "</script>\n"
        "</body></html>";
    AddResource("http://tests/run1.html", testHtml.str(), "text/html");
    testHtml.str("");

    testHtml <<
        "<html><body>\n"
        "<script language=\"JavaScript\">\n"
        // Deserialize the bound values.
        "window.obj1 = JSON.parse(window.test.retrieve('obj1'));\n"
        "window.obj2 = JSON.parse(window.test.retrieve('obj2'));\n"
        // Run the tests.
        << tests.c_str() <<
        "</script>\n"
        "</body></html>";
    AddResource("http://tests/run2.html", testHtml.str(), "text/html");
    testHtml.str("");

    CreateBrowser("http://tests/run1.html");
  }

  virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         int httpStatusCode) OVERRIDE {
    if (nav_ == 0) {
      // Navigate to the next page.
      frame->LoadURL("http://tests/run2.html");
    } else {
      DestroyTest();
    }

    nav_++;
  }

  virtual void OnContextCreated(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefRefPtr<CefV8Context> context) OVERRIDE {
    // Retrieve the 'window' object.
    CefRefPtr<CefV8Value> object = context->GetGlobal();

    if (nav_ == 0) {
      // Create an object without any internal values.
      CefRefPtr<CefV8Value> obj1 = CefV8Value::CreateObject(NULL, NULL);
      obj1->SetValue("value", CefV8Value::CreateString("testval1"),
          V8_PROPERTY_ATTRIBUTE_NONE);
      obj1->SetValue("value2", CefV8Value::CreateString("default1"),
          V8_PROPERTY_ATTRIBUTE_NONE);
      object->SetValue("obj1", obj1, V8_PROPERTY_ATTRIBUTE_NONE);

      // Create an object with Cef::Accessor and Cef::UserData internal values.
      CefRefPtr<CefV8Value> obj2 =
          CefV8Value::CreateObject(new UserData(this), new Accessor(this));
      obj2->SetValue("value", CefV8Value::CreateString("testval2"),
          V8_PROPERTY_ATTRIBUTE_NONE);
      obj2->SetValue("value2", V8_ACCESS_CONTROL_DEFAULT,
          V8_PROPERTY_ATTRIBUTE_NONE);
      object->SetValue("obj2", obj2, V8_PROPERTY_ATTRIBUTE_NONE);

      // Create a function with Cef::Handler internal value.
      CefRefPtr<CefV8Value> func =
          CefV8Value::CreateFunction("func", new Handler(this));
      object->SetValue("func", func, V8_PROPERTY_ATTRIBUTE_NONE);
    }

    // Used for executing the test.
    CefRefPtr<CefV8Handler> handler = new TestHandler(this);
    CefRefPtr<CefV8Value> obj = CefV8Value::CreateObject(NULL, NULL);
    obj->SetValue("store", CefV8Value::CreateFunction("store", handler),
        V8_PROPERTY_ATTRIBUTE_NONE);
    obj->SetValue("retrieve", CefV8Value::CreateFunction("retrieve", handler),
        V8_PROPERTY_ATTRIBUTE_NONE);
    obj->SetValue("userdata", CefV8Value::CreateFunction("userdata", handler),
        V8_PROPERTY_ATTRIBUTE_NONE);
    obj->SetValue("record", CefV8Value::CreateFunction("record", handler),
        V8_PROPERTY_ATTRIBUTE_NONE);
    object->SetValue("test", obj, V8_PROPERTY_ATTRIBUTE_NONE);
  }

  int nav_;
  std::string obj1_json_, obj2_json_;

  TrackCallback got_obj1_json_;
  TrackCallback got_obj2_json_;

  TrackCallback got_userdata_obj1_before_null1_;
  TrackCallback got_userdata_obj2_before_null1_fail_;
  TrackCallback got_userdata_obj1_before_test1_fail_;
  TrackCallback got_userdata_obj2_before_test1_;
  TrackCallback got_userdata_obj1_set_succeed1_;
  TrackCallback got_userdata_obj1_set_except1_fail_;
  TrackCallback got_userdata_obj2_set_succeed1_;
  TrackCallback got_userdata_obj2_set_except1_fail_;
  TrackCallback got_userdata_obj1_after_null1_;
  TrackCallback got_userdata_obj2_after_null1_fail_;
  TrackCallback got_userdata_obj1_after_test1_fail_;
  TrackCallback got_userdata_obj2_after_test1_;

  TrackCallback got_userdata_obj1_before_null2_;
  TrackCallback got_userdata_obj2_before_null2_;
  TrackCallback got_userdata_obj1_before_test2_fail_;
  TrackCallback got_userdata_obj2_before_test2_fail_;
  TrackCallback got_userdata_obj1_set_succeed2_;
  TrackCallback got_userdata_obj1_set_except2_fail_;
  TrackCallback got_userdata_obj2_set_succeed2_;
  TrackCallback got_userdata_obj2_set_except2_fail_;
  TrackCallback got_userdata_obj1_after_null2_;
  TrackCallback got_userdata_obj2_after_null2_;
  TrackCallback got_userdata_obj1_after_test2_fail_;
  TrackCallback got_userdata_obj2_after_test2_fail_;

  TrackCallback got_accessor_get1_;
  TrackCallback got_accessor_get2_fail_;
  TrackCallback got_accessor_set1_;
  TrackCallback got_accessor_set2_fail_;

  TrackCallback got_execute1_;
  TrackCallback got_execute1_fail_;
  TrackCallback got_func_set_succeed_;
  TrackCallback got_func_set_except_fail_;
  TrackCallback got_execute2_;
  TrackCallback got_execute2_fail_;
};

}  // namespace

// Test that messing around with CEF internal values doesn't cause crashes.
TEST(V8Test, Internal) {
  CefRefPtr<TestInternalHandler> handler = new TestInternalHandler();
  handler->ExecuteTest();

  EXPECT_TRUE(handler->got_obj1_json_.isSet());
  EXPECT_TRUE(handler->got_obj2_json_.isSet());

  EXPECT_TRUE(handler->got_userdata_obj1_before_null1_.isSet());
  EXPECT_FALSE(handler->got_userdata_obj2_before_null1_fail_.isSet());
  EXPECT_FALSE(handler->got_userdata_obj1_before_test1_fail_.isSet());
  EXPECT_TRUE(handler->got_userdata_obj2_before_test1_.isSet());
  EXPECT_TRUE(handler->got_userdata_obj1_set_succeed1_.isSet());
  EXPECT_FALSE(handler->got_userdata_obj1_set_except1_fail_.isSet());
  EXPECT_TRUE(handler->got_userdata_obj2_set_succeed1_.isSet());
  EXPECT_FALSE(handler->got_userdata_obj2_set_except1_fail_.isSet());
  EXPECT_TRUE(handler->got_userdata_obj1_after_null1_.isSet());
  EXPECT_FALSE(handler->got_userdata_obj2_after_null1_fail_.isSet());
  EXPECT_FALSE(handler->got_userdata_obj1_after_test1_fail_.isSet());
  EXPECT_TRUE(handler->got_userdata_obj2_after_test1_.isSet());

  EXPECT_TRUE(handler->got_userdata_obj1_before_null2_.isSet());
  EXPECT_TRUE(handler->got_userdata_obj2_before_null2_.isSet());
  EXPECT_FALSE(handler->got_userdata_obj1_before_test2_fail_.isSet());
  EXPECT_FALSE(handler->got_userdata_obj2_before_test2_fail_.isSet());
  EXPECT_TRUE(handler->got_userdata_obj1_set_succeed2_.isSet());
  EXPECT_FALSE(handler->got_userdata_obj1_set_except2_fail_.isSet());
  EXPECT_TRUE(handler->got_userdata_obj2_set_succeed2_.isSet());
  EXPECT_FALSE(handler->got_userdata_obj2_set_except2_fail_.isSet());
  EXPECT_TRUE(handler->got_userdata_obj1_after_null2_.isSet());
  EXPECT_TRUE(handler->got_userdata_obj2_after_null2_.isSet());
  EXPECT_FALSE(handler->got_userdata_obj1_after_test2_fail_.isSet());
  EXPECT_FALSE(handler->got_userdata_obj2_after_test2_fail_.isSet());

  EXPECT_TRUE(handler->got_accessor_get1_.isSet());
  EXPECT_FALSE(handler->got_accessor_get2_fail_.isSet());
  EXPECT_TRUE(handler->got_accessor_set1_.isSet());
  EXPECT_FALSE(handler->got_accessor_set2_fail_.isSet());

  EXPECT_TRUE(handler->got_execute1_.isSet());
  EXPECT_FALSE(handler->got_execute1_fail_.isSet());
  EXPECT_TRUE(handler->got_execute2_.isSet());
  EXPECT_FALSE(handler->got_execute2_fail_.isSet());
}

namespace {

static const int kNumExceptionTests = 3;

class TestExceptionHandler : public TestHandler {
 public:
  class TestHandler : public CefV8Handler {
   public:
    explicit TestHandler(CefRefPtr<TestExceptionHandler> test)
      : test_(test) {
    }

    virtual bool Execute(const CefString& name,
                         CefRefPtr<CefV8Value> object,
                         const CefV8ValueList& arguments,
                         CefRefPtr<CefV8Value>& retval,
                         CefString& exception) OVERRIDE {
      if (name == "register") {
        if (arguments.size() == 1 && arguments[0]->IsFunction()) {
          test_->got_register_.yes();

          // Keep pointers to the callback function and context.
          test_->test_func_ = arguments[0];
          test_->test_context_ = CefV8Context::GetCurrentContext();
          return true;
        }
      } else if (name == "execute") {
        if (arguments.size() == 2 && arguments[0]->IsInt() &&
            arguments[1]->IsBool()) {
          // Execute the test callback function.
          test_->ExecuteTestCallback(arguments[0]->GetIntValue(),
              arguments[1]->GetBoolValue());
          return true;
        }
      } else if (name == "result") {
        if (arguments.size() == 1 && arguments[0]->IsString()) {
          std::string value = arguments[0]->GetStringValue();
          if (value == "no_exception")
            test_->got_no_exception_result_.yes();
          else if (value == "exception")
            test_->got_exception_result_.yes();
          else if (value == "done")
            test_->got_done_result_.yes();
          else
            return false;
          return true;
        }
      }

      return false;
    }

    CefRefPtr<TestExceptionHandler> test_;

    IMPLEMENT_REFCOUNTING(TestHandler);
  };

  TestExceptionHandler() {
  }

  virtual void RunTest() OVERRIDE {
    std::string testHtml =
        "<html><body>\n"
        "<script language=\"JavaScript\">\n"
        // JS callback function that throws an exception.
        "function testFunc() {\n"
        "  throw 'Some test exception';\n"
        "}\n"
        // Register the callback function.
        "window.test.register(testFunc);\n"
        // Test 1: Execute the callback without re-throwing the exception.
        "window.test.execute(1, false);\n"
        // Test 2: Execute the callback, re-throw and catch the exception.
        "try {\n"
        "  window.test.execute(2, true);\n"
        // This line should never execute.
        "  window.test.result('no_exception');\n"
        "} catch(e) {\n"
        "  window.test.result('exception');\n"
        "}\n"
        // Verify that JS execution continues.
         "window.test.result('done');\n"
        "</script>\n"
        "</body></html>";
    AddResource("http://tests/run.html", testHtml, "text/html");

    CreateBrowser("http://tests/run.html");
  }

  // Execute the callback function.
  void ExecuteTestCallback(int test, bool rethrow_exception) {
    if (test <= 0 || test > kNumExceptionTests)
      return;

    got_execute_test_[test-1].yes();

    if (!test_func_.get())
      return;

    CefV8ValueList args;
    CefRefPtr<CefV8Value> retval;
    CefRefPtr<CefV8Exception> exception;
    if (test_func_->ExecuteFunctionWithContext(test_context_, NULL, args,
        retval, exception, rethrow_exception)) {
      got_execute_function_[test-1].yes();

      if (exception.get()) {
        got_exception_[test-1].yes();

        std::string message = exception->GetMessage();
        EXPECT_EQ("Uncaught Some test exception", message) << "test = " << test;

        std::string source_line = exception->GetSourceLine();
        EXPECT_EQ("  throw 'Some test exception';", source_line) << "test = " <<
            test;

        std::string script = exception->GetScriptResourceName();
        EXPECT_EQ("http://tests/run.html", script) << "test = " << test;

        int line_number = exception->GetLineNumber();
        EXPECT_EQ(4, line_number) << "test = " << test;

        int start_pos = exception->GetStartPosition();
        EXPECT_EQ(25, start_pos) << "test = " << test;

        int end_pos = exception->GetEndPosition();
        EXPECT_EQ(26, end_pos) << "test = " << test;

        int start_col = exception->GetStartColumn();
        EXPECT_EQ(2, start_col) << "test = " << test;

        int end_col = exception->GetEndColumn();
        EXPECT_EQ(3, end_col) << "test = " << test;
      }
    }

    if (test == kNumExceptionTests)
      DestroyTest();
  }

  virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         int httpStatusCode) OVERRIDE {
    got_load_end_.yes();

    // Test 3: Execute the callback asynchronously without re-throwing the
    // exception.
    CefPostTask(TID_UI,
        NewCefRunnableMethod(this, &TestExceptionHandler::ExecuteTestCallback,
                             3, false));
  }

  virtual void OnContextCreated(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                CefRefPtr<CefV8Context> context) OVERRIDE {
    // Retrieve the 'window' object.
    CefRefPtr<CefV8Value> object = context->GetGlobal();

    // Create the functions that will be used during the test.
    CefRefPtr<CefV8Value> obj = CefV8Value::CreateObject(NULL, NULL);
    CefRefPtr<CefV8Handler> handler = new TestHandler(this);
    obj->SetValue("register",
                  CefV8Value::CreateFunction("register", handler),
                  V8_PROPERTY_ATTRIBUTE_NONE);
    obj->SetValue("execute",
                  CefV8Value::CreateFunction("execute", handler),
                  V8_PROPERTY_ATTRIBUTE_NONE);
    obj->SetValue("result",
                  CefV8Value::CreateFunction("result", handler),
                  V8_PROPERTY_ATTRIBUTE_NONE);
    object->SetValue("test", obj, V8_PROPERTY_ATTRIBUTE_NONE);
  }

  CefRefPtr<CefV8Value> test_func_;
  CefRefPtr<CefV8Context> test_context_;

  TrackCallback got_register_;
  TrackCallback got_load_end_;
  TrackCallback got_execute_test_[kNumExceptionTests];
  TrackCallback got_execute_function_[kNumExceptionTests];
  TrackCallback got_exception_[kNumExceptionTests];
  TrackCallback got_exception_result_;
  TrackCallback got_no_exception_result_;
  TrackCallback got_done_result_;
};

}  // namespace

// Test V8 exception results.
TEST(V8Test, Exception) {
  CefRefPtr<TestExceptionHandler> handler = new TestExceptionHandler();
  handler->ExecuteTest();

  EXPECT_TRUE(handler->got_register_);
  EXPECT_TRUE(handler->got_load_end_);
  EXPECT_TRUE(handler->got_exception_result_);
  EXPECT_FALSE(handler->got_no_exception_result_);
  EXPECT_TRUE(handler->got_done_result_);

  for (int i = 0; i < kNumExceptionTests; ++i) {
    EXPECT_TRUE(handler->got_execute_test_[i]) << "test = " << i+1;
    EXPECT_TRUE(handler->got_execute_function_[i]) << "test = " << i+1;
    EXPECT_TRUE(handler->got_exception_[i]) << "test = " << i+1;
  }
}

namespace {

class TestPermissionsHandler : public V8TestHandler {
 public:
  explicit TestPermissionsHandler(bool denyExtensions) : V8TestHandler(false) {
    deny_extensions_ = denyExtensions;
  }

  virtual bool OnBeforeScriptExtensionLoad(CefRefPtr<CefBrowser> browser,
                                           CefRefPtr<CefFrame> frame,
                                           const CefString& extensionName) {
    return deny_extensions_;
  }

  bool deny_extensions_;
};

}  // namespace

// Verify extension permissions
TEST(V8Test, Permissions) {
  g_V8TestV8HandlerExecuteCalled = false;

  std::string extensionCode =
    "var test;"
    "if (!test)"
    "  test = {};"
    "(function() {"
    "  test.execute = function(a,b,c,d,e,f,g,h,i) {"
    "    native function execute();"
    "    return execute(a,b,c,d,e,f,g,h,i);"
    "  };"
    "})();";
  CefRegisterExtension("v8/test", extensionCode, new V8TestV8Handler(false));

  CefRefPtr<V8TestHandler> deny_handler = new TestPermissionsHandler(true);
  deny_handler->ExecuteTest();

  ASSERT_FALSE(g_V8TestV8HandlerExecuteCalled);

  CefRefPtr<V8TestHandler> allow_handler = new TestPermissionsHandler(false);
  allow_handler->ExecuteTest();

  ASSERT_TRUE(g_V8TestV8HandlerExecuteCalled);
}
