// Copyright (c) 2024 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "cef/include/test/cef_api_version_test.h"
#include "cef/libcef/common/api_version_util.h"

namespace {

class CefApiVersionTestRefPtrLibraryImpl
    : public CefApiVersionTestRefPtrLibrary {
 public:
  CefApiVersionTestRefPtrLibraryImpl() = default;

  CefApiVersionTestRefPtrLibraryImpl(
      const CefApiVersionTestRefPtrLibraryImpl&) = delete;
  CefApiVersionTestRefPtrLibraryImpl& operator=(
      const CefApiVersionTestRefPtrLibraryImpl&) = delete;

  // Helper for creating this object at all supported API versions.
  static CefApiVersionTestRefPtrLibrary* Create(int value) {
    auto* obj = new CefApiVersionTestRefPtrLibraryImpl();

    if (CEF_API_IS_REMOVED(13301)) {
      obj->SetValue(value);
    } else if (CEF_API_IS_RANGE(13301, 13302)) {
      obj->SetValueV1(value);
    } else if (CEF_API_IS_ADDED(13302)) {
      obj->SetValueV2(value);
    } else {
      CEF_API_NOTREACHED();
    }

    return obj;
  }

  int GetValueLegacy() override { return value_legacy_; }

  void SetValueLegacy(int value) override { value_legacy_ = value; }

  int GetValue() override {
    CEF_API_REQUIRE_REMOVED(13301);
    return value_;
  }

  void SetValue(int value) override {
    CEF_API_REQUIRE_REMOVED(13301);
    value_ = value;
  }

  int GetValueV1() override {
    CEF_API_REQUIRE_RANGE(13301, 13302);
    return value_;
  }

  void SetValueV1(int value) override {
    CEF_API_REQUIRE_RANGE(13301, 13302);
    value_ = value;
  }

  int GetValueV2() override {
    CEF_API_REQUIRE_ADDED(13302);
    return value_;
  }

  void SetValueV2(int value) override {
    CEF_API_REQUIRE_ADDED(13302);
    value_ = value;
  }

  int GetValueExp() override {
    CEF_API_REQUIRE_ADDED(CEF_EXPERIMENTAL);
    return value_exp_;
  }

  void SetValueExp(int value) override {
    CEF_API_REQUIRE_ADDED(CEF_EXPERIMENTAL);
    value_exp_ = value;
  }

 private:
  int value_ = -1;
  int value_legacy_ = -1;
  int value_exp_ = -1;

  IMPLEMENT_REFCOUNTING(CefApiVersionTestRefPtrLibraryImpl);
};

}  // namespace

// static
CefRefPtr<CefApiVersionTestRefPtrLibrary>
CefApiVersionTestRefPtrLibrary::Create() {
  return new CefApiVersionTestRefPtrLibraryImpl();
}

// static
CefRefPtr<CefApiVersionTestRefPtrLibrary>
CefApiVersionTestRefPtrLibrary::Create(int value) {
  CEF_API_REQUIRE_ADDED(13301);
  return CefApiVersionTestRefPtrLibraryImpl::Create(value);
}

namespace {

class CefApiVersionTestRefPtrLibraryChildImpl
    : public CefApiVersionTestRefPtrLibraryChild {
 public:
  CefApiVersionTestRefPtrLibraryChildImpl() = default;

  CefApiVersionTestRefPtrLibraryChildImpl(
      const CefApiVersionTestRefPtrLibraryChildImpl&) = delete;
  CefApiVersionTestRefPtrLibraryChildImpl& operator=(
      const CefApiVersionTestRefPtrLibraryChildImpl&) = delete;

  // Helper for creating this object at all supported API versions.
  static CefApiVersionTestRefPtrLibraryChild* Create(int value,
                                                     int other_value) {
    auto* obj = new CefApiVersionTestRefPtrLibraryChildImpl();

    if (CEF_API_IS_REMOVED(13301)) {
      obj->SetValue(value);
    } else if (CEF_API_IS_RANGE(13301, 13302)) {
      obj->SetValueV1(value);
    } else if (CEF_API_IS_ADDED(13302)) {
      obj->SetValueV2(value);
    } else {
      CEF_API_NOTREACHED();
    }

    obj->SetOtherValue(other_value);

    return obj;
  }

  // CefApiVersionTestRefPtrLibrary methods:

  int GetValueLegacy() override { return value_legacy_; }

  void SetValueLegacy(int value) override { value_legacy_ = value; }

  int GetValue() override {
    CEF_API_REQUIRE_REMOVED(13301);
    return value_;
  }

  void SetValue(int value) override {
    CEF_API_REQUIRE_REMOVED(13301);
    value_ = value;
  }

  int GetValueV1() override {
    CEF_API_REQUIRE_RANGE(13301, 13302);
    return value_;
  }

  void SetValueV1(int value) override {
    CEF_API_REQUIRE_RANGE(13301, 13302);
    value_ = value;
  }

  int GetValueV2() override {
    CEF_API_REQUIRE_ADDED(13302);
    return value_;
  }

  void SetValueV2(int value) override {
    CEF_API_REQUIRE_ADDED(13302);
    value_ = value;
  }

  int GetValueExp() override {
    CEF_API_REQUIRE_ADDED(CEF_EXPERIMENTAL);
    return value_exp_;
  }

  void SetValueExp(int value) override {
    CEF_API_REQUIRE_ADDED(CEF_EXPERIMENTAL);
    value_exp_ = value;
  }

  // CefApiVersionTestRefPtrLibraryChild methods:

  int GetOtherValue() override { return other_value_; }

  void SetOtherValue(int value) override { other_value_ = value; }

 private:
  int value_ = -1;
  int value_legacy_ = -1;
  int value_exp_ = -1;
  int other_value_ = -1;

  IMPLEMENT_REFCOUNTING(CefApiVersionTestRefPtrLibraryChildImpl);
};

}  // namespace

