// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/test/cef_translator_test.h"
#include "tests/ceftests/test_handler.h"
#include "tests/gtest/include/gtest/gtest.h"

// Test getting/setting primitive types.
TEST(TranslatorTest, Primitive) {
  CefRefPtr<CefTranslatorTest> obj = CefTranslatorTest::Create();
  obj->GetVoid();  // Does nothing, but shouldn't crash.
  EXPECT_EQ(TEST_BOOL_VAL, obj->GetBool());
  EXPECT_EQ(TEST_INT_VAL, obj->GetInt());
  EXPECT_EQ(TEST_DOUBLE_VAL, obj->GetDouble());
  EXPECT_EQ(TEST_LONG_VAL, obj->GetLong());
  EXPECT_EQ(TEST_SIZET_VAL, obj->GetSizet());
  EXPECT_TRUE(obj->SetVoid());  // Does nothing, but shouldn't crash.
  EXPECT_TRUE(obj->SetBool(TEST_BOOL_VAL));
  EXPECT_TRUE(obj->SetInt(TEST_INT_VAL));
  EXPECT_TRUE(obj->SetDouble(TEST_DOUBLE_VAL));
  EXPECT_TRUE(obj->SetLong(TEST_LONG_VAL));
  EXPECT_TRUE(obj->SetSizet(TEST_SIZET_VAL));

  // Only one reference to the object should exist.
  EXPECT_TRUE(obj->HasOneRef());
}

// Test getting/setting primitive list types.
TEST(TranslatorTest, PrimitiveList) {
  CefRefPtr<CefTranslatorTest> obj = CefTranslatorTest::Create();

  std::vector<int> list;
  list.push_back(TEST_INT_VAL);
  list.push_back(TEST_INT_VAL2);
  EXPECT_TRUE(obj->SetIntList(list));

  list.clear();
  EXPECT_TRUE(obj->GetIntListByRef(list));
  EXPECT_EQ(2U, list.size());
  EXPECT_EQ(TEST_INT_VAL, list[0]);
  EXPECT_EQ(TEST_INT_VAL2, list[1]);

  // Only one reference to the object should exist.
  EXPECT_TRUE(obj->HasOneRef());
}

// Test getting/setting string types.
TEST(TranslatorTest, String) {
  CefRefPtr<CefTranslatorTest> obj = CefTranslatorTest::Create();
  EXPECT_STREQ(TEST_STRING_VAL, obj->GetString().ToString().c_str());
  EXPECT_TRUE(obj->SetString(TEST_STRING_VAL));

  CefString str;
  obj->GetStringByRef(str);
  EXPECT_STREQ(TEST_STRING_VAL, str.ToString().c_str());

  // Only one reference to the object should exist.
  EXPECT_TRUE(obj->HasOneRef());
}

// Test getting/setting string list types.
TEST(TranslatorTest, StringList) {
  CefRefPtr<CefTranslatorTest> obj = CefTranslatorTest::Create();

  std::vector<CefString> list;
  list.push_back(TEST_STRING_VAL);
  list.push_back(TEST_STRING_VAL2);
  list.push_back(TEST_STRING_VAL3);
  EXPECT_TRUE(obj->SetStringList(list));

  list.clear();
  EXPECT_TRUE(obj->GetStringListByRef(list));
  EXPECT_EQ(3U, list.size());
  EXPECT_STREQ(TEST_STRING_VAL, list[0].ToString().c_str());
  EXPECT_STREQ(TEST_STRING_VAL2, list[1].ToString().c_str());
  EXPECT_STREQ(TEST_STRING_VAL3, list[2].ToString().c_str());

  // Only one reference to the object should exist.
  EXPECT_TRUE(obj->HasOneRef());
}

