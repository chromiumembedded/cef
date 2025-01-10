// Copyright (c) 2024 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <memory>

#include "include/test/cef_api_version_test.h"
#include "tests/ceftests/test_handler.h"
#include "tests/gtest/include/gtest/gtest.h"

namespace {

template <typename T>
static int GetValue(T& obj) {
#if CEF_API_REMOVED(13301)
  return obj->GetValue();
#elif CEF_API_RANGE(13301, 13302)
  return obj->GetValueV1();
#elif CEF_API_ADDED(13302)
  return obj->GetValueV2();
#endif
}

CefRefPtr<CefApiVersionTestRefPtrLibrary> CreateRefPtrLibrary(int val) {
#if CEF_API_ADDED(13301)
  return CefApiVersionTestRefPtrLibrary::Create(val);
#else
  auto obj = CefApiVersionTestRefPtrLibrary::Create();
  obj->SetValue(val);
  return obj;
#endif
}

CefRefPtr<CefApiVersionTestRefPtrLibraryChild> CreateRefPtrLibraryChild(
    int val1,
    int val2) {
#if CEF_API_ADDED(13301)
  return CefApiVersionTestRefPtrLibraryChild::Create(val1, val2);
#else
  auto obj = CefApiVersionTestRefPtrLibraryChild::Create();
  obj->SetValue(val1);
  obj->SetOtherValue(val2);
  return obj;
#endif
}

#if CEF_API_REMOVED(13301)
CefRefPtr<CefApiVersionTestRefPtrLibraryChildChild>
#elif CEF_API_RANGE(13301, 13302)
CefRefPtr<CefApiVersionTestRefPtrLibraryChildChildV1>
#elif CEF_API_ADDED(13302)
CefRefPtr<CefApiVersionTestRefPtrLibraryChildChildV2>
#endif
CreateRefPtrLibraryChildChild(int val1, int val2, int val3) {
#if CEF_API_REMOVED(13301)
  auto obj = CefApiVersionTestRefPtrLibraryChildChild::Create();
  obj->SetValue(val1);
  obj->SetOtherValue(val2);
  obj->SetOtherOtherValue(val3);
  return obj;
#elif CEF_API_RANGE(13301, 13302)
  return CefApiVersionTestRefPtrLibraryChildChildV1::Create(val1, val2, val3);
#elif CEF_API_ADDED(13302)
  return CefApiVersionTestRefPtrLibraryChildChildV2::Create(val1, val2, val3);
#endif
}

}  // namespace

// Test getting/setting library-side RefPtr types.
TEST(ApiVersionTest, RefPtrLibrary) {
  CefRefPtr<CefApiVersionTest> obj = CefApiVersionTest::Create();

  const int kTestVal = 12;
  CefRefPtr<CefApiVersionTestRefPtrLibrary> test_obj =
      CreateRefPtrLibrary(kTestVal);
  EXPECT_EQ(kTestVal, GetValue(test_obj));
  int retval = obj->SetRefPtrLibrary(test_obj);
  EXPECT_EQ(kTestVal, retval);
  EXPECT_EQ(kTestVal, GetValue(test_obj));

  const int kTestVal2 = 30;
  CefRefPtr<CefApiVersionTestRefPtrLibrary> test_obj2 =
      obj->GetRefPtrLibrary(kTestVal2);
  EXPECT_EQ(kTestVal2, GetValue(test_obj2));
  int retval2 = obj->SetRefPtrLibrary(test_obj2);
  EXPECT_EQ(kTestVal2, retval2);
  EXPECT_EQ(kTestVal2, GetValue(test_obj2));

  // Only one reference to the object should exist.
  EXPECT_TRUE(obj->HasOneRef());
  EXPECT_TRUE(test_obj->HasOneRef());
  EXPECT_TRUE(test_obj2->HasOneRef());
}

// Test getting/setting inherited library-side RefPtr types.
TEST(ApiVersionTest, RefPtrLibraryInherit) {
  CefRefPtr<CefApiVersionTest> obj = CefApiVersionTest::Create();

  const int kTestVal = 12;
  const int kTestVal2 = 40;
  auto test_obj = CreateRefPtrLibraryChild(kTestVal, kTestVal2);
  EXPECT_EQ(kTestVal, GetValue(test_obj));
  EXPECT_EQ(kTestVal2, test_obj->GetOtherValue());
  int retval = obj->SetRefPtrLibrary(test_obj);
  EXPECT_EQ(kTestVal, retval);
  EXPECT_EQ(kTestVal, GetValue(test_obj));
  EXPECT_EQ(kTestVal2, test_obj->GetOtherValue());

  EXPECT_EQ(kTestVal, obj->SetChildRefPtrLibrary(test_obj));
  auto parent = obj->SetChildRefPtrLibraryAndReturnParent(test_obj);
  EXPECT_EQ(kTestVal, GetValue(parent));
  parent = nullptr;

  const int kTestVal3 = 100;
  auto test_obj2 =
      CreateRefPtrLibraryChildChild(kTestVal, kTestVal2, kTestVal3);
  EXPECT_EQ(kTestVal, GetValue(test_obj2));
  EXPECT_EQ(kTestVal2, test_obj2->GetOtherValue());
  EXPECT_EQ(kTestVal3, test_obj2->GetOtherOtherValue());
  int retval2 = obj->SetRefPtrLibrary(test_obj2);
  EXPECT_EQ(kTestVal, retval2);
  EXPECT_EQ(kTestVal, GetValue(test_obj2));
  EXPECT_EQ(kTestVal2, test_obj2->GetOtherValue());
  EXPECT_EQ(kTestVal3, test_obj2->GetOtherOtherValue());

  EXPECT_EQ(kTestVal, obj->SetChildRefPtrLibrary(test_obj2));
  auto parent2 = obj->SetChildRefPtrLibraryAndReturnParent(test_obj2);
  EXPECT_EQ(kTestVal, GetValue(parent2));
  parent2 = nullptr;

  // Only one reference to the object should exist.
  EXPECT_TRUE(obj->HasOneRef());
  EXPECT_TRUE(test_obj->HasOneRef());
  EXPECT_TRUE(test_obj2->HasOneRef());
}