// static
CefRefPtr<CefApiVersionTestRefPtrLibraryChild>
CefApiVersionTestRefPtrLibraryChild::Create() {
  return new CefApiVersionTestRefPtrLibraryChildImpl();
}

// static
CefRefPtr<CefApiVersionTestRefPtrLibraryChild>
CefApiVersionTestRefPtrLibraryChild::Create(int value, int other_value) {
  CEF_API_REQUIRE_ADDED(13301);
  return CefApiVersionTestRefPtrLibraryChildImpl::Create(value, other_value);
}

namespace {

// This object will only be created at the required API version, so
// API checks are simplified.
class CefApiVersionTestRefPtrLibraryChildChildImpl
    : public CefApiVersionTestRefPtrLibraryChildChild {
 public:
  CefApiVersionTestRefPtrLibraryChildChildImpl() = default;

  CefApiVersionTestRefPtrLibraryChildChildImpl(
      const CefApiVersionTestRefPtrLibraryChildChildImpl&) = delete;
  CefApiVersionTestRefPtrLibraryChildChildImpl& operator=(
      const CefApiVersionTestRefPtrLibraryChildChildImpl&) = delete;

  // CefApiVersionTestRefPtrLibrary methods:

  int GetValueLegacy() override { return value_legacy_; }

  void SetValueLegacy(int value) override { value_legacy_ = value; }

  int GetValue() override { return value_; }

  void SetValue(int value) override { value_ = value; }

  int GetValueV1() override { CEF_API_NOTREACHED(); }

  void SetValueV1(int value) override { CEF_API_NOTREACHED(); }

  int GetValueV2() override { CEF_API_NOTREACHED(); }

  void SetValueV2(int value) override { CEF_API_NOTREACHED(); }

  // CefApiVersionTestRefPtrLibraryChild methods:

  int GetOtherValue() override { return other_value_; }

  void SetOtherValue(int value) override { other_value_ = value; }

  int GetValueExp() override { CEF_API_NOTREACHED(); }

  void SetValueExp(int value) override { CEF_API_NOTREACHED(); }

  // CefApiVersionTestRefPtrLibraryChildChild methods:

  int GetOtherOtherValue() override { return other_other_value_; }

  void SetOtherOtherValue(int value) override { other_other_value_ = value; }

 private:
  int value_ = -1;
  int value_legacy_ = -1;
  int other_value_ = -1;
  int other_other_value_ = -1;

  IMPLEMENT_REFCOUNTING(CefApiVersionTestRefPtrLibraryChildChildImpl);
};

}  // namespace

// static
CefRefPtr<CefApiVersionTestRefPtrLibraryChildChild>
CefApiVersionTestRefPtrLibraryChildChild::Create() {
  CEF_API_REQUIRE_REMOVED(13301);
  return new CefApiVersionTestRefPtrLibraryChildChildImpl();
}

