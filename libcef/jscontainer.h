// Copyright (c) 2008 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _JSCONTAINER_H
#define _JSCONTAINER_H

#include "../include/cef.h"
#include "third_party/npapi/bindings/npruntime.h"

class WebFrame;

// CefJSContainer lets you map Javascript method calls and property accesses
// directly to C++ method calls and CefVariant* variable access.
// Portions of the implementation are borrowed from
// webkit\glue\cpp_bound_class.cc
class CefJSContainer : public CefThreadSafeBase<CefBase>
{
public:
  CefJSContainer(CefBrowser* browser,
                 CefRefPtr<CefJSHandler> handler);
  ~CefJSContainer();

  // Given a WebFrame, BindToJavascript builds the NPObject that will represent
  // the class and binds it to the frame's window under the given name.  This
  // should generally be called from the WebView delegate's
  // WindowObjectCleared(). A class so bound will be accessible to JavaScript
  // as window.<classname>. The owner of the CefJSContainer is responsible for
  // keeping the object around while the frame is alive, and for destroying it
  // afterwards.
  void BindToJavascript(WebFrame* frame,
                        const std::wstring& classname);

  CefRefPtr<CefJSHandler> GetHandler() { return handler_; }

protected:
  bool HasMethod(NPIdentifier ident);
  bool HasProperty(NPIdentifier ident);
  bool Invoke(NPIdentifier ident,
              WebFrame* frame,
              const NPVariant* args,
              size_t arg_count, 
              NPVariant* result);
  bool GetProperty(NPIdentifier ident,
                   WebFrame* frame,
                   NPVariant* result);
  bool SetProperty(NPIdentifier ident,
                   WebFrame* frame,
                   const NPVariant* value);

  friend struct CefNPObject;

protected:
  CefBrowser* browser_;
  CefRefPtr<CefJSHandler> handler_;

  // A list of all NPObjects we created and bound in BindToJavascript(), so we
  // can clean them up when we're destroyed.
  typedef std::vector<NPObject*> BoundObjectList;
  BoundObjectList bound_objects_;
};


#endif // _JSHANDLER_CONTAINER_H
