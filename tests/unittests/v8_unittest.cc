// Copyright (c) 2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef.h"
#include "include/cef_runnable.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "test_handler.h"

namespace {

bool g_V8TestV8HandlerExecuteCalled;
bool g_V8TestV8HandlerExecute2Called;

class V8TestV8Handler : public CefV8Handler
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
      ASSERT_EQ(date.day_of_week, 1);
      ASSERT_EQ(date.hour, 12);
      ASSERT_EQ(date.minute, 30);
      ASSERT_EQ(date.second, 10);
      ASSERT_EQ(date.millisecond, 100);
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

        value = object->GetValue("dateVal");
        ASSERT_TRUE(value.get() != NULL);
        ASSERT_TRUE(value->IsDate());
        CefTime date = value->GetDateValue();
        ASSERT_EQ(date.year, 2010);
        ASSERT_EQ(date.month, 5);
        ASSERT_EQ(date.day_of_week, 1);
        ASSERT_EQ(date.day_of_month, 3);
        ASSERT_EQ(date.hour, 12);
        ASSERT_EQ(date.minute, 30);
        ASSERT_EQ(date.second, 10);
        ASSERT_EQ(date.millisecond, 100);

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

  IMPLEMENT_REFCOUNTING(V8TestV8Handler);
};

class V8TestHandler : public TestHandler
{
public:
  V8TestHandler(bool bindingTest) { binding_test_ = bindingTest; }

  virtual void RunTest() OVERRIDE
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
                         int httpStatusCode) OVERRIDE
  {
    if(!browser->IsPopup() && frame->IsMain())
      DestroyTest();
  }

  virtual void OnJSBinding(CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefFrame> frame,
                           CefRefPtr<CefV8Value> object) OVERRIDE
  {
    if(binding_test_)
      TestHandleJSBinding(browser, frame, object);
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

    cef_time_t date = {2010, 5, 1, 3, 12, 30, 10, 100};
    ASSERT_TRUE(testObj->SetValue("dateVal", CefV8Value::CreateDate(date)));

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

} // namespace

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

// Using a delegate so that the code below can remain inline.
class CefV8HandlerDelegate
{
public:
  virtual bool Execute(const CefString& name,
                       CefRefPtr<CefV8Value> object,
                       const CefV8ValueList& arguments,
                       CefRefPtr<CefV8Value>& retval,
                       CefString& exception) = 0;

  virtual bool Get(const CefString& name, const CefRefPtr<CefV8Value> object, 
                   CefRefPtr<CefV8Value>& retval) = 0;

  virtual bool Set(const CefString& name, const CefRefPtr<CefV8Value> object, 
                   const CefRefPtr<CefV8Value> value) = 0;
};

class DelegatingV8Handler : public CefV8Handler
{
public:
  DelegatingV8Handler(CefV8HandlerDelegate *delegate): 
  delegate_(delegate) { }
  
  ~DelegatingV8Handler()
  {
  }
  
  bool Execute(const CefString& name, 
               CefRefPtr<CefV8Value> object,
               const CefV8ValueList& arguments,
               CefRefPtr<CefV8Value>& retval,
               CefString& exception)
  {
    return delegate_->Execute(name, object, arguments, retval, exception);
  }
  
private:
  CefV8HandlerDelegate *delegate_;

  IMPLEMENT_REFCOUNTING(DelegatingV8Handler);
};

class DelegatingV8Accessor: public CefV8Accessor
{
public:
  DelegatingV8Accessor(CefV8HandlerDelegate *delegate)
    : delegate_(delegate) { }

  bool Get(const CefString& name, const CefRefPtr<CefV8Value> object, 
           CefRefPtr<CefV8Value>& retval)
  {
    return delegate_->Get(name, object, retval);
  }

  bool Set(const CefString& name, const CefRefPtr<CefV8Value> object, 
           const CefRefPtr<CefV8Value> value)
  {
    return delegate_->Set(name, object, value);
  }

private:
  CefV8HandlerDelegate *delegate_;

  IMPLEMENT_REFCOUNTING(DelegatingV8Accessor);
};

class TestContextHandler: public TestHandler, public CefV8HandlerDelegate
{
public:
  TestContextHandler() {}
  
  virtual void RunTest() OVERRIDE
  {
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
    "try { point.x = -1; } catch(e) {  }\n" // should not have any effect.
    "try { point.y = point.x;  theY = point.y; } catch(e) { point.y = 4321; }\n"
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
                         int httpStatusCode) OVERRIDE
  {
  }
  
  virtual void OnJSBinding(CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefFrame> frame,
                           CefRefPtr<CefV8Value> object) OVERRIDE
  {
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
    object->SetValue("hello", helloFunc);
    
    CefRefPtr<CefV8Value> fromIFrameFunc = 
    CefV8Value::CreateFunction("fromIFrame", funcHandler);
    object->SetValue("fromIFrame", fromIFrameFunc);
    
    CefRefPtr<CefV8Value> goFunc = 
    CefV8Value::CreateFunction("begin", funcHandler);
    object->SetValue("begin", goFunc);
    
    CefRefPtr<CefV8Value> doneFunc = 
    CefV8Value::CreateFunction("end", funcHandler);
    object->SetValue("end", doneFunc);

    CefRefPtr<CefV8Value> compFunc = 
    CefV8Value::CreateFunction("comp", funcHandler);
    object->SetValue("comp", compFunc);

    // Create an object with accessor based properties:
    CefRefPtr<CefBase> blankBase;
    CefRefPtr<CefV8Accessor> accessor(new DelegatingV8Accessor(this));
    CefRefPtr<CefV8Value> point = CefV8Value::CreateObject(blankBase, accessor);

    point->SetValue("x", V8_ACCESS_CONTROL_DEFAULT, 
        V8_PROPERTY_ATTRIBUTE_READONLY);
    point->SetValue("y", V8_ACCESS_CONTROL_DEFAULT, 
        V8_PROPERTY_ATTRIBUTE_NONE);

    object->SetValue("point", point);
  }
  