namespace {

// This object will only be created at the required API version, so
// API checks are simplified.
class CefApiVersionTestRefPtrLibraryChildChildV1Impl
    : public CefApiVersionTestRefPtrLibraryChildChildV1 {
 public:
  CefApiVersionTestRefPtrLibraryChildChildV1Impl() = default;

  CefApiVersionTestRefPtrLibraryChildChildV1Impl(
      const CefApiVersionTestRefPtrLibraryChildChildV1Impl&) = delete;
  CefApiVersionTestRefPtrLibraryChildChildV1Impl& operator=(
      const CefApiVersionTestRefPtrLibraryChildChildV1Impl&) = delete;

  // CefApiVersionTestRefPtrLibrary methods:

  int GetValueLegacy() override { return value_legacy_; }

  void SetValueLegacy(int value) override { value_legacy_ = value; }

  int GetValue() override { CEF_API_NOTREACHED(); }

  void SetValue(int value) override { CEF_API_NOTREACHED(); }

  int GetValueV1() override { return value_; }

  void SetValueV1(int value) override { value_ = value; }

  int GetValueV2() override { CEF_API_NOTREACHED(); }

  void SetValueV2(int value) override { CEF_API_NOTREACHED(); }

  int GetValueExp() override { CEF_API_NOTREACHED(); }

  void SetValueExp(int value) override { CEF_API_NOTREACHED(); }

  // CefApiVersionTestRefPtrLibraryChild methods:

  int GetOtherValue() override { return other_value_; }

  void SetOtherValue(int value) override { other_value_ = value; }

  // CefApiVersionTestRefPtrLibraryChildChildV1 methods:

  int GetOtherOtherValue() override { return other_other_value_; }

  void SetOtherOtherValue(int value) override { other_other_value_ = value; }

 private:
  int value_ = -1;
  int value_legacy_ = -1;
  int other_value_ = -1;
  int other_other_value_ = -1;

  IMPLEMENT_REFCOUNTING(CefApiVersionTestRefPtrLibraryChildChildV1Impl);
};

}  // namespace

// static
CefRefPtr<CefApiVersionTestRefPtrLibraryChildChildV1>
CefApiVersionTestRefPtrLibraryChildChildV1::Create() {
  CEF_API_REQUIRE_RANGE(13301, 13302);
  return new CefApiVersionTestRefPtrLibraryChildChildV1Impl();
}

// static
CefRefPtr<CefApiVersionTestRefPtrLibraryChildChildV1>
CefApiVersionTestRefPtrLibraryChildChildV1::Create(int value,
                                                   int other_value,
                                                   int other_other_value) {
  CEF_API_REQUIRE_RANGE(13301, 13302);
  auto* obj = new CefApiVersionTestRefPtrLibraryChildChildV1Impl();
  obj->SetValueV1(value);
  obj->SetOtherValue(other_value);
  obj->SetOtherOtherValue(other_other_value);
  return obj;
}

namespace {

// This object will only be created at the required API version, so
// API checks are simplified.
class CefApiVersionTestRefPtrLibraryChildChildV2Impl
    : public CefApiVersionTestRefPtrLibraryChildChildV2 {
 public:
  CefApiVersionTestRefPtrLibraryChildChildV2Impl() = default;

  CefApiVersionTestRefPtrLibraryChildChildV2Impl(
      const CefApiVersionTestRefPtrLibraryChildChildV1Impl&) = delete;
  CefApiVersionTestRefPtrLibraryChildChildV1Impl& operator=(
      const CefApiVersionTestRefPtrLibraryChildChildV1Impl&) = delete;

  // CefApiVersionTestRefPtrLibrary methods:

  int GetValueLegacy() override { return value_legacy_; }

  void SetValueLegacy(int value) override { value_legacy_ = value; }

  int GetValue() override { CEF_API_NOTREACHED(); }

  void SetValue(int value) override { CEF_API_NOTREACHED(); }

  int GetValueV1() override { CEF_API_NOTREACHED(); }

  void SetValueV1(int value) override { CEF_API_NOTREACHED(); }

  int GetValueV2() override { return value_; }

  void SetValueV2(int value) override { value_ = value; }

  int GetValueExp() override {
    CEF_API_REQUIRE_ADDED(CEF_EXPERIMENTAL);
    return value_exp_;
  }

  void SetValueExp(int value) override {
    CEF_API_REQUIRE_ADDED(CEF_EXPERIMENTAL);
    value_exp_ = value;
  }

  // CefApiVersionTestRefPtrLibraryChild methods:

  int GetOtherValue() override { return other_value_; }

  void SetOtherValue(int value) override { other_value_ = value; }

  // CefApiVersionTestRefPtrLibraryChildChildV2 methods:

  int GetOtherOtherValue() override { return other_other_value_; }

  void SetOtherOtherValue(int value) override { other_other_value_ = value; }

 private:
  int value_ = -1;
  int value_legacy_ = -1;
  int value_exp_ = -1;
  int other_value_ = -1;
  int other_other_value_ = -1;

  IMPLEMENT_REFCOUNTING(CefApiVersionTestRefPtrLibraryChildChildV2Impl);
};

}  // namespace

// static
CefRefPtr<CefApiVersionTestRefPtrLibraryChildChildV2>
CefApiVersionTestRefPtrLibraryChildChildV2::Create() {
  CEF_API_REQUIRE_ADDED(13302);
  return new CefApiVersionTestRefPtrLibraryChildChildV2Impl();
}

// static
CefRefPtr<CefApiVersionTestRefPtrLibraryChildChildV2>
CefApiVersionTestRefPtrLibraryChildChildV2::Create(int value,
                                                   int other_value,
                                                   int other_other_value) {
  CEF_API_REQUIRE_ADDED(13302);
  auto* obj = new CefApiVersionTestRefPtrLibraryChildChildV2Impl();
  obj->SetValueV2(value);
  obj->SetOtherValue(other_value);
  obj->SetOtherOtherValue(other_other_value);
  return obj;
}

