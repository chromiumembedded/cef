// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/cef.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "test_handler.h"

namespace {

static const char* kOrigin = "http://tests";
static const char* kNav1 = "http://tests/nav1.html";
static const char* kNav2 = "http://tests/nav2.html";

static const char* kKey1 = "foo";
static const char* kVal1 = "bar";
static const char* kKey2 = "choo";
static const char* kVal2 = "whatzit";

class StorageTestHandler : public TestHandler
{
public:
  class V8Handler : public CefV8Handler
  {
  public:
    V8Handler(CefRefPtr<StorageTestHandler> tester)
      : tester_(tester) {}

    virtual bool Execute(const CefString& name,
                         CefRefPtr<CefV8Value> object,
                         const CefV8ValueList& arguments,
                         CefRefPtr<CefV8Value>& retval,
                         CefString& exception) OVERRIDE
    {
      if (arguments.size() != 2)
        return false;

      std::string key = arguments[0]->GetStringValue();
      std::string val = arguments[1]->GetStringValue();

      if (key == kKey1 && val == kVal1)
        tester_->got_js_read1_.yes();
      else if (key == kKey2 && val == kVal2)
        tester_->got_js_read2_.yes();

      return true;
    }

    CefRefPtr<StorageTestHandler> tester_;

    IMPLEMENT_REFCOUNTING(V8Handler);
  };

  class StorageVisitor : public CefStorageVisitor
  {
  public:
    enum Mode {
      VisitKey,
      DeleteKey1,
      DeleteKey2
    };

    StorageVisitor(CefRefPtr<StorageTestHandler> tester,
                   const std::string& description, Mode mode,
                   TrackCallback* callback1, TrackCallback* callback2,
                   int expected_total)
      : tester_(tester), description_(description), mode_(mode),
        callback1_(callback1), callback2_(callback2),
        expected_total_(expected_total), actual_total_(0)
    {
    }
    virtual ~StorageVisitor()
    {
      EXPECT_EQ(expected_total_, actual_total_) << "test = "<< description_;
    }

    virtual bool Visit(CefStorageType type, const CefString& origin,
                      const CefString& key, const CefString& value, int count,
                      int total, bool& deleteData) OVERRIDE
    {
      EXPECT_EQ(type, tester_->type_);
      std::string originStr = origin;
      EXPECT_EQ(originStr, kOrigin);

      std::string keyStr = key;
      std::string valueStr = value;
      if (keyStr == kKey1 && valueStr == kVal1)
        callback1_->yes();
      else if(keyStr == kKey2 && valueStr == kVal2)
        callback2_->yes();

      EXPECT_EQ(expected_total_, total) << "test = "<< description_;
      
      if((mode_ == DeleteKey1 && keyStr == kKey1) ||
         (mode_ == DeleteKey2 && keyStr == kKey2))
        deleteData = true;

      actual_total_++;

      return true;
    }

    CefRefPtr<StorageTestHandler> tester_;
    std::string description_;
    Mode mode_;
    TrackCallback* callback1_;
    TrackCallback* callback2_;
    int expected_total_;
    int actual_total_;

    IMPLEMENT_REFCOUNTING(StorageVisitor);
  };

  StorageTestHandler(CefStorageType type)
    : type_(type), nav_(0) {}

  virtual void RunTest() OVERRIDE
  {
    std::stringstream ss;

    std::string func = (type_==ST_LOCALSTORAGE?"localStorage":"sessionStorage");

    // Values will be set vis JS on page load.
    ss << "<html><head><script language=\"JavaScript\">" <<
          func << ".setItem('" << std::string(kKey1) << "', '" <<
          std::string(kVal1) << "');" <<
          func << ".setItem('" << std::string(kKey2) << "', '" <<
          std::string(kVal2) << "');"
          "</script></head><body>Nav1</body></html>";
    AddResource(kNav1, ss.str(), "text/html");
    ss.str("");

    // Values will be verified vis JS on page load.
    ss << "<html><head><script language=\"JavaScript\">"
          "window.test.result('" << std::string(kKey1) << "', " <<
          func << ".getItem('" << std::string(kKey1) << "'));"
          "window.test.result('" << std::string(kKey2) << "', " <<
          func << ".getItem('" << std::string(kKey2) << "'));"
          "</script></head><body>Nav2</body></html>";
    AddResource(kNav2, ss.str(), "text/html");
    ss.str("");

    // Create the browser.
    CreateBrowser(kNav1);
  }