  void CallIFrame()
  {
    CefV8ValueList args;
    CefRefPtr<CefV8Value> rv;
    CefString exception;
    CefRefPtr<CefV8Value> empty;
    ASSERT_TRUE(funcIFrame_->ExecuteFunctionWithContext(contextIFrame_, empty,
                                                        args, rv, exception));
  }
  
  void AsyncTestContext(CefRefPtr<CefV8Context> ec, 
                        CefRefPtr<CefV8Context> cc)
  {
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
                          CefRefPtr<CefV8Value> func)
  {
    CefV8ValueList args;
    CefRefPtr<CefV8Value> rv;
    CefString exception;
    CefRefPtr<CefV8Value> empty;
    ASSERT_TRUE(func->ExecuteFunctionWithContext(context, empty, args, rv,
                                                 exception));
    if(exception == "Uncaught My Exception")
      got_exception_.yes();
  }
  
  void AsyncTestNavigation(CefRefPtr<CefV8Context> context,
                           CefRefPtr<CefV8Value> func)
  {
    CefString exception;
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

      obj = CefV8Value::CreateObject(NULL);
      url = CefV8Value::CreateString("http://tests/end.html");

      obj->SetValue("url", url);
      obj->SetValue("foobar", foobarFunc);
      obj->SetValue("anArray", anArray);

      args.push_back(obj);
  
      ASSERT_TRUE(func->ExecuteFunctionWithContext(context, global, args, rv,
                                                   exception));
      if(exception.empty())
        got_navigation_.yes();
  
      context->Exit();
    }
  }
  
  bool Execute(const CefString& name, 
               CefRefPtr<CefV8Value> object,
               const CefV8ValueList& arguments,
               CefRefPtr<CefV8Value>& retval,
               CefString& exception)
  {
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
      if(arguments.size() == 2 && arguments[0]->IsString() &&
         arguments[1]->IsFunction()) {
        CefString msg = arguments[0]->GetStringValue();
        if(msg == "main") {
          got_hello_main_.yes();
          contextIFrame_ = cc;
          funcIFrame_ = arguments[1];
        }
      } else if(arguments.size() == 1 && arguments[0]->IsString()) {
        CefString msg = arguments[0]->GetStringValue();
        if(msg == "iframe")
          got_hello_iframe_.yes();
      }
      else
        return false;
      
      if(got_hello_main_ && got_hello_iframe_ && funcIFrame_->IsFunction()) {
        // NB: At this point, enteredURL == http://tests/iframe.html which is
        // expected since the iframe made the call on its own. The unexpected
        // behavior is that in the call to fromIFrame (below) the enteredURL
        // == http://tests/main.html even though the iframe.html context was 
        // entered first.
        //  -- Perhaps WebKit does something other than look at the bottom 
        //     of stack for the entered context.
        if(enteredURL == "http://tests/iframe.html")
          got_iframe_as_entered_url_.yes();
        CallIFrame();
      }
      return true;
    } else if(name == "fromIFrame") {
      if(enteredURL == "http://tests/main.html")
        got_correct_entered_url_.yes();
      if(currentURL == "http://tests/iframe.html")
        got_correct_current_url_.yes();
      CefPostTask(TID_UI, NewCefRunnableMethod(this, 
          &TestContextHandler::AsyncTestContext, ec, cc));
      return true;
    } else if(name == "begin") {
      if(arguments.size() == 2 && arguments[0]->IsFunction() &&
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
      if(arguments.size() == 3)
      {
        CefRefPtr<CefV8Value> expected = arguments[0];
        CefRefPtr<CefV8Value> one = arguments[1];
        CefRefPtr<CefV8Value> two = arguments[2];

        bool bExpected = expected->GetBoolValue();
        bool bOne2Two = one->IsSame(two);
        bool bTwo2One = two->IsSame(one);

        // IsSame should match the expected
        if ( bExpected != bOne2Two || bExpected != bTwo2One)
          got_bad_is_same_.yes();
      }
      else
      {
        got_bad_is_same_.yes();
      }
    } else if (name == "end") {
      got_testcomplete_.yes();
      DestroyTest();
      return true;
    }
    return false;
  }

  bool Get(const CefString& name, const CefRefPtr<CefV8Value> object, 
           CefRefPtr<CefV8Value>& retval)
  {
    if(name == "x") {
      got_point_x_read_.yes();
      retval = CefV8Value::CreateInt(1234);
      return true;
    } else if(name == "y") {
      got_point_y_read_.yes();
      retval = CefV8Value::CreateInt(y_);
      return true;
    }
    return false;
  }

  bool Set(const CefString& name, const CefRefPtr<CefV8Value> object, 
           const CefRefPtr<CefV8Value> value)
  {
    if(name == "y") {
      y_ = value->GetIntValue();
      if( y_ == 1234)
        got_point_y_write_.yes();
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
  TrackCallback got_navigation_;
  TrackCallback got_testcomplete_;

  int y_;
};

} // namespace

// Verify context works to allow async v8 callbacks
TEST(V8Test, Context)
{
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
  EXPECT_TRUE(handler->got_navigation_);
  EXPECT_TRUE(handler->got_testcomplete_);
}
