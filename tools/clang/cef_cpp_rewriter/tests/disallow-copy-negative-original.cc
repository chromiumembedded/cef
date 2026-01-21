// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

// Negative test file for DISALLOW_COPY_AND_ASSIGN transformation
// These cases should NOT be transformed
// Path contains /cef/ to pass path filter

// Mimic CEF's macro definitions
#define DISALLOW_COPY(TypeName) TypeName(const TypeName&) = delete
#define DISALLOW_ASSIGN(TypeName) TypeName& operator=(const TypeName&) = delete
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  DISALLOW_COPY(TypeName);                 \
  DISALLOW_ASSIGN(TypeName)
#define DISALLOW_IMPLICIT_CONSTRUCTORS(TypeName) \
  TypeName() = delete;                           \
  DISALLOW_COPY_AND_ASSIGN(TypeName)

// Forward declaration - no body, should be skipped
class ForwardDeclared;

// Class without the macro - should not be modified
class NoMacro {
 public:
  NoMacro();
  ~NoMacro();

 private:
  int member_;
};

// Class with already-explicit deleted declarations - should not be modified
class AlreadyExplicit {
 public:
  AlreadyExplicit();
  ~AlreadyExplicit();

  AlreadyExplicit(const AlreadyExplicit&) = delete;
  AlreadyExplicit& operator=(const AlreadyExplicit&) = delete;

 private:
  int member_;
};

// Class with DISALLOW_IMPLICIT_CONSTRUCTORS - different macro, not handled
// (This macro includes DISALLOW_COPY_AND_ASSIGN internally, but we don't
// parse macro expansions, so the text search won't find it)
class ImplicitConstructors {
 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ImplicitConstructors);
};

// Class with explicit (non-deleted) copy constructor - should not transform
// (The tool should detect the existing copy constructor)
class ExplicitCopyCtor {
 public:
  ExplicitCopyCtor();
  ExplicitCopyCtor(const ExplicitCopyCtor& other);  // Not deleted

 private:
  int member_;
};

// Class with only DISALLOW_COPY (separate macro) - not DISALLOW_COPY_AND_ASSIGN
class OnlyDisallowCopy {
 public:
  OnlyDisallowCopy();

 private:
  DISALLOW_COPY(OnlyDisallowCopy);
};

// Class with only DISALLOW_ASSIGN (separate macro) - not DISALLOW_COPY_AND_ASSIGN
class OnlyDisallowAssign {
 public:
  OnlyDisallowAssign();

 private:
  DISALLOW_ASSIGN(OnlyDisallowAssign);
};

// Struct without the macro - should not be modified
struct PlainStruct {
  PlainStruct();
  int value;
};