  virtual void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         int httpStatusCode) OVERRIDE
  {
    if (nav_ == 0) {
      // Verify read all.
      CefVisitStorage(type_, "", "",
          new StorageVisitor(this, "all_read",
                             StorageVisitor::VisitKey,
                             &got_cpp_all_read1_,
                             &got_cpp_all_read2_, 2));

      // Verify read origin.
      CefVisitStorage(type_, kOrigin, "",
          new StorageVisitor(this, "origin_read",
                             StorageVisitor::VisitKey,
                             &got_cpp_origin_read1_,
                             &got_cpp_origin_read2_, 2));

      // Verify read key1.
      CefVisitStorage(type_, kOrigin, kKey1,
          new StorageVisitor(this, "key1_read",
                             StorageVisitor::VisitKey,
                             &got_cpp_key_read1_,
                             &got_cpp_key_read1_fail_, 1));

      // Verify read key2.
      CefVisitStorage(type_, kOrigin, kKey2,
          new StorageVisitor(this, "key2_read",
                             StorageVisitor::VisitKey,
                             &got_cpp_key_read2_fail_,
                             &got_cpp_key_read2_, 1));

      // Delete key1. Verify that key2 still gets read.
      CefVisitStorage(type_, kOrigin, "",
          new StorageVisitor(this, "key1_delete",
                             StorageVisitor::DeleteKey1,
                             &got_cpp_key_delete1_delete_,
                             &got_cpp_key_delete1_, 2));

      // Verify that key1 was deleted.
      CefVisitStorage(type_, kOrigin, "",
          new StorageVisitor(this, "key1_delete_verify",
                             StorageVisitor::VisitKey,
                             &got_cpp_afterdeletevisit1_fail_,
                             &got_cpp_afterdeletevisit1_, 1));

      // Delete key2.
      CefVisitStorage(type_, kOrigin, "",
          new StorageVisitor(this, "key2_delete",
                             StorageVisitor::DeleteKey2,
                             &got_cpp_key_delete2_fail_,
                             &got_cpp_key_delete2_delete_, 1));

      // Verify that all keys have been deleted.
      CefVisitStorage(type_, kOrigin, "",
          new StorageVisitor(this, "key2_delete_verify",
                             StorageVisitor::VisitKey,
                             &got_cpp_afterdeletevisit2_fail_,
                             &got_cpp_afterdeletevisit2_fail_, 0));

      // Reset the values.
      CefSetStorage(type_, kOrigin, kKey1, kVal1);
      CefSetStorage(type_, kOrigin, kKey2, kVal2);

      // Verify that all values have been reset.
      CefVisitStorage(type_, "", "",
          new StorageVisitor(this, "reset1a_verify",
                             StorageVisitor::VisitKey,
                             &got_cpp_all_reset1a_,
                             &got_cpp_all_reset2a_, 2));

      // Delete all values.
      CefDeleteStorage(type_, "", "");

      // Verify that all values have been deleted.
      CefVisitStorage(type_, "", "",
          new StorageVisitor(this, "delete_all_verify",
                             StorageVisitor::VisitKey,
                             &got_cpp_afterdeleteall_fail_,
                             &got_cpp_afterdeleteall_fail_, 0));
      
      // Reset all values.
      CefSetStorage(type_, kOrigin, kKey1, kVal1);
      CefSetStorage(type_, kOrigin, kKey2, kVal2);

      // Verify that all values have been reset.
      CefVisitStorage(type_, "", "",
          new StorageVisitor(this, "reset1b_verify",
                             StorageVisitor::VisitKey,
                             &got_cpp_all_reset1b_,
                             &got_cpp_all_reset2b_, 2));

      // Delete all values by origin.
      CefDeleteStorage(type_, kOrigin, "");

      // Verify that all values have been deleted.
      CefVisitStorage(type_, "", "",
          new StorageVisitor(this, "delete_origin_verify",
                             StorageVisitor::VisitKey,
                             &got_cpp_afterdeleteorigin_fail_,
                             &got_cpp_afterdeleteorigin_fail_, 0));

      // Reset the values.
      CefSetStorage(type_, kOrigin, kKey1, kVal1);
      CefSetStorage(type_, kOrigin, kKey2, kVal2);

      // Verify that all values have been reset.
      CefVisitStorage(type_, "", "",
          new StorageVisitor(this, "reset1c_verify",
                             StorageVisitor::VisitKey,
                             &got_cpp_all_reset1c_,
                             &got_cpp_all_reset2c_, 2));

      // Delete key1.
      CefDeleteStorage(type_, kOrigin, kKey1);

      // Verify that key1 has been deleted.
      CefVisitStorage(type_, "", "",
          new StorageVisitor(this, "direct_key1_delete_verify",
                             StorageVisitor::VisitKey,
                             &got_cpp_afterdeletekey1_fail_,
                             &got_cpp_afterdeletekey1_, 1));

      // Delete key2.
      CefDeleteStorage(type_, kOrigin, kKey2);

      // Verify that all values have been deleted.
      CefVisitStorage(type_, "", "",
          new StorageVisitor(this, "direct_key2_delete_verify",
                             StorageVisitor::VisitKey,
                             &got_cpp_afterdeletekey2_fail_,
                             &got_cpp_afterdeletekey2_fail_, 0));

      // Reset all values.
      CefSetStorage(type_, kOrigin, kKey1, kVal1);
      CefSetStorage(type_, kOrigin, kKey2, kVal2);

      // Verify that all values have been reset.
      CefVisitStorage(type_, "", "",
          new StorageVisitor(this, "reset1d_verify",
                             StorageVisitor::VisitKey,
                             &got_cpp_all_reset1d_,
                             &got_cpp_all_reset2d_, 2));

      nav_++;
      // Verify JS read after navigation.
      frame->LoadURL(kNav2);
    } else {
      DestroyTest();
    }
  }

