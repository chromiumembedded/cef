// Copyright (c) 2010 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "include/internal/cef_string.h"
#include "include/internal/cef_string_list.h"
#include "include/internal/cef_string_map.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <map>
#include <vector>

// Test UTF8 strings.
TEST(StringTest, UTF8)
{
  CefStringUTF8 str1("Test String");
  ASSERT_EQ(str1.length(), (size_t)11);
  ASSERT_FALSE(str1.empty());
  ASSERT_TRUE(str1.IsOwner());

  // Test equality.
  CefStringUTF8 str2("Test String");
  ASSERT_EQ(str1, str2);
  ASSERT_LE(str1, str2);
  ASSERT_GE(str1, str2);

  str2 = "Test Test";
  ASSERT_LT(str1, str2);
  ASSERT_GT(str2, str1);

  // When strings are the same but of unequal length, the longer string is
  // greater.
  str2 = "Test";
  ASSERT_LT(str2, str1);
  ASSERT_GT(str1, str2);

  // Test conversions.
  str2 = str1.ToString();
  ASSERT_EQ(str1, str2);
  str2 = str1.ToWString();
  ASSERT_EQ(str1, str2);

  // Test userfree assignment.
  cef_string_userfree_utf8_t uf = str2.DetachToUserFree();
  ASSERT_TRUE(uf != NULL);
  ASSERT_TRUE(str2.empty());
  str2.AttachToUserFree(uf);
  ASSERT_FALSE(str2.empty());
  ASSERT_EQ(str1, str2);
}

// Test UTF16 strings.
TEST(StringTest, UTF16)
{
  CefStringUTF16 str1("Test String");
  ASSERT_EQ(str1.length(), (size_t)11);
  ASSERT_FALSE(str1.empty());
  ASSERT_TRUE(str1.IsOwner());

  // Test equality.
  CefStringUTF16 str2("Test String");
  ASSERT_EQ(str1, str2);
  ASSERT_LE(str1, str2);
  ASSERT_GE(str1, str2);

  str2 = "Test Test";
  ASSERT_LT(str1, str2);
  ASSERT_GT(str2, str1);

  // When strings are the same but of unequal length, the longer string is
  // greater.
  str2 = "Test";
  ASSERT_LT(str2, str1);
  ASSERT_GT(str1, str2);

  // Test conversions.
  str2 = str1.ToString();
  ASSERT_EQ(str1, str2);
  str2 = str1.ToWString();
  ASSERT_EQ(str1, str2);

  // Test userfree assignment.
  cef_string_userfree_utf16_t uf = str2.DetachToUserFree();
  ASSERT_TRUE(uf != NULL);
  ASSERT_TRUE(str2.empty());
  str2.AttachToUserFree(uf);
  ASSERT_FALSE(str2.empty());
  ASSERT_EQ(str1, str2);
}

// Test wide strings.
TEST(StringTest, Wide)
{
  CefStringWide str1("Test String");
  ASSERT_EQ(str1.length(), (size_t)11);
  ASSERT_FALSE(str1.empty());
  ASSERT_TRUE(str1.IsOwner());

  // Test equality.
  CefStringWide str2("Test String");
  ASSERT_EQ(str1, str2);
  ASSERT_LE(str1, str2);
  ASSERT_GE(str1, str2);

  str2 = "Test Test";
  ASSERT_LT(str1, str2);
  ASSERT_GT(str2, str1);

  // When strings are the same but of unequal length, the longer string is
  // greater.
  str2 = "Test";
  ASSERT_LT(str2, str1);
  ASSERT_GT(str1, str2);

  // Test conversions.
  str2 = str1.ToString();
  ASSERT_EQ(str1, str2);
  str2 = str1.ToWString();
  ASSERT_EQ(str1, str2);

  // Test userfree assignment.
  cef_string_userfree_wide_t uf = str2.DetachToUserFree();
  ASSERT_TRUE(uf != NULL);
  ASSERT_TRUE(str2.empty());
  str2.AttachToUserFree(uf);
  ASSERT_FALSE(str2.empty());
  ASSERT_EQ(str1, str2);
}

