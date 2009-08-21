// Copyright (c) 2008-2009 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "stdafx.h"
#include "extension_test.h"


// Implementation of the V8 handler class for the "cef.test" extension.
class ClientV8ExtensionHandler : public CefThreadSafeBase<CefV8Handler>
{
public:
  ClientV8ExtensionHandler() : test_param_(L"An initial string value.") {}
  virtual ~ClientV8ExtensionHandler() {}

  // Execute with the specified argument list and return value.  Return true if
  // the method was handled.
  virtual bool Execute(const std::wstring& name,
                       CefRefPtr<CefV8Value> object,
                       const CefV8ValueList& arguments,
                       CefRefPtr<CefV8Value>& retval,
                       std::wstring& exception)
  {
    if(name == L"SetTestParam")
    {
      // Handle the SetTestParam native function by saving the string argument
      // into the local member.
      if(arguments.size() != 1 || !arguments[0]->IsString())
        return false;
      
      test_param_ = arguments[0]->GetStringValue();
      return true;
    }
    else if(name == L"GetTestParam")
    {
      // Handle the GetTestParam native function by returning the local member
      // value.
      retval = CefV8Value::CreateString(test_param_);
      return true;
    }
    else if(name == L"GetTestObject")
    {
      // Handle the GetTestObject native function by creating and returning a
      // new V8 object.
      retval = CefV8Value::CreateObject(NULL);
      // Add a string parameter to the new V8 object.
      retval->SetValue(L"param", CefV8Value::CreateString(
          L"Retrieving a parameter on a native object succeeded."));
      // Add a function to the new V8 object.
      retval->SetValue(L"GetMessage",
          CefV8Value::CreateFunction(L"GetMessage", this));
      return true;
    }
    else if(name == L"GetMessage")
    {
      // Handle the GetMessage object function by returning a string.
      retval = CefV8Value::CreateString(
          L"Calling a function on a native object succeeded.");
      return true;
    }
    return false;
  }

private:
  std::wstring test_param_;
};


void InitExtensionTest()
{
  // Register a V8 extension with the below JavaScript code that calls native
  // methods implemented in ClientV8ExtensionHandler.
  std::wstring code = L"var cef;"
    L"if (!cef)"
    L"  cef = {};"
    L"if (!cef.test)"
    L"  cef.test = {};"
    L"(function() {"
    L"  cef.test.__defineGetter__('test_param', function() {"
    L"    native function GetTestParam();"
    L"    return GetTestParam();"
    L"  });"
    L"  cef.test.__defineSetter__('test_param', function(b) {"
    L"    native function SetTestParam();"
    L"    if(b) SetTestParam(b);"
    L"  });"
    L"  cef.test.test_object = function() {"
    L"    native function GetTestObject();"
    L"    return GetTestObject();"
    L"  };"
    L"})();";
  CefRegisterExtension(L"v8/test", code, new ClientV8ExtensionHandler());
}

void RunExtensionTest(CefRefPtr<CefBrowser> browser)
{
  std::wstring html =
    L"<html><body>ClientV8ExtensionHandler says:<br><pre>"
    L"<script language=\"JavaScript\">"
    L"cef.test.test_param ="
    L"  'Assign and retrieve a value succeeded the first time.';"
    L"document.writeln(cef.test.test_param);"
    L"cef.test.test_param ="
    L"  'Assign and retrieve a value succeeded the second time.';"
    L"document.writeln(cef.test.test_param);"
    L"var obj = cef.test.test_object();"
    L"document.writeln(obj.param);"
    L"document.writeln(obj.GetMessage());"
    L"</script>"
    L"</pre></body></html>";
  browser->GetMainFrame()->LoadString(html, L"about:blank");
}