namespace {

class CefApiVersionTestScopedLibraryImpl
    : public CefApiVersionTestScopedLibrary {
 public:
  CefApiVersionTestScopedLibraryImpl() = default;

  CefApiVersionTestScopedLibraryImpl(
      const CefApiVersionTestScopedLibraryImpl&) = delete;
  CefApiVersionTestScopedLibraryImpl& operator=(
      const CefApiVersionTestScopedLibraryImpl&) = delete;

  // Helper for creating this object at all supported API versions.
  static CefApiVersionTestScopedLibrary* Create(int value) {
    auto* obj = new CefApiVersionTestScopedLibraryImpl();

    if (CEF_API_IS_REMOVED(13301)) {
      obj->SetValue(value);
    } else if (CEF_API_IS_RANGE(13301, 13302)) {
      obj->SetValueV1(value);
    } else if (CEF_API_IS_ADDED(13302)) {
      obj->SetValueV2(value);
    } else {
      CEF_API_NOTREACHED();
    }

    return obj;
  }

  int GetValueLegacy() override { return value_legacy_; }

  void SetValueLegacy(int value) override { value_legacy_ = value; }

  int GetValue() override {
    CEF_API_REQUIRE_REMOVED(13301);
    return value_;
  }

  void SetValue(int value) override {
    CEF_API_REQUIRE_REMOVED(13301);
    value_ = value;
  }

  int GetValueV1() override {
    CEF_API_REQUIRE_RANGE(13301, 13302);
    return value_;
  }

  void SetValueV1(int value) override {
    CEF_API_REQUIRE_RANGE(13301, 13302);
    value_ = value;
  }

  int GetValueV2() override {
    CEF_API_REQUIRE_ADDED(13302);
    return value_;
  }

  void SetValueV2(int value) override {
    CEF_API_REQUIRE_ADDED(13302);
    value_ = value;
  }

  int GetValueExp() override {
    CEF_API_REQUIRE_ADDED(CEF_EXPERIMENTAL);
    return value_exp_;
  }

  void SetValueExp(int value) override {
    CEF_API_REQUIRE_ADDED(CEF_EXPERIMENTAL);
    value_exp_ = value;
  }

 private:
  int value_ = -1;
  int value_legacy_ = -1;
  int value_exp_ = -1;
};

}  // namespace

// static
CefOwnPtr<CefApiVersionTestScopedLibrary>
CefApiVersionTestScopedLibrary::Create() {
  return CefOwnPtr<CefApiVersionTestScopedLibrary>(
      new CefApiVersionTestScopedLibraryImpl());
}

// static
CefOwnPtr<CefApiVersionTestScopedLibrary>
CefApiVersionTestScopedLibrary::Create(int value) {
  CEF_API_REQUIRE_ADDED(13301);
  return CefOwnPtr<CefApiVersionTestScopedLibrary>(
      CefApiVersionTestScopedLibraryImpl::Create(value));
}

namespace {

class CefApiVersionTestScopedLibraryChildImpl
    : public CefApiVersionTestScopedLibraryChild {
 public:
  CefApiVersionTestScopedLibraryChildImpl() = default;

  CefApiVersionTestScopedLibraryChildImpl(
      const CefApiVersionTestScopedLibraryChildImpl&) = delete;
  CefApiVersionTestScopedLibraryChildImpl& operator=(
      const CefApiVersionTestScopedLibraryChildImpl&) = delete;

  // Helper for creating this object at all supported API versions.
  static CefApiVersionTestScopedLibraryChild* Create(int value,
                                                     int other_value) {
    auto* obj = new CefApiVersionTestScopedLibraryChildImpl();

    if (CEF_API_IS_REMOVED(13301)) {
      obj->SetValue(value);
    } else if (CEF_API_IS_RANGE(13301, 13302)) {
      obj->SetValueV1(value);
    } else if (CEF_API_IS_ADDED(13302)) {
      obj->SetValueV2(value);
    } else {
      CEF_API_NOTREACHED();
    }

    obj->SetOtherValue(other_value);

    return obj;
  }

  // CefApiVersionTestScopedLibrary methods:

  int GetValueLegacy() override { return value_legacy_; }

  void SetValueLegacy(int value) override { value_legacy_ = value; }

  int GetValue() override {
    CEF_API_REQUIRE_REMOVED(13301);
    return value_;
  }

  void SetValue(int value) override {
    CEF_API_REQUIRE_REMOVED(13301);
    value_ = value;
  }

  int GetValueV1() override {
    CEF_API_REQUIRE_RANGE(13301, 13302);
    return value_;
  }

  void SetValueV1(int value) override {
    CEF_API_REQUIRE_RANGE(13301, 13302);
    value_ = value;
  }

  int GetValueV2() override {
    CEF_API_REQUIRE_ADDED(13302);
    return value_;
  }

  void SetValueV2(int value) override {
    CEF_API_REQUIRE_ADDED(13302);
    value_ = value;
  }

  int GetValueExp() override {
    CEF_API_REQUIRE_ADDED(CEF_EXPERIMENTAL);
    return value_exp_;
  }

  void SetValueExp(int value) override {
    CEF_API_REQUIRE_ADDED(CEF_EXPERIMENTAL);
    value_exp_ = value;
  }

  // CefApiVersionTestScopedLibraryChild methods:

  int GetOtherValue() override { return other_value_; }

  void SetOtherValue(int value) override { other_value_ = value; }

 private:
  int value_ = -1;
  int value_legacy_ = -1;
  int value_exp_ = -1;
  int other_value_ = -1;
};

}  // namespace

