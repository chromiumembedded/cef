// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "include/test/cef_translator_test.h"

class CefTranslatorTestObjectImpl : public CefTranslatorTestObject {
 public:
  explicit CefTranslatorTestObjectImpl(int value)
    : value_(value) {
  }

  int GetValue() override {
    return value_;
  }

  void SetValue(int value) override {
    value_ = value;
  }

 protected:
  int value_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CefTranslatorTestObjectImpl);
  IMPLEMENT_REFCOUNTING(CefTranslatorTestObjectImpl);
};

// static
CefRefPtr<CefTranslatorTestObject> CefTranslatorTestObject::Create(int value) {
  return new CefTranslatorTestObjectImpl(value);
}

class CefTranslatorTestObjectChildImpl : public CefTranslatorTestObjectChild {
 public:
  CefTranslatorTestObjectChildImpl(int value, int other_value)
    : value_(value),
      other_value_(other_value) {
  }

  int GetValue() override {
    return value_;
  }

  void SetValue(int value) override {
    value_ = value;
  }

  int GetOtherValue() override {
    return other_value_;
  }

  void SetOtherValue(int value) override {
    other_value_ = value;
  }

 protected:
  int value_;
  int other_value_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CefTranslatorTestObjectChildImpl);
  IMPLEMENT_REFCOUNTING(CefTranslatorTestObjectChildImpl);
};

// static
CefRefPtr<CefTranslatorTestObjectChild> CefTranslatorTestObjectChild::Create(
    int value,
    int other_value) {
  return new CefTranslatorTestObjectChildImpl(value, other_value);
}

class CefTranslatorTestObjectChildChildImpl :
    public CefTranslatorTestObjectChildChild {
 public:
  CefTranslatorTestObjectChildChildImpl(int value,
                                        int other_value,
                                        int other_other_value)
    : value_(value),
      other_value_(other_value),
      other_other_value_(other_other_value) {
  }

  int GetValue() override {
    return value_;
  }

  void SetValue(int value) override {
    value_ = value;
  }

  int GetOtherValue() override {
    return other_value_;
  }

  void SetOtherValue(int value) override {
    other_value_ = value;
  }

  int GetOtherOtherValue() override {
    return other_other_value_;
  }

  void SetOtherOtherValue(int value) override {
    other_other_value_ = value;
  }

 protected:
  int value_;
  int other_value_;
  int other_other_value_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CefTranslatorTestObjectChildChildImpl);
  IMPLEMENT_REFCOUNTING(CefTranslatorTestObjectChildChildImpl);
};

// static
CefRefPtr<CefTranslatorTestObjectChildChild>
CefTranslatorTestObjectChildChild::Create(
    int value,
    int other_value,
    int other_other_value) {
  return new CefTranslatorTestObjectChildChildImpl(value, other_value,
                                                   other_other_value);
}

class CefTranslatorTestImpl : public CefTranslatorTest {
 public:
  CefTranslatorTestImpl() {
  }

  // PRIMITIVE VALUES

  void GetVoid() override {
  }

  bool GetBool() override {
    return TEST_BOOL_VAL;
  }

  int GetInt() override {
    return TEST_INT_VAL;
  }

  double GetDouble() override {
    return TEST_DOUBLE_VAL;
  }

  long GetLong() override {
    return TEST_LONG_VAL;
  }

  size_t GetSizet() override {
    return TEST_SIZET_VAL;
  }

  bool SetVoid() override {
    return true;
  }

  bool SetBool(bool val) override {
    return (val == TEST_BOOL_VAL);
  }

  bool SetInt(int val) override {
    return (val == TEST_INT_VAL);
  }

  bool SetDouble(double val) override {
    return (val == TEST_DOUBLE_VAL);
  }

  bool SetLong(long val) override {
    return (val == TEST_LONG_VAL);
  }

  bool SetSizet(size_t val) override {
    return (val == TEST_SIZET_VAL);
  }


  // PRIMITIVE LIST VALUES