// Test getting/setting library-side RefPtr list types.
TEST(ApiVersionTest, RefPtrLibraryList) {
  CefRefPtr<CefApiVersionTest> obj = CefApiVersionTest::Create();

  const int kVal1 = 34;
  const int kVal2 = 10;

  CefRefPtr<CefApiVersionTestRefPtrLibrary> val1 = CreateRefPtrLibrary(kVal1);
  CefRefPtr<CefApiVersionTestRefPtrLibrary> val2 =
      CreateRefPtrLibraryChild(kVal2, 0);

  std::vector<CefRefPtr<CefApiVersionTestRefPtrLibrary>> list;
  list.push_back(val1);
  list.push_back(val2);
  EXPECT_TRUE(obj->SetRefPtrLibraryList(list, kVal1, kVal2));

  list.clear();
  EXPECT_TRUE(obj->GetRefPtrLibraryListByRef(list, kVal1, kVal2));
  EXPECT_EQ(2U, list.size());
  EXPECT_EQ(kVal1, GetValue(list[0]));
  EXPECT_EQ(kVal2, GetValue(list[1]));

  list.clear();

  // Only one reference to the object should exist.
  EXPECT_TRUE(obj->HasOneRef());
  EXPECT_TRUE(val1->HasOneRef());
  EXPECT_TRUE(val2->HasOneRef());
}

namespace {

class ApiVersionTestRefPtrClient : public CefApiVersionTestRefPtrClient {
 public:
  explicit ApiVersionTestRefPtrClient(int val) : val_(val) {}

  int GetValueLegacy() override { return val_legacy_; }

#if CEF_API_ADDED(CEF_EXPERIMENTAL)
  int GetValueExp() override { return val_exp_; }
#endif

#if CEF_API_REMOVED(13301)
  int GetValue() override { return val_; }
#elif CEF_API_RANGE(13301, 13302)
  int GetValueV1() override { return val_; }
#elif CEF_API_ADDED(13302)
  int GetValueV2() override { return val_; }
#endif

 private:
  const int val_;
  int val_legacy_ = -1;
#if CEF_API_ADDED(CEF_EXPERIMENTAL)
  int val_exp_ = -1;
#endif

  IMPLEMENT_REFCOUNTING(ApiVersionTestRefPtrClient);
  DISALLOW_COPY_AND_ASSIGN(ApiVersionTestRefPtrClient);
};

#if CEF_API_REMOVED(13302)

class ApiVersionTestRefPtrClientChild
    : public CefApiVersionTestRefPtrClientChild {
 public:
  ApiVersionTestRefPtrClientChild(int val, int other_val)
      : val_(val), other_val_(other_val) {}

  int GetValueLegacy() override { return val_legacy_; }

#if CEF_API_ADDED(CEF_EXPERIMENTAL)
  int GetValueExp() override { return val_exp_; }
#endif

#if CEF_API_REMOVED(13301)
  int GetValue() override { return val_; }
#elif CEF_API_RANGE(13301, 13302)
  int GetValueV1() override { return val_; }
#elif CEF_API_ADDED(13302)
  int GetValueV2() override { return val_; }
#endif

#if CEF_API_REMOVED(13301)
  int GetOtherValue() override { return other_val_; }
#else
  int GetOtherValueV1() override { return other_val_; }
#endif

 private:
  const int val_;
  const int other_val_;
  int val_legacy_ = -1;
#if CEF_API_ADDED(CEF_EXPERIMENTAL)
  int val_exp_ = -1;
#endif

  IMPLEMENT_REFCOUNTING(ApiVersionTestRefPtrClientChild);
  DISALLOW_COPY_AND_ASSIGN(ApiVersionTestRefPtrClientChild);
};

using ApiVersionTestRefPtrClientChildType = ApiVersionTestRefPtrClientChild;

#else  // CEF_API_ADDED(13302)

class ApiVersionTestRefPtrClientChildV2
    : public CefApiVersionTestRefPtrClientChildV2 {
 public:
  ApiVersionTestRefPtrClientChildV2(int val, int other_val)
      : val_(val), other_val_(other_val) {}

  int GetValueLegacy() override { return val_legacy_; }

#if CEF_API_ADDED(CEF_EXPERIMENTAL)
  int GetValueExp() override { return val_exp_; }
#endif

#if CEF_API_REMOVED(13301)
  int GetValue() override { return val_; }
#elif CEF_API_RANGE(13301, 13302)
  int GetValueV1() override { return val_; }
#elif CEF_API_ADDED(13302)
  int GetValueV2() override { return val_; }
#endif

  int GetOtherValue() override { return other_val_; }

#if CEF_API_ADDED(13303)
  int GetAnotherValue() override { return another_val_; }
#endif

 private:
  const int val_;
  const int other_val_;
#if CEF_API_ADDED(13303)
  int another_val_ = -1;
#endif
  int val_legacy_ = -1;
#if CEF_API_ADDED(CEF_EXPERIMENTAL)
  int val_exp_ = -1;
#endif

  IMPLEMENT_REFCOUNTING(ApiVersionTestRefPtrClientChildV2);
  DISALLOW_COPY_AND_ASSIGN(ApiVersionTestRefPtrClientChildV2);
};

using ApiVersionTestRefPtrClientChildType = ApiVersionTestRefPtrClientChildV2;

#endif  // CEF_API_ADDED(13302)

template <typename T>
static int GetOtherValue(T& obj) {
#if CEF_API_REMOVED(13301)
  // ApiVersionTestRefPtrClientChild
  return obj->GetOtherValue();
#elif CEF_API_RANGE(13301, 13302)
  // ApiVersionTestRefPtrClientChild
  return obj->GetOtherValueV1();
#elif CEF_API_ADDED(13302)
  // ApiVersionTestRefPtrClientChildV2
  return obj->GetOtherValue();
#endif
}

}  // namespace