// static
CefOwnPtr<CefApiVersionTestScopedLibraryChild>
CefApiVersionTestScopedLibraryChild::Create() {
  return CefOwnPtr<CefApiVersionTestScopedLibraryChild>(
      new CefApiVersionTestScopedLibraryChildImpl());
}

// static
CefOwnPtr<CefApiVersionTestScopedLibraryChild>
CefApiVersionTestScopedLibraryChild::Create(int value, int other_value) {
  CEF_API_REQUIRE_ADDED(13301);
  return CefOwnPtr<CefApiVersionTestScopedLibraryChild>(
      CefApiVersionTestScopedLibraryChildImpl::Create(value, other_value));
}

namespace {

// This object will only be created at the required API version, so
// API checks are simplified.
class CefApiVersionTestScopedLibraryChildChildImpl
    : public CefApiVersionTestScopedLibraryChildChild {
 public:
  CefApiVersionTestScopedLibraryChildChildImpl() = default;

  CefApiVersionTestScopedLibraryChildChildImpl(
      const CefApiVersionTestScopedLibraryChildChildImpl&) = delete;
  CefApiVersionTestScopedLibraryChildChildImpl& operator=(
      const CefApiVersionTestScopedLibraryChildChildImpl&) = delete;

  // CefApiVersionTestScopedLibrary methods:

  int GetValueLegacy() override { return value_legacy_; }

  void SetValueLegacy(int value) override { value_legacy_ = value; }

  int GetValue() override { return value_; }

  void SetValue(int value) override { value_ = value; }

  int GetValueV1() override { CEF_API_NOTREACHED(); }

  void SetValueV1(int value) override { CEF_API_NOTREACHED(); }

  int GetValueV2() override { CEF_API_NOTREACHED(); }

  void SetValueV2(int value) override { CEF_API_NOTREACHED(); }

  // CefApiVersionTestScopedLibraryChild methods:

  int GetOtherValue() override { return other_value_; }

  void SetOtherValue(int value) override { other_value_ = value; }

  int GetValueExp() override { CEF_API_NOTREACHED(); }

  void SetValueExp(int value) override { CEF_API_NOTREACHED(); }

  // CefApiVersionTestScopedLibraryChildChild methods:

  int GetOtherOtherValue() override { return other_other_value_; }

  void SetOtherOtherValue(int value) override { other_other_value_ = value; }

 private:
  int value_ = -1;
  int value_legacy_ = -1;
  int other_value_ = -1;
  int other_other_value_ = -1;
};

}  // namespace

// static
CefOwnPtr<CefApiVersionTestScopedLibraryChildChild>
CefApiVersionTestScopedLibraryChildChild::Create() {
  CEF_API_REQUIRE_REMOVED(13301);
  return CefOwnPtr<CefApiVersionTestScopedLibraryChildChild>(
      new CefApiVersionTestScopedLibraryChildChildImpl());
}

namespace {

// This object will only be created at the required API version, so
// API checks are simplified.
class CefApiVersionTestScopedLibraryChildChildV1Impl
    : public CefApiVersionTestScopedLibraryChildChildV1 {
 public:
  CefApiVersionTestScopedLibraryChildChildV1Impl() = default;

  CefApiVersionTestScopedLibraryChildChildV1Impl(
      const CefApiVersionTestScopedLibraryChildChildV1Impl&) = delete;
  CefApiVersionTestScopedLibraryChildChildV1Impl& operator=(
      const CefApiVersionTestScopedLibraryChildChildV1Impl&) = delete;

  // CefApiVersionTestScopedLibrary methods:

  int GetValueLegacy() override { return value_legacy_; }

  void SetValueLegacy(int value) override { value_legacy_ = value; }

  int GetValue() override { CEF_API_NOTREACHED(); }

  void SetValue(int value) override { CEF_API_NOTREACHED(); }

  int GetValueV1() override { return value_; }

  void SetValueV1(int value) override { value_ = value; }

  int GetValueV2() override { CEF_API_NOTREACHED(); }

  void SetValueV2(int value) override { CEF_API_NOTREACHED(); }

  int GetValueExp() override { CEF_API_NOTREACHED(); }

  void SetValueExp(int value) override { CEF_API_NOTREACHED(); }

  // CefApiVersionTestScopedLibraryChild methods:

  int GetOtherValue() override { return other_value_; }

  void SetOtherValue(int value) override { other_value_ = value; }

  // CefApiVersionTestScopedLibraryChildChildV1 methods:

  int GetOtherOtherValue() override { return other_other_value_; }

  void SetOtherOtherValue(int value) override { other_other_value_ = value; }

 private:
  int value_ = -1;
  int value_legacy_ = -1;
  int other_value_ = -1;
  int other_other_value_ = -1;
};

}  // namespace

