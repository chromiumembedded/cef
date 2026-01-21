// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

// Test file for DISALLOW_COPY_AND_ASSIGN transformation
// Path contains /cef/ to pass path filter

// Mimic CEF's macro definitions
#define DISALLOW_COPY(TypeName) TypeName(const TypeName&) = delete
#define DISALLOW_ASSIGN(TypeName) TypeName& operator=(const TypeName&) = delete
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  DISALLOW_COPY(TypeName);                 \
  DISALLOW_ASSIGN(TypeName)

// Test 1: Basic class with constructor and destructor in public
class BasicClass {
 public:
  BasicClass();
  ~BasicClass();

  BasicClass(const BasicClass&) = delete;
  BasicClass& operator=(const BasicClass&) = delete;

  void DoSomething();

 private:
  int member_;
};

// Test 2: Class where private section ONLY has the macro (should remove private:)
class OnlyMacroInPrivate {
 public:
  OnlyMacroInPrivate();
  ~OnlyMacroInPrivate();

  OnlyMacroInPrivate(const OnlyMacroInPrivate&) = delete;
  OnlyMacroInPrivate& operator=(const OnlyMacroInPrivate&) = delete;

  void Method();
};

// Test 3: Class with protected section before empty private
class WithProtected {
 public:
  WithProtected();

  WithProtected(const WithProtected&) = delete;
  WithProtected& operator=(const WithProtected&) = delete;

 protected:
  int protected_member_;
};

// Test 4: Class with multiple constructors (insert after last)
class MultipleConstructors {
 public:
  MultipleConstructors();
  MultipleConstructors(int x);
  MultipleConstructors(int x, int y);
  ~MultipleConstructors();

  MultipleConstructors(const MultipleConstructors&) = delete;
  MultipleConstructors& operator=(const MultipleConstructors&) = delete;
};

// Test 5: Class with only destructor in public (no constructor)
class OnlyDestructor {
 public:
  ~OnlyDestructor();

  OnlyDestructor(const OnlyDestructor&) = delete;
  OnlyDestructor& operator=(const OnlyDestructor&) = delete;

  void Method();
};

// Test 6: Class with inline constructor/destructor
class InlineCtorDtor {
 public:
  InlineCtorDtor() {}
  ~InlineCtorDtor() {}

  InlineCtorDtor(const InlineCtorDtor&) = delete;
  InlineCtorDtor& operator=(const InlineCtorDtor&) = delete;
};

// Test 7: Class with other members in private section (keep private:)
class OtherPrivateMembers {
 public:
  OtherPrivateMembers();

  OtherPrivateMembers(const OtherPrivateMembers&) = delete;
  OtherPrivateMembers& operator=(const OtherPrivateMembers&) = delete;

 private:
  int member_;
};

// Test 8: Nested class - both should be transformed independently
class OuterClass {
 public:
  OuterClass();

  OuterClass(const OuterClass&) = delete;
  OuterClass& operator=(const OuterClass&) = delete;

  class InnerClass {
   public:
    InnerClass();

    InnerClass(const InnerClass&) = delete;
    InnerClass& operator=(const InnerClass&) = delete;
  };
};

// Test 9: Template class
template <typename T>
class TemplateClass {
 public:
  TemplateClass();

  TemplateClass(const TemplateClass&) = delete;
  TemplateClass& operator=(const TemplateClass&) = delete;

 private:
  T value_;
};

// Test 10: Class with macro in protected section with other members (keep protected:)
class MacroInProtectedWithMembers {
 public:
  MacroInProtectedWithMembers();

  MacroInProtectedWithMembers(const MacroInProtectedWithMembers&) = delete;
  MacroInProtectedWithMembers& operator=(const MacroInProtectedWithMembers&) = delete;

 protected:
  int protected_member_;
};

// Test 11: Class with macro ONLY in protected section (should remove protected:)
class OnlyMacroInProtected {
 public:
  OnlyMacroInProtected();

  OnlyMacroInProtected(const OnlyMacroInProtected&) = delete;
  OnlyMacroInProtected& operator=(const OnlyMacroInProtected&) = delete;
};

// Test 12: Class with no public section (should create one)
class NoPublicSection {
 public:
  NoPublicSection(const NoPublicSection&) = delete;
  NoPublicSection& operator=(const NoPublicSection&) = delete;

 private:
  NoPublicSection();
  int member_;
};

// Test 13: Struct (default public access)
struct MyStruct {
  MyStruct();
  ~MyStruct();

  MyStruct(const MyStruct&) = delete;
  MyStruct& operator=(const MyStruct&) = delete;

  int value;
};

// Test 14: Class in namespace
namespace myns {
namespace inner {

class NamespacedClass {
 public:
  NamespacedClass();
  ~NamespacedClass();

  NamespacedClass(const NamespacedClass&) = delete;
  NamespacedClass& operator=(const NamespacedClass&) = delete;
};

}  // namespace inner
}  // namespace myns

// Test 15: Class with defaulted constructor
class DefaultedCtor {
 public:
  DefaultedCtor() = default;
  ~DefaultedCtor() = default;

  DefaultedCtor(const DefaultedCtor&) = delete;
  DefaultedCtor& operator=(const DefaultedCtor&) = delete;
};

// Test 16: Class with virtual destructor
class VirtualDtor {
 public:
  VirtualDtor();
  virtual ~VirtualDtor();

  VirtualDtor(const VirtualDtor&) = delete;
  VirtualDtor& operator=(const VirtualDtor&) = delete;

  virtual void Method() = 0;
};

// Test 17: Class with macro having trailing semicolon (common mistake)
class TrailingSemicolon {
 public:
  TrailingSemicolon();

  TrailingSemicolon(const TrailingSemicolon&) = delete;
  TrailingSemicolon& operator=(const TrailingSemicolon&) = delete;
};

// Test 18: Class with comments around macro
class WithComments {
 public:
  WithComments();
  ~WithComments();

  WithComments(const WithComments&) = delete;
  WithComments& operator=(const WithComments&) = delete;

 private:
  // This class should not be copied
  // End of class
};

// Test 19: Base class for inheritance test
class BaseClass {
 public:
  virtual ~BaseClass() = default;
  virtual void Method() = 0;
};

// Test 20: Class with inheritance, only ctor (no explicit dtor),
// and other public members between ctor and private section.
// This mimics the ClientApp structure that exposed the implicit declaration bug.
class DerivedClass : public BaseClass {
 public:
  DerivedClass();

  DerivedClass(const DerivedClass&) = delete;
  DerivedClass& operator=(const DerivedClass&) = delete;

  enum Mode {
    ModeA,
    ModeB,
  };

  static Mode GetMode();
  void Method() override;

 private:
  void PrivateHelper();
};
