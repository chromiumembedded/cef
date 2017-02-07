// Copyright (c) 2015 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "include/test/cef_translator_test.h"

class CefTranslatorTestRefPtrLibraryImpl :
    public CefTranslatorTestRefPtrLibrary {
 public:
  explicit CefTranslatorTestRefPtrLibraryImpl(int value)
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
  DISALLOW_COPY_AND_ASSIGN(CefTranslatorTestRefPtrLibraryImpl);
  IMPLEMENT_REFCOUNTING(CefTranslatorTestRefPtrLibraryImpl);
};

// static
CefRefPtr<CefTranslatorTestRefPtrLibrary>
CefTranslatorTestRefPtrLibrary::Create(int value) {
  return new CefTranslatorTestRefPtrLibraryImpl(value);
}

class CefTranslatorTestRefPtrLibraryChildImpl :
    public CefTranslatorTestRefPtrLibraryChild {
 public:
  CefTranslatorTestRefPtrLibraryChildImpl(int value, int other_value)
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
  DISALLOW_COPY_AND_ASSIGN(CefTranslatorTestRefPtrLibraryChildImpl);
  IMPLEMENT_REFCOUNTING(CefTranslatorTestRefPtrLibraryChildImpl);
};

// static
CefRefPtr<CefTranslatorTestRefPtrLibraryChild>
CefTranslatorTestRefPtrLibraryChild::Create(
    int value,
    int other_value) {
  return new CefTranslatorTestRefPtrLibraryChildImpl(value, other_value);
}