  virtual void OnJSBinding(CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefFrame> frame,
                           CefRefPtr<CefV8Value> object) OVERRIDE
  {
    CefRefPtr<CefV8Handler> handler = new V8Handler(this);
    CefRefPtr<CefV8Value> testObj = CefV8Value::CreateObject(NULL, NULL);
    testObj->SetValue("result", CefV8Value::CreateFunction("result", handler));
    object->SetValue("test", testObj);
  }

  CefStorageType type_;
  int nav_;

  TrackCallback got_cpp_all_read1_;
  TrackCallback got_cpp_all_read2_;
  TrackCallback got_cpp_origin_read1_;
  TrackCallback got_cpp_origin_read2_;
  TrackCallback got_cpp_key_read1_;
  TrackCallback got_cpp_key_read1_fail_;
  TrackCallback got_cpp_key_read2_;
  TrackCallback got_cpp_key_read2_fail_;
  TrackCallback got_cpp_key_delete1_;
  TrackCallback got_cpp_key_delete1_delete_;
  TrackCallback got_cpp_key_delete2_delete_;
  TrackCallback got_cpp_key_delete2_fail_;
  TrackCallback got_cpp_afterdeletevisit1_;
  TrackCallback got_cpp_afterdeletevisit1_fail_;
  TrackCallback got_cpp_afterdeletevisit2_fail_;
  TrackCallback got_cpp_all_reset1a_;
  TrackCallback got_cpp_all_reset2a_;
  TrackCallback got_cpp_afterdeleteall_fail_;
  TrackCallback got_cpp_all_reset1b_;
  TrackCallback got_cpp_all_reset2b_;
  TrackCallback got_cpp_afterdeleteorigin_fail_;
  TrackCallback got_cpp_all_reset1c_;
  TrackCallback got_cpp_all_reset2c_;
  TrackCallback got_cpp_afterdeletekey1_;
  TrackCallback got_cpp_afterdeletekey1_fail_;
  TrackCallback got_cpp_afterdeletekey2_fail_;
  TrackCallback got_cpp_all_reset1d_;
  TrackCallback got_cpp_all_reset2d_;
  TrackCallback got_js_read1_;
  TrackCallback got_js_read2_;
};

void StorageTest(CefStorageType type)
{
  CefRefPtr<StorageTestHandler> handler = new StorageTestHandler(type);
  handler->ExecuteTest();

  EXPECT_TRUE(handler->got_cpp_all_read1_);
  EXPECT_TRUE(handler->got_cpp_all_read2_);
  EXPECT_TRUE(handler->got_cpp_origin_read1_);
  EXPECT_TRUE(handler->got_cpp_origin_read2_);
  EXPECT_TRUE(handler->got_cpp_key_read1_);
  EXPECT_FALSE(handler->got_cpp_key_read1_fail_);
  EXPECT_TRUE(handler->got_cpp_key_read2_);
  EXPECT_FALSE(handler->got_cpp_key_read2_fail_);
  EXPECT_TRUE(handler->got_cpp_key_delete1_);
  EXPECT_TRUE(handler->got_cpp_key_delete1_delete_);
  EXPECT_TRUE(handler->got_cpp_key_delete2_delete_);
  EXPECT_FALSE(handler->got_cpp_key_delete2_fail_);
  EXPECT_TRUE(handler->got_cpp_afterdeletevisit1_);
  EXPECT_FALSE(handler->got_cpp_afterdeletevisit1_fail_);
  EXPECT_FALSE(handler->got_cpp_afterdeletevisit2_fail_);
  EXPECT_TRUE(handler->got_cpp_all_reset1a_);
  EXPECT_TRUE(handler->got_cpp_all_reset2a_);
  EXPECT_FALSE(handler->got_cpp_afterdeleteall_fail_);
  EXPECT_TRUE(handler->got_cpp_all_reset1b_);
  EXPECT_TRUE(handler->got_cpp_all_reset2b_);
  EXPECT_FALSE(handler->got_cpp_afterdeleteorigin_fail_);
  EXPECT_TRUE(handler->got_cpp_all_reset1c_);
  EXPECT_TRUE(handler->got_cpp_all_reset2c_);
  EXPECT_TRUE(handler->got_cpp_afterdeletekey1_);
  EXPECT_FALSE(handler->got_cpp_afterdeletekey1_fail_);
  EXPECT_FALSE(handler->got_cpp_afterdeletekey2_fail_);
  EXPECT_TRUE(handler->got_cpp_all_reset1d_);
  EXPECT_TRUE(handler->got_cpp_all_reset2d_);
  EXPECT_TRUE(handler->got_js_read1_);
  EXPECT_TRUE(handler->got_js_read2_);
}

} // namespace

// Test localStorage.
TEST(StorageTest, Local)
{
  StorageTest(ST_LOCALSTORAGE);
}

// Test sessionStorage.
TEST(StorageTest, Session)
{
  StorageTest(ST_SESSIONSTORAGE);
}