// Test getting/setting client-side RefPtr types.
TEST(ApiVersionTest, RefPtrClient) {
  CefRefPtr<CefApiVersionTest> obj = CefApiVersionTest::Create();

  const int kTestVal = 12;

  CefRefPtr<ApiVersionTestRefPtrClient> test_obj =
      new ApiVersionTestRefPtrClient(kTestVal);
  EXPECT_EQ(kTestVal, GetValue(test_obj));
  EXPECT_EQ(kTestVal, obj->SetRefPtrClient(test_obj.get()));
  CefRefPtr<CefApiVersionTestRefPtrClient> handler =
      obj->SetRefPtrClientAndReturn(test_obj.get());
  EXPECT_EQ(test_obj.get(), handler.get());
  EXPECT_EQ(kTestVal, GetValue(handler));
  handler = nullptr;

  // Only one reference to the object should exist.
  EXPECT_TRUE(obj->HasOneRef());
  EXPECT_TRUE(test_obj->HasOneRef());
}

// Test getting/setting inherited client-side RefPtr types.
TEST(ApiVersionTest, RefPtrClientInherit) {
  CefRefPtr<CefApiVersionTest> obj = CefApiVersionTest::Create();

  const int kTestVal = 12;
  const int kTestVal2 = 86;

  CefRefPtr<ApiVersionTestRefPtrClientChildType> test_obj =
      new ApiVersionTestRefPtrClientChildType(kTestVal, kTestVal2);
  EXPECT_EQ(kTestVal, GetValue(test_obj));
  EXPECT_EQ(kTestVal2, GetOtherValue(test_obj));
  int retval = obj->SetRefPtrClient(test_obj);
  EXPECT_EQ(kTestVal, retval);
  EXPECT_EQ(kTestVal, GetValue(test_obj));
  EXPECT_EQ(kTestVal2, GetOtherValue(test_obj));

  EXPECT_EQ(kTestVal, obj->SetChildRefPtrClient(test_obj));
  CefRefPtr<CefApiVersionTestRefPtrClient> handler =
      obj->SetChildRefPtrClientAndReturnParent(test_obj);
  EXPECT_EQ(kTestVal, GetValue(handler));
  EXPECT_EQ(test_obj.get(), handler.get());
  handler = nullptr;

  // Only one reference to the object should exist.
  EXPECT_TRUE(obj->HasOneRef());
  EXPECT_TRUE(test_obj->HasOneRef());
}

// Test getting/setting client-side RefPtr list types.
TEST(ApiVersionTest, RefPtrClientList) {
  CefRefPtr<CefApiVersionTest> obj = CefApiVersionTest::Create();

  const int kVal1 = 34;
  const int kVal2 = 10;

  CefRefPtr<CefApiVersionTestRefPtrClient> val1 =
      new ApiVersionTestRefPtrClient(kVal1);
  CefRefPtr<CefApiVersionTestRefPtrClient> val2 =
      new ApiVersionTestRefPtrClientChildType(kVal2, 0);

  std::vector<CefRefPtr<CefApiVersionTestRefPtrClient>> list;
  list.push_back(val1);
  list.push_back(val2);
  EXPECT_TRUE(obj->SetRefPtrClientList(list, kVal1, kVal2));

  list.clear();
  EXPECT_TRUE(obj->GetRefPtrClientListByRef(list, val1, val2));
  EXPECT_EQ(2U, list.size());
  EXPECT_EQ(kVal1, GetValue(list[0]));
  EXPECT_EQ(val1.get(), list[0].get());
  EXPECT_EQ(kVal2, GetValue(list[1]));
  EXPECT_EQ(val2.get(), list[1].get());

  list.clear();

  // Only one reference to the object should exist.
  EXPECT_TRUE(obj->HasOneRef());
  EXPECT_TRUE(val1->HasOneRef());
  EXPECT_TRUE(val2->HasOneRef());
}

namespace {

CefOwnPtr<CefApiVersionTestScopedLibrary> CreateScopedLibrary(int val) {
#if CEF_API_ADDED(13301)
  return CefApiVersionTestScopedLibrary::Create(val);
#else
  auto obj = CefApiVersionTestScopedLibrary::Create();
  obj->SetValue(val);
  return obj;
#endif
}

CefOwnPtr<CefApiVersionTestScopedLibraryChild> CreateScopedLibraryChild(
    int val1,
    int val2) {
#if CEF_API_ADDED(13301)
  return CefApiVersionTestScopedLibraryChild::Create(val1, val2);
#else
  auto obj = CefApiVersionTestScopedLibraryChild::Create();
  obj->SetValue(val1);
  obj->SetOtherValue(val2);
  return obj;
#endif
}

#if CEF_API_REMOVED(13301)
CefOwnPtr<CefApiVersionTestScopedLibraryChildChild>
#elif CEF_API_RANGE(13301, 13302)
CefOwnPtr<CefApiVersionTestScopedLibraryChildChildV1>
#elif CEF_API_ADDED(13302)
CefOwnPtr<CefApiVersionTestScopedLibraryChildChildV2>
#endif
CreateScopedLibraryChildChild(int val1, int val2, int val3) {
#if CEF_API_REMOVED(13301)
  auto obj = CefApiVersionTestScopedLibraryChildChild::Create();
  obj->SetValue(val1);
  obj->SetOtherValue(val2);
  obj->SetOtherOtherValue(val3);
  return obj;
#elif CEF_API_RANGE(13301, 13302)
  return CefApiVersionTestScopedLibraryChildChildV1::Create(val1, val2, val3);
#elif CEF_API_ADDED(13302)
  return CefApiVersionTestScopedLibraryChildChildV2::Create(val1, val2, val3);
#endif
}

}  // namespace

