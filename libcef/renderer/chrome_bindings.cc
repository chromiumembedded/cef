// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/renderer/chrome_bindings.h"
#include "libcef/renderer/browser_impl.h"
#include <string>
#include "base/logging.h"
#include "base/values.h"

namespace scheme {

namespace {

void SetList(CefRefPtr<CefV8Value> source, base::ListValue* target);

// Transfer a V8 value to a List index.
void SetListValue(base::ListValue* list, int index,
                  CefRefPtr<CefV8Value> value) {
  if (value->IsArray()) {
    base::ListValue* new_list = new base::ListValue();
    SetList(value, new_list);
    list->Set(index, new_list);
  } else if (value->IsString()) {
    list->Set(index,
        base::Value::CreateStringValue(value->GetStringValue().ToString()));
  } else if (value->IsBool()) {
    list->Set(index, base::Value::CreateBooleanValue(value->GetBoolValue()));
  } else if (value->IsInt()) {
    list->Set(index, base::Value::CreateIntegerValue(value->GetIntValue()));
  } else if (value->IsDouble()) {
    list->Set(index, base::Value::CreateDoubleValue(value->GetDoubleValue()));
  }
}

// Transfer a V8 array to a List.
void SetList(CefRefPtr<CefV8Value> source, base::ListValue* target) {
  DCHECK(source->IsArray());

  int arg_length = source->GetArrayLength();
  if (arg_length == 0)
    return;

  for (int i = 0; i < arg_length; ++i)
    SetListValue(target, i, source->GetValue(i));
}

class V8Handler : public CefV8Handler {
public:
  V8Handler() {}

  virtual bool Execute(const CefString& name,
                       CefRefPtr<CefV8Value> object,
                       const CefV8ValueList& arguments,
                       CefRefPtr<CefV8Value>& retval,
                       CefString& exception) OVERRIDE {
    std::string nameStr = name;
    if (nameStr == "send") {
      if (arguments.size() > 0 && arguments.size() <= 2 &&
          arguments[0]->IsString()) {
        base::ListValue args;
        SetListValue(&args, 0, arguments[0]);
        if (arguments.size() > 1)
           SetListValue(&args, 1, arguments[1]);

        CefRefPtr<CefBrowserImpl> browser = static_cast<CefBrowserImpl*>(
            CefV8Context::GetCurrentContext()->GetBrowser().get());
        browser->SendProcessMessage(PID_BROWSER, kChromeProcessMessage, &args,
                                    false);

        retval = CefV8Value::CreateBool(true);
      } else {
        exception = "Invalid number of arguments or argument format";
      }
      return true;
    } else if (nameStr == "bind") {
      // Return the "send" object.
      DCHECK(object->GetFunctionName() == "send");
      retval = object;
      return true;
    }

    NOTREACHED();
    return false;
  }

  IMPLEMENT_REFCOUNTING(V8Handler);
};

}  // namespace

void OnContextCreated(CefRefPtr<CefBrowserImpl> browser,
                      CefRefPtr<CefFrameImpl> frame,
                      CefRefPtr<CefV8Context> context) {
  GURL url = GURL(frame->GetURL().ToString());
  if (url.scheme() != kChromeScheme)
    return;

  CefRefPtr<CefV8Value> global = context->GetGlobal();

  CefRefPtr<CefV8Handler> handler = new V8Handler();

  // Add "chrome".
  CefRefPtr<CefV8Value> chrome = CefV8Value::CreateObject(NULL);
  global->SetValue("chrome", chrome, V8_PROPERTY_ATTRIBUTE_NONE);

  // Add "chrome.send".
  CefRefPtr<CefV8Value> send = CefV8Value::CreateFunction("send", handler);
  chrome->SetValue("send", send, V8_PROPERTY_ATTRIBUTE_NONE);

  // Add "chrome.send.bind".
  CefRefPtr<CefV8Value> bind = CefV8Value::CreateFunction("bind", handler);
  send->SetValue("bind", bind, V8_PROPERTY_ATTRIBUTE_NONE);
}

}  // namespace scheme
