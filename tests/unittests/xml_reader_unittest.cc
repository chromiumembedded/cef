// Copyright (c) 2010 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef.h"
#include "include/cef_wrapper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

char g_test_xml[] =
    "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n"
    "<?my_instruction my_value?>\n"
    "<!DOCTYPE my_document SYSTEM \"example.dtd\" [\n"
    "    <!ENTITY EA \"EA Value\">\n"
    "    <!ENTITY EB \"EB Value\">\n"
    "]>\n"
    "<ns:obj xmlns:ns=\"http://www.example.org/ns\">\n"
    "  <ns:objA>value A</ns:objA>\n"
    "  <!-- my comment -->\n"
    "  <ns:objB>\n"
    "    <ns:objB_1>value B1</ns:objB_1>\n"
    "    <ns:objB_2><![CDATA[some <br/> data]]></ns:objB_2>\n"
    "    <ns:objB_3>&EB;</ns:objB_3>\n"
    "    <ns:objB_4><b>this is</b> mixed content &EA;</ns:objB_4>\n"
    "  </ns:objB>\n"
    "  <ns:objC ns:attr1=\"value C1\" ns:attr2=\"value C2\"/><ns:objD></ns:objD>\n"
    "</ns:obj>\n";

} // namespace

// Test XML reading
TEST(XmlReaderTest, Read)
{
  // Create the stream reader.
  CefRefPtr<CefStreamReader> stream(
      CefStreamReader::CreateForData(g_test_xml, sizeof(g_test_xml) - 1));
  ASSERT_TRUE(stream.get() != NULL);

  // Create the XML reader.
  CefRefPtr<CefXmlReader> reader(
      CefXmlReader::Create(stream, XML_ENCODING_NONE,
      L"http://www.example.org/example.xml"));
  ASSERT_TRUE(reader.get() != NULL);

  // Move to the processing instruction node.
  ASSERT_TRUE(reader->MoveToNextElement());
  ASSERT_EQ(reader->GetDepth(), 0);
  ASSERT_EQ(reader->GetType(), XML_NODE_PROCESSING_INSTRUCTION);
  ASSERT_EQ(reader->GetLocalName(), L"my_instruction");
  ASSERT_EQ(reader->GetQualifiedName(), L"my_instruction");
  ASSERT_TRUE(reader->HasValue());
  ASSERT_EQ(reader->GetValue(), L"my_value");

  // Move to the DOCTYPE node.
  ASSERT_TRUE(reader->MoveToNextElement());
  ASSERT_EQ(reader->GetDepth(), 0);
  ASSERT_EQ(reader->GetType(), XML_NODE_DOCUMENT_TYPE);
  ASSERT_EQ(reader->GetLocalName(), L"my_document");
  ASSERT_EQ(reader->GetQualifiedName(), L"my_document");
  ASSERT_FALSE(reader->HasValue());

  // Move to ns:obj element start node.
  ASSERT_TRUE(reader->MoveToNextElement());
  ASSERT_EQ(reader->GetDepth(), 0);
  ASSERT_EQ(reader->GetType(), XML_NODE_ELEMENT_START);
  ASSERT_EQ(reader->GetLocalName(), L"obj");
  ASSERT_EQ(reader->GetPrefix(), L"ns");
  ASSERT_EQ(reader->GetQualifiedName(), L"ns:obj");
  ASSERT_EQ(reader->GetNamespaceURI(), L"http://www.example.org/ns");
  ASSERT_TRUE(reader->HasAttributes());
  ASSERT_EQ(reader->GetAttributeCount(), 1);
  ASSERT_EQ(reader->GetAttribute(0), L"http://www.example.org/ns");
  ASSERT_EQ(reader->GetAttribute(L"xmlns:ns"), L"http://www.example.org/ns");
  ASSERT_EQ(reader->GetAttribute(L"ns", L"http://www.w3.org/2000/xmlns/"),
      L"http://www.example.org/ns");

  // Move to the whitespace node.
  ASSERT_TRUE(reader->MoveToNextElement());
  ASSERT_EQ(reader->GetType(), XML_NODE_WHITESPACE);

  // Move to the ns:objA element start node.
  ASSERT_TRUE(reader->MoveToNextElement());
  ASSERT_EQ(reader->GetDepth(), 1);
  ASSERT_EQ(reader->GetType(), XML_NODE_ELEMENT_START);
  ASSERT_EQ(reader->GetLocalName(), L"objA");
  ASSERT_EQ(reader->GetPrefix(), L"ns");
  ASSERT_EQ(reader->GetQualifiedName(), L"ns:objA");
  ASSERT_EQ(reader->GetNamespaceURI(), L"http://www.example.org/ns");
  ASSERT_FALSE(reader->IsEmptyElement());
  ASSERT_FALSE(reader->HasAttributes());
  ASSERT_FALSE(reader->HasValue());

  // Move to the ns:objA value node.
  ASSERT_TRUE(reader->MoveToNextElement());
  ASSERT_EQ(reader->GetDepth(), 2);
  ASSERT_EQ(reader->GetType(), XML_NODE_TEXT);
  ASSERT_EQ(reader->GetLocalName(), L"#text");
  ASSERT_EQ(reader->GetQualifiedName(), L"#text");
  ASSERT_TRUE(reader->HasValue());
  ASSERT_EQ(reader->GetValue(), L"value A");

  // Move to the ns:objA element ending node.
  ASSERT_TRUE(reader->MoveToNextElement());
  ASSERT_EQ(reader->GetDepth(), 1);
  ASSERT_EQ(reader->GetType(), XML_NODE_ELEMENT_END);
  ASSERT_EQ(reader->GetLocalName(), L"objA");
  ASSERT_EQ(reader->GetPrefix(), L"ns");
  ASSERT_EQ(reader->GetQualifiedName(), L"ns:objA");
  ASSERT_EQ(reader->GetNamespaceURI(), L"http://www.example.org/ns");
  ASSERT_FALSE(reader->IsEmptyElement());
  ASSERT_FALSE(reader->HasAttributes());
  ASSERT_FALSE(reader->HasValue());

  // Move to the whitespace node.
  ASSERT_TRUE(reader->MoveToNextElement());
  ASSERT_EQ(reader->GetDepth(), 1);
  ASSERT_EQ(reader->GetType(), XML_NODE_WHITESPACE);

  // Move to the comment node.
  ASSERT_TRUE(reader->MoveToNextElement());
  ASSERT_EQ(reader->GetDepth(), 1);
  ASSERT_EQ(reader->GetType(), XML_NODE_COMMENT);
  ASSERT_EQ(reader->GetLocalName(), L"#comment");
  ASSERT_EQ(reader->GetQualifiedName(), L"#comment");
  ASSERT_TRUE(reader->HasValue());
  ASSERT_EQ(reader->GetValue(), L" my comment ");
  
  // Move to the whitespace node.
  ASSERT_TRUE(reader->MoveToNextElement());
  ASSERT_EQ(reader->GetType(), XML_NODE_WHITESPACE);

  // Move to the ns:objB element start node.
  ASSERT_TRUE(reader->MoveToNextElement());
  ASSERT_EQ(reader->GetDepth(), 1);
  ASSERT_EQ(reader->GetType(), XML_NODE_ELEMENT_START);
  ASSERT_EQ(reader->GetLocalName(), L"objB");
  ASSERT_EQ(reader->GetPrefix(), L"ns");
  ASSERT_EQ(reader->GetQualifiedName(), L"ns:objB");
  ASSERT_EQ(reader->GetNamespaceURI(), L"http://www.example.org/ns");
  ASSERT_FALSE(reader->IsEmptyElement());
  ASSERT_FALSE(reader->HasAttributes());
  ASSERT_FALSE(reader->HasValue());

  // Move to the whitespace node.
  ASSERT_TRUE(reader->MoveToNextElement());
  ASSERT_EQ(reader->GetType(), XML_NODE_WHITESPACE);

  // Move to the ns:objB_1 element start node.
  ASSERT_TRUE(reader->MoveToNextElement());
  ASSERT_EQ(reader->GetDepth(), 2);
  ASSERT_EQ(reader->GetType(), XML_NODE_ELEMENT_START);
  ASSERT_EQ(reader->GetLocalName(), L"objB_1");
  ASSERT_EQ(reader->GetPrefix(), L"ns");
  ASSERT_EQ(reader->GetQualifiedName(), L"ns:objB_1");
  ASSERT_EQ(reader->GetNamespaceURI(), L"http://www.example.org/ns");
  ASSERT_FALSE(reader->IsEmptyElement());
  ASSERT_FALSE(reader->HasAttributes());
  ASSERT_FALSE(reader->HasValue());

  // Move to the ns:objB_1 value node.
  ASSERT_TRUE(reader->MoveToNextElement());
  ASSERT_EQ(reader->GetDepth(), 3);
  ASSERT_EQ(reader->GetType(), XML_NODE_TEXT);
  ASSERT_TRUE(reader->HasValue());
  ASSERT_EQ(reader->GetValue(), L"value B1");

  // Move to the ns:objB_1 element ending node.
  ASSERT_TRUE(reader->MoveToNextElement());
  ASSERT_EQ(reader->GetDepth(), 2);
  ASSERT_EQ(reader->GetType(), XML_NODE_ELEMENT_END);
  ASSERT_EQ(reader->GetLocalName(), L"objB_1");
  ASSERT_EQ(reader->GetPrefix(), L"ns");
  ASSERT_EQ(reader->GetQualifiedName(), L"ns:objB_1");
  ASSERT_EQ(reader->GetNamespaceURI(), L"http://www.example.org/ns");
  ASSERT_FALSE(reader->IsEmptyElement());
  ASSERT_FALSE(reader->HasAttributes());
  ASSERT_FALSE(reader->HasValue());

  // Move to the whitespace node.
  ASSERT_TRUE(reader->MoveToNextElement());
  ASSERT_EQ(reader->GetType(), XML_NODE_WHITESPACE);

  // Move to the ns:objB_2 element start node.
  ASSERT_TRUE(reader->MoveToNextElement());
  ASSERT_EQ(reader->GetDepth(), 2);
  ASSERT_EQ(reader->GetType(), XML_NODE_ELEMENT_START);
  ASSERT_EQ(reader->GetLocalName(), L"objB_2");
  ASSERT_EQ(reader->GetPrefix(), L"ns");
  ASSERT_EQ(reader->GetQualifiedName(), L"ns:objB_2");
  ASSERT_EQ(reader->GetNamespaceURI(), L"http://www.example.org/ns");
  ASSERT_FALSE(reader->IsEmptyElement());
  ASSERT_FALSE(reader->HasAttributes());
  ASSERT_FALSE(reader->HasValue());

  // Move to the ns:objB_2 value node.
  ASSERT_TRUE(reader->MoveToNextElement());
  ASSERT_EQ(reader->GetDepth(), 3);
  ASSERT_EQ(reader->GetType(), XML_NODE_CDATA);
  ASSERT_TRUE(reader->HasValue());
  ASSERT_EQ(reader->GetValue(), L"some <br/> data");

  // Move to the ns:objB_2 element ending node.
  ASSERT_TRUE(reader->MoveToNextElement());
  ASSERT_EQ(reader->GetDepth(), 2);
  ASSERT_EQ(reader->GetType(), XML_NODE_ELEMENT_END);
  ASSERT_EQ(reader->GetLocalName(), L"objB_2");
  ASSERT_EQ(reader->GetPrefix(), L"ns");
  ASSERT_EQ(reader->GetQualifiedName(), L"ns:objB_2");
  ASSERT_EQ(reader->GetNamespaceURI(), L"http://www.example.org/ns");
  ASSERT_FALSE(reader->IsEmptyElement());
  ASSERT_FALSE(reader->HasAttributes());
  ASSERT_FALSE(reader->HasValue());

  // Move to the whitespace node.
  ASSERT_TRUE(reader->MoveToNextElement());
  ASSERT_EQ(reader->GetType(), XML_NODE_WHITESPACE);

  // Move to the ns:objB_3 element start node.
  ASSERT_TRUE(reader->MoveToNextElement());
  ASSERT_EQ(reader->GetDepth(), 2);
  ASSERT_EQ(reader->GetType(), XML_NODE_ELEMENT_START);
  ASSERT_EQ(reader->GetLocalName(), L"objB_3");
  ASSERT_EQ(reader->GetPrefix(), L"ns");
  ASSERT_EQ(reader->GetQualifiedName(), L"ns:objB_3");
  ASSERT_EQ(reader->GetNamespaceURI(), L"http://www.example.org/ns");
  ASSERT_FALSE(reader->IsEmptyElement());
  ASSERT_FALSE(reader->HasAttributes());
  ASSERT_FALSE(reader->HasValue());

  // Move to the EB entity reference node.
  ASSERT_TRUE(reader->MoveToNextElement());
  ASSERT_EQ(reader->GetDepth(), 3);
  ASSERT_EQ(reader->GetType(), XML_NODE_ENTITY_REFERENCE);
  ASSERT_EQ(reader->GetLocalName(), L"EB");
  ASSERT_EQ(reader->GetQualifiedName(), L"EB");
  ASSERT_TRUE(reader->HasValue());
  ASSERT_EQ(reader->GetValue(), L"EB Value");
  
  // Move to the ns:objB_3 element ending node.
  ASSERT_TRUE(reader->MoveToNextElement());
  ASSERT_EQ(reader->GetDepth(), 2);
  ASSERT_EQ(reader->GetType(), XML_NODE_ELEMENT_END);
  ASSERT_EQ(reader->GetLocalName(), L"objB_3");
  ASSERT_EQ(reader->GetPrefix(), L"ns");
  ASSERT_EQ(reader->GetQualifiedName(), L"ns:objB_3");
  ASSERT_EQ(reader->GetNamespaceURI(), L"http://www.example.org/ns");
  ASSERT_FALSE(reader->IsEmptyElement());
  ASSERT_FALSE(reader->HasAttributes());
  ASSERT_FALSE(reader->HasValue());

  // Move to the whitespace node.
  ASSERT_TRUE(reader->MoveToNextElement());
  ASSERT_EQ(reader->GetType(), XML_NODE_WHITESPACE);

  // Move to the ns:objB_4 element start node.
  ASSERT_TRUE(reader->MoveToNextElement());
  ASSERT_EQ(reader->GetDepth(), 2);
  ASSERT_EQ(reader->GetType(), XML_NODE_ELEMENT_START);
  ASSERT_EQ(reader->GetLocalName(), L"objB_4");
  ASSERT_EQ(reader->GetPrefix(), L"ns");
  ASSERT_EQ(reader->GetQualifiedName(), L"ns:objB_4");
  ASSERT_EQ(reader->GetNamespaceURI(), L"http://www.example.org/ns");
  ASSERT_FALSE(reader->IsEmptyElement());
  ASSERT_FALSE(reader->HasAttributes());
  ASSERT_FALSE(reader->HasValue());
  ASSERT_EQ(reader->GetInnerXml(), L"<b>this is</b> mixed content &EA;");
  ASSERT_EQ(reader->GetOuterXml(),
      L"<ns:objB_4 xmlns:ns=\"http://www.example.org/ns\">"
      L"<b>this is</b> mixed content &EA;</ns:objB_4>");

  // Move to the <b> element node.
  ASSERT_TRUE(reader->MoveToNextElement());
  ASSERT_EQ(reader->GetDepth(), 3);
  ASSERT_EQ(reader->GetType(), XML_NODE_ELEMENT_START);
  ASSERT_EQ(reader->GetLocalName(), L"b");
  ASSERT_EQ(reader->GetQualifiedName(), L"b");
  ASSERT_FALSE(reader->IsEmptyElement());
  ASSERT_FALSE(reader->HasAttributes());
  ASSERT_FALSE(reader->HasValue());
  
  // Move to the text node.
  ASSERT_TRUE(reader->MoveToNextElement());
  ASSERT_EQ(reader->GetDepth(), 4);
  ASSERT_EQ(reader->GetType(), XML_NODE_TEXT);
  ASSERT_EQ(reader->GetLocalName(), L"#text");
  ASSERT_EQ(reader->GetQualifiedName(), L"#text");
  ASSERT_TRUE(reader->HasValue());
  ASSERT_EQ(reader->GetValue(), L"this is");
  
  // Move to the </b> element node.
  ASSERT_TRUE(reader->MoveToNextElement());
  ASSERT_EQ(reader->GetDepth(), 3);
  ASSERT_EQ(reader->GetType(), XML_NODE_ELEMENT_END);
  ASSERT_EQ(reader->GetLocalName(), L"b");
  ASSERT_EQ(reader->GetQualifiedName(), L"b");
  
  // Move to the text node.
  ASSERT_TRUE(reader->MoveToNextElement());
  ASSERT_EQ(reader->GetDepth(), 3);
  ASSERT_EQ(reader->GetType(), XML_NODE_TEXT);
  ASSERT_EQ(reader->GetLocalName(), L"#text");
  ASSERT_EQ(reader->GetQualifiedName(), L"#text");
  ASSERT_TRUE(reader->HasValue());
  ASSERT_EQ(reader->GetValue(), L" mixed content ");
  
  // Move to the EA entity reference node.
  ASSERT_TRUE(reader->MoveToNextElement());
  ASSERT_EQ(reader->GetDepth(), 3);
  ASSERT_EQ(reader->GetType(), XML_NODE_ENTITY_REFERENCE);
  ASSERT_EQ(reader->GetLocalName(), L"EA");
  ASSERT_EQ(reader->GetQualifiedName(), L"EA");
  ASSERT_TRUE(reader->HasValue());
  ASSERT_EQ(reader->GetValue(), L"EA Value");
  
  // Move to the ns:objB_4 element ending node.
  ASSERT_TRUE(reader->MoveToNextElement());
  ASSERT_EQ(reader->GetDepth(), 2);
  ASSERT_EQ(reader->GetType(), XML_NODE_ELEMENT_END);
  ASSERT_EQ(reader->GetLocalName(), L"objB_4");
  ASSERT_EQ(reader->GetPrefix(), L"ns");
  ASSERT_EQ(reader->GetQualifiedName(), L"ns:objB_4");
  ASSERT_EQ(reader->GetNamespaceURI(), L"http://www.example.org/ns");
  ASSERT_FALSE(reader->IsEmptyElement());
  ASSERT_FALSE(reader->HasAttributes());
  ASSERT_FALSE(reader->HasValue());

  // Move to the whitespace node.
  ASSERT_TRUE(reader->MoveToNextElement());
  ASSERT_EQ(reader->GetType(), XML_NODE_WHITESPACE);

  // Move to the ns:objB element ending node.
  ASSERT_TRUE(reader->MoveToNextElement());
  ASSERT_EQ(reader->GetDepth(), 1);
  ASSERT_EQ(reader->GetType(), XML_NODE_ELEMENT_END);
  ASSERT_EQ(reader->GetLocalName(), L"objB");
  ASSERT_EQ(reader->GetPrefix(), L"ns");
  ASSERT_EQ(reader->GetQualifiedName(), L"ns:objB");
  ASSERT_EQ(reader->GetNamespaceURI(), L"http://www.example.org/ns");
  ASSERT_FALSE(reader->IsEmptyElement());
  ASSERT_FALSE(reader->HasAttributes());
  ASSERT_FALSE(reader->HasValue());

  // Move to the whitespace node.
  ASSERT_TRUE(reader->MoveToNextElement());
  ASSERT_EQ(reader->GetType(), XML_NODE_WHITESPACE);

  // Move to the ns:objC element start node.
  ASSERT_TRUE(reader->MoveToNextElement());
  ASSERT_EQ(reader->GetDepth(), 1);
  ASSERT_EQ(reader->GetType(), XML_NODE_ELEMENT_START);
  ASSERT_EQ(reader->GetLocalName(), L"objC");
  ASSERT_EQ(reader->GetPrefix(), L"ns");
  ASSERT_EQ(reader->GetQualifiedName(), L"ns:objC");
  ASSERT_EQ(reader->GetNamespaceURI(), L"http://www.example.org/ns");
  ASSERT_TRUE(reader->IsEmptyElement());
  ASSERT_TRUE(reader->HasAttributes());
  ASSERT_FALSE(reader->HasValue());
  ASSERT_EQ(reader->GetAttributeCount(), 2);
  ASSERT_EQ(reader->GetAttribute(0), L"value C1");
  ASSERT_EQ(reader->GetAttribute(L"ns:attr1"), L"value C1");
  ASSERT_EQ(reader->GetAttribute(L"attr1", L"http://www.example.org/ns"),
      L"value C1");
  ASSERT_EQ(reader->GetAttribute(1), L"value C2");
  ASSERT_EQ(reader->GetAttribute(L"ns:attr2"), L"value C2");
  ASSERT_EQ(reader->GetAttribute(L"attr2", L"http://www.example.org/ns"),
      L"value C2");
  
  // Move to the ns:attr1 attribute.
  ASSERT_TRUE(reader->MoveToFirstAttribute());
  ASSERT_EQ(reader->GetDepth(), 2);
  ASSERT_EQ(reader->GetType(), XML_NODE_ATTRIBUTE);
  ASSERT_EQ(reader->GetLocalName(), L"attr1");
  ASSERT_EQ(reader->GetPrefix(), L"ns");
  ASSERT_EQ(reader->GetQualifiedName(), L"ns:attr1");
  ASSERT_EQ(reader->GetNamespaceURI(), L"http://www.example.org/ns");
  ASSERT_TRUE(reader->HasValue());
  ASSERT_FALSE(reader->IsEmptyElement());
  ASSERT_FALSE(reader->HasAttributes());
  ASSERT_EQ(reader->GetValue(), L"value C1");
  
  // Move to the ns:attr2 attribute.
  ASSERT_TRUE(reader->MoveToNextAttribute());
  ASSERT_EQ(reader->GetDepth(), 2);
  ASSERT_EQ(reader->GetType(), XML_NODE_ATTRIBUTE);
  ASSERT_EQ(reader->GetLocalName(), L"attr2");
  ASSERT_EQ(reader->GetPrefix(), L"ns");
  ASSERT_EQ(reader->GetQualifiedName(), L"ns:attr2");
  ASSERT_EQ(reader->GetNamespaceURI(), L"http://www.example.org/ns");
  ASSERT_TRUE(reader->HasValue());
  ASSERT_FALSE(reader->IsEmptyElement());
  ASSERT_FALSE(reader->HasAttributes());
  ASSERT_EQ(reader->GetValue(), L"value C2");

  // No more attributes.
  ASSERT_FALSE(reader->MoveToNextAttribute());

  // Return to the ns:objC element start node.
  ASSERT_TRUE(reader->MoveToCarryingElement());
  ASSERT_EQ(reader->GetDepth(), 1);
  ASSERT_EQ(reader->GetType(), XML_NODE_ELEMENT_START);
  ASSERT_EQ(reader->GetQualifiedName(), L"ns:objC");
  
  // Move to the ns:attr1 attribute.
  ASSERT_TRUE(reader->MoveToAttribute(0));
  ASSERT_EQ(reader->GetDepth(), 2);
  ASSERT_EQ(reader->GetType(), XML_NODE_ATTRIBUTE);
  ASSERT_EQ(reader->GetLocalName(), L"attr1");
  ASSERT_EQ(reader->GetPrefix(), L"ns");
  ASSERT_EQ(reader->GetQualifiedName(), L"ns:attr1");
  ASSERT_EQ(reader->GetNamespaceURI(), L"http://www.example.org/ns");
  ASSERT_TRUE(reader->HasValue());
  ASSERT_FALSE(reader->IsEmptyElement());
  ASSERT_FALSE(reader->HasAttributes());
  ASSERT_EQ(reader->GetValue(), L"value C1");
  
  // Return to the ns:objC element start node.
  ASSERT_TRUE(reader->MoveToCarryingElement());
  ASSERT_EQ(reader->GetDepth(), 1);
  ASSERT_EQ(reader->GetType(), XML_NODE_ELEMENT_START);
  ASSERT_EQ(reader->GetQualifiedName(), L"ns:objC");
  
  // Move to the ns:attr2 attribute.
  ASSERT_TRUE(reader->MoveToAttribute(L"ns:attr2"));
  ASSERT_EQ(reader->GetDepth(), 2);
  ASSERT_EQ(reader->GetType(), XML_NODE_ATTRIBUTE);
  ASSERT_EQ(reader->GetLocalName(), L"attr2");
  ASSERT_EQ(reader->GetPrefix(), L"ns");
  ASSERT_EQ(reader->GetQualifiedName(), L"ns:attr2");
  ASSERT_EQ(reader->GetNamespaceURI(), L"http://www.example.org/ns");
  ASSERT_TRUE(reader->HasValue());
  ASSERT_FALSE(reader->IsEmptyElement());
  ASSERT_FALSE(reader->HasAttributes());
  ASSERT_EQ(reader->GetValue(), L"value C2");
  
  // Move to the ns:attr1 attribute without returning to the ns:objC element.
  ASSERT_TRUE(reader->MoveToAttribute(L"attr1", L"http://www.example.org/ns"));
  ASSERT_EQ(reader->GetDepth(), 2);
  ASSERT_EQ(reader->GetType(), XML_NODE_ATTRIBUTE);
  ASSERT_EQ(reader->GetLocalName(), L"attr1");
  ASSERT_EQ(reader->GetPrefix(), L"ns");
  ASSERT_EQ(reader->GetQualifiedName(), L"ns:attr1");
  ASSERT_EQ(reader->GetNamespaceURI(), L"http://www.example.org/ns");
  ASSERT_TRUE(reader->HasValue());
  ASSERT_FALSE(reader->IsEmptyElement());
  ASSERT_FALSE(reader->HasAttributes());
  ASSERT_EQ(reader->GetValue(), L"value C1");
  
  // Move to the ns:objD element start node.
  ASSERT_TRUE(reader->MoveToNextElement());
  ASSERT_EQ(reader->GetDepth(), 1);
  ASSERT_EQ(reader->GetType(), XML_NODE_ELEMENT_START);
  ASSERT_EQ(reader->GetLocalName(), L"objD");
  ASSERT_EQ(reader->GetPrefix(), L"ns");
  ASSERT_EQ(reader->GetQualifiedName(), L"ns:objD");
  ASSERT_FALSE(reader->IsEmptyElement());
  ASSERT_FALSE(reader->HasAttributes());
  ASSERT_FALSE(reader->HasValue());

  // Move to the ns:objD element end node.
  ASSERT_TRUE(reader->MoveToNextElement());
  ASSERT_EQ(reader->GetDepth(), 1);
  ASSERT_EQ(reader->GetType(), XML_NODE_ELEMENT_END);
  ASSERT_EQ(reader->GetLocalName(), L"objD");
  ASSERT_EQ(reader->GetPrefix(), L"ns");
  ASSERT_EQ(reader->GetQualifiedName(), L"ns:objD");
  ASSERT_FALSE(reader->IsEmptyElement());
  ASSERT_FALSE(reader->HasAttributes());
  ASSERT_FALSE(reader->HasValue());
  
  // Move to the whitespace node without returning to the ns:objC element.
  ASSERT_TRUE(reader->MoveToNextElement());
  ASSERT_EQ(reader->GetType(), XML_NODE_WHITESPACE);

  // Move to ns:obj element ending node.
  ASSERT_TRUE(reader->MoveToNextElement());
  ASSERT_EQ(reader->GetDepth(), 0);
  ASSERT_EQ(reader->GetType(), XML_NODE_ELEMENT_END);
  ASSERT_EQ(reader->GetLocalName(), L"obj");
  ASSERT_EQ(reader->GetPrefix(), L"ns");
  ASSERT_EQ(reader->GetQualifiedName(), L"ns:obj");
  ASSERT_EQ(reader->GetNamespaceURI(), L"http://www.example.org/ns");
  ASSERT_FALSE(reader->IsEmptyElement());
  ASSERT_TRUE(reader->HasAttributes());
  ASSERT_FALSE(reader->HasValue());
  // Strangely, the end node will report if the starting node has attributes
  // but will not provide access to them.
  ASSERT_TRUE(reader->HasAttributes());
  ASSERT_EQ(reader->GetAttributeCount(), 0);

  // And we're done.
  ASSERT_FALSE(reader->MoveToNextElement());

  ASSERT_TRUE(reader->Close());
}

