// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "test_handler.h"

namespace {

bool g_ContentFilterTestHandlerHandleResourceResponseCalled;
bool g_ContentFilterProcessDataCalled;
bool g_ContentFilterDrainCalled;

class TestContentFilter : public CefContentFilter
{
public:
    TestContentFilter()
    {
      look_for_ = "FAILURE!";
      replace_with_ = "BIG SUCCESS!";
    }

    virtual void ProcessData(const void* data, int data_size,
                             CefRefPtr<CefStreamReader>& substitute_data)
                             OVERRIDE
    {
      EXPECT_TRUE(CefCurrentlyOn(TID_UI));

      g_ContentFilterProcessDataCalled = true;

      std::string in_out((char*)data, data_size);
      std::string::const_iterator look_for_it = look_for_.begin();

      bool is_modified = false;
      if (!remainder_.empty()) {
          in_out.insert(in_out.begin(), remainder_.begin(), remainder_.end());
          remainder_.clear();
      }

      std::string::size_type off = 0;
      do {
        if ((look_for_it == look_for_.end()) ||
            (look_for_it == look_for_.begin())) {
          // start over
          off = in_out.find(look_for_[0], off);
          if (off == in_out.npos)
              break;
          look_for_it = look_for_.begin();
        }
  
        while (look_for_it != look_for_.end()) {
          if (*look_for_it != in_out[off]) {
              look_for_it = look_for_.begin();
              break;
          }

          if (++off == in_out.length())
              off = in_out.npos;

          if (off == in_out.npos)
              break;
  
          look_for_it++;
        }

        if (look_for_it == look_for_.end()) {
            // found it
            in_out.replace(in_out.begin() + off - look_for_.length(),
                           in_out.begin() + off,
                           replace_with_);
            off += replace_with_.length() - look_for_.length();
            if (off >= in_out.length())
                off = in_out.npos;

            look_for_it = look_for_.begin();
            is_modified = true;
        }
      } while (off != in_out.npos);

      if (look_for_it != look_for_.begin()) {
          // partial match at the end of the buffer
          // save for next packet
          size_t slice_off =
              in_out.length() - (look_for_it - look_for_.begin()) - 1;

          remainder_ = in_out.substr(slice_off);
          in_out.erase(slice_off);
      }
      
      if (is_modified) {
        substitute_data =
            CefStreamReader::CreateForData((void*)in_out.data(), in_out.size());
      }
    }

    virtual void Drain(CefRefPtr<CefStreamReader>& remainder) OVERRIDE
    {
        EXPECT_TRUE(CefCurrentlyOn(TID_UI));
      
        g_ContentFilterDrainCalled = true;

        if (remainder_.empty())
            return;

        remainder = CefStreamReader::CreateForData((void*)remainder_.data(),
                                                   remainder_.size());
    }

protected:
    IMPLEMENT_REFCOUNTING(TestContentFilter);

private:
    std::string look_for_;
    std::string replace_with_;
    std::string remainder_;
};

class ContentFilterTestHandler : public TestHandler
{
public:
  class Visitor : public CefDOMVisitor
  {
  public:
    Visitor(ContentFilterTestHandler* handler) : handler_(handler) {}

    // Test if the filter succeeded in modifying the content
    void TestContentReplaced(CefRefPtr<CefDOMDocument> document)
    {
      // Navigate the complete document structure.
      CefRefPtr<CefDOMNode> resultNode =
          document->GetElementById("test_result");

      EXPECT_TRUE(resultNode.get());
      EXPECT_EQ("BIG SUCCESS!", resultNode->GetElementInnerText().ToString());
    }

    virtual void Visit(CefRefPtr<CefDOMDocument> document) OVERRIDE
    {
      EXPECT_TRUE(CefCurrentlyOn(TID_UI));
      
      handler_->got_visitor_called_.yes();

      TestContentReplaced(document);

      handler_->DestroyTest();
    }

  protected:
    ContentFilterTestHandler* handler_;
    IMPLEMENT_REFCOUNTING(Visitor);
 };

  ContentFilterTestHandler()
  {
    visitor_ = new Visitor(this);
  }

  virtual void RunTest() OVERRIDE
  {
    std::string mainHtml =
        "<p>If filtering works you should see BIG SUCCESS! below:</p>"
        "<div id=\"test_result\">FAILURE!</div>";
    
    AddResource("http://tests/test_filter.html", mainHtml, "text/html");
    CreateBrowser("http://tests/test_filter.html");
  }

  virtual void OnResourceResponse(CefRefPtr<CefBrowser> browser,
                                  const CefString& url,
                                  CefRefPtr<CefResponse> response,
                                  CefRefPtr<CefContentFilter>& filter) OVERRIDE
  {
    EXPECT_TRUE(CefCurrentlyOn(TID_UI));
    
    g_ContentFilterTestHandlerHandleResourceResponseCalled = true;

    ASSERT_EQ(url, "http://tests/test_filter.html");

    CefResponse::HeaderMap headers;
    response->GetHeaderMap(headers);
    std::string mime_type = response->GetMimeType();
    int status_code = response->GetStatus();
    std::string status_text = response->GetStatusText();

    ASSERT_TRUE(headers.empty());
    ASSERT_EQ(mime_type, "text/html");
    ASSERT_EQ(status_code, 200);
    ASSERT_EQ(status_text, "OK");

    filter = new TestContentFilter();
  }

  virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         int httpStatusCode) OVERRIDE
  {
    EXPECT_TRUE(CefCurrentlyOn(TID_UI));
    
    if(frame->IsMain()) {
      // The page is done loading so visit the DOM.
      browser->GetMainFrame()->VisitDOM(visitor_.get());
    }
  }

  TrackCallback got_visitor_called_;

private:
  CefRefPtr<Visitor> visitor_;
};

} // anonymous

// Verify send and recieve
TEST(ContentFilterTest, ContentFilter)
{
  g_ContentFilterTestHandlerHandleResourceResponseCalled = false;
  g_ContentFilterProcessDataCalled = false;
  g_ContentFilterDrainCalled = false;

  CefRefPtr<ContentFilterTestHandler> handler =
      new ContentFilterTestHandler();
  handler->ExecuteTest();

  ASSERT_TRUE(handler->got_visitor_called_);
  ASSERT_TRUE(g_ContentFilterTestHandlerHandleResourceResponseCalled);
  ASSERT_TRUE(g_ContentFilterProcessDataCalled);
  ASSERT_TRUE(g_ContentFilterDrainCalled);
}
