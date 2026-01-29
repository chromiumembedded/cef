This page explains how to use V8 JavaScript integration in client applications.

**Contents**

- [Introduction](#introduction)
- [ExecuteJavaScript](#executejavascript)
- [Window Binding](#window-binding)
- [Extensions](#extensions)
- [Basic JS Types](#basic-js-types)
- [JS Arrays](#js-arrays)
- [JS Objects](#js-objects)
  - [Objects with Accessors](#objects-with-accessors)
- [JS Functions](#js-functions)
  - [Functions and Window Binding](#functions-and-window-binding)
  - [Functions and Extensions](#functions-and-extensions)
- [Working with Contexts](#working-with-contexts)
- [Executing Functions](#executing-functions)
  - [Using JS Callbacks](#using-js-callbacks)
  - [Rethrowing Exceptions](#rethrowing-exceptions)

---

# Introduction

Chromium and CEF use the [V8 JavaScript Engine](http://code.google.com/p/v8/) for their internal JavaScript (JS) implementation. Each frame in a browser has its own JS context that provides scope and security for JS code executing in that frame (see the "Working with Contexts" section for more information). CEF exposes a large number of JS features for integration in client applications.

With CEF3 Blink (WebKit) and JS execution run in a separate renderer process. The main thread in a renderer process is identified as TID\_RENDERER and all V8 execution must take place on this thread. Callbacks related to JS execution are exposed via the CefRenderProcessHandler interface. This interface is retrieved via CefApp::GetRenderProcessHandler() when a new renderer process is initialized.

JS APIs that communicate between the browser and renderer processes should be designed using asynchronous callbacks. See the "Asynchronous JavaScript Bindings" section of the [General Usage](general_usage.md) wiki page for more information.

# ExecuteJavaScript

The simplest way to execute JS from a client application is using the CefFrame::ExecuteJavaScript() function. This function is available in both the browser process and the renderer process and can safely be used from outside of a JS context.

```cpp
CefRefPtr<CefBrowser> browser = ...;
CefRefPtr<CefFrame> frame = browser->GetMainFrame();
frame->ExecuteJavaScript("alert('ExecuteJavaScript works!');",
    frame->GetURL(), 0);
```

The above example will cause `alert('ExecuteJavaScript works!');` to be executed in the browser's main frame.

The ExecuteJavaScript() function can be used to interact with functions and variables in the frame's JS context. In order to return values from JS to the client application consider using Window Binding or Extensions.


# Window Binding

Window binding allows the client application to attach values to a frame's `window` object. Window bindings are implemented using the CefRenderProcessHandler::OnContextCreated() method.

```cpp
void MyRenderProcessHandler::OnContextCreated(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefV8Context> context) {
  // Retrieve the context's window object.
  CefRefPtr<CefV8Value> object = context->GetGlobal();

  // Create a new V8 string value. See the "Basic JS Types" section below.
  CefRefPtr<CefV8Value> str = CefV8Value::CreateString("My Value!");

  // Add the string to the window object as "window.myval". See the "JS Objects" section below.
  object->SetValue("myval", str, V8_PROPERTY_ATTRIBUTE_NONE);
}
```

JavaScript in the frame can then interact with the window bindings.

```html
<script language="JavaScript">
alert(window.myval); // Shows an alert box with "My Value!"
</script>
```

Window bindings are reloaded each time a frame is reloaded giving the client application an opportunity to change the bindings if necessary. For example, different frames may be given access to different features in the client application by modifying the values that are bound to the window object for that frame.


# Extensions

Extensions are like window bindings except they are loaded into the context for every frame and cannot be modified once loaded. The DOM does not exist when an extension is loaded and attempts to access the DOM during extension loading will result in a crash. Extensions are registered using the CefRegisterExtension() function which should be called from the CefRenderProcessHandler::OnWebKitInitialized() method.

```cpp
void MyRenderProcessHandler::OnWebKitInitialized() {
  // Define the extension contents.
  std::string extensionCode =
    "var test;"
    "if (!test)"
    "  test = {};"
    "(function() {"
    "  test.myval = 'My Value!';"
    "})();";

  // Register the extension.
  CefRegisterExtension("v8/test", extensionCode, NULL);
}
```

The string represented by `extensionCode` can be any valid JS code. JS in the frame can then interact with the extension code.

```html
<script language="JavaScript">
alert(test.myval); // Shows an alert box with "My Value!"
</script>
```

# Basic JS Types

CEF supports the creation of basic JS data types including undefined, null, bool, int, double, date and string. These types are created using the CefV8Value::Create\*() static methods. For example, to create a new JS string value use the CreateString() method.

```cpp
CefRefPtr<CefV8Value> str = CefV8Value::CreateString("My Value!");
```

Basic value types can be created at any time and are not initially associated with a particular context (see the "Working with Contexts" section for more information).

To test the value type use the Is\*() methods.

```cpp
CefRefPtr<CefV8Value> val = ...;
if (val.IsString()) {
  // The value is a string.
}
```

To retrieve the value use the Get\*Value() methods.

```
CefString strVal = val.GetStringValue();
```

# JS Arrays

Arrays are created using the CefV8Value::CreateArray() static method which accepts a length argument. Arrays can only be created and used from within a context (see the "Working with Contexts" section for more information).

```cpp
// Create an array that can contain two values.
CefRefPtr<CefV8Value> arr = CefV8Value::CreateArray(2);
```

Values are assigned to an array using the SetValue() method variant that takes an index as the first argument.

```cpp
// Add two values to the array.
arr->SetValue(0, CefV8Value::CreateString("My First String!"));
arr->SetValue(1, CefV8Value::CreateString("My Second String!"));
```

To test if a CefV8Value is an array use the IsArray() method. To get the length of an array use the GetArrayLength() method. To get a value from an array use the GetValue() variant that takes an index as the first argument.

# JS Objects

Objects are created using the CefV8Value::CreateObject() static method that takes an optional CefV8Accessor argument. Objects can only be created and used from within a context (see the "Working with Contexts" section for more information).

```cpp
CefRefPtr<CefV8Value> obj = CefV8Value::CreateObject(NULL);
```

Values are assigned to an object using the SetValue() method variant that takes a key string as the first argument.

```cpp
obj->SetValue("myval", CefV8Value::CreateString("My String!"));
```

## Objects with Accessors

Objects can optionally have an associated CefV8Accessor that provides a native implementation for getting and setting values.

```cpp
CefRefPtr<CefV8Accessor> accessor = …;
CefRefPtr<CefV8Value> obj = CefV8Value::CreateObject(accessor);
```

An implementation of the CefV8Accessor interface that must be provided by the client application.

```cpp
class MyV8Accessor : public CefV8Accessor {
public:
  MyV8Accessor() {}

  virtual bool Get(const CefString& name,
                   const CefRefPtr<CefV8Value> object,
                   CefRefPtr<CefV8Value>& retval,
                   CefString& exception) OVERRIDE {
    if (name == "myval") {
      // Return the value.
      retval = CefV8Value::CreateString(myval_);
      return true;
    }

    // Value does not exist.
    return false;
  }

  virtual bool Set(const CefString& name,
                   const CefRefPtr<CefV8Value> object,
                   const CefRefPtr<CefV8Value> value,
                   CefString& exception) OVERRIDE {
    if (name == "myval") {
      if (value->IsString()) {
        // Store the value.
        myval_ = value->GetStringValue();
      } else {
        // Throw an exception.
        exception = "Invalid value type";
      }
      return true;
    }

    // Value does not exist.
    return false;
  }

  // Variable used for storing the value.
  CefString myval_;

  // Provide the reference counting implementation for this class.
  IMPLEMENT_REFCOUNTING(MyV8Accessor);
};
```

In order for a value to be passed to the accessor it must be set using the SetValue() method variant that accepts AccessControl and PropertyAttribute arguments.

```
obj->SetValue("myval", V8_ACCESS_CONTROL_DEFAULT, 
    V8_PROPERTY_ATTRIBUTE_NONE);
```

# JS Functions

CEF supports the creation of JS functions with native implementations. Functions are created using the CefV8Value::CreateFunction() static method that accepts name and CefV8Handler arguments. Functions can only be created and used from within a context (see the "Working with Contexts" section for more information).

```cpp
CefRefPtr<CefV8Handler> handler = …;
CefRefPtr<CefV8Value> func = CefV8Value::CreateFunction("myfunc", handler);
```

An implementation of the CefV8Handler interface that must be provided by the client application.

```cpp
class MyV8Handler : public CefV8Handler {
public:
  MyV8Handler() {}

  virtual bool Execute(const CefString& name,
                       CefRefPtr<CefV8Value> object,
                       const CefV8ValueList& arguments,
                       CefRefPtr<CefV8Value>& retval,
                       CefString& exception) OVERRIDE {
    if (name == "myfunc") {
      // Return my string value.
      retval = CefV8Value::CreateString("My Value!");
      return true;
    }

    // Function does not exist.
    return false;
  }

  // Provide the reference counting implementation for this class.
  IMPLEMENT_REFCOUNTING(MyV8Handler);
};
```

## Functions and Window Binding

Functions can be used to create complex window bindings.

```cpp
void MyRenderProcessHandler::OnContextCreated(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefV8Context> context) {
  // Retrieve the context's window object.
  CefRefPtr<CefV8Value> object = context->GetGlobal();

  // Create an instance of my CefV8Handler object.
  CefRefPtr<CefV8Handler> handler = new MyV8Handler();

  // Create the "myfunc" function.
  CefRefPtr<CefV8Value> func = CefV8Value::CreateFunction("myfunc", handler);

  // Add the "myfunc" function to the "window" object.
  object->SetValue("myfunc", func, V8_PROPERTY_ATTRIBUTE_NONE);
}
```

```html
<script language="JavaScript">
alert(window.myfunc()); // Shows an alert box with "My Value!"
</script>
```

## Functions and Extensions

Functions can be used to create complex extensions. Note the use of the "native function" forward declaration which is required when using extensions.

```cpp
void MyRenderProcessHandler::OnWebKitInitialized() {
  // Define the extension contents.
  std::string extensionCode =
    "var test;"
    "if (!test)"
    "  test = {};"
    "(function() {"
    "  test.myfunc = function() {"
    "    native function myfunc();"
    "    return myfunc();"
    "  };"
    "})();";

  // Create an instance of my CefV8Handler object.
  CefRefPtr<CefV8Handler> handler = new MyV8Handler();

  // Register the extension.
  CefRegisterExtension("v8/test", extensionCode, handler);
}
```

```html
<script language="JavaScript">
alert(test.myfunc()); // Shows an alert box with "My Value!"
</script>
```


# Working with Contexts

Each frame in a browser window has its own V8 context. The context defines the scope for all variables, objects and functions defined in that frame. V8 will be inside a context if the current code location has a CefV8Handler, CefV8Accessor or OnContextCreated()/OnContextReleased() callback higher in the call stack.

The OnContextCreated() and OnContextReleased() methods define the complete life span for the V8 context associated with a frame. You should be careful to follow the below rules when using these methods:

1. Do not hold onto or use a V8 context reference past the call to OnContextReleased() for that context.

2. The lifespan of all V8 objects is unspecified (up to the GC). Be careful when maintaining references directly from V8 objects to your own internal implementation objects. In many cases it may be better to use a proxy object that your application associates with the V8 context and which can be "disconnected" (allowing your internal implementation object to be freed) when OnContextReleased() is called for the context.

If V8 is not currently inside a context, or if you need to retrieve and store a reference to a context, you can use one of two available CefV8Context static methods. GetCurrentContext() returns the context for the frame that is currently executing JS. GetEnteredContext() returns the context for the frame where JS execution began. For example, if a function in frame1 calls a function in frame2 then the current context will be frame2 and the entered context will be frame1.

Arrays, objects and functions may only be created, modified and, in the case of functions, executed, if V8 is inside a context. If V8 is not inside a context then the application needs to enter a context by calling Enter() and exit the context by calling Exit(). The Enter() and Exit() methods should only be used:

1. When creating a V8 object, function or array outside of an existing context. For example, when creating a JS object in response to a native menu callback.

2. When creating a V8 object, function or array in a context other than the current context. For example, if a call originating from frame1 needs to modify the context of frame2.

# Executing Functions

Native code can execute JS functions by using the ExecuteFunction() and ExecuteFunctionWithContext() methods. The ExecuteFunction() method should only be used if V8 is already inside a context as described in the "Working with Contexts" section. The ExecuteFunctionWithContext() method allows the application to specify the context that will be entered for execution.

## Using JS Callbacks

When registering a JS function callback with native code the application should store a reference to both the current context and the JS function in the native code. This could be implemented as follows.

1\. Create a "register" function in OnJSBinding().

```cpp
void MyRenderProcessHandler::OnContextCreated(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    CefRefPtr<CefV8Context> context) {
  // Retrieve the context's window object.
  CefRefPtr<CefV8Value> object = context->GetGlobal();

  CefRefPtr<CefV8Handler> handler = new MyV8Handler(this);
  object->SetValue("register",
                   CefV8Value::CreateFunction("register", handler),
                   V8_PROPERTY_ATTRIBUTE_NONE);
}
```

2\. In the MyV8Handler::Execute() implementation for the "register" function keep a reference to both the context and the function.

```cpp
bool MyV8Handler::Execute(const CefString& name,
                          CefRefPtr<CefV8Value> object,
                          const CefV8ValueList& arguments,
                          CefRefPtr<CefV8Value>& retval,
                          CefString& exception) {
  if (name == "register") {
    if (arguments.size() == 1 && arguments[0]->IsFunction()) {
      callback_func_ = arguments[0];
      callback_context_ = CefV8Context::GetCurrentContext();
      return true;
    }
  }

  return false;
}
```

3\. Register the JS callback via JavaScript.

```html
<script language="JavaScript">
function myFunc() {
  // do something in JS.
}
window.register(myFunc);
</script>
```

4\. Execute the JS callback at some later time.

```cpp
CefV8ValueList args;
CefRefPtr<CefV8Value> retval;
CefRefPtr<CefV8Exception> exception;
if (callback_func_->ExecuteFunctionWithContext(callback_context_, NULL, args, retval, exception, false)) {
  if (exception.get()) {
    // Execution threw an exception.
  } else {
    // Execution succeeded.
  }
}
```

See the [Asynchronous JavaScript Bindings](general_usage.md#asynchronous-javascript-bindings) section of the GeneralUsage wiki page for more information on using callbacks.

## Rethrowing Exceptions

If CefV8Value::SetRethrowExceptions(true) is called before CefV8Value::ExecuteFunction\*() then any exceptions generated by V8 during function execution will be immediately rethrown. If an exception is rethrown any native code needs to immediately return. Exceptions should only be rethrown if there is a JS call higher in the call stack. For example, consider the following call stacks where "JS" is a JS function and "EF" is a native ExecuteFunction call:

Stack 1: JS1 -> EF1 -> JS2 -> EF2

Stack 2: Native Menu -> EF1 -> JS2 -> EF2

With stack 1 rethrow should be true for both EF1 and EF2. With stack 2 rethrow should false for EF1 and true for EF2.

This can be implemented by having two varieties of call sites for EF in the native code:

1. Only called from a V8 handler. This covers EF 1 and EF2 in stack 1 and EF2 in stack 2. Rethrow is always true.

2. Only called natively. This covers EF1 in stack 2. Rethrow is always false.

Be very careful when rethrowing exceptions. Incorrect usage (for example, calling ExecuteFunction() immediately after exception has been rethrown) may cause your application to crash or malfunction in hard to debug ways.