// Test getting/setting string map types.
TEST(TranslatorTest, StringMap) {
  CefRefPtr<CefTranslatorTest> obj = CefTranslatorTest::Create();

  CefTranslatorTest::StringMap map;
  map.insert(std::make_pair(TEST_STRING_KEY, TEST_STRING_VAL));
  map.insert(std::make_pair(TEST_STRING_KEY2, TEST_STRING_VAL2));
  map.insert(std::make_pair(TEST_STRING_KEY3, TEST_STRING_VAL3));
  EXPECT_TRUE(obj->SetStringMap(map));

  map.clear();
  EXPECT_TRUE(obj->GetStringMapByRef(map));
  EXPECT_EQ(3U, map.size());

  CefTranslatorTest::StringMap::const_iterator it;

  it = map.find(TEST_STRING_KEY);
  EXPECT_TRUE(it != map.end() && it->second == TEST_STRING_VAL);
  it = map.find(TEST_STRING_KEY2);
  EXPECT_TRUE(it != map.end() && it->second == TEST_STRING_VAL2);
  it = map.find(TEST_STRING_KEY3);
  EXPECT_TRUE(it != map.end() && it->second == TEST_STRING_VAL3);

  // Only one reference to the object should exist.
  EXPECT_TRUE(obj->HasOneRef());
}

// Test getting/setting string multimap types.
TEST(TranslatorTest, StringMultimap) {
  CefRefPtr<CefTranslatorTest> obj = CefTranslatorTest::Create();

  CefTranslatorTest::StringMultimap map;
  map.insert(std::make_pair(TEST_STRING_KEY, TEST_STRING_VAL));
  map.insert(std::make_pair(TEST_STRING_KEY2, TEST_STRING_VAL2));
  map.insert(std::make_pair(TEST_STRING_KEY3, TEST_STRING_VAL3));
  EXPECT_TRUE(obj->SetStringMultimap(map));

  map.clear();
  EXPECT_TRUE(obj->GetStringMultimapByRef(map));
  EXPECT_EQ(3U, map.size());

  CefTranslatorTest::StringMultimap::const_iterator it;

  it = map.find(TEST_STRING_KEY);
  EXPECT_TRUE(it != map.end() && it->second == TEST_STRING_VAL);
  it = map.find(TEST_STRING_KEY2);
  EXPECT_TRUE(it != map.end() && it->second == TEST_STRING_VAL2);
  it = map.find(TEST_STRING_KEY3);
  EXPECT_TRUE(it != map.end() && it->second == TEST_STRING_VAL3);

  // Only one reference to the object should exist.
  EXPECT_TRUE(obj->HasOneRef());
}

// Test getting/setting struct types.
TEST(TranslatorTest, Struct) {
  CefRefPtr<CefTranslatorTest> obj = CefTranslatorTest::Create();

  CefPoint point(TEST_X_VAL, TEST_Y_VAL);
  EXPECT_EQ(point, obj->GetPoint());
  EXPECT_TRUE(obj->SetPoint(point));

  CefPoint point2;
  obj->GetPointByRef(point2);
  EXPECT_EQ(point, point2);

  // Only one reference to the object should exist.
  EXPECT_TRUE(obj->HasOneRef());
}

// Test getting/setting struct list types.
TEST(TranslatorTest, StructList) {
  CefRefPtr<CefTranslatorTest> obj = CefTranslatorTest::Create();

  std::vector<CefPoint> list;
  list.push_back(CefPoint(TEST_X_VAL, TEST_Y_VAL));
  list.push_back(CefPoint(TEST_X_VAL2, TEST_Y_VAL2));
  EXPECT_TRUE(obj->SetPointList(list));

  list.clear();
  EXPECT_TRUE(obj->GetPointListByRef(list));
  EXPECT_EQ(2U, list.size());
  EXPECT_EQ(CefPoint(TEST_X_VAL, TEST_Y_VAL), list[0]);
  EXPECT_EQ(CefPoint(TEST_X_VAL2, TEST_Y_VAL2), list[1]);

  // Only one reference to the object should exist.
  EXPECT_TRUE(obj->HasOneRef());
}

// Test getting/setting library-side RefPtr types.
TEST(TranslatorTest, RefPtrLibrary) {
  CefRefPtr<CefTranslatorTest> obj = CefTranslatorTest::Create();

  const int kTestVal = 12;
  CefRefPtr<CefTranslatorTestRefPtrLibrary> test_obj =
      CefTranslatorTestRefPtrLibrary::Create(kTestVal);
  EXPECT_EQ(kTestVal, test_obj->GetValue());
  int retval = obj->SetRefPtrLibrary(test_obj);
  EXPECT_EQ(kTestVal, retval);
  EXPECT_EQ(kTestVal, test_obj->GetValue());

  const int kTestVal2 = 30;
  CefRefPtr<CefTranslatorTestRefPtrLibrary> test_obj2 =
      obj->GetRefPtrLibrary(kTestVal2);
  EXPECT_EQ(kTestVal2, test_obj2->GetValue());
  int retval2 = obj->SetRefPtrLibrary(test_obj2);
  EXPECT_EQ(kTestVal2, retval2);
  EXPECT_EQ(kTestVal2, test_obj2->GetValue());

  // Only one reference to the object should exist.
  EXPECT_TRUE(obj->HasOneRef());
  EXPECT_TRUE(test_obj->HasOneRef());
  EXPECT_TRUE(test_obj2->HasOneRef());
}

