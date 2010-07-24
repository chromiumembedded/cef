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

  virtual bool Execute(const std::wstring& name,
                       CefRefPtr<CefV8Value> object,
                       const CefV8ValueList& arguments,
                       CefRefPtr<CefV8Value>& retval,
                       std::wstring& exception)
  {
    TestExecute(name, object, arguments, retval, exception);
    return true;
  }

  void TestExecute(const std::wstring& name,
                   CefRefPtr<CefV8Value> object,
                   const CefV8ValueList& arguments,
                   CefRefPtr<CefV8Value>& retval,
                   std::wstring& exception)
  {
    if(name == L"execute") {
      g_V8TestV8HandlerExecuteCalled = true;
      
      ASSERT_EQ(8, arguments.size());
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
      ASSERT_EQ(L"test string", arguments[argct]->GetStringValue());
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
        ASSERT_EQ(L"another string", value->GetStringValue());
        subargct++;
      }
      argct++;

      // object
      ASSERT_TRUE(arguments[argct]->IsObject());
      {
        value = arguments[argct]->GetValue(L"arg0");
        ASSERT_TRUE(value.get() != NULL);
        ASSERT_TRUE(value->IsInt());
        ASSERT_EQ(2, value->GetIntValue());

        value = arguments[argct]->GetValue(L"arg1");
        ASSERT_TRUE(value.get() != NULL);
        ASSERT_TRUE(value->IsDouble());
        ASSERT_EQ(3.433, value->GetDoubleValue());

        value = arguments[argct]->GetValue(L"arg2");
        ASSERT_TRUE(value.get() != NULL);
        ASSERT_TRUE(value->IsBool());
        ASSERT_EQ(true, value->GetBoolValue());

        value = arguments[argct]->GetValue(L"arg3");
        ASSERT_TRUE(value.get() != NULL);
        ASSERT_TRUE(value->IsString());
        ASSERT_EQ(L"some string", value->GetStringValue());
      }
      argct++;

      // function that returns a value
      ASSERT_TRUE(arguments[argct]->IsFunction());
      {
        CefV8ValueList args;
        args.push_back(CefV8Value::CreateInt(5));
        args.push_back(CefV8Value::CreateDouble(3.5));
        args.push_back(CefV8Value::CreateBool(true));
        args.push_back(CefV8Value::CreateString(L"10"));
        CefRefPtr<CefV8Value> rv;
        std::wstring exception;
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
        std::wstring exception;
        ASSERT_TRUE(arguments[argct]->ExecuteFunction(
            arguments[argct], args, rv, exception));
        ASSERT_EQ(L"Uncaught My Exception", exception);
      }
      argct++;

      if(binding_test_)
      {
        // values
        value = object->GetValue(L"intVal");
        ASSERT_TRUE(value.get() != NULL);
        ASSERT_TRUE(value->IsInt());
        ASSERT_EQ(12, value->GetIntValue());

        value = object->GetValue(L"doubleVal");
        ASSERT_TRUE(value.get() != NULL);
        ASSERT_TRUE(value->IsDouble());
        ASSERT_EQ(5.432, value->GetDoubleValue());

        value = object->GetValue(L"boolVal");
        ASSERT_TRUE(value.get() != NULL);
        ASSERT_TRUE(value->IsBool());
        ASSERT_EQ(true, value->GetBoolValue());

        value = object->GetValue(L"stringVal");
        ASSERT_TRUE(value.get() != NULL);
        ASSERT_TRUE(value->IsString());
        ASSERT_EQ(L"the string", value->GetStringValue());

        value = object->GetValue(L"arrayVal");
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
          ASSERT_EQ(L"a string", value2->GetStringValue());
          subargct++;
        }
      }

      retval = CefV8Value::CreateInt(5);
    } else if(name == L"execute2") {
      g_V8TestV8HandlerExecute2Called = true;
      
      // check the result of calling the "execute" function
      ASSERT_EQ(1, arguments.size());
      ASSERT_TRUE(arguments[0]->IsInt());
      ASSERT_EQ(5, arguments[0]->GetIntValue());
    }
  }

  bool binding_test_;
};

class V8TestHandler : public TestHandler
{
public:
  V8TestHandler() {}

  virtual void RunTest()
  {
    // extension uses a global object
    std::string object = "test";

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
	
    AddResource(L"http://tests/run.html", testHtml.str(), L"text/html");
    CreateBrowser(L"http://tests/run.html");
  }

  virtual RetVal HandleLoadEnd(CefRefPtr<CefBrowser> browser,
                               CefRefPtr<CefFrame> frame)
  {
    if(!browser->IsPopup() && !frame.get())
      DestroyTest();
    return RV_CONTINUE;
  }
};

// Verify extensions
TEST(V8Test, Extension)
{
  g_V8TestV8HandlerExecuteCalled = false;
  g_V8TestV8HandlerExecute2Called = false;

  std::wstring extensionCode =
    L"var test;"
    L"if (!test)"
    L"  test = {};"
    L"(function() {"
    L"  test.execute = function(a,b,c,d,e,f,g,h) {"
    L"    native function execute();"
    L"    return execute(a,b,c,d,e,f,g,h);"
    L"  };"
    L"  test.execute2 = function(a) {"
    L"    native function execute2();"
    L"    return execute2(a);"
    L"  };"
    L"})();";
  CefRegisterExtension(L"v8/test", extensionCode, new V8TestV8Handler(false));

  CefRefPtr<V8TestHandler> handler = new V8TestHandler();
  handler->ExecuteTest();

  ASSERT_TRUE(g_V8TestV8HandlerExecuteCalled);
  ASSERT_TRUE(g_V8TestV8HandlerExecute2Called);
}