class CefTranslatorTestRefPtrLibraryChildChildImpl :
    public CefTranslatorTestRefPtrLibraryChildChild {
 public:
  CefTranslatorTestRefPtrLibraryChildChildImpl(int value,
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
  DISALLOW_COPY_AND_ASSIGN(CefTranslatorTestRefPtrLibraryChildChildImpl);
  IMPLEMENT_REFCOUNTING(CefTranslatorTestRefPtrLibraryChildChildImpl);
};

// static
CefRefPtr<CefTranslatorTestRefPtrLibraryChildChild>
CefTranslatorTestRefPtrLibraryChildChild::Create(
    int value,
    int other_value,
    int other_other_value) {
  return new CefTranslatorTestRefPtrLibraryChildChildImpl(value, other_value,
                                                          other_other_value);
}


class CefTranslatorTestScopedLibraryImpl :
    public CefTranslatorTestScopedLibrary {
 public:
  explicit CefTranslatorTestScopedLibraryImpl(int value)
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
  DISALLOW_COPY_AND_ASSIGN(CefTranslatorTestScopedLibraryImpl);
};

// static
CefOwnPtr<CefTranslatorTestScopedLibrary>
CefTranslatorTestScopedLibrary::Create(int value) {
  return CefOwnPtr<CefTranslatorTestScopedLibrary>(
      new CefTranslatorTestScopedLibraryImpl(value));
}

class CefTranslatorTestScopedLibraryChildImpl :
    public CefTranslatorTestScopedLibraryChild {
 public:
  CefTranslatorTestScopedLibraryChildImpl(int value, int other_value)
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
  DISALLOW_COPY_AND_ASSIGN(CefTranslatorTestScopedLibraryChildImpl);
};

// static
CefOwnPtr<CefTranslatorTestScopedLibraryChild>
CefTranslatorTestScopedLibraryChild::Create(
    int value,
    int other_value) {
  return CefOwnPtr<CefTranslatorTestScopedLibraryChild>(
      new CefTranslatorTestScopedLibraryChildImpl(value, other_value));
}

class CefTranslatorTestScopedLibraryChildChildImpl :
    public CefTranslatorTestScopedLibraryChildChild {
 public:
  CefTranslatorTestScopedLibraryChildChildImpl(int value,
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
  DISALLOW_COPY_AND_ASSIGN(CefTranslatorTestScopedLibraryChildChildImpl);
};

// static
CefOwnPtr<CefTranslatorTestScopedLibraryChildChild>
CefTranslatorTestScopedLibraryChildChild::Create(
    int value,
    int other_value,
    int other_other_value) {
  return CefOwnPtr<CefTranslatorTestScopedLibraryChildChild>(
      new CefTranslatorTestScopedLibraryChildChildImpl(value, other_value,
                                                       other_other_value));
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


  // LIBRARY-SIDE REFPTR VALUES

  CefRefPtr<CefTranslatorTestRefPtrLibrary> GetRefPtrLibrary(int val) override {
    return new CefTranslatorTestRefPtrLibraryChildImpl(val, 0);
  }

  int SetRefPtrLibrary(CefRefPtr<CefTranslatorTestRefPtrLibrary> val) override {
    return val->GetValue();
  }

  CefRefPtr<CefTranslatorTestRefPtrLibrary> SetRefPtrLibraryAndReturn(
      CefRefPtr<CefTranslatorTestRefPtrLibrary> val) override {
    return val;
  }

  int SetChildRefPtrLibrary(
      CefRefPtr<CefTranslatorTestRefPtrLibraryChild> val) override {
    return val->GetValue();
  }

  CefRefPtr<CefTranslatorTestRefPtrLibrary>
  SetChildRefPtrLibraryAndReturnParent(
      CefRefPtr<CefTranslatorTestRefPtrLibraryChild> val) override {
    return val;
  }


  // LIBRARY-SIDE REFPTR LIST VALUES

  bool SetRefPtrLibraryList(
      const std::vector<CefRefPtr<CefTranslatorTestRefPtrLibrary> >& val,
      int val1, int val2) override {
    if (val.size() != 2U)
      return false;
    return val[0]->GetValue() == val1 && val[1]->GetValue() == val2;
  }

  bool GetRefPtrLibraryListByRef(RefPtrLibraryList& val, int val1,
                                 int val2) override {
    if (val.size() != GetRefPtrLibraryListSize())
      return false;
    val.clear();
    val.push_back(new CefTranslatorTestRefPtrLibraryChildImpl(val1, 0));
    val.push_back(new CefTranslatorTestRefPtrLibraryImpl(val2));
    return true;
  }

  size_t GetRefPtrLibraryListSize() override {
    return 2U;
  }


  // CLIENT-SIDE REFPTR VALUES

  int SetRefPtrClient(CefRefPtr<CefTranslatorTestRefPtrClient> val) override {
    return val->GetValue();
  }

  CefRefPtr<CefTranslatorTestRefPtrClient> SetRefPtrClientAndReturn(
      CefRefPtr<CefTranslatorTestRefPtrClient> val) override {
    return val;
  }

  int SetChildRefPtrClient(
      CefRefPtr<CefTranslatorTestRefPtrClientChild> val) override {
    return val->GetValue();
  }

  CefRefPtr<CefTranslatorTestRefPtrClient> SetChildRefPtrClientAndReturnParent(
      CefRefPtr<CefTranslatorTestRefPtrClientChild> val) override {
    return val;
  }


  // CLIENT-SIDE REFPTR LIST VALUES

  bool SetRefPtrClientList(
      const std::vector<CefRefPtr<CefTranslatorTestRefPtrClient> >& val,
      int val1, int val2) override {
    if (val.size() != 2U)
      return false;
    return val[0]->GetValue() == val1 && val[1]->GetValue() == val2;
  }

  bool GetRefPtrClientListByRef(
      RefPtrClientList& val,
      CefRefPtr<CefTranslatorTestRefPtrClient> val1,
      CefRefPtr<CefTranslatorTestRefPtrClient> val2) override {
    if (val.size() != GetRefPtrClientListSize())
      return false;
    val.clear();
    val.push_back(val1);
    val.push_back(val2);
    return true;
  }

  size_t GetRefPtrClientListSize() override {
    return 2U;
  }


  // LIBRARY-SIDE OWNPTR VALUES

  CefOwnPtr<CefTranslatorTestScopedLibrary> GetOwnPtrLibrary(int val) override {
    return CefOwnPtr<CefTranslatorTestScopedLibrary>(
        new CefTranslatorTestScopedLibraryChildImpl(val, 0));
  }

  int SetOwnPtrLibrary(CefOwnPtr<CefTranslatorTestScopedLibrary> val) override {
    return val->GetValue();
  }

  CefOwnPtr<CefTranslatorTestScopedLibrary> SetOwnPtrLibraryAndReturn(
      CefOwnPtr<CefTranslatorTestScopedLibrary> val) override {
    return val;
  }

  int SetChildOwnPtrLibrary(
      CefOwnPtr<CefTranslatorTestScopedLibraryChild> val) override {
    return val->GetValue();
  }

  CefOwnPtr<CefTranslatorTestScopedLibrary>
  SetChildOwnPtrLibraryAndReturnParent(
      CefOwnPtr<CefTranslatorTestScopedLibraryChild> val) override {
    return CefOwnPtr<CefTranslatorTestScopedLibrary>(val.release());
  }


  // CLIENT-SIDE OWNPTR VALUES

  int SetOwnPtrClient(CefOwnPtr<CefTranslatorTestScopedClient> val) override {
    return val->GetValue();
  }

  CefOwnPtr<CefTranslatorTestScopedClient> SetOwnPtrClientAndReturn(
      CefOwnPtr<CefTranslatorTestScopedClient> val) override {
    return val;
  }

  int SetChildOwnPtrClient(
      CefOwnPtr<CefTranslatorTestScopedClientChild> val) override {
    return val->GetValue();
  }

  CefOwnPtr<CefTranslatorTestScopedClient> SetChildOwnPtrClientAndReturnParent(
      CefOwnPtr<CefTranslatorTestScopedClientChild> val) override {
    return CefOwnPtr<CefTranslatorTestScopedClient>(val.release());
  }


  // LIBRARY-SIDE RAWPTR VALUES

  int SetRawPtrLibrary(CefRawPtr<CefTranslatorTestScopedLibrary> val) override {
    return val->GetValue();
  }

  int SetChildRawPtrLibrary(
      CefRawPtr<CefTranslatorTestScopedLibraryChild> val) override {
    return val->GetValue();
  }


  // LIBRARY-SIDE RAWPTR LIST VALUES

  bool SetRawPtrLibraryList(
      const std::vector<CefRawPtr<CefTranslatorTestScopedLibrary> >& val,
      int val1, int val2) override {
    if (val.size() != 2U)
      return false;
    return val[0]->GetValue() == val1 && val[1]->GetValue() == val2;
  }


  // CLIENT-SIDE RAWPTR VALUES

  int SetRawPtrClient(CefRawPtr<CefTranslatorTestScopedClient> val) override {
    return val->GetValue();
  }

  int SetChildRawPtrClient(
      CefRawPtr<CefTranslatorTestScopedClientChild> val) override {
    return val->GetValue();
  }


  // CLIENT-SIDE RAWPTR LIST VALUES

  bool SetRawPtrClientList(
      const std::vector<CefRawPtr<CefTranslatorTestScopedClient> >& val,
      int val1, int val2) override {
    if (val.size() != 2U)
      return false;
    return val[0]->GetValue() == val1 && val[1]->GetValue() == val2;
  }

 private:
  IMPLEMENT_REFCOUNTING(CefTranslatorTestImpl);
  DISALLOW_COPY_AND_ASSIGN(CefTranslatorTestImpl);
};

// static
CefRefPtr<CefTranslatorTest> CefTranslatorTest::Create() {
  return new CefTranslatorTestImpl();
}