// Test getting/setting inherited library-side RefPtr types.
TEST(TranslatorTest, RefPtrLibraryInherit) {
  CefRefPtr<CefTranslatorTest> obj = CefTranslatorTest::Create();

  const int kTestVal = 12;
  const int kTestVal2 = 40;
  CefRefPtr<CefTranslatorTestRefPtrLibraryChild> test_obj =
      CefTranslatorTestRefPtrLibraryChild::Create(kTestVal, kTestVal2);
  EXPECT_EQ(kTestVal, test_obj->GetValue());
  EXPECT_EQ(kTestVal2, test_obj->GetOtherValue());
  int retval = obj->SetRefPtrLibrary(test_obj);
  EXPECT_EQ(kTestVal, retval);
  EXPECT_EQ(kTestVal, test_obj->GetValue());
  EXPECT_EQ(kTestVal2, test_obj->GetOtherValue());

  EXPECT_EQ(kTestVal, obj->SetChildRefPtrLibrary(test_obj));
  EXPECT_EQ(kTestVal,
            obj->SetChildRefPtrLibraryAndReturnParent(test_obj)->GetValue());

  const int kTestVal3 = 100;
  CefRefPtr<CefTranslatorTestRefPtrLibraryChildChild> test_obj2 =
      CefTranslatorTestRefPtrLibraryChildChild::Create(kTestVal, kTestVal2,
                                                       kTestVal3);
  EXPECT_EQ(kTestVal, test_obj2->GetValue());
  EXPECT_EQ(kTestVal2, test_obj2->GetOtherValue());
  EXPECT_EQ(kTestVal3, test_obj2->GetOtherOtherValue());
  int retval2 = obj->SetRefPtrLibrary(test_obj2);
  EXPECT_EQ(kTestVal, retval2);
  EXPECT_EQ(kTestVal, test_obj2->GetValue());
  EXPECT_EQ(kTestVal2, test_obj2->GetOtherValue());
  EXPECT_EQ(kTestVal3, test_obj2->GetOtherOtherValue());

  EXPECT_EQ(kTestVal, obj->SetChildRefPtrLibrary(test_obj2));
  EXPECT_EQ(kTestVal,
            obj->SetChildRefPtrLibraryAndReturnParent(test_obj2)->GetValue());

  // Only one reference to the object should exist.
  EXPECT_TRUE(obj->HasOneRef());
  EXPECT_TRUE(test_obj->HasOneRef());
  EXPECT_TRUE(test_obj2->HasOneRef());
}

// Test getting/setting library-side RefPtr list types.
TEST(TranslatorTest, RefPtrLibraryList) {
  CefRefPtr<CefTranslatorTest> obj = CefTranslatorTest::Create();

  const int kVal1 = 34;
  const int kVal2 = 10;

  CefRefPtr<CefTranslatorTestRefPtrLibrary> val1 =
      CefTranslatorTestRefPtrLibrary::Create(kVal1);
  CefRefPtr<CefTranslatorTestRefPtrLibrary> val2 =
      CefTranslatorTestRefPtrLibraryChild::Create(kVal2, 0);

  std::vector<CefRefPtr<CefTranslatorTestRefPtrLibrary>> list;
  list.push_back(val1);
  list.push_back(val2);
  EXPECT_TRUE(obj->SetRefPtrLibraryList(list, kVal1, kVal2));

  list.clear();
  EXPECT_TRUE(obj->GetRefPtrLibraryListByRef(list, kVal1, kVal2));
  EXPECT_EQ(2U, list.size());
  EXPECT_EQ(kVal1, list[0]->GetValue());
  EXPECT_EQ(kVal2, list[1]->GetValue());

  list.clear();

  // Only one reference to the object should exist.
  EXPECT_TRUE(obj->HasOneRef());
  EXPECT_TRUE(val1->HasOneRef());
  EXPECT_TRUE(val2->HasOneRef());
}