// Test string lists.
TEST(StringTest, List)
{
  typedef std::vector<CefString> ListType;
  ListType list;
  list.push_back("String 1");
  list.push_back("String 2");
  list.push_back("String 3");

  ASSERT_EQ(list[0], "String 1");
  ASSERT_EQ(list[1], "String 2");
  ASSERT_EQ(list[2], "String 3");

  cef_string_list_t listPtr = cef_string_list_alloc();
  ASSERT_TRUE(listPtr != NULL);
  ListType::const_iterator it = list.begin();
  for(; it != list.end(); ++it)
    cef_string_list_append(listPtr, it->GetStruct());

  CefString str;
  int ret;

  ASSERT_EQ(cef_string_list_size(listPtr), 3);
  
  ret = cef_string_list_value(listPtr, 0, str.GetWritableStruct());
  ASSERT_TRUE(ret);
  ASSERT_EQ(str, "String 1");
  ret = cef_string_list_value(listPtr, 1, str.GetWritableStruct());
  ASSERT_TRUE(ret);
  ASSERT_EQ(str, "String 2");
  ret = cef_string_list_value(listPtr, 2, str.GetWritableStruct());
  ASSERT_TRUE(ret);
  ASSERT_EQ(str, "String 3");

  cef_string_list_t listPtr2 = cef_string_list_copy(listPtr);
  cef_string_list_clear(listPtr);
  ASSERT_EQ(cef_string_list_size(listPtr), 0);
  cef_string_list_free(listPtr);

  ASSERT_EQ(cef_string_list_size(listPtr2), 3);

  ret = cef_string_list_value(listPtr2, 0, str.GetWritableStruct());
  ASSERT_TRUE(ret);
  ASSERT_EQ(str, "String 1");
  ret = cef_string_list_value(listPtr2, 1, str.GetWritableStruct());
  ASSERT_TRUE(ret);
  ASSERT_EQ(str, "String 2");
  ret = cef_string_list_value(listPtr2, 2, str.GetWritableStruct());
  ASSERT_TRUE(ret);
  ASSERT_EQ(str, "String 3");

  cef_string_list_free(listPtr2);
}

// Test string maps.
TEST(StringTest, Map)
{
  typedef std::map<CefString,CefString> MapType;
  MapType map;
  map.insert(std::make_pair("Key 1", "String 1"));
  map.insert(std::make_pair("Key 2", "String 2"));
  map.insert(std::make_pair("Key 3", "String 3"));

  MapType::const_iterator it;
  
  it = map.find("Key 2");
  ASSERT_TRUE(it != map.end());
  ASSERT_EQ(it->first, "Key 2");
  ASSERT_EQ(it->second, "String 2");

  it = map.find(L"Key 2");
  ASSERT_TRUE(it != map.end());
  ASSERT_EQ(it->first, L"Key 2");
  ASSERT_EQ(it->second, L"String 2");

  ASSERT_EQ(map["Key 1"], "String 1");
  ASSERT_EQ(map["Key 2"], "String 2");
  ASSERT_EQ(map["Key 3"], "String 3");

  cef_string_map_t mapPtr = cef_string_map_alloc();
  
  it = map.begin();
  for(; it != map.end(); ++it) {
    cef_string_map_append(mapPtr, it->first.GetStruct(),
        it->second.GetStruct());
  }

  CefString str;
  int ret;

  ASSERT_EQ(cef_string_map_size(mapPtr), 3);
  
  ret = cef_string_map_key(mapPtr, 0, str.GetWritableStruct());
  ASSERT_TRUE(ret);
  ASSERT_EQ(str, "Key 1");
  ret = cef_string_map_value(mapPtr, 0, str.GetWritableStruct());
  ASSERT_TRUE(ret);
  ASSERT_EQ(str, "String 1");

  ret = cef_string_map_key(mapPtr, 1, str.GetWritableStruct());
  ASSERT_TRUE(ret);
  ASSERT_EQ(str, "Key 2");
  ret = cef_string_map_value(mapPtr, 1, str.GetWritableStruct());
  ASSERT_TRUE(ret);
  ASSERT_EQ(str, "String 2");

  ret = cef_string_map_key(mapPtr, 2, str.GetWritableStruct());
  ASSERT_TRUE(ret);
  ASSERT_EQ(str, "Key 3");
  ret = cef_string_map_value(mapPtr, 2, str.GetWritableStruct());
  ASSERT_TRUE(ret);
  ASSERT_EQ(str, "String 3");

  CefString key;
  key.FromASCII("Key 2");
  ret = cef_string_map_find(mapPtr, key.GetStruct(), str.GetWritableStruct());
  ASSERT_TRUE(ret);
  ASSERT_EQ(str, "String 2");

  cef_string_map_clear(mapPtr);
  ASSERT_EQ(cef_string_map_size(mapPtr), 0);

  cef_string_map_free(mapPtr);
}
