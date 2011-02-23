// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef _DOM_NODE_IMPL_H
#define _DOM_NODE_IMPL_H

#include "include/cef.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNode.h"

class CefDOMDocumentImpl;

class CefDOMNodeImpl : public CefThreadSafeBase<CefDOMNode>
{
public:
  CefDOMNodeImpl(CefRefPtr<CefDOMDocumentImpl> document,
                 const WebKit::WebNode& node);
  virtual ~CefDOMNodeImpl();

  virtual Type GetType();
  virtual bool IsText();
  virtual bool IsElement();
  virtual CefString GetName();
  virtual CefString GetValue();
  virtual bool SetValue(const CefString& value);
  virtual CefString GetAsMarkup();
  virtual CefRefPtr<CefDOMDocument> GetDocument();
  virtual CefRefPtr<CefDOMNode> GetParent();
  virtual CefRefPtr<CefDOMNode> GetPreviousSibling();
  virtual CefRefPtr<CefDOMNode> GetNextSibling();
  virtual bool HasChildren();
  virtual CefRefPtr<CefDOMNode> GetFirstChild();
  virtual CefRefPtr<CefDOMNode> GetLastChild();
  virtual void AddEventListener(const CefString& eventType,
                                CefRefPtr<CefDOMEventListener> listener,
                                bool useCapture);
  virtual CefString GetElementTagName();
  virtual bool HasElementAttributes();
  virtual bool HasElementAttribute(const CefString& attrName);
  virtual CefString GetElementAttribute(const CefString& attrName);
  virtual void GetElementAttributes(AttributeMap& attrMap);
  virtual bool SetElementAttribute(const CefString& attrName,
                                   const CefString& value);
  virtual CefString GetElementInnerText();

  // Will be called from CefDOMDocumentImpl::Detach().
  void Detach();

  // Verify that the object exists and is being accessed on the UI thread.
  bool VerifyContext();

protected:
  CefRefPtr<CefDOMDocumentImpl> document_;
  WebKit::WebNode node_;
};

#endif // _DOM_NODE_IMPL_H