namespace {

class TranslatorTestRefPtrClient : public CefTranslatorTestRefPtrClient {
 public:
  explicit TranslatorTestRefPtrClient(const int val) : val_(val) {}

  virtual int GetValue() override { return val_; }

 private:
  const int val_;

  IMPLEMENT_REFCOUNTING(TranslatorTestRefPtrClient);
  DISALLOW_COPY_AND_ASSIGN(TranslatorTestRefPtrClient);
};

class TranslatorTestRefPtrClientChild
    : public CefTranslatorTestRefPtrClientChild {
 public:
  TranslatorTestRefPtrClientChild(const int val, const int other_val)
      : val_(val), other_val_(other_val) {}

  virtual int GetValue() override { return val_; }

  virtual int GetOtherValue() override { return other_val_; }

 private:
  const int val_;
  const int other_val_;

  IMPLEMENT_REFCOUNTING(TranslatorTestRefPtrClientChild);
  DISALLOW_COPY_AND_ASSIGN(TranslatorTestRefPtrClientChild);
};

}  // namespace

// Test getting/setting client-side RefPtr types.
TEST(TranslatorTest, RefPtrClient) {
  CefRefPtr<CefTranslatorTest> obj = CefTranslatorTest::Create();

  const int kTestVal = 12;

  CefRefPtr<TranslatorTestRefPtrClient> test_obj =
      new TranslatorTestRefPtrClient(kTestVal);
  EXPECT_EQ(kTestVal, test_obj->GetValue());
  EXPECT_EQ(kTestVal, obj->SetRefPtrClient(test_obj.get()));
  CefRefPtr<CefTranslatorTestRefPtrClient> handler =
      obj->SetRefPtrClientAndReturn(test_obj.get());
  EXPECT_EQ(test_obj.get(), handler.get());
  EXPECT_EQ(kTestVal, handler->GetValue());
  handler = nullptr;

  // Only one reference to the object should exist.
  EXPECT_TRUE(obj->HasOneRef());
  EXPECT_TRUE(test_obj->HasOneRef());
}

// Test getting/setting inherited client-side RefPtr types.
TEST(TranslatorTest, RefPtrClientInherit) {
  CefRefPtr<CefTranslatorTest> obj = CefTranslatorTest::Create();

  const int kTestVal = 12;
  const int kTestVal2 = 86;

  CefRefPtr<TranslatorTestRefPtrClientChild> test_obj =
      new TranslatorTestRefPtrClientChild(kTestVal, kTestVal2);
  EXPECT_EQ(kTestVal, test_obj->GetValue());
  EXPECT_EQ(kTestVal2, test_obj->GetOtherValue());
  int retval = obj->SetRefPtrClient(test_obj);
  EXPECT_EQ(kTestVal, retval);
  EXPECT_EQ(kTestVal, test_obj->GetValue());
  EXPECT_EQ(kTestVal2, test_obj->GetOtherValue());

  EXPECT_EQ(kTestVal, obj->SetChildRefPtrClient(test_obj));
  CefRefPtr<CefTranslatorTestRefPtrClient> handler =
      obj->SetChildRefPtrClientAndReturnParent(test_obj);
  EXPECT_EQ(kTestVal, handler->GetValue());
  EXPECT_EQ(test_obj.get(), handler.get());
  handler = nullptr;

  // Only one reference to the object should exist.
  EXPECT_TRUE(obj->HasOneRef());
  EXPECT_TRUE(test_obj->HasOneRef());
}

// Test getting/setting client-side RefPtr list types.
TEST(TranslatorTest, RefPtrClientList) {
  CefRefPtr<CefTranslatorTest> obj = CefTranslatorTest::Create();

  const int kVal1 = 34;
  const int kVal2 = 10;

  CefRefPtr<CefTranslatorTestRefPtrClient> val1 =
      new TranslatorTestRefPtrClient(kVal1);
  CefRefPtr<CefTranslatorTestRefPtrClient> val2 =
      new TranslatorTestRefPtrClientChild(kVal2, 0);

  std::vector<CefRefPtr<CefTranslatorTestRefPtrClient>> list;
  list.push_back(val1);
  list.push_back(val2);
  EXPECT_TRUE(obj->SetRefPtrClientList(list, kVal1, kVal2));

  list.clear();
  EXPECT_TRUE(obj->GetRefPtrClientListByRef(list, val1, val2));
  EXPECT_EQ(2U, list.size());
  EXPECT_EQ(kVal1, list[0]->GetValue());
  EXPECT_EQ(val1.get(), list[0].get());
  EXPECT_EQ(kVal2, list[1]->GetValue());
  EXPECT_EQ(val2.get(), list[1].get());

  list.clear();

  // Only one reference to the object should exist.
  EXPECT_TRUE(obj->HasOneRef());
  EXPECT_TRUE(val1->HasOneRef());
  EXPECT_TRUE(val2->HasOneRef());
}