  bool SetIntList(const std::vector<int>& val) override {
    if (val.size() != 2U)
      return false;
    return val[0] == TEST_INT_VAL && val[1] == TEST_INT_VAL2;
  }

  bool GetIntListByRef(IntList& val) override {
    if (val.size() != GetIntListSize())
      return false;
    val.clear();
    val.push_back(TEST_INT_VAL);
    val.push_back(TEST_INT_VAL2);
    return true;
  }

  size_t GetIntListSize() override {
    return 2U;
  }


  // STRING VALUES

  CefString GetString() override {
    return TEST_STRING_VAL;
  }

  bool SetString(const CefString& val) override {
    return (val.ToString() == TEST_STRING_VAL);
  }

  void GetStringByRef(CefString& val) override {
    val = TEST_STRING_VAL;
  }


  // STRING LIST VALUES

  bool SetStringList(const std::vector<CefString>& val) override {
    if (val.size() != 3U)
      return false;
    return val[0] == TEST_STRING_VAL &&
           val[1] == TEST_STRING_VAL2 &&
           val[2] == TEST_STRING_VAL3;
  }

  bool GetStringListByRef(StringList& val) override {
    if (val.size() != 0U)
      return false;
    val.push_back(TEST_STRING_VAL);
    val.push_back(TEST_STRING_VAL2);
    val.push_back(TEST_STRING_VAL3);
    return true;
  }


  // STRING MAP VALUES

  bool SetStringMap(const StringMap& val) override {
    if (val.size() != 3U)
      return false;

    StringMap::const_iterator it;
    
    it = val.find(TEST_STRING_KEY);
    if (it == val.end() || it->second != TEST_STRING_VAL)
      return false;
    it = val.find(TEST_STRING_KEY2);
    if (it == val.end() || it->second != TEST_STRING_VAL2)
      return false;
    it = val.find(TEST_STRING_KEY3);
    if (it == val.end() || it->second != TEST_STRING_VAL3)
      return false;
    return true;
  }

  bool GetStringMapByRef(std::map<CefString, CefString>& val) override {
    if (val.size() != 0U)
      return false;

    val.insert(std::make_pair(TEST_STRING_KEY, TEST_STRING_VAL));
    val.insert(std::make_pair(TEST_STRING_KEY2, TEST_STRING_VAL2));
    val.insert(std::make_pair(TEST_STRING_KEY3, TEST_STRING_VAL3));
    return true;
  }


  // STRING MULTIMAP VALUES

  bool SetStringMultimap(
      const std::multimap<CefString, CefString>& val) override {
    if (val.size() != 3U)
      return false;

    StringMultimap::const_iterator it;
    
    it = val.find(TEST_STRING_KEY);
    if (it == val.end() || it->second != TEST_STRING_VAL)
      return false;
    it = val.find(TEST_STRING_KEY2);
    if (it == val.end() || it->second != TEST_STRING_VAL2)
      return false;
    it = val.find(TEST_STRING_KEY3);
    if (it == val.end() || it->second != TEST_STRING_VAL3)
      return false;
    return true;
  }

  bool GetStringMultimapByRef(StringMultimap& val) override {
    if (val.size() != 0U)
      return false;

    val.insert(std::make_pair(TEST_STRING_KEY, TEST_STRING_VAL));
    val.insert(std::make_pair(TEST_STRING_KEY2, TEST_STRING_VAL2));
    val.insert(std::make_pair(TEST_STRING_KEY3, TEST_STRING_VAL3));
    return true;
  }


  // STRUCT VALUES

  CefPoint GetPoint() override {
    return CefPoint(TEST_X_VAL, TEST_Y_VAL);
  }

  bool SetPoint(const CefPoint& val) override {
    return val.x == TEST_X_VAL && val.y == TEST_Y_VAL;
  }

  void GetPointByRef(CefPoint& val) override {
    val = CefPoint(TEST_X_VAL, TEST_Y_VAL);
  }


  // STRUCT LIST VALUES

