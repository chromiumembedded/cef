// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "test_handler.h"

namespace {

class TestDOMHandler : public TestHandler
{
public:
  class Visitor : public CefDOMVisitor
  {
  public:
    Visitor(TestDOMHandler* handler) : handler_(handler) {}

    void TestHeadNodeStructure(CefRefPtr<CefDOMNode> headNode)
    {
      EXPECT_TRUE(headNode.get());
      EXPECT_TRUE(headNode->IsElement());
      EXPECT_FALSE(headNode->IsText());
      EXPECT_EQ(headNode->GetName(), "HEAD");
      EXPECT_EQ(headNode->GetElementTagName(), "HEAD");

      EXPECT_TRUE(headNode->HasChildren());
      EXPECT_FALSE(headNode->HasElementAttributes());
      
      CefRefPtr<CefDOMNode> titleNode = headNode->GetFirstChild();
      EXPECT_TRUE(titleNode.get());
      EXPECT_TRUE(titleNode->IsElement());
      EXPECT_FALSE(titleNode->IsText());
      EXPECT_EQ(titleNode->GetName(), "TITLE");
      EXPECT_EQ(titleNode->GetElementTagName(), "TITLE");
      EXPECT_TRUE(titleNode->GetParent()->IsSame(headNode));

      EXPECT_FALSE(titleNode->GetNextSibling().get());
      EXPECT_FALSE(titleNode->GetPreviousSibling().get());
      EXPECT_TRUE(titleNode->HasChildren());
      EXPECT_FALSE(titleNode->HasElementAttributes());

      CefRefPtr<CefDOMNode> textNode = titleNode->GetFirstChild();
      EXPECT_TRUE(textNode.get());
      EXPECT_FALSE(textNode->IsElement());
      EXPECT_TRUE(textNode->IsText());
      EXPECT_EQ(textNode->GetValue(), "The Title");
      EXPECT_TRUE(textNode->GetParent()->IsSame(titleNode));

      EXPECT_FALSE(textNode->GetNextSibling().get());
      EXPECT_FALSE(textNode->GetPreviousSibling().get());
      EXPECT_FALSE(textNode->HasChildren());
    }
    
    void TestBodyNodeStructure(CefRefPtr<CefDOMNode> bodyNode)
    {
      EXPECT_TRUE(bodyNode.get());
      EXPECT_TRUE(bodyNode->IsElement());
      EXPECT_FALSE(bodyNode->IsText());
      EXPECT_EQ(bodyNode->GetName(), "BODY");
      EXPECT_EQ(bodyNode->GetElementTagName(), "BODY");

      EXPECT_TRUE(bodyNode->HasChildren());
      EXPECT_FALSE(bodyNode->HasElementAttributes());

      CefRefPtr<CefDOMNode> h1Node = bodyNode->GetFirstChild();
      EXPECT_TRUE(h1Node.get());
      EXPECT_TRUE(h1Node->IsElement());
      EXPECT_FALSE(h1Node->IsText());
      EXPECT_EQ(h1Node->GetName(), "H1");
      EXPECT_EQ(h1Node->GetElementTagName(), "H1");

      EXPECT_FALSE(h1Node->GetNextSibling().get());
      EXPECT_FALSE(h1Node->GetPreviousSibling().get());
      EXPECT_TRUE(h1Node->HasChildren());
      EXPECT_FALSE(h1Node->HasElementAttributes());

      CefRefPtr<CefDOMNode> textNode = h1Node->GetFirstChild();
      EXPECT_TRUE(textNode.get());
      EXPECT_FALSE(textNode->IsElement());
      EXPECT_TRUE(textNode->IsText());
      EXPECT_EQ(textNode->GetValue(), "Hello From");

      EXPECT_FALSE(textNode->GetPreviousSibling().get());
      EXPECT_FALSE(textNode->HasChildren());

      CefRefPtr<CefDOMNode> brNode = textNode->GetNextSibling();
      EXPECT_TRUE(brNode.get());
      EXPECT_TRUE(brNode->IsElement());
      EXPECT_FALSE(brNode->IsText());
      EXPECT_EQ(brNode->GetName(), "BR");
      EXPECT_EQ(brNode->GetElementTagName(), "BR");

      EXPECT_FALSE(brNode->HasChildren());

      EXPECT_TRUE(brNode->HasElementAttributes());
      EXPECT_TRUE(brNode->HasElementAttribute("class"));
      EXPECT_EQ(brNode->GetElementAttribute("class"), "some_class");
      EXPECT_TRUE(brNode->HasElementAttribute("id"));
      EXPECT_EQ(brNode->GetElementAttribute("id"), "some_id");
      EXPECT_FALSE(brNode->HasElementAttribute("no_existing"));

      CefDOMNode::AttributeMap map;
      brNode->GetElementAttributes(map);
      ASSERT_EQ(map.size(), (size_t)2);
      EXPECT_EQ(map["class"], "some_class");
      EXPECT_EQ(map["id"], "some_id");

      // Can also retrieve by ID.
      brNode = bodyNode->GetDocument()->GetElementById("some_id");
      EXPECT_TRUE(brNode.get());
      EXPECT_TRUE(brNode->IsElement());
      EXPECT_FALSE(brNode->IsText());
      EXPECT_EQ(brNode->GetName(), "BR");
      EXPECT_EQ(brNode->GetElementTagName(), "BR");
      
      textNode = brNode->GetNextSibling();
      EXPECT_TRUE(textNode.get());
      EXPECT_FALSE(textNode->IsElement());
      EXPECT_TRUE(textNode->IsText());
      EXPECT_EQ(textNode->GetValue(), "Main Frame");

      EXPECT_FALSE(textNode->GetNextSibling().get());
      EXPECT_FALSE(textNode->HasChildren());
    }