// Test getting/setting library-side OwnPtr types.
TEST(TranslatorTest, OwnPtrLibrary) {
  CefRefPtr<CefTranslatorTest> obj = CefTranslatorTest::Create();

  const int kTestVal = 12;
  CefOwnPtr<CefTranslatorTestScopedLibrary> test_obj =
      CefTranslatorTestScopedLibrary::Create(kTestVal);
  EXPECT_TRUE(test_obj.get());
  EXPECT_EQ(kTestVal, test_obj->GetValue());
  int retval = obj->SetOwnPtrLibrary(std::move(test_obj));
  EXPECT_EQ(kTestVal, retval);
  EXPECT_FALSE(test_obj.get());

  const int kTestVal2 = 30;
  CefOwnPtr<CefTranslatorTestScopedLibrary> test_obj2 =
      obj->GetOwnPtrLibrary(kTestVal2);
  EXPECT_TRUE(test_obj2.get());
  EXPECT_EQ(kTestVal2, test_obj2->GetValue());
  int retval2 = obj->SetOwnPtrLibrary(std::move(test_obj2));
  EXPECT_EQ(kTestVal2, retval2);
  EXPECT_FALSE(test_obj2.get());

  // Only one reference to the object should exist.
  EXPECT_TRUE(obj->HasOneRef());
}

// Test getting/setting inherited library-side OwnPtr types.
TEST(TranslatorTest, OwnPtrLibraryInherit) {
  CefRefPtr<CefTranslatorTest> obj = CefTranslatorTest::Create();

  const int kTestVal = 12;
  const int kTestVal2 = 40;
  CefOwnPtr<CefTranslatorTestScopedLibraryChild> test_obj =
      CefTranslatorTestScopedLibraryChild::Create(kTestVal, kTestVal2);
  EXPECT_TRUE(test_obj.get());
  EXPECT_EQ(kTestVal, test_obj->GetValue());
  EXPECT_EQ(kTestVal2, test_obj->GetOtherValue());
  int retval = obj->SetOwnPtrLibrary(std::move(test_obj));
  EXPECT_EQ(kTestVal, retval);
  EXPECT_FALSE(test_obj.get());

  test_obj = CefTranslatorTestScopedLibraryChild::Create(kTestVal, kTestVal2);
  EXPECT_TRUE(test_obj.get());
  EXPECT_EQ(kTestVal, obj->SetChildOwnPtrLibrary(std::move(test_obj)));
  EXPECT_FALSE(test_obj.get());

  test_obj = CefTranslatorTestScopedLibraryChild::Create(kTestVal, kTestVal2);
  EXPECT_TRUE(test_obj.get());
  CefOwnPtr<CefTranslatorTestScopedLibrary> test_obj_parent =
      obj->SetChildOwnPtrLibraryAndReturnParent(std::move(test_obj));
  EXPECT_FALSE(test_obj.get());
  EXPECT_TRUE(test_obj_parent.get());
  EXPECT_EQ(kTestVal, test_obj_parent->GetValue());
  test_obj_parent.reset(nullptr);

  const int kTestVal3 = 100;
  CefOwnPtr<CefTranslatorTestScopedLibraryChildChild> test_obj2 =
      CefTranslatorTestScopedLibraryChildChild::Create(kTestVal, kTestVal2,
                                                       kTestVal3);
  EXPECT_EQ(kTestVal, test_obj2->GetValue());
  EXPECT_EQ(kTestVal2, test_obj2->GetOtherValue());
  EXPECT_EQ(kTestVal3, test_obj2->GetOtherOtherValue());
  int retval2 = obj->SetOwnPtrLibrary(std::move(test_obj2));
  EXPECT_EQ(kTestVal, retval2);
  EXPECT_FALSE(test_obj2.get());

  test_obj2 = CefTranslatorTestScopedLibraryChildChild::Create(
      kTestVal, kTestVal2, kTestVal3);
  EXPECT_EQ(kTestVal, obj->SetChildOwnPtrLibrary(std::move(test_obj2)));
  EXPECT_FALSE(test_obj2.get());

  test_obj2 = CefTranslatorTestScopedLibraryChildChild::Create(
      kTestVal, kTestVal2, kTestVal3);
  test_obj_parent =
      obj->SetChildOwnPtrLibraryAndReturnParent(std::move(test_obj2));
  EXPECT_FALSE(test_obj2.get());
  EXPECT_TRUE(test_obj_parent.get());
  EXPECT_EQ(kTestVal, test_obj_parent->GetValue());
  test_obj_parent.reset(nullptr);

  // Only one reference to the object should exist.
  EXPECT_TRUE(obj->HasOneRef());
}

