// Copyright (c) 2008-2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "binding_test.h"
#include <sstream>


// Implementation of the V8 handler class for the "window.cef_test.Dump"
// function.
class ClientV8FunctionHandler : public CefV8Handler
{
public:
  ClientV8FunctionHandler() {}
  virtual ~ClientV8FunctionHandler() {}

  // Execute with the specified argument list and return value.  Return true if
  // the method was handled.
  virtual bool Execute(const CefString& name,
                       CefRefPtr<CefV8Value> object,
                       const CefV8ValueList& arguments,
                       CefRefPtr<CefV8Value>& retval,
                       CefString& exception)
  {
    if(name == "Dump")
    {
      // The "Dump" function will return a human-readable dump of the input
      // arguments.
      std::stringstream stream;
      size_t i;

      for(i = 0; i < arguments.size(); ++i)
      {
        stream << "arg[" << i << "] = ";
        PrintValue(arguments[i], stream, 0);
        stream << "\n";
      }

      retval = CefV8Value::CreateString(stream.str());
      return true;
    }
    else if(name == "Call")
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
  void PrintValue(CefRefPtr<CefV8Value> value, std::stringstream &stream,
                  int indent)
  {
    std::stringstream indent_stream;
    for(int i = 0; i < indent; ++i)
      indent_stream << "  ";
    std::string indent_str = indent_stream.str();
    
    if(value->IsUndefined())
      stream << "(undefined)";
    else if(value->IsNull())
      stream << "(null)";
    else if(value->IsBool())
      stream << "(bool) " << (value->GetBoolValue() ? "true" : "false");
    else if(value->IsInt())
      stream << "(int) " << value->GetIntValue();
    else if(value->IsDouble())
      stream << "(double) " << value->GetDoubleValue();
    else if(value->IsString())
      stream << "(string) " << std::string(value->GetStringValue());
    else if(value->IsFunction())
      stream << "(function) " << std::string(value->GetFunctionName());
    else if(value->IsArray()) {
      stream << "(array) [";
      int len = value->GetArrayLength();
      for(int i = 0; i < len; ++i) {
        stream << "\n  " << indent_str.c_str() << i << " = ";
        PrintValue(value->GetValue(i), stream, indent+1);
      }
      stream << "\n" << indent_str.c_str() << "]";
    } else if(value->IsObject()) {
      stream << "(object) [";
      std::vector<CefString> keys;
      if(value->GetKeys(keys)) {
        for(size_t i = 0; i < keys.size(); ++i) {
          stream << "\n  " << indent_str.c_str() << keys[i].c_str() << " = ";
          PrintValue(value->GetValue(keys[i]), stream, indent+1);
        }
      }
      stream << "\n" << indent_str.c_str() << "]";
    }
  }

  IMPLEMENT_REFCOUNTING(ClientV8FunctionHandler);
};


void InitBindingTest(CefRefPtr<CefBrowser> browser,
                     CefRefPtr<CefFrame> frame,
                     CefRefPtr<CefV8Value> object)
{
  // Create the new V8 object.
  CefRefPtr<CefV8Value> testObjPtr = CefV8Value::CreateObject(NULL);
  // Add the new V8 object to the global window object with the name
  // "cef_test".
  object->SetValue("cef_test", testObjPtr);

  // Create an instance of ClientV8FunctionHandler as the V8 handler.
  CefRefPtr<CefV8Handler> handlerPtr = new ClientV8FunctionHandler();

  // Add a new V8 function to the cef_test object with the name "Dump".
  testObjPtr->SetValue("Dump",
      CefV8Value::CreateFunction("Dump", handlerPtr));
  // Add a new V8 function to the cef_test object with the name "Call".
  testObjPtr->SetValue("Call",
      CefV8Value::CreateFunction("Call", handlerPtr));
}

void RunBindingTest(CefRefPtr<CefBrowser> browser)
{
  std::string html =
    "<html><body>ClientV8FunctionHandler says:<br><pre>"
    "<script language=\"JavaScript\">"
    "document.writeln(window.cef_test.Dump(false, 1, 7.6654,'bar',"
    "  [false,true],[5, 7.654, 1, 'foo', [true, 'bar'], 8]));"
    "document.writeln(window.cef_test.Dump(cef));"
    "document.writeln("
    "  window.cef_test.Call(cef.test.test_object, 'GetMessage'));"
    "function my_object() {"
    "  var obj = {};"
    "  (function() {"
    "    obj.GetMessage = function(a) {"
    "      return 'Calling a function with value '+a+' on a user object succeeded.';"
    "    };"
    "  })();"
    "  return obj;"
    "};"
    "document.writeln("
    "  window.cef_test.Call(my_object, 'GetMessage', 'foobar'));"
    "</script>"
    "</pre></body></html>";
  browser->GetMainFrame()->LoadString(html, "about:blank");
}