    // Test document structure by iterating through the DOM tree.
    void TestStructure(CefRefPtr<CefDOMDocument> document)
    {
      EXPECT_EQ(document->GetTitle(), "The Title");
      EXPECT_EQ(document->GetBaseURL(), "http://tests/main.html");
      EXPECT_EQ(document->GetCompleteURL("foo.html"), "http://tests/foo.html");

      // Navigate the complete document structure.
      CefRefPtr<CefDOMNode> docNode = document->GetDocument();
      EXPECT_TRUE(docNode.get());
      EXPECT_FALSE(docNode->IsElement());
      EXPECT_FALSE(docNode->IsText());

      CefRefPtr<CefDOMNode> htmlNode = docNode->GetFirstChild();
      EXPECT_TRUE(htmlNode.get());
      EXPECT_TRUE(htmlNode->IsElement());
      EXPECT_FALSE(htmlNode->IsText());
      EXPECT_EQ(htmlNode->GetName(), "HTML");
      EXPECT_EQ(htmlNode->GetElementTagName(), "HTML");

      EXPECT_TRUE(htmlNode->HasChildren());
      EXPECT_FALSE(htmlNode->HasElementAttributes());

      CefRefPtr<CefDOMNode> headNode = htmlNode->GetFirstChild();
      TestHeadNodeStructure(headNode);

      CefRefPtr<CefDOMNode> bodyNode = headNode->GetNextSibling();
      TestBodyNodeStructure(bodyNode);
      
      // Retrieve the head node directly.
      headNode = document->GetHead();
      TestHeadNodeStructure(headNode);

      // Retrieve the body node directly.
      bodyNode = document->GetBody();
      TestBodyNodeStructure(bodyNode);
    }

    // Test document modification by changing the H1 tag.
    void TestModify(CefRefPtr<CefDOMDocument> document)
    {
      CefRefPtr<CefDOMNode> bodyNode = document->GetBody();
      CefRefPtr<CefDOMNode> h1Node = bodyNode->GetFirstChild();

      ASSERT_EQ(h1Node->GetAsMarkup(),
          "<h1>Hello From<br class=\"some_class\" id=\"some_id\">Main Frame</h1>");
      
      CefRefPtr<CefDOMNode> textNode = h1Node->GetFirstChild();
      ASSERT_EQ(textNode->GetValue(), "Hello From");
      ASSERT_TRUE(textNode->SetValue("A Different Message From"));
      ASSERT_EQ(textNode->GetValue(), "A Different Message From");

      CefRefPtr<CefDOMNode> brNode = textNode->GetNextSibling();
      EXPECT_EQ(brNode->GetElementAttribute("class"), "some_class");
      EXPECT_TRUE(brNode->SetElementAttribute("class", "a_different_class"));
      EXPECT_EQ(brNode->GetElementAttribute("class"), "a_different_class");

      ASSERT_EQ(h1Node->GetAsMarkup(),
          "<h1>A Different Message From<br class=\"a_different_class\" id=\"some_id\">Main Frame</h1>");

      ASSERT_FALSE(h1Node->SetValue("Something Different"));
    }

    virtual void Visit(CefRefPtr<CefDOMDocument> document)
    {
      handler_->got_visitor_called_.yes();

      if (handler_->test_type_ == STRUCTURE)
        TestStructure(document);
      else if (handler_->test_type_ == MODIFY)
        TestModify(document);

      handler_->DestroyTest();
    }

  protected:
    TestDOMHandler* handler_;

    IMPLEMENT_REFCOUNTING(Visitor);
  };

  enum TestType {
    STRUCTURE,
    MODIFY,
  };

  TestDOMHandler(TestType test) : test_type_(test)
  {
    visitor_ = new Visitor(this);
  }
  
  virtual void RunTest() OVERRIDE
  {
    std::stringstream mainHtml;
    mainHtml <<
    "<html>"
    "<head><title>The Title</title></head>"
    "<body>"
    "<h1>Hello From<br class=\"some_class\"/ id=\"some_id\"/>Main Frame</h1>"
    "</body>"
    "</html>";
    
    AddResource("http://tests/main.html", mainHtml.str(), "text/html");
    CreateBrowser("http://tests/main.html");
  }
  
  virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         int httpStatusCode) OVERRIDE
  {
    if(frame->IsMain()) {
      // The page is done loading so visit the DOM.
      browser->GetMainFrame()->VisitDOM(visitor_.get());
    }
  }

  CefRefPtr<Visitor> visitor_;
  TestType test_type_;

  TrackCallback got_visitor_called_;
};

} // namespace

// Test DOM structure reading.
TEST(DOMTest, Read)
{
  CefRefPtr<TestDOMHandler> handler =
      new TestDOMHandler(TestDOMHandler::STRUCTURE);
  handler->ExecuteTest();

  EXPECT_TRUE(handler->got_visitor_called_);
}

// Test DOM modifications.
TEST(DOMTest, Modify)
{
  CefRefPtr<TestDOMHandler> handler =
      new TestDOMHandler(TestDOMHandler::MODIFY);
  handler->ExecuteTest();

  EXPECT_TRUE(handler->got_visitor_called_);
}
