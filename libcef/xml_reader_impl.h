// Copyright (c) 20010 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef _XML_READER_IMPL_H
#define _XML_READER_IMPL_H

#include "../include/cef.h"
#include "base/platform_thread.h"
#include <libxml/xmlreader.h>
#include <sstream>

// Implementation of CefXmlReader
class CefXmlReaderImpl : public CefThreadSafeBase<CefXmlReader>
{
public:
  CefXmlReaderImpl();
  ~CefXmlReaderImpl();

  // Initialize the reader context.
  bool Initialize(CefRefPtr<CefStreamReader> stream,
                  EncodingType encodingType, const std::wstring& URI);

  virtual bool MoveToNextElement();
  virtual bool Close();
  virtual bool HasError();
  virtual std::wstring GetError();
  virtual NodeType GetType();
  virtual int GetDepth();
  virtual std::wstring GetLocalName();
  virtual std::wstring GetPrefix();
  virtual std::wstring GetQualifiedName();
  virtual std::wstring GetNamespaceURI();
  virtual std::wstring GetBaseURI();
  virtual std::wstring GetXmlLang();
  virtual bool IsEmptyElement();
  virtual bool HasValue();
  virtual std::wstring GetValue();
  virtual bool HasAttributes();
  virtual size_t GetAttributeCount();
  virtual std::wstring GetAttribute(int index);
  virtual std::wstring GetAttribute(const std::wstring& qualifiedName);
  virtual std::wstring GetAttribute(const std::wstring& localName,
                                    const std::wstring& namespaceURI);
  virtual std::wstring GetInnerXml();
  virtual std::wstring GetOuterXml();
  virtual int GetLineNumber();
  virtual bool MoveToAttribute(int index);
  virtual bool MoveToAttribute(const std::wstring& qualifiedName);
  virtual bool MoveToAttribute(const std::wstring& localName,
                               const std::wstring& namespaceURI);
  virtual bool MoveToFirstAttribute();
  virtual bool MoveToNextAttribute();
  virtual bool MoveToCarryingElement();

  // Add another line to the error string.
  void AppendError(const std::wstring& error_str);

  // Verify that the reader exists and is being accessed from the correct
  // thread.
  bool VerifyContext();

protected:
  PlatformThreadId supported_thread_id_;
  xmlTextReaderPtr reader_;
  std::wstringstream error_buf_;
};

#endif // _XML_READER_IMPL_H