// Test getting/setting library-side OwnPtr types.
TEST(ApiVersionTest, OwnPtrLibrary) {
  CefRefPtr<CefApiVersionTest> obj = CefApiVersionTest::Create();

  const int kTestVal = 12;
  CefOwnPtr<CefApiVersionTestScopedLibrary> test_obj =
      CreateScopedLibrary(kTestVal);
  EXPECT_TRUE(test_obj.get());
  EXPECT_EQ(kTestVal, GetValue(test_obj));
  int retval = obj->SetOwnPtrLibrary(std::move(test_obj));
  EXPECT_EQ(kTestVal, retval);
  EXPECT_FALSE(test_obj.get());

  const int kTestVal2 = 30;
  CefOwnPtr<CefApiVersionTestScopedLibrary> test_obj2 =
      obj->GetOwnPtrLibrary(kTestVal2);
  EXPECT_TRUE(test_obj2.get());
  EXPECT_EQ(kTestVal2, GetValue(test_obj2));
  int retval2 = obj->SetOwnPtrLibrary(std::move(test_obj2));
  EXPECT_EQ(kTestVal2, retval2);
  EXPECT_FALSE(test_obj2.get());

  // Only one reference to the object should exist.
  EXPECT_TRUE(obj->HasOneRef());
}

// Test getting/setting inherited library-side OwnPtr types.
TEST(ApiVersionTest, OwnPtrLibraryInherit) {
  CefRefPtr<CefApiVersionTest> obj = CefApiVersionTest::Create();

  const int kTestVal = 12;
  const int kTestVal2 = 40;
  auto test_obj = CreateScopedLibraryChild(kTestVal, kTestVal2);
  EXPECT_TRUE(test_obj.get());
  EXPECT_EQ(kTestVal, GetValue(test_obj));
  EXPECT_EQ(kTestVal2, test_obj->GetOtherValue());
  int retval = obj->SetOwnPtrLibrary(std::move(test_obj));
  EXPECT_EQ(kTestVal, retval);
  EXPECT_FALSE(test_obj.get());

  test_obj = CreateScopedLibraryChild(kTestVal, kTestVal2);
  EXPECT_TRUE(test_obj.get());
  EXPECT_EQ(kTestVal, obj->SetChildOwnPtrLibrary(std::move(test_obj)));
  EXPECT_FALSE(test_obj.get());

  test_obj = CreateScopedLibraryChild(kTestVal, kTestVal2);
  EXPECT_TRUE(test_obj.get());
  CefOwnPtr<CefApiVersionTestScopedLibrary> test_obj_parent =
      obj->SetChildOwnPtrLibraryAndReturnParent(std::move(test_obj));
  EXPECT_FALSE(test_obj.get());
  EXPECT_TRUE(test_obj_parent.get());
  EXPECT_EQ(kTestVal, GetValue(test_obj_parent));
  test_obj_parent.reset(nullptr);

  const int kTestVal3 = 100;
  auto test_obj2 =
      CreateScopedLibraryChildChild(kTestVal, kTestVal2, kTestVal3);
  EXPECT_EQ(kTestVal, GetValue(test_obj2));
  EXPECT_EQ(kTestVal2, test_obj2->GetOtherValue());
  EXPECT_EQ(kTestVal3, test_obj2->GetOtherOtherValue());
  int retval2 = obj->SetOwnPtrLibrary(std::move(test_obj2));
  EXPECT_EQ(kTestVal, retval2);
  EXPECT_FALSE(test_obj2.get());

  test_obj2 = CreateScopedLibraryChildChild(kTestVal, kTestVal2, kTestVal3);
  EXPECT_EQ(kTestVal, obj->SetChildOwnPtrLibrary(std::move(test_obj2)));
  EXPECT_FALSE(test_obj2.get());

  test_obj2 = CreateScopedLibraryChildChild(kTestVal, kTestVal2, kTestVal3);
  test_obj_parent =
      obj->SetChildOwnPtrLibraryAndReturnParent(std::move(test_obj2));
  EXPECT_FALSE(test_obj2.get());
  EXPECT_TRUE(test_obj_parent.get());
  EXPECT_EQ(kTestVal, GetValue(test_obj_parent));
  test_obj_parent.reset(nullptr);

  // Only one reference to the object should exist.
  EXPECT_TRUE(obj->HasOneRef());
}

namespace {

class ApiVersionTestScopedClient : public CefApiVersionTestScopedClient {
 public:
  ApiVersionTestScopedClient(int val, TrackCallback* got_delete)
      : val_(val), got_delete_(got_delete) {}
  ~ApiVersionTestScopedClient() override { got_delete_->yes(); }

  int GetValueLegacy() override { return val_legacy_; }

#if CEF_API_ADDED(CEF_EXPERIMENTAL)
  int GetValueExp() override { return val_exp_; }
#endif

#if CEF_API_REMOVED(13301)
  int GetValue() override { return val_; }
#elif CEF_API_RANGE(13301, 13302)
  int GetValueV1() override { return val_; }
#elif CEF_API_ADDED(13302)
  int GetValueV2() override { return val_; }
#endif

 private:
  const int val_;
  int val_legacy_ = -1;
#if CEF_API_ADDED(CEF_EXPERIMENTAL)
  int val_exp_ = -1;
#endif

