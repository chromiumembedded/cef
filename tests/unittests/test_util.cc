// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.


#include "tests/unittests/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"


void TestBinaryEqual(CefRefPtr<CefBinaryValue> val1,
                     CefRefPtr<CefBinaryValue> val2) {
  EXPECT_TRUE(val1.get());
  EXPECT_TRUE(val2.get());

  size_t data_size = val1->GetSize();
  EXPECT_EQ(data_size, val2->GetSize());

  EXPECT_GT(data_size, (size_t)0);

  char* data1 = new char[data_size+1];
  char* data2 = new char[data_size+1];

  EXPECT_EQ(data_size, val1->GetData(data1, data_size, 0));
  data1[data_size] = 0;
  EXPECT_EQ(data_size, val2->GetData(data2, data_size, 0));
  data2[data_size] = 0;

  EXPECT_STREQ(data1, data2);

  delete [] data1;
  delete [] data2;
}

void TestDictionaryEqual(CefRefPtr<CefDictionaryValue> val1,
                         CefRefPtr<CefDictionaryValue> val2) {
  EXPECT_TRUE(val1.get());
  EXPECT_TRUE(val2.get());

  EXPECT_EQ(val1->GetSize(), val2->GetSize());

  CefDictionaryValue::KeyList keys;
  EXPECT_TRUE(val1->GetKeys(keys));

  CefDictionaryValue::KeyList::const_iterator it = keys.begin();
  for (; it != keys.end(); ++it) {
    CefString key = *it;
    EXPECT_TRUE(val2->HasKey(key));
    CefValueType type = val1->GetType(key);
    EXPECT_EQ(type, val2->GetType(key));
    switch (type) {
      case VTYPE_INVALID:
      case VTYPE_NULL:
        break;
      case VTYPE_BOOL:
        EXPECT_EQ(val1->GetBool(key), val2->GetBool(key));
        break;
      case VTYPE_INT:
        EXPECT_EQ(val1->GetInt(key), val2->GetInt(key));
        break;
      case VTYPE_DOUBLE:
        EXPECT_EQ(val1->GetDouble(key), val2->GetDouble(key));
        break;
      case VTYPE_STRING:
        EXPECT_EQ(val1->GetString(key), val2->GetString(key));
        break;
      case VTYPE_BINARY:
        TestBinaryEqual(val1->GetBinary(key), val2->GetBinary(key));
        break;
      case VTYPE_DICTIONARY:
        TestDictionaryEqual(val1->GetDictionary(key), val2->GetDictionary(key));
        break;
      case VTYPE_LIST:
        TestListEqual(val1->GetList(key), val2->GetList(key));
        break;
    }
  }
}

void TestListEqual(CefRefPtr<CefListValue> val1,
                   CefRefPtr<CefListValue> val2) {
  EXPECT_TRUE(val1.get());
  EXPECT_TRUE(val2.get());

  size_t size = val1->GetSize();
  EXPECT_EQ(size, val2->GetSize());

  for (size_t i = 0; i < size; ++i) {
    CefValueType type = val1->GetType(i);
    EXPECT_EQ(type, val2->GetType(i));
    switch (type) {
      case VTYPE_INVALID:
      case VTYPE_NULL:
        break;
      case VTYPE_BOOL:
        EXPECT_EQ(val1->GetBool(i), val2->GetBool(i));
        break;
      case VTYPE_INT:
        EXPECT_EQ(val1->GetInt(i), val2->GetInt(i));
        break;
      case VTYPE_DOUBLE:
        EXPECT_EQ(val1->GetDouble(i), val2->GetDouble(i));
        break;
      case VTYPE_STRING:
        EXPECT_EQ(val1->GetString(i), val2->GetString(i));
        break;
      case VTYPE_BINARY:
        TestBinaryEqual(val1->GetBinary(i), val2->GetBinary(i));
        break;
      case VTYPE_DICTIONARY:
        TestDictionaryEqual(val1->GetDictionary(i), val2->GetDictionary(i));
        break;
      case VTYPE_LIST:
        TestListEqual(val1->GetList(i), val2->GetList(i));
        break;
    }
  }
}

void TestProcessMessageEqual(CefRefPtr<CefProcessMessage> val1,
                             CefRefPtr<CefProcessMessage> val2) {
  EXPECT_TRUE(val1.get());
  EXPECT_TRUE(val2.get());
  EXPECT_EQ(val1->GetName(), val2->GetName());

  TestListEqual(val1->GetArgumentList(), val2->GetArgumentList());
}