namespace {

class TranslatorTestScopedClient : public CefTranslatorTestScopedClient {
 public:
  TranslatorTestScopedClient(const int val, TrackCallback* got_delete)
      : val_(val), got_delete_(got_delete) {}
  ~TranslatorTestScopedClient() override { got_delete_->yes(); }

  virtual int GetValue() override { return val_; }

 private:
  const int val_;
  TrackCallback* got_delete_;

  DISALLOW_COPY_AND_ASSIGN(TranslatorTestScopedClient);
};

class TranslatorTestScopedClientChild
    : public CefTranslatorTestScopedClientChild {
 public:
  TranslatorTestScopedClientChild(const int val,
                                  const int other_val,
                                  TrackCallback* got_delete)
      : val_(val), other_val_(other_val), got_delete_(got_delete) {}
  ~TranslatorTestScopedClientChild() override { got_delete_->yes(); }

  virtual int GetValue() override { return val_; }

  virtual int GetOtherValue() override { return other_val_; }

 private:
  const int val_;
  const int other_val_;
  TrackCallback* got_delete_;

  DISALLOW_COPY_AND_ASSIGN(TranslatorTestScopedClientChild);
};

}  // namespace

// Test getting/setting client-side OwnPtr types.
TEST(TranslatorTest, OwnPtrClient) {
  CefRefPtr<CefTranslatorTest> obj = CefTranslatorTest::Create();

  const int kTestVal = 12;
  TrackCallback got_delete;

  CefOwnPtr<CefTranslatorTestScopedClient> test_obj(
      new TranslatorTestScopedClient(kTestVal, &got_delete));
  EXPECT_EQ(kTestVal, test_obj->GetValue());
  EXPECT_EQ(kTestVal, obj->SetOwnPtrClient(std::move(test_obj)));
  EXPECT_FALSE(test_obj.get());
  EXPECT_TRUE(got_delete);

  got_delete.reset();
  test_obj.reset(new TranslatorTestScopedClient(kTestVal, &got_delete));
  CefOwnPtr<CefTranslatorTestScopedClient> handler =
      obj->SetOwnPtrClientAndReturn(std::move(test_obj));
  EXPECT_FALSE(test_obj.get());
  EXPECT_TRUE(handler.get());
  EXPECT_FALSE(got_delete);
  EXPECT_EQ(kTestVal, handler->GetValue());
  handler.reset(nullptr);
  EXPECT_TRUE(got_delete);

  // Only one reference to the object should exist.
  EXPECT_TRUE(obj->HasOneRef());
}