// Test XML read error handling.
TEST(XmlReaderTest, ReadError)
{
  char test_str[] =
    "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n"
    "<!ATTRIBUTE foo bar>\n";

  // Create the stream reader.
  CefRefPtr<CefStreamReader> stream(
      CefStreamReader::CreateForData(test_str, sizeof(test_str) - 1));
  ASSERT_TRUE(stream.get() != NULL);

  // Create the XML reader.
  CefRefPtr<CefXmlReader> reader(
      CefXmlReader::Create(stream, XML_ENCODING_NONE,
      L"http://www.example.org/example.xml"));
  ASSERT_TRUE(reader.get() != NULL);

  // Move to the processing instruction node and generate parser error.
  ASSERT_FALSE(reader->MoveToNextElement());
  ASSERT_TRUE(reader->HasError());
}

// Test XmlObject load behavior.
TEST(XmlReaderTest, ObjectLoad)
{
  // Create the stream reader.
  CefRefPtr<CefStreamReader> stream(
      CefStreamReader::CreateForData(g_test_xml, sizeof(g_test_xml) - 1));
  ASSERT_TRUE(stream.get() != NULL);

  // Create the XML reader.
  CefRefPtr<CefXmlObject> object(new CefXmlObject(L"object"));
  ASSERT_TRUE(object->Load(stream, XML_ENCODING_NONE,
      L"http://www.example.org/example.xml", NULL));

  ASSERT_FALSE(object->HasAttributes());
  ASSERT_TRUE(object->HasChildren());
  ASSERT_EQ(object->GetChildCount(), 1);

  CefRefPtr<CefXmlObject> obj(object->FindChild(L"ns:obj"));
  ASSERT_TRUE(obj.get());
  ASSERT_TRUE(obj->HasChildren());
  ASSERT_EQ(obj->GetChildCount(), 4);

  CefRefPtr<CefXmlObject> obj_child(obj->FindChild(L"ns:objC"));
  ASSERT_TRUE(obj_child.get());
  ASSERT_EQ(obj_child->GetName(), L"ns:objC");
  ASSERT_FALSE(obj_child->HasChildren());
  ASSERT_FALSE(obj_child->HasValue());
  ASSERT_TRUE(obj_child->HasAttributes());

  CefXmlObject::ObjectVector obj_children;
  ASSERT_EQ(obj->GetChildren(obj_children), 4);
  ASSERT_EQ(obj_children.size(), 4);

  CefXmlObject::ObjectVector::const_iterator it = obj_children.begin();
  for (int ct = 0; it != obj_children.end(); ++it, ++ct) {
    obj_child = *it;
    ASSERT_TRUE(obj_child.get());
    if (ct == 0) {
      // ns:objA
      ASSERT_EQ(obj_child->GetName(), L"ns:objA");
      ASSERT_FALSE(obj_child->HasChildren());
      ASSERT_TRUE(obj_child->HasValue());
      ASSERT_FALSE(obj_child->HasAttributes());
      ASSERT_EQ(obj_child->GetValue(), L"value A");
    } else if (ct == 1) {
      // ns:objB
      ASSERT_EQ(obj_child->GetName(), L"ns:objB");
      ASSERT_TRUE(obj_child->HasChildren());
      ASSERT_FALSE(obj_child->HasValue());
      ASSERT_FALSE(obj_child->HasAttributes());
      ASSERT_EQ(obj_child->GetChildCount(), 4);
      obj_child = obj_child->FindChild(L"ns:objB_4");
      ASSERT_TRUE(obj_child.get());
      ASSERT_TRUE(obj_child->HasValue());
      ASSERT_EQ(obj_child->GetValue(),
          L"<b>this is</b> mixed content EA Value");
    } else if (ct == 2) {
      // ns:objC
      ASSERT_EQ(obj_child->GetName(), L"ns:objC");
      ASSERT_FALSE(obj_child->HasChildren());
      ASSERT_FALSE(obj_child->HasValue());
      ASSERT_TRUE(obj_child->HasAttributes());

      CefXmlObject::AttributeMap attribs;
      ASSERT_EQ(obj_child->GetAttributes(attribs), 2);
      ASSERT_EQ(attribs.size(), 2);
      ASSERT_EQ(attribs[L"ns:attr1"], L"value C1");
      ASSERT_EQ(attribs[L"ns:attr2"], L"value C2");

      ASSERT_EQ(obj_child->GetAttributeCount(), 2);
      ASSERT_TRUE(obj_child->HasAttribute(L"ns:attr1"));
      ASSERT_EQ(obj_child->GetAttributeValue(L"ns:attr1"), L"value C1");
      ASSERT_TRUE(obj_child->HasAttribute(L"ns:attr2"));
      ASSERT_EQ(obj_child->GetAttributeValue(L"ns:attr2"), L"value C2");
    } else if (ct == 3) {
      // ns:objD
      ASSERT_EQ(obj_child->GetName(), L"ns:objD");
      ASSERT_FALSE(obj_child->HasChildren());
      ASSERT_FALSE(obj_child->HasValue());
      ASSERT_FALSE(obj_child->HasAttributes());
    }
  }
}

