// Copyright (c) 2010 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include <map>
#include <string_view>
#include <vector>

#include "include/base/cef_logging.h"
#include "include/internal/cef_string.h"
#include "include/internal/cef_string_list.h"
#include "include/internal/cef_string_map.h"
#include "include/internal/cef_string_multimap.h"
#include "tests/gtest/include/gtest/gtest.h"

// Test UTF8 strings.
TEST(StringTest, UTF8) {
  CefStringUTF8 str1("Test String");
  EXPECT_EQ(str1.length(), (size_t)11);
  EXPECT_FALSE(str1.empty());
  EXPECT_TRUE(str1.IsOwner());

  // Test equality.
  CefStringUTF8 str2("Test String");
  EXPECT_EQ(str1, str2);
  EXPECT_LE(str1, str2);
  EXPECT_GE(str1, str2);

  str2 = "Test Test";
  EXPECT_LT(str1, str2);
  EXPECT_GT(str2, str1);

  // When strings are the same but of unequal length, the longer string is
  // greater.
  str2 = "Test";
  EXPECT_LT(str2, str1);
  EXPECT_GT(str1, str2);

  // Test conversions.
  str2 = str1.ToString();
  EXPECT_EQ(str1, str2);
  str2 = str1.ToWString();
  EXPECT_EQ(str1, str2);

  // Test userfree assignment.
  cef_string_userfree_utf8_t uf = str2.DetachToUserFree();
  EXPECT_TRUE(uf != nullptr);
  EXPECT_TRUE(str2.empty());
  str2.AttachToUserFree(uf);
  EXPECT_FALSE(str2.empty());
  EXPECT_EQ(str1, str2);
}

// Test UTF16 strings.
TEST(StringTest, UTF16) {
  CefStringUTF16 str1("Test String");
  EXPECT_EQ(str1.length(), (size_t)11);
  EXPECT_FALSE(str1.empty());
  EXPECT_TRUE(str1.IsOwner());

  // Test equality.
  CefStringUTF16 str2("Test String");
  EXPECT_EQ(str1, str2);
  EXPECT_LE(str1, str2);
  EXPECT_GE(str1, str2);

  str2 = "Test Test";
  EXPECT_LT(str1, str2);
  EXPECT_GT(str2, str1);

  // When strings are the same but of unequal length, the longer string is
  // greater.
  str2 = "Test";
  EXPECT_LT(str2, str1);
  EXPECT_GT(str1, str2);

  // Test conversions.
  str2 = str1.ToString();
  EXPECT_EQ(str1, str2);
  str2 = str1.ToWString();
  EXPECT_EQ(str1, str2);

  // Test userfree assignment.
  cef_string_userfree_utf16_t uf = str2.DetachToUserFree();
  EXPECT_TRUE(uf != nullptr);
  EXPECT_TRUE(str2.empty());
  str2.AttachToUserFree(uf);
  EXPECT_FALSE(str2.empty());
  EXPECT_EQ(str1, str2);
}

// Test wide strings.
TEST(StringTest, Wide) {
  CefStringWide str1("Test String");
  EXPECT_EQ(str1.length(), (size_t)11);
  EXPECT_FALSE(str1.empty());
  EXPECT_TRUE(str1.IsOwner());

  // Test equality.
  CefStringWide str2("Test String");
  EXPECT_EQ(str1, str2);
  EXPECT_LE(str1, str2);
  EXPECT_GE(str1, str2);

  str2 = "Test Test";
  EXPECT_LT(str1, str2);
  EXPECT_GT(str2, str1);

  // When strings are the same but of unequal length, the longer string is
  // greater.
  str2 = "Test";
  EXPECT_LT(str2, str1);
  EXPECT_GT(str1, str2);

  // Test conversions.
  str2 = str1.ToString();
  EXPECT_EQ(str1, str2);
  str2 = str1.ToWString();
  EXPECT_EQ(str1, str2);

  // Test userfree assignment.
  cef_string_userfree_wide_t uf = str2.DetachToUserFree();
  EXPECT_TRUE(uf != nullptr);
  EXPECT_TRUE(str2.empty());
  str2.AttachToUserFree(uf);
  EXPECT_FALSE(str2.empty());
  EXPECT_EQ(str1, str2);
}