// static
CefOwnPtr<CefApiVersionTestScopedLibraryChildChildV1>
CefApiVersionTestScopedLibraryChildChildV1::Create() {
  CEF_API_REQUIRE_RANGE(13301, 13302);
  return CefOwnPtr<CefApiVersionTestScopedLibraryChildChildV1>(
      new CefApiVersionTestScopedLibraryChildChildV1Impl());
}

// static
CefOwnPtr<CefApiVersionTestScopedLibraryChildChildV1>
CefApiVersionTestScopedLibraryChildChildV1::Create(int value,
                                                   int other_value,
                                                   int other_other_value) {
  CEF_API_REQUIRE_RANGE(13301, 13302);
  auto* obj = new CefApiVersionTestScopedLibraryChildChildV1Impl();
  obj->SetValueV1(value);
  obj->SetOtherValue(other_value);
  obj->SetOtherOtherValue(other_other_value);
  return CefOwnPtr<CefApiVersionTestScopedLibraryChildChildV1>(obj);
}

namespace {

// This object will only be created at the required API version, so
// API checks are simplified.
class CefApiVersionTestScopedLibraryChildChildV2Impl
    : public CefApiVersionTestScopedLibraryChildChildV2 {
 public:
  CefApiVersionTestScopedLibraryChildChildV2Impl() = default;

  CefApiVersionTestScopedLibraryChildChildV2Impl(
      const CefApiVersionTestScopedLibraryChildChildV1Impl&) = delete;
  CefApiVersionTestScopedLibraryChildChildV1Impl& operator=(
      const CefApiVersionTestScopedLibraryChildChildV1Impl&) = delete;

  // CefApiVersionTestScopedLibrary methods:

  int GetValueLegacy() override { return value_legacy_; }

  void SetValueLegacy(int value) override { value_legacy_ = value; }

  int GetValue() override { CEF_API_NOTREACHED(); }

  void SetValue(int value) override { CEF_API_NOTREACHED(); }

  int GetValueV1() override { CEF_API_NOTREACHED(); }

  void SetValueV1(int value) override { CEF_API_NOTREACHED(); }

  int GetValueV2() override { return value_; }

  void SetValueV2(int value) override { value_ = value; }

  int GetValueExp() override {
    CEF_API_REQUIRE_ADDED(CEF_EXPERIMENTAL);
    return value_exp_;
  }

  void SetValueExp(int value) override {
    CEF_API_REQUIRE_ADDED(CEF_EXPERIMENTAL);
    value_exp_ = value;
  }

  // CefApiVersionTestScopedLibraryChild methods:

  int GetOtherValue() override { return other_value_; }

  void SetOtherValue(int value) override { other_value_ = value; }

  // CefApiVersionTestScopedLibraryChildChildV2 methods:

  int GetOtherOtherValue() override { return other_other_value_; }

  void SetOtherOtherValue(int value) override { other_other_value_ = value; }

 private:
  int value_ = -1;
  int value_legacy_ = -1;
  int value_exp_ = -1;
  int other_value_ = -1;
  int other_other_value_ = -1;
};

}  // namespace

// static
CefOwnPtr<CefApiVersionTestScopedLibraryChildChildV2>
CefApiVersionTestScopedLibraryChildChildV2::Create() {
  CEF_API_REQUIRE_ADDED(13302);
  return CefOwnPtr<CefApiVersionTestScopedLibraryChildChildV2>(
      new CefApiVersionTestScopedLibraryChildChildV2Impl());
}

// static
CefOwnPtr<CefApiVersionTestScopedLibraryChildChildV2>
CefApiVersionTestScopedLibraryChildChildV2::Create(int value,
                                                   int other_value,
                                                   int other_other_value) {
  CEF_API_REQUIRE_ADDED(13302);
  auto* obj = new CefApiVersionTestScopedLibraryChildChildV2Impl();
  obj->SetValueV2(value);
  obj->SetOtherValue(other_value);
  obj->SetOtherOtherValue(other_other_value);
  return CefOwnPtr<CefApiVersionTestScopedLibraryChildChildV2>(obj);
}

