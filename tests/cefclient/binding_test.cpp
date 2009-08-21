// Copyright (c) 2008-2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "stdafx.h"
#include "binding_test.h"
#include <sstream>


// Implementation of the V8 handler class for the "window.cef_test.Dump"
// function.
class ClientV8FunctionHandler : public CefThreadSafeBase<CefV8Handler>
{
public:
  ClientV8FunctionHandler() {}
  virtual ~ClientV8FunctionHandler() {}

  // Execute with the specified argument list and return value.  Return true if
  // the method was handled.
  virtual bool Execute(const std::wstring& name,
                       CefRefPtr<CefV8Value> object,
                       const CefV8ValueList& arguments,
                       CefRefPtr<CefV8Value>& retval,
                       std::wstring& exception)
  {
    if(name == L"Dump")
    {
      // The "Dump" function will return a human-readable dump of the input
      // arguments.
      std::wstringstream stream;
      size_t i;

      for(i = 0; i < arguments.size(); ++i)
      {
        stream << L"arg[" << i << L"] = ";
        PrintValue(arguments[i], stream, 0);
        stream << L"\n";
      }

      retval = CefV8Value::CreateString(stream.str());
      return true;
    }
    else if(name == L"Call")
    {
      // The "Call" function will execute a function to get an object and then
      // return the result of calling a function belonging to that object.  The
      // first arument is the function that will return an object and the second
      // argument is the function that will be called on that returned object.
      int argSize = arguments.size();
      if(argSize < 2 || !arguments[0]->IsFunction()
          || !arguments[1]->IsString())
        return false;

      CefV8ValueList argList;
      
      // Execute the function stored in the first argument to retrieve an
      // object.
      CefRefPtr<CefV8Value> objectPtr;
      if(!arguments[0]->ExecuteFunction(object, argList, objectPtr, exception))
        return false;
      // Verify that the returned value is an object.
      if(!objectPtr.get() || !objectPtr->IsObject())
        return false;

      // Retrieve the member function specified by name in the second argument
      // from the object.
      CefRefPtr<CefV8Value> funcPtr =
          objectPtr->GetValue(arguments[1]->GetStringValue());
      // Verify that the returned value is a function.
      if(!funcPtr.get() || !funcPtr->IsFunction())
        return false;

      // Pass any additional arguments on to the member function.
      for(int i = 2; i < argSize; ++i)
        argList.push_back(arguments[i]);
      
      // Execute the member function.
      return funcPtr->ExecuteFunction(arguments[0], argList, retval, exception);
    }
    return false;
  }

  // Simple function for formatted output of a V8 value.
  void PrintValue(CefRefPtr<CefV8Value> value, std::wstringstream &stream,
                  int indent)
  {
    std::wstringstream indent_stream;
    for(int i = 0; i < indent; ++i)
      indent_stream << L"  ";
    std::wstring indent_str = indent_stream.str();
    
    if(value->IsUndefined())
      stream << L"(undefined)";
    else if(value->IsNull())
      stream << L"(null)";
    else if(value->IsBool())
      stream << L"(bool) " << (value->GetBoolValue() ? L"true" : L"false");
    else if(value->IsInt())
      stream << L"(int) " << value->GetIntValue();
    else if(value->IsDouble())
      stream << L"(double) " << value->GetDoubleValue();
    else if(value->IsString())
      stream << L"(string) " << value->GetStringValue().c_str();
    else if(value->IsFunction())
      stream << L"(function) " << value->GetFunctionName().c_str();
    else if(value->IsArray()) {
      stream << L"(array) [";
      int len = value->GetArrayLength();
      for(int i = 0; i < len; ++i) {
        stream << L"\n  " << indent_str.c_str() << i << L" = ";
        PrintValue(value->GetValue(i), stream, indent+1);
      }
      stream << L"\n" << indent_str.c_str() << L"]";
    } else if(value->IsObject()) {
      stream << L"(object) [";
      std::vector<std::wstring> keys;
      if(value->GetKeys(keys)) {
        for(size_t i = 0; i < keys.size(); ++i) {
          stream << L"\n  " << indent_str.c_str() << keys[i].c_str() << L" = ";
          PrintValue(value->GetValue(keys[i]), stream, indent+1);
        }
      }
      stream << L"\n" << indent_str.c_str() << L"]";
    }
  }
};


void InitBindingTest(CefRefPtr<CefBrowser> browser,
                     CefRefPtr<CefFrame> frame,
                     CefRefPtr<CefV8Value> object)
{
  // Create the new V8 object.
  CefRefPtr<CefV8Value> testObjPtr = CefV8Value::CreateObject(NULL);
  // Add the new V8 object to the global window object with the name
  // "cef_test".
  object->SetValue(L"cef_test", testObjPtr);

  // Create an instance of ClientV8FunctionHandler as the V8 handler.
  CefRefPtr<CefV8Handler> handlerPtr = new ClientV8FunctionHandler();

  // Add a new V8 function to the cef_test object with the name "Dump".
  testObjPtr->SetValue(L"Dump",
      CefV8Value::CreateFunction(L"Dump", handlerPtr));
  // Add a new V8 function to the cef_test object with the name "Call".
  testObjPtr->SetValue(L"Call",
      CefV8Value::CreateFunction(L"Call", handlerPtr));
}

void RunBindingTest(CefRefPtr<CefBrowser> browser)
{
  std::wstring html =
    L"<html><body>ClientV8FunctionHandler says:<br><pre>"
    L"<script language=\"JavaScript\">"
    L"document.writeln(window.cef_test.Dump(false, 1, 7.6654,'bar',"
    L"  [false,true],[5, 7.654, 1, 'foo', [true, 'bar'], 8]));"
    L"document.writeln(window.cef_test.Dump(cef));"
    L"document.writeln("
    L"  window.cef_test.Call(cef.test.test_object, 'GetMessage'));"
    L"function my_object() {"
    L"  var obj = {};"
    L"  (function() {"
    L"    obj.GetMessage = function(a) {"
    L"      return 'Calling a function with value '+a+' on a user object succeeded.';"
    L"    };"
    L"  })();"
    L"  return obj;"
    L"};"
    L"document.writeln("
    L"  window.cef_test.Call(my_object, 'GetMessage', 'foobar'));"
    L"</script>"
    L"</pre></body></html>";
  browser->GetMainFrame()->LoadString(html, L"about:blank");
}