// Test std::u16string convertion to/from CefString types.
TEST(StringTest, string16) {
  CefStringUTF8 str8("Test String 1"), str8b;
  CefStringUTF16 str16("Test String 2"), str16b;
  CefStringWide strwide("Test String 3"), strwideb;
  std::u16string base_str;

  base_str = str8;
  str8b = base_str;
  EXPECT_EQ(str8, base_str);
  EXPECT_EQ(str8, str8b);

  base_str = str16;
  str16b = base_str;
  EXPECT_EQ(str16, base_str);
  EXPECT_EQ(str16, str16b);

  base_str = strwide;
  strwideb = base_str;
  EXPECT_EQ(strwide, base_str);
  EXPECT_EQ(strwide, strwideb);
}

// Test string lists.
TEST(StringTest, List) {
  typedef std::vector<CefString> ListType;
  ListType list;
  list.push_back("String 1");
  list.push_back("String 2");
  list.push_back("String 3");

  EXPECT_EQ(list[0], "String 1");
  EXPECT_EQ(list[1], "String 2");
  EXPECT_EQ(list[2], "String 3");

  cef_string_list_t listPtr = cef_string_list_alloc();
  EXPECT_TRUE(listPtr != nullptr);
  for (const auto& item : list) {
    cef_string_list_append(listPtr, item.GetStruct());
  }

  CefString str;
  int ret;

  EXPECT_EQ(cef_string_list_size(listPtr), 3U);

  ret = cef_string_list_value(listPtr, 0U, str.GetWritableStruct());
  EXPECT_TRUE(ret);
  EXPECT_EQ(str, "String 1");
  ret = cef_string_list_value(listPtr, 1U, str.GetWritableStruct());
  EXPECT_TRUE(ret);
  EXPECT_EQ(str, "String 2");
  ret = cef_string_list_value(listPtr, 2U, str.GetWritableStruct());
  EXPECT_TRUE(ret);
  EXPECT_EQ(str, "String 3");

  cef_string_list_t listPtr2 = cef_string_list_copy(listPtr);
  cef_string_list_clear(listPtr);
  EXPECT_EQ(cef_string_list_size(listPtr), 0U);
  cef_string_list_free(listPtr);

  EXPECT_EQ(cef_string_list_size(listPtr2), 3U);

  ret = cef_string_list_value(listPtr2, 0U, str.GetWritableStruct());
  EXPECT_TRUE(ret);
  EXPECT_EQ(str, "String 1");
  ret = cef_string_list_value(listPtr2, 1U, str.GetWritableStruct());
  EXPECT_TRUE(ret);
  EXPECT_EQ(str, "String 2");
  ret = cef_string_list_value(listPtr2, 2U, str.GetWritableStruct());
  EXPECT_TRUE(ret);
  EXPECT_EQ(str, "String 3");

  cef_string_list_free(listPtr2);
}

// Test string maps.
TEST(StringTest, Map) {
  typedef std::map<CefString, CefString> MapType;
  MapType map;
  map.insert(std::make_pair("Key 1", "String 1"));
  map.insert(std::make_pair("Key 2", "String 2"));
  map.insert(std::make_pair("Key 3", "String 3"));

  MapType::const_iterator it;

  it = map.find("Key 2");
  EXPECT_TRUE(it != map.end());
  EXPECT_EQ(it->first, "Key 2");
  EXPECT_EQ(it->second, "String 2");

  it = map.find(L"Key 2");
  EXPECT_TRUE(it != map.end());
  EXPECT_EQ(it->first, L"Key 2");
  EXPECT_EQ(it->second, L"String 2");

  EXPECT_EQ(map["Key 1"], "String 1");
  EXPECT_EQ(map["Key 2"], "String 2");
  EXPECT_EQ(map["Key 3"], "String 3");

  cef_string_map_t mapPtr = cef_string_map_alloc();

  for (const auto& [key, value] : map) {
    cef_string_map_append(mapPtr, key.GetStruct(), value.GetStruct());
  }

  CefString str;
  int ret;

  EXPECT_EQ(cef_string_map_size(mapPtr), 3U);

  ret = cef_string_map_key(mapPtr, 0U, str.GetWritableStruct());
  EXPECT_TRUE(ret);
  EXPECT_EQ(str, "Key 1");
  ret = cef_string_map_value(mapPtr, 0U, str.GetWritableStruct());
  EXPECT_TRUE(ret);
  EXPECT_EQ(str, "String 1");

  ret = cef_string_map_key(mapPtr, 1U, str.GetWritableStruct());
  EXPECT_TRUE(ret);
  EXPECT_EQ(str, "Key 2");
  ret = cef_string_map_value(mapPtr, 1U, str.GetWritableStruct());
  EXPECT_TRUE(ret);
  EXPECT_EQ(str, "String 2");

  ret = cef_string_map_key(mapPtr, 2U, str.GetWritableStruct());
  EXPECT_TRUE(ret);
  EXPECT_EQ(str, "Key 3");
  ret = cef_string_map_value(mapPtr, 2U, str.GetWritableStruct());
  EXPECT_TRUE(ret);
  EXPECT_EQ(str, "String 3");

  CefString key;
  key.FromASCII("Key 2");
  ret = cef_string_map_find(mapPtr, key.GetStruct(), str.GetWritableStruct());
  EXPECT_TRUE(ret);
  EXPECT_EQ(str, "String 2");

  cef_string_map_clear(mapPtr);
  EXPECT_EQ(cef_string_map_size(mapPtr), 0U);

  cef_string_map_free(mapPtr);
}

