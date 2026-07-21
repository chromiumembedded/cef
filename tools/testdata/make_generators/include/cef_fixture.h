// Fixture header for CEF translator generator tests.

#ifndef CEF_INCLUDE_CEF_FIXTURE_H_
#define CEF_INCLUDE_CEF_FIXTURE_H_
#pragma once

#include <map>
#include <vector>

#include "include/cef_base.h"
#include "include/internal/cef_types.h"
#include "include/internal/cef_types_geometry.h"

class CefFixtureClient;
class CefFixtureLibrary;
class CefFixtureLibraryChild;
class CefFixtureScoped;

///
/// Add two fixture values.
///
/*--cef()--*/
int CefFixtureAdd(int left, int right);

///
/// Return a fixture value added in API version 13302.
///
/*--cef(added=13302)--*/
int CefFixtureAdded();

///
/// Library-side fixture object.
///
/*--cef(source=library)--*/
class CefFixtureLibrary : public CefBaseRefCounted {
 public:
  ///
  /// Create a library-side fixture.
  ///
  /*--cef()--*/
  static CefRefPtr<CefFixtureLibrary> Create();

  ///
  /// Return the current name.
  ///
  /*--cef()--*/
  virtual CefString GetName() = 0;

  ///
  /// Return a structure by value.
  ///
  /*--cef()--*/
  virtual CefRect GetRect() = 0;

  ///
  /// Set primitive, optional and index values.
  ///
  /*--cef(optional_param=flag,index_param=index)--*/
  virtual bool SetValues(int value, bool flag, size_t index) = 0;

  ///
  /// Set a string list.
  ///
  /*--cef()--*/
  virtual void SetNames(const std::vector<CefString>& names) = 0;

  ///
  /// Set a string map.
  ///
  /*--cef()--*/
  virtual void SetNameMap(
      const std::map<CefString, CefString>& names) = 0;

  ///
  /// Return a client-side object.
  ///
  /*--cef()--*/
  virtual CefRefPtr<CefFixtureClient> GetClient() = 0;

  ///
  /// Consume a non-owning scoped pointer.
  ///
  /*--cef()--*/
  virtual int SetRaw(CefRawPtr<CefFixtureScoped> value) = 0;

#if CEF_API_REMOVED(13302)
  ///
  /// Return the legacy versioned value.
  ///
  /*--cef(removed=13302)--*/
  virtual int GetVersionedValue() = 0;
#endif

#if CEF_API_ADDED(13302)
  ///
  /// Return the current versioned value.
  ///
  /*--cef(added=13302,capi_name=get_versioned_value_v2)--*/
  virtual int GetVersionedValue() = 0;
#endif

  IMPLEMENT_REFCOUNTING(CefFixtureLibrary);
};

///
/// Derived library-side fixture object.
///
/*--cef(source=library)--*/
class CefFixtureLibraryChild : public CefFixtureLibrary {
 public:
  ///
  /// Create a derived fixture.
  ///
  /*--cef()--*/
  static CefRefPtr<CefFixtureLibraryChild> Create();
};

///
/// Client-side fixture object.
///
/*--cef(source=client)--*/
class CefFixtureClient : public virtual CefBaseRefCounted {
 public:
  ///
  /// Return a value.
  ///
  /*--cef()--*/
  virtual int GetValue() = 0;

  IMPLEMENT_REFCOUNTING(CefFixtureClient);
};

#endif  // CEF_INCLUDE_CEF_FIXTURE_H_
