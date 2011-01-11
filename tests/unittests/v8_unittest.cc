// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "test_handler.h"

bool g_V8TestV8HandlerExecuteCalled;
bool g_V8TestV8HandlerExecute2Called;

class V8TestV8Handler : public CefThreadSafeBase<CefV8Handler>
{
public:
  V8TestV8Handler(bool bindingTest) { binding_test_ = bindingTest; }

  virtual bool Execute(const CefString& name,
                       CefRefPtr<CefV8Value> object,
                       const CefV8ValueList& arguments,
                       CefRefPtr<CefV8Value>& retval,
                       CefString& exception)
  {
    TestExecute(name, object, arguments, retval, exception);
    return true;
  }

  void TestExecute(const CefString& name,
                   CefRefPtr<CefV8Value> object,
                   const CefV8ValueList& arguments,
                   CefRefPtr<CefV8Value>& retval,
                   CefString& exception)
  {
    if(name == "execute") {
      g_V8TestV8HandlerExecuteCalled = true;
      
      ASSERT_EQ((size_t)8, arguments.size());
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
        ASSERT_EQ(false, value->GetBoolValue());
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
        CefString exception;
        ASSERT_TRUE(arguments[argct]->ExecuteFunction(
            arguments[argct], args, rv, exception));
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
        CefString exception;
        ASSERT_TRUE(arguments[argct]->ExecuteFunction(
            arguments[argct], args, rv, exception));
        ASSERT_EQ(exception, "Uncaught My Exception");
      }
      argct++;

      if(binding_test_)
      {
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
    } else if(name == "execute2") {
      g_V8TestV8HandlerExecute2Called = true;
      
      // check the result of calling the "execute" function
      ASSERT_EQ((size_t)1, arguments.size());
      ASSERT_TRUE(arguments[0]->IsInt());
      ASSERT_EQ(5, arguments[0]->GetIntValue());
    }
  }

  bool binding_test_;
};

class V8TestHandler : public TestHandler
{
public:
  V8TestHandler(bool bindingTest) { binding_test_ = bindingTest; }

  virtual void RunTest()
  {
    std::string object;
    if(binding_test_) {
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
      "  " << object << ".execute(5, 6.543, true, \"test string\","
      "    [7, 5.432, false, \"another string\"],"
      "    {arg0:2, arg1:3.433, arg2:true, arg3:\"some string\"}, func, func2)"
      ");"
      "</script>"
      "</body></html>";
	
    AddResource("http://tests/run.html", testHtml.str(), "text/html");
    CreateBrowser("http://tests/run.html");
  }

  virtual RetVal HandleLoadEnd(CefRefPtr<CefBrowser> browser,
                               CefRefPtr<CefFrame> frame,
                               bool isMainContent)
  {
    if(!browser->IsPopup() && !frame.get())
      DestroyTest();
    return RV_CONTINUE;
  }

  virtual RetVal HandleJSBinding(CefRefPtr<CefBrowser> browser,
                                 CefRefPtr<CefFrame> frame,
                                 CefRefPtr<CefV8Value> object)
  {
    if(binding_test_) {
      TestHandleJSBinding(browser, frame, object);
      return RV_HANDLED;
    }
    return RV_CONTINUE;
  }

  void TestHandleJSBinding(CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefFrame> frame,
                           CefRefPtr<CefV8Value> object)
  {
    // Create the new V8 object
    CefRefPtr<CefV8Value> testObj = CefV8Value::CreateObject(NULL);
    ASSERT_TRUE(testObj.get() != NULL);
    ASSERT_TRUE(object->SetValue("test", testObj));

    // Create an instance of V8ExecuteV8Handler
    CefRefPtr<CefV8Handler> testHandler(new V8TestV8Handler(true));
    ASSERT_TRUE(testHandler.get() != NULL);

    // Add the functions
    CefRefPtr<CefV8Value> testFunc;
    testFunc = CefV8Value::CreateFunction("execute", testHandler);
    ASSERT_TRUE(testFunc.get() != NULL);
    ASSERT_TRUE(testObj->SetValue("execute", testFunc));
    testFunc = CefV8Value::CreateFunction("execute2", testHandler);
    ASSERT_TRUE(testFunc.get() != NULL);
    ASSERT_TRUE(testObj->SetValue("execute2", testFunc));

    // Add the values
    ASSERT_TRUE(testObj->SetValue("intVal",
        CefV8Value::CreateInt(12)));
    ASSERT_TRUE(testObj->SetValue("doubleVal",
        CefV8Value::CreateDouble(5.432)));
    ASSERT_TRUE(testObj->SetValue("boolVal",
        CefV8Value::CreateBool(true)));
    ASSERT_TRUE(testObj->SetValue("stringVal",
        CefV8Value::CreateString("the string")));

    CefRefPtr<CefV8Value> testArray(CefV8Value::CreateArray());
    ASSERT_TRUE(testArray.get() != NULL);
    ASSERT_TRUE(testObj->SetValue("arrayVal", testArray));
    ASSERT_TRUE(testArray->SetValue(0, CefV8Value::CreateInt(4)));
    ASSERT_TRUE(testArray->SetValue(1, CefV8Value::CreateDouble(120.43)));
    ASSERT_TRUE(testArray->SetValue(2, CefV8Value::CreateBool(true)));
    ASSERT_TRUE(testArray->SetValue(3, CefV8Value::CreateString("a string")));
  }

  bool binding_test_;
};

// Verify window binding
TEST(V8Test, Binding)
{
  g_V8TestV8HandlerExecuteCalled = false;
  g_V8TestV8HandlerExecute2Called = false;

  CefRefPtr<V8TestHandler> handler = new V8TestHandler(true);
  handler->ExecuteTest();

  ASSERT_TRUE(g_V8TestV8HandlerExecuteCalled);
  ASSERT_TRUE(g_V8TestV8HandlerExecute2Called);
}

// Verify extensions
TEST(V8Test, Extension)
{
  g_V8TestV8HandlerExecuteCalled = false;
  g_V8TestV8HandlerExecute2Called = false;

  std::string extensionCode =
    "var test;"
    "if (!test)"
    "  test = {};"
    "(function() {"
    "  test.execute = function(a,b,c,d,e,f,g,h) {"
    "    native function execute();"
    "    return execute(a,b,c,d,e,f,g,h);"
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