  bool SetPointList(const std::vector<CefPoint>& val) override {
    if (val.size() != 2U)
      return false;
    return val[0].x == TEST_X_VAL && val[0].y == TEST_Y_VAL &&
           val[1].x == TEST_X_VAL2 && val[1].y == TEST_Y_VAL2;
  }

  bool GetPointListByRef(PointList& val) override {
    if (val.size() != GetPointListSize())
      return false;
    val.clear();
    val.push_back(CefPoint(TEST_X_VAL, TEST_Y_VAL));
    val.push_back(CefPoint(TEST_X_VAL2, TEST_Y_VAL2));
    return true;
  }

  size_t GetPointListSize() override {
    return 2U;
  }


  // LIBRARY-SIDE OBJECT VALUES

  CefRefPtr<CefTranslatorTestObject> GetObject(int val) override {
    return new CefTranslatorTestObjectChildImpl(val, 0);
  }

  int SetObject(CefRefPtr<CefTranslatorTestObject> val) override {
    return val->GetValue();
  }

  CefRefPtr<CefTranslatorTestObject> SetObjectAndReturn(
      CefRefPtr<CefTranslatorTestObject> val) override {
    return val;
  }

  int SetChildObject(CefRefPtr<CefTranslatorTestObjectChild> val) override {
    return val->GetValue();
  }

  CefRefPtr<CefTranslatorTestObject> SetChildObjectAndReturnParent(
      CefRefPtr<CefTranslatorTestObjectChild> val) override {
    return val;
  }


  // LIBRARY-SIDE OBJECT LIST VALUES

  bool SetObjectList(
      const std::vector<CefRefPtr<CefTranslatorTestObject> >& val,
      int val1, int val2) override {
    if (val.size() != 2U)
      return false;
    return val[0]->GetValue() == val1 && val[1]->GetValue() == val2;
  }

  bool GetObjectListByRef(ObjectList& val, int val1, int val2) override {
    if (val.size() != GetObjectListSize())
      return false;
    val.clear();
    val.push_back(new CefTranslatorTestObjectChildImpl(val1, 0));
    val.push_back(new CefTranslatorTestObjectImpl(val2));
    return true;
  }

  size_t GetObjectListSize() override {
    return 2U;
  }


  // CLIENT-SIDE OBJECT VALUES

  int SetHandler(CefRefPtr<CefTranslatorTestHandler> val) override {
    return val->GetValue();
  }

  CefRefPtr<CefTranslatorTestHandler> SetHandlerAndReturn(
      CefRefPtr<CefTranslatorTestHandler> val) override {
    return val;
  }

  int SetChildHandler(CefRefPtr<CefTranslatorTestHandlerChild> val) override {
    return val->GetValue();
  }

  CefRefPtr<CefTranslatorTestHandler> SetChildHandlerAndReturnParent(
      CefRefPtr<CefTranslatorTestHandlerChild> val) override {
    return val;
  }


  // CLIENT-SIDE OBJECT LIST VALUES

  bool SetHandlerList(
      const std::vector<CefRefPtr<CefTranslatorTestHandler> >& val,
      int val1, int val2) override {
    if (val.size() != 2U)
      return false;
    return val[0]->GetValue() == val1 && val[1]->GetValue() == val2;
  }

  bool GetHandlerListByRef(
      HandlerList& val,
      CefRefPtr<CefTranslatorTestHandler> val1,
      CefRefPtr<CefTranslatorTestHandler> val2) override {
    if (val.size() != GetHandlerListSize())
      return false;
    val.clear();
    val.push_back(val1);
    val.push_back(val2);
    return true;
  }

  size_t GetHandlerListSize() override {
    return 2U;
  }

 private:
  IMPLEMENT_REFCOUNTING(CefTranslatorTestImpl);
  DISALLOW_COPY_AND_ASSIGN(CefTranslatorTestImpl);
};

// static
CefRefPtr<CefTranslatorTest> CefTranslatorTest::Create() {
  return new CefTranslatorTestImpl();
}