  TrackCallback* got_delete_;

  DISALLOW_COPY_AND_ASSIGN(ApiVersionTestScopedClient);
};

#if CEF_API_REMOVED(13302)

class ApiVersionTestScopedClientChild
    : public CefApiVersionTestScopedClientChild {
 public:
  ApiVersionTestScopedClientChild(int val,
                                  int other_val,
                                  TrackCallback* got_delete)
      : val_(val), other_val_(other_val), got_delete_(got_delete) {}
  ~ApiVersionTestScopedClientChild() override { got_delete_->yes(); }

  int GetValueLegacy() override { return val_legacy_; }

#if CEF_API_ADDED(CEF_EXPERIMENTAL)
  int GetValueExp() override { return val_exp_; }
#endif

#if CEF_API_REMOVED(13301)
  int GetValue() override { return val_; }
#elif CEF_API_RANGE(13301, 13302)
  int GetValueV1() override { return val_; }
#elif CEF_API_ADDED(13302)
  int GetValueV2() override { return val_; }
#endif

#if CEF_API_REMOVED(13301)
  int GetOtherValue() override { return other_val_; }
#else
  int GetOtherValueV1() override { return other_val_; }
#endif

 private:
  const int val_;
  const int other_val_;
  int val_legacy_ = -1;
#if CEF_API_ADDED(CEF_EXPERIMENTAL)
  int val_exp_ = -1;
#endif

  TrackCallback* got_delete_;

  DISALLOW_COPY_AND_ASSIGN(ApiVersionTestScopedClientChild);
};

using ApiVersionTestScopedClientChildType = ApiVersionTestScopedClientChild;

#else  // CEF_API_ADDED(13302)

class ApiVersionTestScopedClientChildV2
    : public CefApiVersionTestScopedClientChildV2 {
 public:
  ApiVersionTestScopedClientChildV2(int val,
                                    int other_val,
                                    TrackCallback* got_delete)
      : val_(val), other_val_(other_val), got_delete_(got_delete) {}
  ~ApiVersionTestScopedClientChildV2() override { got_delete_->yes(); }

  int GetValueLegacy() override { return val_legacy_; }

#if CEF_API_ADDED(CEF_EXPERIMENTAL)
  int GetValueExp() override { return val_exp_; }
#endif

#if CEF_API_REMOVED(13301)
  int GetValue() override { return val_; }
#elif CEF_API_RANGE(13301, 13302)
  int GetValueV1() override { return val_; }
#elif CEF_API_ADDED(13302)
  int GetValueV2() override { return val_; }
#endif

  int GetOtherValue() override { return other_val_; }

#if CEF_API_ADDED(13303)
  int GetAnotherValue() override { return another_val_; }
#endif

 private:
  const int val_;
  const int other_val_;
#if CEF_API_ADDED(13303)
  int another_val_ = -1;
#endif
  int val_legacy_ = -1;
#if CEF_API_ADDED(CEF_EXPERIMENTAL)
  int val_exp_ = -1;
#endif

  TrackCallback* got_delete_;

  DISALLOW_COPY_AND_ASSIGN(ApiVersionTestScopedClientChildV2);
};

using ApiVersionTestScopedClientChildType = ApiVersionTestScopedClientChildV2;

#endif  // CEF_API_ADDED(13302)

}  // namespace

// Test getting/setting client-side OwnPtr types.
TEST(ApiVersionTest, OwnPtrClient) {
  CefRefPtr<CefApiVersionTest> obj = CefApiVersionTest::Create();

  const int kTestVal = 12;
  TrackCallback got_delete;

  CefOwnPtr<CefApiVersionTestScopedClient> test_obj(
      new ApiVersionTestScopedClient(kTestVal, &got_delete));
  EXPECT_EQ(kTestVal, GetValue(test_obj));
  EXPECT_EQ(kTestVal, obj->SetOwnPtrClient(std::move(test_obj)));
  EXPECT_FALSE(test_obj.get());
  EXPECT_TRUE(got_delete);

  got_delete.reset();
  test_obj =
      std::make_unique<ApiVersionTestScopedClient>(kTestVal, &got_delete);
  CefOwnPtr<CefApiVersionTestScopedClient> handler =
      obj->SetOwnPtrClientAndReturn(std::move(test_obj));
  EXPECT_FALSE(test_obj.get());
  EXPECT_TRUE(handler.get());
  EXPECT_FALSE(got_delete);
  EXPECT_EQ(kTestVal, GetValue(handler));
  handler.reset(nullptr);
  EXPECT_TRUE(got_delete);

  // Only one reference to the object should exist.
  EXPECT_TRUE(obj->HasOneRef());
}

// Test getting/setting inherited client-side OwnPtr types.
TEST(ApiVersionTest, OwnPtrClientInherit) {
  CefRefPtr<CefApiVersionTest> obj = CefApiVersionTest::Create();

  const int kTestVal = 12;
  const int kTestVal2 = 86;
  TrackCallback got_delete;

  CefOwnPtr<ApiVersionTestScopedClientChildType> test_obj(
      new ApiVersionTestScopedClientChildType(kTestVal, kTestVal2,
                                              &got_delete));
  EXPECT_EQ(kTestVal, GetValue(test_obj));
  EXPECT_EQ(kTestVal2, GetOtherValue(test_obj));
  EXPECT_EQ(kTestVal, obj->SetOwnPtrClient(std::move(test_obj)));
  EXPECT_FALSE(test_obj.get());
  EXPECT_TRUE(got_delete);

  got_delete.reset();
  test_obj = std::make_unique<ApiVersionTestScopedClientChildType>(
      kTestVal, kTestVal2, &got_delete);
  EXPECT_EQ(kTestVal, obj->SetChildOwnPtrClient(std::move(test_obj)));
  EXPECT_FALSE(test_obj.get());
  EXPECT_TRUE(got_delete);

  got_delete.reset();
  test_obj = std::make_unique<ApiVersionTestScopedClientChildType>(
      kTestVal, kTestVal2, &got_delete);
  CefOwnPtr<CefApiVersionTestScopedClient> handler(
      obj->SetChildOwnPtrClientAndReturnParent(std::move(test_obj)));
  EXPECT_EQ(kTestVal, GetValue(handler));
  EXPECT_FALSE(test_obj.get());
  EXPECT_FALSE(got_delete);
  handler.reset(nullptr);
  EXPECT_TRUE(got_delete);

  // Only one reference to the object should exist.
  EXPECT_TRUE(obj->HasOneRef());
}