// Test getting/setting inherited client-side OwnPtr types.
TEST(TranslatorTest, OwnPtrClientInherit) {
  CefRefPtr<CefTranslatorTest> obj = CefTranslatorTest::Create();

  const int kTestVal = 12;
  const int kTestVal2 = 86;
  TrackCallback got_delete;

  CefOwnPtr<CefTranslatorTestScopedClientChild> test_obj(
      new TranslatorTestScopedClientChild(kTestVal, kTestVal2, &got_delete));
  EXPECT_EQ(kTestVal, test_obj->GetValue());
  EXPECT_EQ(kTestVal2, test_obj->GetOtherValue());
  EXPECT_EQ(kTestVal, obj->SetOwnPtrClient(std::move(test_obj)));
  EXPECT_FALSE(test_obj.get());
  EXPECT_TRUE(got_delete);

  got_delete.reset();
  test_obj.reset(
      new TranslatorTestScopedClientChild(kTestVal, kTestVal2, &got_delete));
  EXPECT_EQ(kTestVal, obj->SetChildOwnPtrClient(std::move(test_obj)));
  EXPECT_FALSE(test_obj.get());
  EXPECT_TRUE(got_delete);

  got_delete.reset();
  test_obj.reset(
      new TranslatorTestScopedClientChild(kTestVal, kTestVal2, &got_delete));
  CefOwnPtr<CefTranslatorTestScopedClient> handler(
      obj->SetChildOwnPtrClientAndReturnParent(std::move(test_obj)));
  EXPECT_EQ(kTestVal, handler->GetValue());
  EXPECT_FALSE(test_obj.get());
  EXPECT_FALSE(got_delete);
  handler.reset(nullptr);
  EXPECT_TRUE(got_delete);

  // Only one reference to the object should exist.
  EXPECT_TRUE(obj->HasOneRef());
}

// Test getting/setting library-side RawPtr types.
TEST(TranslatorTest, RawPtrLibrary) {
  CefRefPtr<CefTranslatorTest> obj = CefTranslatorTest::Create();

  const int kTestVal = 12;
  CefOwnPtr<CefTranslatorTestScopedLibrary> test_obj(
      CefTranslatorTestScopedLibrary::Create(kTestVal));
  EXPECT_EQ(kTestVal, test_obj->GetValue());
  int retval = obj->SetRawPtrLibrary(test_obj.get());
  EXPECT_EQ(kTestVal, retval);
  EXPECT_EQ(kTestVal, test_obj->GetValue());

  const int kTestVal2 = 30;
  CefOwnPtr<CefTranslatorTestScopedLibrary> test_obj2(
      obj->GetOwnPtrLibrary(kTestVal2));
  EXPECT_EQ(kTestVal2, test_obj2->GetValue());
  int retval2 = obj->SetRawPtrLibrary(test_obj2.get());
  EXPECT_EQ(kTestVal2, retval2);
  EXPECT_EQ(kTestVal2, test_obj2->GetValue());

  // Only one reference to the object should exist.
  EXPECT_TRUE(obj->HasOneRef());
}

// Test getting/setting inherited library-side RawPtr types.
TEST(TranslatorTest, RawPtrLibraryInherit) {
  CefRefPtr<CefTranslatorTest> obj = CefTranslatorTest::Create();

  const int kTestVal = 12;
  const int kTestVal2 = 40;
  CefOwnPtr<CefTranslatorTestScopedLibraryChild> test_obj(
      CefTranslatorTestScopedLibraryChild::Create(kTestVal, kTestVal2));
  EXPECT_EQ(kTestVal, test_obj->GetValue());
  EXPECT_EQ(kTestVal2, test_obj->GetOtherValue());
  int retval = obj->SetRawPtrLibrary(test_obj.get());
  EXPECT_EQ(kTestVal, retval);
  EXPECT_EQ(kTestVal, test_obj->GetValue());
  EXPECT_EQ(kTestVal2, test_obj->GetOtherValue());

  EXPECT_EQ(kTestVal, obj->SetChildRawPtrLibrary(test_obj.get()));

  const int kTestVal3 = 100;
  CefOwnPtr<CefTranslatorTestScopedLibraryChildChild> test_obj2(
      CefTranslatorTestScopedLibraryChildChild::Create(kTestVal, kTestVal2,
                                                       kTestVal3));
  EXPECT_EQ(kTestVal, test_obj2->GetValue());
  EXPECT_EQ(kTestVal2, test_obj2->GetOtherValue());
  EXPECT_EQ(kTestVal3, test_obj2->GetOtherOtherValue());
  int retval2 = obj->SetRawPtrLibrary(test_obj2.get());
  EXPECT_EQ(kTestVal, retval2);
  EXPECT_EQ(kTestVal, test_obj2->GetValue());
  EXPECT_EQ(kTestVal2, test_obj2->GetOtherValue());
  EXPECT_EQ(kTestVal3, test_obj2->GetOtherOtherValue());

  EXPECT_EQ(kTestVal, obj->SetChildRawPtrLibrary(test_obj2.get()));

  // Only one reference to the object should exist.
  EXPECT_TRUE(obj->HasOneRef());
}

