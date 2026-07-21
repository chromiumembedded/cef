// Fixture scoped header for CEF translator generator tests.

#ifndef CEF_INCLUDE_CEF_FIXTURE_SCOPED_H_
#define CEF_INCLUDE_CEF_FIXTURE_SCOPED_H_
#pragma once

#include "include/cef_base.h"
#include "include/cef_fixture.h"

///
/// Library-side scoped fixture.
///
/*--cef(source=library)--*/
class CefFixtureScoped : public CefBaseScoped {
 public:
  ///
  /// Return a fixture value.
  ///
  /*--cef()--*/
  virtual int GetValue() = 0;
};

///
/// Derived scoped fixture.
///
/*--cef(source=library)--*/
class CefFixtureScopedChild : public CefFixtureScoped {
 public:
  ///
  /// Return an owned scoped fixture.
  ///
  /*--cef()--*/
  static CefOwnPtr<CefFixtureScopedChild> Create();
};

#endif  // CEF_INCLUDE_CEF_FIXTURE_SCOPED_H_