// Test getting/setting library-side RawPtr types.
TEST(ApiVersionTest, RawPtrLibrary) {
  CefRefPtr<CefApiVersionTest> obj = CefApiVersionTest::Create();

  const int kTestVal = 12;
  auto test_obj = CreateScopedLibrary(kTestVal);
  EXPECT_EQ(kTestVal, GetValue(test_obj));
  int retval = obj->SetRawPtrLibrary(test_obj.get());
  EXPECT_EQ(kTestVal, retval);
  EXPECT_EQ(kTestVal, GetValue(test_obj));

  const int kTestVal2 = 30;
  auto test_obj2 = obj->GetOwnPtrLibrary(kTestVal2);
  EXPECT_EQ(kTestVal2, GetValue(test_obj2));
  int retval2 = obj->SetRawPtrLibrary(test_obj2.get());
  EXPECT_EQ(kTestVal2, retval2);
  EXPECT_EQ(kTestVal2, GetValue(test_obj2));

  // Only one reference to the object should exist.
  EXPECT_TRUE(obj->HasOneRef());
}

// Test getting/setting inherited library-side RawPtr types.
TEST(ApiVersionTest, RawPtrLibraryInherit) {
  CefRefPtr<CefApiVersionTest> obj = CefApiVersionTest::Create();

  const int kTestVal = 12;
  const int kTestVal2 = 40;
  auto test_obj = CreateScopedLibraryChild(kTestVal, kTestVal2);
  EXPECT_EQ(kTestVal, GetValue(test_obj));
  EXPECT_EQ(kTestVal2, test_obj->GetOtherValue());
  int retval = obj->SetRawPtrLibrary(test_obj.get());
  EXPECT_EQ(kTestVal, retval);
  EXPECT_EQ(kTestVal, GetValue(test_obj));
  EXPECT_EQ(kTestVal2, test_obj->GetOtherValue());

  EXPECT_EQ(kTestVal, obj->SetChildRawPtrLibrary(test_obj.get()));

  const int kTestVal3 = 100;
  auto test_obj2 =
      CreateScopedLibraryChildChild(kTestVal, kTestVal2, kTestVal3);
  EXPECT_EQ(kTestVal, GetValue(test_obj2));
  EXPECT_EQ(kTestVal2, test_obj2->GetOtherValue());
  EXPECT_EQ(kTestVal3, test_obj2->GetOtherOtherValue());
  int retval2 = obj->SetRawPtrLibrary(test_obj2.get());
  EXPECT_EQ(kTestVal, retval2);
  EXPECT_EQ(kTestVal, GetValue(test_obj2));
  EXPECT_EQ(kTestVal2, test_obj2->GetOtherValue());
  EXPECT_EQ(kTestVal3, test_obj2->GetOtherOtherValue());

  EXPECT_EQ(kTestVal, obj->SetChildRawPtrLibrary(test_obj2.get()));

  // Only one reference to the object should exist.
  EXPECT_TRUE(obj->HasOneRef());
}

// Test getting/setting library-side RawPtr list types.
TEST(ApiVersionTest, RawPtrLibraryList) {
  CefRefPtr<CefApiVersionTest> obj = CefApiVersionTest::Create();

  const int kVal1 = 34;
  const int kVal2 = 10;

  auto val1 = CreateScopedLibrary(kVal1);
  auto val2 = CreateScopedLibraryChild(kVal2, 0);

  std::vector<CefRawPtr<CefApiVersionTestScopedLibrary>> list;
  list.push_back(val1.get());
  list.push_back(val2.get());
  EXPECT_TRUE(obj->SetRawPtrLibraryList(list, kVal1, kVal2));
  list.clear();

  // Only one reference to the object should exist.
  EXPECT_TRUE(obj->HasOneRef());
}

// Test getting/setting client-side RawPtr types.
TEST(ApiVersionTest, RawPtrClient) {
  CefRefPtr<CefApiVersionTest> obj = CefApiVersionTest::Create();

  const int kTestVal = 12;
  TrackCallback got_delete;

  CefOwnPtr<ApiVersionTestScopedClient> test_obj(
      new ApiVersionTestScopedClient(kTestVal, &got_delete));
  EXPECT_EQ(kTestVal, GetValue(test_obj));
  EXPECT_EQ(kTestVal, obj->SetRawPtrClient(test_obj.get()));
  EXPECT_FALSE(got_delete);
  test_obj.reset(nullptr);
  EXPECT_TRUE(got_delete);

  // Only one reference to the object should exist.
  EXPECT_TRUE(obj->HasOneRef());
}

