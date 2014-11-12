// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_XML_READER_IMPL_H_
#define CEF_LIBCEF_BROWSER_XML_READER_IMPL_H_
#pragma once

#include <libxml/xmlreader.h>
#include <sstream>

#include "include/cef_xml_reader.h"
#include "base/threading/platform_thread.h"

// Implementation of CefXmlReader
class CefXmlReaderImpl : public CefXmlReader {
 public:
  CefXmlReaderImpl();
  ~CefXmlReaderImpl() override;

  // Initialize the reader context.
  bool Initialize(CefRefPtr<CefStreamReader> stream,
                  EncodingType encodingType, const CefString& URI);

  bool MoveToNextNode() override;
  bool Close() override;
  bool HasError() override;
  CefString GetError() override;
  NodeType GetType() override;
  int GetDepth() override;
  CefString GetLocalName() override;
  CefString GetPrefix() override;
  CefString GetQualifiedName() override;
  CefString GetNamespaceURI() override;
  CefString GetBaseURI() override;
  CefString GetXmlLang() override;
  bool IsEmptyElement() override;
  bool HasValue() override;
  CefString GetValue() override;
  bool HasAttributes() override;
  size_t GetAttributeCount() override;
  CefString GetAttribute(int index) override;
  CefString GetAttribute(const CefString& qualifiedName) override;
  CefString GetAttribute(const CefString& localName,
                         const CefString& namespaceURI) override;
  CefString GetInnerXml() override;
  CefString GetOuterXml() override;
  int GetLineNumber() override;
  bool MoveToAttribute(int index) override;
  bool MoveToAttribute(const CefString& qualifiedName) override;
  bool MoveToAttribute(const CefString& localName,
                       const CefString& namespaceURI) override;
  bool MoveToFirstAttribute() override;
  bool MoveToNextAttribute() override;
  bool MoveToCarryingElement() override;

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

  IMPLEMENT_REFCOUNTING(CefXmlReaderImpl);
};

#endif  // CEF_LIBCEF_BROWSER_XML_READER_IMPL_H_
