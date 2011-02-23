// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef _DOM_EVENT_IMPL_H
#define _DOM_EVENT_IMPL_H

#include "include/cef.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDOMEvent.h"

class CefDOMDocumentImpl;

class CefDOMEventImpl : public CefThreadSafeBase<CefDOMEvent>
{
public:
  CefDOMEventImpl(CefRefPtr<CefDOMDocumentImpl> document,
                  const WebKit::WebDOMEvent& event);
  virtual ~CefDOMEventImpl();
  
  virtual CefString GetType();
  virtual Category GetCategory();
  virtual Phase GetPhase();
  virtual bool CanBubble();
  virtual bool CanCancel();
  virtual CefRefPtr<CefDOMDocument> GetDocument();
  virtual CefRefPtr<CefDOMNode> GetTarget();
  virtual CefRefPtr<CefDOMNode> GetCurrentTarget();

  // Will be called from CefDOMEventListenerWrapper::handleEvent().
  void Detach();

  // Verify that the object exists and is being accessed on the UI thread.
  bool VerifyContext();

protected:
  CefRefPtr<CefDOMDocumentImpl> document_;
  WebKit::WebDOMEvent event_;
};

#endif // _DOM_EVENT_IMPL_H
