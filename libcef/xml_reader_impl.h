// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef _XML_READER_IMPL_H
#define _XML_READER_IMPL_H

#include "../include/cef.h"
#include "base/threading/platform_thread.h"
#include <libxml/xmlreader.h>
#include <sstream>

// Implementation of CefXmlReader
class CefXmlReaderImpl : public CefXmlReader
{
public:
  CefXmlReaderImpl();
  ~CefXmlReaderImpl();

  // Initialize the reader context.
  bool Initialize(CefRefPtr<CefStreamReader> stream,
                  EncodingType encodingType, const CefString& URI);

  virtual bool MoveToNextNode();
  virtual bool Close();
  virtual bool HasError();
  virtual CefString GetError();
  virtual NodeType GetType();
  virtual int GetDepth();
  virtual CefString GetLocalName();
  virtual CefString GetPrefix();
  virtual CefString GetQualifiedName();
  virtual CefString GetNamespaceURI();
  virtual CefString GetBaseURI();
  virtual CefString GetXmlLang();
  virtual bool IsEmptyElement();
  virtual bool HasValue();
  virtual CefString GetValue();
  virtual bool HasAttributes();
  virtual size_t GetAttributeCount();
  virtual CefString GetAttribute(int index);
  virtual CefString GetAttribute(const CefString& qualifiedName);
  virtual CefString GetAttribute(const CefString& localName,
                                 const CefString& namespaceURI);
  virtual CefString GetInnerXml();
  virtual CefString GetOuterXml();
  virtual int GetLineNumber();
  virtual bool MoveToAttribute(int index);
  virtual bool MoveToAttribute(const CefString& qualifiedName);
  virtual bool MoveToAttribute(const CefString& localName,
                               const CefString& namespaceURI);
  virtual bool MoveToFirstAttribute();
  virtual bool MoveToNextAttribute();
  virtual bool MoveToCarryingElement();

  // Add another line to the error string.
  void AppendError(const CefString& error_str);

  // Verify that the reader exists and is being accessed from the correct
  // thread.
  bool VerifyContext();

protected:
  base::PlatformThreadId supported_thread_id_;
  CefRefPtr<CefStreamReader> stream_;
  xmlTextReaderPtr reader_;
  std::stringstream error_buf_;

  IMPLEMENT_REFCOUNTING(CefXMLReaderImpl);
};

#endif // _XML_READER_IMPL_H