// Test getting/setting inherited client-side RawPtr types.
TEST(ApiVersionTest, RawPtrClientInherit) {
  CefRefPtr<CefApiVersionTest> obj = CefApiVersionTest::Create();

  const int kTestVal = 12;
  const int kTestVal2 = 86;
  TrackCallback got_delete;

  CefOwnPtr<ApiVersionTestScopedClientChildType> test_obj(
      new ApiVersionTestScopedClientChildType(kTestVal, kTestVal2,
                                              &got_delete));
  EXPECT_EQ(kTestVal, GetValue(test_obj));
  EXPECT_EQ(kTestVal2, GetOtherValue(test_obj));
  int retval = obj->SetRawPtrClient(test_obj.get());
  EXPECT_EQ(kTestVal, retval);
  EXPECT_EQ(kTestVal, GetValue(test_obj));
  EXPECT_EQ(kTestVal2, GetOtherValue(test_obj));
  EXPECT_FALSE(got_delete);

  EXPECT_EQ(kTestVal, obj->SetChildRawPtrClient(test_obj.get()));
  EXPECT_FALSE(got_delete);
  test_obj.reset(nullptr);
  EXPECT_TRUE(got_delete);

  // Only one reference to the object should exist.
  EXPECT_TRUE(obj->HasOneRef());
}