namespace {

class CefApiVersionTestImpl : public CefApiVersionTest {
 public:
  CefApiVersionTestImpl() = default;

  CefApiVersionTestImpl(const CefApiVersionTestImpl&) = delete;
  CefApiVersionTestImpl& operator=(const CefApiVersionTestImpl&) = delete;

  // LIBRARY-SIDE REFPTR VALUES

  CefRefPtr<CefApiVersionTestRefPtrLibrary> GetRefPtrLibrary(int val) override {
    return CefApiVersionTestRefPtrLibraryChildImpl::Create(val, 0);
  }

  int SetRefPtrLibrary(CefRefPtr<CefApiVersionTestRefPtrLibrary> val) override {
    return GetValue(val);
  }

  CefRefPtr<CefApiVersionTestRefPtrLibrary> SetRefPtrLibraryAndReturn(
      CefRefPtr<CefApiVersionTestRefPtrLibrary> val) override {
    return val;
  }

  int SetChildRefPtrLibrary(
      CefRefPtr<CefApiVersionTestRefPtrLibraryChild> val) override {
    return GetValue(val);
  }

  CefRefPtr<CefApiVersionTestRefPtrLibrary>
  SetChildRefPtrLibraryAndReturnParent(
      CefRefPtr<CefApiVersionTestRefPtrLibraryChild> val) override {
    return val;
  }

  // LIBRARY-SIDE REFPTR LIST VALUES

  bool SetRefPtrLibraryList(
      const std::vector<CefRefPtr<CefApiVersionTestRefPtrLibrary>>& val,
      int val1,
      int val2) override {
    if (val.size() != 2U) {
      return false;
    }
    return GetValue(val[0]) == val1 && GetValue(val[1]) == val2;
  }

  bool GetRefPtrLibraryListByRef(RefPtrLibraryList& val,
                                 int val1,
                                 int val2) override {
    if (val.size() != GetRefPtrLibraryListSize()) {
      return false;
    }
    val.clear();
    val.push_back(CefApiVersionTestRefPtrLibraryChildImpl::Create(val1, 0));
    val.push_back(CefApiVersionTestRefPtrLibraryImpl::Create(val2));
    return true;
  }

  size_t GetRefPtrLibraryListSize() override { return 2U; }

  // CLIENT-SIDE REFPTR VALUES

  int SetRefPtrClient(CefRefPtr<CefApiVersionTestRefPtrClient> val) override {
    return GetValue(val);
  }

  CefRefPtr<CefApiVersionTestRefPtrClient> SetRefPtrClientAndReturn(
      CefRefPtr<CefApiVersionTestRefPtrClient> val) override {
    return val;
  }

  int SetChildRefPtrClient(
      CefRefPtr<CefApiVersionTestRefPtrClientChild> val) override {
    CEF_API_REQUIRE_REMOVED(13302);
    return GetValue(val);
  }

  CefRefPtr<CefApiVersionTestRefPtrClient> SetChildRefPtrClientAndReturnParent(
      CefRefPtr<CefApiVersionTestRefPtrClientChild> val) override {
    CEF_API_REQUIRE_REMOVED(13302);
    return val;
  }

  int SetChildRefPtrClient(
      CefRefPtr<CefApiVersionTestRefPtrClientChildV2> val) override {
    CEF_API_REQUIRE_ADDED(13302);
    return GetValue(val);
  }

  CefRefPtr<CefApiVersionTestRefPtrClient> SetChildRefPtrClientAndReturnParent(
      CefRefPtr<CefApiVersionTestRefPtrClientChildV2> val) override {
    CEF_API_REQUIRE_ADDED(13302);
    return val;
  }

  // CLIENT-SIDE REFPTR LIST VALUES

  bool SetRefPtrClientList(
      const std::vector<CefRefPtr<CefApiVersionTestRefPtrClient>>& val,
      int val1,
      int val2) override {
    if (val.size() != 2U) {
      return false;
    }
    return GetValue(val[0]) == val1 && GetValue(val[1]) == val2;
  }

  bool GetRefPtrClientListByRef(
      RefPtrClientList& val,
      CefRefPtr<CefApiVersionTestRefPtrClient> val1,
      CefRefPtr<CefApiVersionTestRefPtrClient> val2) override {
    if (val.size() != GetRefPtrClientListSize()) {
      return false;
    }
    val.clear();
    val.push_back(val1);
    val.push_back(val2);
    return true;
  }

  size_t GetRefPtrClientListSize() override { return 2U; }

  // LIBRARY-SIDE OWNPTR VALUES

  CefOwnPtr<CefApiVersionTestScopedLibrary> GetOwnPtrLibrary(int val) override {
    return CefOwnPtr<CefApiVersionTestScopedLibrary>(
        CefApiVersionTestScopedLibraryChildImpl::Create(val, 0));
  }