// Test XmlObject load error handling behavior.
TEST(XmlReaderTest, ObjectLoadError)
{
  // Test start/end tag mismatch error.
  {
    char error_xml[] = "<obj>\n<foo>\n</obj>\n</foo>";
    
    // Create the stream reader.
    CefRefPtr<CefStreamReader> stream(
        CefStreamReader::CreateForData(error_xml, sizeof(error_xml) - 1));
    ASSERT_TRUE(stream.get() != NULL);

    std::wstring error_str;
    
    // Create the XML reader.
    CefRefPtr<CefXmlObject> object(new CefXmlObject(L"object"));
    ASSERT_FALSE(object->Load(stream, XML_ENCODING_NONE,
        L"http://www.example.org/example.xml", &error_str));
    ASSERT_EQ(error_str,
        L"Opening and ending tag mismatch: foo line 2 and obj, line 3");
  }

  // Test value following child error.
  {
    char error_xml[] = "<obj>\n<foo>\n</foo>disallowed value\n</obj>";
    
    // Create the stream reader.
    CefRefPtr<CefStreamReader> stream(
        CefStreamReader::CreateForData(error_xml, sizeof(error_xml) - 1));
    ASSERT_TRUE(stream.get() != NULL);

    std::wstring error_str;
    
    // Create the XML reader.
    CefRefPtr<CefXmlObject> object(new CefXmlObject(L"object"));
    ASSERT_FALSE(object->Load(stream, XML_ENCODING_NONE,
        L"http://www.example.org/example.xml", &error_str));
    ASSERT_EQ(error_str,
        L"Value following child element, line 4");
  }
}