// Test getting/setting client-side RawPtr list types.
TEST(ApiVersionTest, RawPtrClientList) {
  CefRefPtr<CefApiVersionTest> obj = CefApiVersionTest::Create();

  const int kVal1 = 34;
  const int kVal2 = 10;
  TrackCallback got_delete1, got_delete2;

  CefOwnPtr<CefApiVersionTestScopedClient> val1(
      new ApiVersionTestScopedClient(kVal1, &got_delete1));
  CefOwnPtr<CefApiVersionTestScopedClient> val2(
      new ApiVersionTestScopedClientChildType(kVal2, 0, &got_delete2));

  std::vector<CefRawPtr<CefApiVersionTestScopedClient>> list;
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

namespace {

// Example of the same struct at different versions.
struct test_struct_v1_t {
  size_t size;
  int val1;
};

struct test_struct_v2_t {
  size_t size;
  int val1;
  cef_string_t val2;
};

// Example of a simple struct wrapper without traits.
using TestClassV1 = CefStructBaseSimple<test_struct_v1_t>;

// Same example with traits.
struct TestClassV1Traits {
  using struct_type = test_struct_v1_t;

  static inline void init(struct_type* s) { s->size = sizeof(struct_type); }

  static inline void clear(struct_type* s) {}

  static inline void set(const struct_type* src,
                         struct_type* target,
                         bool copy) {
    target->val1 = src->val1;
  }
};

using TestClassV1Ex = CefStructBase<TestClassV1Traits>;

// Structs containing strings require traits.
struct TestClassV2Traits {
  using struct_type = test_struct_v2_t;

  static inline void init(struct_type* s) { s->size = sizeof(struct_type); }

  static inline void clear(struct_type* s) { cef_string_clear(&s->val2); }

  static inline void set(const struct_type* src,
                         struct_type* target,
                         bool copy) {
    target->val1 = src->val1;

    // Need to check that the newer member exists before accessing.
    if (CEF_MEMBER_EXISTS(src, val2)) {
      cef_string_set(src->val2.str, src->val2.length, &target->val2, copy);
    }
  }
};

using TestClassV2 = CefStructBase<TestClassV2Traits>;

}  // namespace

// Test usage of struct and wrapper at the same version.
TEST(ApiVersionTest, StructVersionSame) {
  TestClassV1 classv1;
  EXPECT_EQ(classv1.size, sizeof(test_struct_v1_t));
  EXPECT_EQ(classv1.val1, 0);

  test_struct_v1_t structv1{sizeof(test_struct_v1_t), 10};
  EXPECT_EQ(structv1.size, sizeof(test_struct_v1_t));
  EXPECT_EQ(structv1.val1, 10);

  classv1 = structv1;
  EXPECT_EQ(classv1.size, sizeof(test_struct_v1_t));
  EXPECT_EQ(classv1.val1, 10);

  TestClassV2 classv2;
  EXPECT_EQ(classv2.size, sizeof(test_struct_v2_t));
  EXPECT_EQ(classv2.val1, 0);
  EXPECT_EQ(classv2.val2.length, 0U);

  const std::string testStr = "Test";

  test_struct_v2_t structv2{sizeof(test_struct_v2_t), 10};
  EXPECT_EQ(structv2.size, sizeof(test_struct_v2_t));
  EXPECT_EQ(structv2.val1, 10);
  EXPECT_EQ(structv2.val2.length, 0U);

  CefString(&(structv2.val2)) = testStr;
  const auto* testStrPtr = structv2.val2.str;

  classv2.AttachTo(structv2);

  // Both |classv2| and |structv2| reference the same thing.
  EXPECT_EQ(classv2.size, sizeof(test_struct_v2_t));
  EXPECT_EQ(classv2.val1, 10);
  EXPECT_EQ(classv2.val2.str, testStrPtr);
  EXPECT_EQ(structv2.size, sizeof(test_struct_v2_t));
  EXPECT_EQ(structv2.val1, 10);
  EXPECT_EQ(structv2.val2.str, testStrPtr);

  classv2.DetachTo(structv2);

  // Now only |structv2| references it.
  EXPECT_EQ(classv2.size, sizeof(test_struct_v2_t));
  EXPECT_EQ(classv2.val1, 0);
  EXPECT_EQ(classv2.val2.length, 0U);
  EXPECT_EQ(structv2.size, sizeof(test_struct_v2_t));
  EXPECT_EQ(structv2.val1, 10);
  EXPECT_EQ(structv2.val2.str, testStrPtr);

  classv2 = structv2;

  // Now |classv2| has a copy of the string.
  EXPECT_EQ(classv2.size, sizeof(test_struct_v2_t));
  EXPECT_EQ(classv2.val1, 10);
  EXPECT_GT(classv2.val2.length, 0U);
  EXPECT_NE(classv2.val2.str, testStrPtr);
  EXPECT_STREQ(testStr.c_str(), CefString(&(classv2.val2)).ToString().c_str());
  EXPECT_EQ(structv2.size, sizeof(test_struct_v2_t));
  EXPECT_EQ(structv2.val1, 10);
  EXPECT_EQ(structv2.val2.str, testStrPtr);

  // Cleanup the struct.
  cef_string_clear(&(structv2.val2));
}

// Test usage of older wrapper with newer struct.
TEST(ApiVersionTest, StructVersionNewer) {
  // V1 starts at V1 size.
  TestClassV1 classv1;
  EXPECT_EQ(classv1.size, sizeof(test_struct_v1_t));
  EXPECT_EQ(classv1.val1, 0);

  // V2 starts at V2 size.
  test_struct_v2_t structv2{sizeof(test_struct_v2_t), 10};
  EXPECT_EQ(structv2.size, sizeof(test_struct_v2_t));
  EXPECT_EQ(structv2.val1, 10);
  EXPECT_EQ(structv2.val2.length, 0U);

  const std::string testStr = "Test";
  CefString(&(structv2.val2)) = testStr;

  classv1 = reinterpret_cast<test_struct_v1_t&>(structv2);

  // Now |classv1| has the same value (up to V1 size).
  EXPECT_EQ(classv1.size, sizeof(test_struct_v1_t));
  EXPECT_EQ(classv1.val1, 10);

  // Cleanup the struct.
  cef_string_clear(&(structv2.val2));
}

// Same as above, but with traits.
TEST(ApiVersionTest, StructVersionNewerEx) {
  // V1 starts at V1 size.
  TestClassV1Ex classv1;
  EXPECT_EQ(classv1.size, sizeof(test_struct_v1_t));
  EXPECT_EQ(classv1.val1, 0);

  // V2 starts at V2 size.
  test_struct_v2_t structv2{sizeof(test_struct_v2_t), 10};
  EXPECT_EQ(structv2.size, sizeof(test_struct_v2_t));
  EXPECT_EQ(structv2.val1, 10);
  EXPECT_EQ(structv2.val2.length, 0U);

  const std::string testStr = "Test";
  CefString(&(structv2.val2)) = testStr;
  const auto* testStrPtr = structv2.val2.str;

  classv1.AttachTo(reinterpret_cast<test_struct_v1_t&>(structv2));

  // Both |classv1| and |structv2| now reference the same thing (up to V1 size).
  EXPECT_EQ(classv1.size, sizeof(test_struct_v1_t));
  EXPECT_EQ(classv1.val1, 10);
  EXPECT_EQ(structv2.size, sizeof(test_struct_v2_t));
  EXPECT_EQ(structv2.val1, 10);
  EXPECT_EQ(structv2.val2.str, testStrPtr);

  classv1.DetachTo(reinterpret_cast<test_struct_v1_t&>(structv2));

  // Now only |structv1| references it (up to V1 size), and the rest is left
  // alone.
  EXPECT_EQ(classv1.size, sizeof(test_struct_v1_t));
  EXPECT_EQ(classv1.val1, 0);
  EXPECT_EQ(structv2.size, sizeof(test_struct_v1_t));
  EXPECT_EQ(structv2.val1, 10);
  EXPECT_EQ(structv2.val2.str, testStrPtr);

  classv1 = reinterpret_cast<test_struct_v1_t&>(structv2);

  // Now |classv1| has the same value (up to V1 size).
  EXPECT_EQ(classv1.size, sizeof(test_struct_v1_t));
  EXPECT_EQ(classv1.val1, 10);

  // Cleanup the struct.
  cef_string_clear(&(structv2.val2));
}

// Test usage of newer wrapper with older struct.
TEST(ApiVersionTest, StructVersionOlder) {
  // V1 starts at V1 size.
  test_struct_v1_t structv1{sizeof(test_struct_v1_t), 10};
  EXPECT_EQ(structv1.size, sizeof(test_struct_v1_t));
  EXPECT_EQ(structv1.val1, 10);

  // V2 starts at V2 size.
  TestClassV2 classv2;
  EXPECT_EQ(classv2.size, sizeof(test_struct_v2_t));
  EXPECT_EQ(classv2.val1, 0);
  EXPECT_EQ(classv2.val2.length, 0U);

  const std::string testStr = "Test";
  CefString(&(classv2.val2)) = testStr;

  classv2.AttachTo(reinterpret_cast<test_struct_v2_t&>(structv1));

  // Both |classv2| and |structv1| now reference the same thing (up to V1 size),
  // and the rest is cleared.
  EXPECT_EQ(classv2.size, sizeof(test_struct_v1_t));
  EXPECT_EQ(classv2.val1, 10);
  EXPECT_EQ(classv2.val2.length, 0U);
  EXPECT_EQ(structv1.size, sizeof(test_struct_v1_t));
  EXPECT_EQ(structv1.val1, 10);

  classv2.DetachTo(reinterpret_cast<test_struct_v2_t&>(structv1));

  // Now only |structv1| references it (up to V1 size). Note that |classv2| is
  // back to V2 size.
  EXPECT_EQ(classv2.size, sizeof(test_struct_v2_t));
  EXPECT_EQ(classv2.val1, 0);
  EXPECT_EQ(classv2.val2.length, 0U);
  EXPECT_EQ(structv1.size, sizeof(test_struct_v1_t));
  EXPECT_EQ(structv1.val1, 10);

  CefString(&(classv2.val2)) = testStr;

  classv2 = reinterpret_cast<test_struct_v2_t&>(structv1);

  // Now |classv1| has the same value (up to V1 size), and the rest is cleared.
  EXPECT_EQ(classv2.size, sizeof(test_struct_v2_t));
  EXPECT_EQ(classv2.val1, 10);
  EXPECT_EQ(classv2.val2.length, 0U);
}