// Test getting/setting library-side RawPtr list types.
TEST(TranslatorTest, RawPtrLibraryList) {
  CefRefPtr<CefTranslatorTest> obj = CefTranslatorTest::Create();

  const int kVal1 = 34;
  const int kVal2 = 10;

  CefOwnPtr<CefTranslatorTestScopedLibrary> val1(
      CefTranslatorTestScopedLibrary::Create(kVal1));
  CefOwnPtr<CefTranslatorTestScopedLibraryChild> val2(
      CefTranslatorTestScopedLibraryChild::Create(kVal2, 0));

  std::vector<CefRawPtr<CefTranslatorTestScopedLibrary>> list;
  list.push_back(val1.get());
  list.push_back(val2.get());
  EXPECT_TRUE(obj->SetRawPtrLibraryList(list, kVal1, kVal2));
  list.clear();

  // Only one reference to the object should exist.
  EXPECT_TRUE(obj->HasOneRef());
}

// Test getting/setting client-side RawPtr types.
TEST(TranslatorTest, RawPtrClient) {
  CefRefPtr<CefTranslatorTest> obj = CefTranslatorTest::Create();

  const int kTestVal = 12;
  TrackCallback got_delete;

  CefOwnPtr<TranslatorTestScopedClient> test_obj(
      new TranslatorTestScopedClient(kTestVal, &got_delete));
  EXPECT_EQ(kTestVal, test_obj->GetValue());
  EXPECT_EQ(kTestVal, obj->SetRawPtrClient(test_obj.get()));
  EXPECT_FALSE(got_delete);
  test_obj.reset(nullptr);
  EXPECT_TRUE(got_delete);

  // Only one reference to the object should exist.
  EXPECT_TRUE(obj->HasOneRef());
}

// Test getting/setting inherited client-side RawPtr types.
TEST(TranslatorTest, RawPtrClientInherit) {
  CefRefPtr<CefTranslatorTest> obj = CefTranslatorTest::Create();

  const int kTestVal = 12;
  const int kTestVal2 = 86;
  TrackCallback got_delete;

  CefOwnPtr<TranslatorTestScopedClientChild> test_obj(
      new TranslatorTestScopedClientChild(kTestVal, kTestVal2, &got_delete));
  EXPECT_EQ(kTestVal, test_obj->GetValue());
  EXPECT_EQ(kTestVal2, test_obj->GetOtherValue());
  int retval = obj->SetRawPtrClient(test_obj.get());
  EXPECT_EQ(kTestVal, retval);
  EXPECT_EQ(kTestVal, test_obj->GetValue());
  EXPECT_EQ(kTestVal2, test_obj->GetOtherValue());
  EXPECT_FALSE(got_delete);

  EXPECT_EQ(kTestVal, obj->SetChildRawPtrClient(test_obj.get()));
  EXPECT_FALSE(got_delete);
  test_obj.reset(nullptr);
  EXPECT_TRUE(got_delete);

  // Only one reference to the object should exist.
  EXPECT_TRUE(obj->HasOneRef());
}

// Test getting/setting client-side RawPtr list types.
TEST(TranslatorTest, RawPtrClientList) {
  CefRefPtr<CefTranslatorTest> obj = CefTranslatorTest::Create();

  const int kVal1 = 34;
  const int kVal2 = 10;
  TrackCallback got_delete1, got_delete2;

  CefOwnPtr<CefTranslatorTestScopedClient> val1(
      new TranslatorTestScopedClient(kVal1, &got_delete1));
  CefOwnPtr<CefTranslatorTestScopedClient> val2(
      new TranslatorTestScopedClientChild(kVal2, 0, &got_delete2));

  std::vector<CefRawPtr<CefTranslatorTestScopedClient>> list;
  list.push_back(val1.get());
  list.push_back(val2.get());
  EXPECT_TRUE(obj->SetRawPtrClientList(list, kVal1, kVal2));
  list.clear();

  EXPECT_FALSE(got_delete1);
  val1.reset(nullptr);
  EXPECT_TRUE(got_delete1);

  EXPECT_FALSE(got_delete2);
  val2.reset(nullptr);
  EXPECT_TRUE(got_delete2);

  // Only one reference to the object should exist.
  EXPECT_TRUE(obj->HasOneRef());
}