  int SetOwnPtrLibrary(CefOwnPtr<CefApiVersionTestScopedLibrary> val) override {
    return GetValue(val);
  }

  CefOwnPtr<CefApiVersionTestScopedLibrary> SetOwnPtrLibraryAndReturn(
      CefOwnPtr<CefApiVersionTestScopedLibrary> val) override {
    return val;
  }

  int SetChildOwnPtrLibrary(
      CefOwnPtr<CefApiVersionTestScopedLibraryChild> val) override {
    return GetValue(val);
  }

  CefOwnPtr<CefApiVersionTestScopedLibrary>
  SetChildOwnPtrLibraryAndReturnParent(
      CefOwnPtr<CefApiVersionTestScopedLibraryChild> val) override {
    return CefOwnPtr<CefApiVersionTestScopedLibrary>(val.release());
  }

  // CLIENT-SIDE OWNPTR VALUES

  int SetOwnPtrClient(CefOwnPtr<CefApiVersionTestScopedClient> val) override {
    return GetValue(val);
  }

  CefOwnPtr<CefApiVersionTestScopedClient> SetOwnPtrClientAndReturn(
      CefOwnPtr<CefApiVersionTestScopedClient> val) override {
    return val;
  }

  int SetChildOwnPtrClient(
      CefOwnPtr<CefApiVersionTestScopedClientChild> val) override {
    CEF_API_REQUIRE_REMOVED(13302);
    return GetValue(val);
  }

  CefOwnPtr<CefApiVersionTestScopedClient> SetChildOwnPtrClientAndReturnParent(
      CefOwnPtr<CefApiVersionTestScopedClientChild> val) override {
    CEF_API_REQUIRE_REMOVED(13302);
    return CefOwnPtr<CefApiVersionTestScopedClient>(val.release());
  }

  int SetChildOwnPtrClient(
      CefOwnPtr<CefApiVersionTestScopedClientChildV2> val) override {
    CEF_API_REQUIRE_ADDED(13302);
    return GetValue(val);
  }

  CefOwnPtr<CefApiVersionTestScopedClient> SetChildOwnPtrClientAndReturnParent(
      CefOwnPtr<CefApiVersionTestScopedClientChildV2> val) override {
    CEF_API_REQUIRE_ADDED(13302);
    return CefOwnPtr<CefApiVersionTestScopedClient>(val.release());
  }

  // LIBRARY-SIDE RAWPTR VALUES

  int SetRawPtrLibrary(CefRawPtr<CefApiVersionTestScopedLibrary> val) override {
    return GetValue(val);
  }

  int SetChildRawPtrLibrary(
      CefRawPtr<CefApiVersionTestScopedLibraryChild> val) override {
    return GetValue(val);
  }

  // LIBRARY-SIDE RAWPTR LIST VALUES

  bool SetRawPtrLibraryList(
      const std::vector<CefRawPtr<CefApiVersionTestScopedLibrary>>& val,
      int val1,
      int val2) override {
    if (val.size() != 2U) {
      return false;
    }
    return GetValue(val[0]) == val1 && GetValue(val[1]) == val2;
  }

  // CLIENT-SIDE RAWPTR VALUES

  int SetRawPtrClient(CefRawPtr<CefApiVersionTestScopedClient> val) override {
    return GetValue(val);
  }

  int SetChildRawPtrClient(
      CefRawPtr<CefApiVersionTestScopedClientChild> val) override {
    CEF_API_REQUIRE_REMOVED(13302);
    return GetValue(val);
  }

  int SetChildRawPtrClient(
      CefRawPtr<CefApiVersionTestScopedClientChildV2> val) override {
    CEF_API_REQUIRE_ADDED(13302);
    return GetValue(val);
  }

  // CLIENT-SIDE RAWPTR LIST VALUES

  bool SetRawPtrClientList(
      const std::vector<CefRawPtr<CefApiVersionTestScopedClient>>& val,
      int val1,
      int val2) override {
    if (val.size() != 2U) {
      return false;
    }
    return GetValue(val[0]) == val1 && GetValue(val[1]) == val2;
  }

 private:
  template <typename T>
  static int GetValue(T& obj) {
    if (CEF_API_IS_REMOVED(13301)) {
      return obj->GetValue();
    }
    if (CEF_API_IS_RANGE(13301, 13302)) {
      return obj->GetValueV1();
    }
    if (CEF_API_IS_ADDED(13302)) {
      return obj->GetValueV2();
    }
    CEF_API_NOTREACHED();
  }

  IMPLEMENT_REFCOUNTING(CefApiVersionTestImpl);
};

}  // namespace

// static
CefRefPtr<CefApiVersionTest> CefApiVersionTest::Create() {
  return new CefApiVersionTestImpl();
}