// Test string maps.
TEST(StringTest, Multimap) {
  typedef std::multimap<CefString, CefString> MapType;
  MapType map;
  map.insert(std::make_pair("Key 1", "String 1"));
  map.insert(std::make_pair("Key 2", "String 2"));
  map.insert(std::make_pair("Key 2", "String 2.1"));
  map.insert(std::make_pair("Key 3", "String 3"));

  MapType::const_iterator it;

  it = map.find("Key 2");
  EXPECT_TRUE(it != map.end());
  EXPECT_EQ(it->first, "Key 2");
  EXPECT_EQ(it->second, "String 2");

  std::pair<MapType::const_iterator, MapType::const_iterator> range_it =
      map.equal_range("Key 2");
  EXPECT_TRUE(range_it.first != range_it.second);
  MapType::const_iterator same_key_it = range_it.first;
  // Either of "String 2" or "String 2.1" is fine since
  // std::multimap provides no guarantee wrt the order of
  // values with the same key.
  EXPECT_EQ(same_key_it->second.ToString().find("String 2"), 0U);
  EXPECT_EQ((++same_key_it)->second.ToString().find("String 2"), 0U);
  EXPECT_EQ(map.count("Key 2"), 2U);

  EXPECT_EQ(map.find("Key 1")->second, "String 1");
  EXPECT_EQ(map.find("Key 3")->second, "String 3");

  cef_string_multimap_t mapPtr = cef_string_multimap_alloc();

  for (const auto& [key, value] : map) {
    cef_string_multimap_append(mapPtr, key.GetStruct(), value.GetStruct());
  }

  CefString str;
  int ret;

  EXPECT_EQ(cef_string_multimap_size(mapPtr), 4U);

  ret = cef_string_multimap_key(mapPtr, 0U, str.GetWritableStruct());
  EXPECT_TRUE(ret);
  EXPECT_EQ(str, "Key 1");
  ret = cef_string_multimap_value(mapPtr, 0U, str.GetWritableStruct());
  EXPECT_TRUE(ret);
  EXPECT_EQ(str, "String 1");

  ret = cef_string_multimap_key(mapPtr, 1U, str.GetWritableStruct());
  EXPECT_TRUE(ret);
  EXPECT_EQ(str, "Key 2");
  ret = cef_string_multimap_value(mapPtr, 1U, str.GetWritableStruct());
  EXPECT_TRUE(ret);
  EXPECT_EQ(str.ToString().find("String 2"), 0U);

  ret = cef_string_multimap_key(mapPtr, 2U, str.GetWritableStruct());
  EXPECT_TRUE(ret);
  EXPECT_EQ(str, "Key 2");
  ret = cef_string_multimap_value(mapPtr, 2U, str.GetWritableStruct());
  EXPECT_TRUE(ret);
  EXPECT_EQ(str.ToString().find("String 2"), 0U);

  ret = cef_string_multimap_key(mapPtr, 3U, str.GetWritableStruct());
  EXPECT_TRUE(ret);
  EXPECT_EQ(str, "Key 3");
  ret = cef_string_multimap_value(mapPtr, 3U, str.GetWritableStruct());
  EXPECT_TRUE(ret);
  EXPECT_EQ(str, "String 3");

  CefString key;
  key.FromASCII("Key 2");
  size_t size = cef_string_multimap_find_count(mapPtr, key.GetStruct());
  EXPECT_EQ(size, 2U);

  ret = cef_string_multimap_enumerate(mapPtr, key.GetStruct(), 0U,
                                      str.GetWritableStruct());
  EXPECT_TRUE(ret);
  EXPECT_EQ(str.ToString().find("String 2"), 0U);

  ret = cef_string_multimap_enumerate(mapPtr, key.GetStruct(), 1U,
                                      str.GetWritableStruct());
  EXPECT_TRUE(ret);
  EXPECT_EQ(str.ToString().find("String 2"), 0U);

  cef_string_multimap_clear(mapPtr);
  EXPECT_EQ(cef_string_multimap_size(mapPtr), 0U);

  cef_string_multimap_free(mapPtr);
}

