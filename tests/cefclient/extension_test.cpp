// Copyright (c) 2008-2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "extension_test.h"


// Implementation of the V8 handler class for the "cef.test" extension.
class ClientV8ExtensionHandler : public CefV8Handler
{
public:
  ClientV8ExtensionHandler() : test_param_("An initial string value.") {}
  virtual ~ClientV8ExtensionHandler() {}

  // Execute with the specified argument list and return value.  Return true if
  // the method was handled.
  virtual bool Execute(const CefString& name,
                       CefRefPtr<CefV8Value> object,
                       const CefV8ValueList& arguments,
                       CefRefPtr<CefV8Value>& retval,
                       CefString& exception)
  {
    if(name == "SetTestParam")
    {
      // Handle the SetTestParam native function by saving the string argument
      // into the local member.
      if(arguments.size() != 1 || !arguments[0]->IsString())
        return false;
      
      test_param_ = arguments[0]->GetStringValue();
      return true;
    }
    else if(name == "GetTestParam")
    {
      // Handle the GetTestParam native function by returning the local member
      // value.
      retval = CefV8Value::CreateString(test_param_);
      return true;
    }
    else if(name == "GetTestObject")
    {
      // Handle the GetTestObject native function by creating and returning a
      // new V8 object.
      retval = CefV8Value::CreateObject(NULL);
      // Add a string parameter to the new V8 object.
      retval->SetValue("param", CefV8Value::CreateString(
          "Retrieving a parameter on a native object succeeded."));
      // Add a function to the new V8 object.
      retval->SetValue("GetMessage",
          CefV8Value::CreateFunction("GetMessage", this));
      return true;
    }
    else if(name == "GetMessage")
    {
      // Handle the GetMessage object function by returning a string.
      retval = CefV8Value::CreateString(
          "Calling a function on a native object succeeded.");
      return true;
    }
    return false;
  }

private:
  CefString test_param_;

  IMPLEMENT_REFCOUNTING(ClientV8ExtensionHandler);
};


void InitExtensionTest()
{
  // Register a V8 extension with the below JavaScript code that calls native
  // methods implemented in ClientV8ExtensionHandler.
  std::string code = "var cef;"
    "if (!cef)"
    "  cef = {};"
    "if (!cef.test)"
    "  cef.test = {};"
    "(function() {"
    "  cef.test.__defineGetter__('test_param', function() {"
    "    native function GetTestParam();"
    "    return GetTestParam();"
    "  });"
    "  cef.test.__defineSetter__('test_param', function(b) {"
    "    native function SetTestParam();"
    "    if(b) SetTestParam(b);"
    "  });"
    "  cef.test.test_object = function() {"
    "    native function GetTestObject();"
    "    return GetTestObject();"
    "  };"
    "})();";
  CefRegisterExtension("v8/test", code, new ClientV8ExtensionHandler());
}

void RunExtensionTest(CefRefPtr<CefBrowser> browser)
{
  std::string html =
    "<html><body>ClientV8ExtensionHandler says:<br><pre>"
    "<script language=\"JavaScript\">"
    "cef.test.test_param ="
    "  'Assign and retrieve a value succeeded the first time.';"
    "document.writeln(cef.test.test_param);"
    "cef.test.test_param ="
    "  'Assign and retrieve a value succeeded the second time.';"
    "document.writeln(cef.test.test_param);"
    "var obj = cef.test.test_object();"
    "document.writeln(obj.param);"
    "document.writeln(obj.GetMessage());"
    "</script>"
    "</pre></body></html>";
  browser->GetMainFrame()->LoadString(html, "about:blank");
}
