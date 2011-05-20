// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef _DOM_NODE_IMPL_H
#define _DOM_NODE_IMPL_H

#include "include/cef.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNode.h"

class CefDOMDocumentImpl;

class CefDOMNodeImpl : public CefDOMNode
{
public:
  CefDOMNodeImpl(CefRefPtr<CefDOMDocumentImpl> document,
                 const WebKit::WebNode& node);
  virtual ~CefDOMNodeImpl();

  virtual Type GetType() OVERRIDE;
  virtual bool IsText() OVERRIDE;
  virtual bool IsElement() OVERRIDE;
  virtual bool IsSame(CefRefPtr<CefDOMNode> that) OVERRIDE;
  virtual CefString GetName() OVERRIDE;
  virtual CefString GetValue() OVERRIDE;
  virtual bool SetValue(const CefString& value) OVERRIDE;
  virtual CefString GetAsMarkup() OVERRIDE;
  virtual CefRefPtr<CefDOMDocument> GetDocument() OVERRIDE;
  virtual CefRefPtr<CefDOMNode> GetParent() OVERRIDE;
  virtual CefRefPtr<CefDOMNode> GetPreviousSibling() OVERRIDE;
  virtual CefRefPtr<CefDOMNode> GetNextSibling() OVERRIDE;
  virtual bool HasChildren() OVERRIDE;
  virtual CefRefPtr<CefDOMNode> GetFirstChild() OVERRIDE;
  virtual CefRefPtr<CefDOMNode> GetLastChild() OVERRIDE;
  virtual void AddEventListener(const CefString& eventType,
                                CefRefPtr<CefDOMEventListener> listener,
                                bool useCapture) OVERRIDE;
  virtual CefString GetElementTagName() OVERRIDE;
  virtual bool HasElementAttributes() OVERRIDE;
  virtual bool HasElementAttribute(const CefString& attrName) OVERRIDE;
  virtual CefString GetElementAttribute(const CefString& attrName) OVERRIDE;
  virtual void GetElementAttributes(AttributeMap& attrMap) OVERRIDE;
  virtual bool SetElementAttribute(const CefString& attrName,
                                   const CefString& value) OVERRIDE;
  virtual CefString GetElementInnerText() OVERRIDE;

  // Will be called from CefDOMDocumentImpl::Detach().
  void Detach();

  // Verify that the object exists and is being accessed on the UI thread.
  bool VerifyContext();

protected:
  CefRefPtr<CefDOMDocumentImpl> document_;
  WebKit::WebNode node_;

  IMPLEMENT_REFCOUNTING(CefDOMNodeImpl);
};

#endif // _DOM_NODE_IMPL_H