// Test that CefString ownership behaves as expected.
TEST(StringTest, Ownership) {
  const char* test_cstr = "Test string";

  // Initial owner makes a copy of |test_cstr|.
  CefStringUTF8 str(test_cstr);
  EXPECT_TRUE(str.IsOwner());
  EXPECT_STREQ(test_cstr, str.c_str());
  const char* str_str = str.c_str();
  const auto str_struct = str.GetStruct();
  EXPECT_NE(test_cstr, str_str);

  // REFERENCE CREATION

  // Take a reference (requires explicit use of GetStruct).
  CefStringUTF8 str_ref(str.GetStruct());

  // Nothing changes with |str|.
  EXPECT_TRUE(str.IsOwner());
  EXPECT_STREQ(test_cstr, str.c_str());
  EXPECT_EQ(str.GetStruct(), str_struct);
  EXPECT_EQ(str.c_str(), str_str);

  // |str_ref| has the same value.
  EXPECT_FALSE(str_ref.IsOwner());
  EXPECT_STREQ(test_cstr, str_ref.c_str());
  // Referencing the same structure and string.
  EXPECT_EQ(str.GetStruct(), str_ref.GetStruct());
  EXPECT_EQ(str.c_str(), str_ref.c_str());

  // REFERENCE DETACH/ATTACH

  // DetachToUserFree. |str_ref| loses its reference.
  auto userfree = str_ref.DetachToUserFree();

  // Nothing changes with |str|.
  EXPECT_TRUE(str.IsOwner());
  EXPECT_STREQ(test_cstr, str.c_str());
  EXPECT_EQ(str.GetStruct(), str_struct);
  EXPECT_EQ(str.c_str(), str_str);

  // |str_ref| is now empty.
  EXPECT_FALSE(str_ref.IsOwner());
  EXPECT_TRUE(str_ref.empty());
  EXPECT_EQ(nullptr, str_ref.GetStruct());

  // AttachToUserFree. |str2| becomes an owner of the copy.
  CefStringUTF8 str2;
  str2.AttachToUserFree(userfree);

  // Nothing changes with |str|.
  EXPECT_TRUE(str.IsOwner());
  EXPECT_STREQ(test_cstr, str.c_str());
  EXPECT_EQ(str.GetStruct(), str_struct);
  EXPECT_EQ(str.c_str(), str_str);

  // |str2| is now an owner of a copy.
  EXPECT_TRUE(str2.IsOwner());
  EXPECT_STREQ(test_cstr, str2.c_str());
  // Not referencing the same structure or string.
  EXPECT_NE(str.GetStruct(), str2.GetStruct());
  EXPECT_NE(str.c_str(), str2.c_str());

  // |str_ref| is still empty.
  EXPECT_FALSE(str_ref.IsOwner());
  EXPECT_TRUE(str_ref.empty());
  EXPECT_EQ(nullptr, str_ref.GetStruct());

  // OWNER COPY CREATION

  // Making a copy (default copy constructor behavior).
  CefStringUTF8 str3(str);

  // Nothing changes with |str|.
  EXPECT_TRUE(str.IsOwner());
  EXPECT_STREQ(test_cstr, str.c_str());
  EXPECT_EQ(str.GetStruct(), str_struct);
  EXPECT_EQ(str.c_str(), str_str);

  // |str3| is now an owner of a copy.
  EXPECT_TRUE(str3.IsOwner());
  EXPECT_STREQ(test_cstr, str3.c_str());
  // Not referencing the same structure or string.
  EXPECT_NE(str.GetStruct(), str3.GetStruct());
  EXPECT_NE(str.c_str(), str3.c_str());

  // OWNER DETACH/ATTACH

  // DetachToUserFree. |str3| loses its ownership.
  const char* str3_str = str3.c_str();
  auto userfree3 = str3.DetachToUserFree();

  // Nothing changes with |str|.
  EXPECT_TRUE(str.IsOwner());
  EXPECT_STREQ(test_cstr, str.c_str());
  EXPECT_EQ(str.GetStruct(), str_struct);
  EXPECT_EQ(str.c_str(), str_str);

  // |str3| is now empty.
  EXPECT_FALSE(str3.IsOwner());
  EXPECT_TRUE(str3.empty());
  EXPECT_EQ(nullptr, str3.GetStruct());

  // AttachToUserFree. |str3| regains its ownership.
  str3.AttachToUserFree(userfree3);

  // Nothing changes with |str|.
  EXPECT_TRUE(str.IsOwner());
  EXPECT_STREQ(test_cstr, str.c_str());
  EXPECT_EQ(str.GetStruct(), str_struct);
  EXPECT_EQ(str.c_str(), str_str);

  // |str3| is now an owner of the same string that it had previously.
  // The structure will also be re-allocated, but we don't test that because
  // the same address might be reused.
  EXPECT_TRUE(str3.IsOwner());
  EXPECT_STREQ(test_cstr, str3.c_str());
  EXPECT_EQ(str3_str, str3.c_str());
}

// Test UTF16ToUTF8 conversion.
TEST(StringTest, UTF16ToUTF8) {
  using cef::logging::internal::UTF16ToUTF8;

  // Empty string.
  EXPECT_EQ("", UTF16ToUTF8(u""));
  EXPECT_EQ("", UTF16ToUTF8(std::u16string_view()));

  // ASCII string.
  EXPECT_EQ("Hello", UTF16ToUTF8(u"Hello"));

  // 2-byte UTF-8 characters (U+0080 to U+07FF).
  // U+00E9 = é (LATIN SMALL LETTER E WITH ACUTE)
  EXPECT_EQ("caf\xC3\xA9", UTF16ToUTF8(u"caf\u00E9"));

  // 3-byte UTF-8 characters (U+0800 to U+FFFF).
  // U+4E2D = 中 (CJK character)
  EXPECT_EQ("\xE4\xB8\xAD\xE6\x96\x87", UTF16ToUTF8(u"\u4E2D\u6587"));

  // 4-byte UTF-8 characters via surrogate pairs (U+10000 and above).
  // U+1F600 = 😀 (GRINNING FACE) = surrogate pair D83D DE00
  EXPECT_EQ("\xF0\x9F\x98\x80", UTF16ToUTF8(u"\U0001F600"));

  // Mixed content.
  EXPECT_EQ("Hello \xE4\xB8\x96\xE7\x95\x8C \xF0\x9F\x98\x80",
            UTF16ToUTF8(u"Hello \u4E16\u754C \U0001F600"));
}

#if defined(OS_WIN)
// Test WideToUTF8 conversion (Windows only).
TEST(StringTest, WideToUTF8) {
  using cef::logging::internal::WideToUTF8;

  // Empty string.
  EXPECT_EQ("", WideToUTF8(L""));
  EXPECT_EQ("", WideToUTF8(std::wstring_view()));

  // ASCII string.
  EXPECT_EQ("Hello", WideToUTF8(L"Hello"));

  // 2-byte UTF-8 characters.
  EXPECT_EQ("caf\xC3\xA9", WideToUTF8(L"caf\u00E9"));

  // 3-byte UTF-8 characters.
  EXPECT_EQ("\xE4\xB8\xAD\xE6\x96\x87", WideToUTF8(L"\u4E2D\u6587"));

  // 4-byte UTF-8 characters via surrogate pairs.
  EXPECT_EQ("\xF0\x9F\x98\x80", WideToUTF8(L"\U0001F600"));
}
#endif  // defined(OS_WIN)
