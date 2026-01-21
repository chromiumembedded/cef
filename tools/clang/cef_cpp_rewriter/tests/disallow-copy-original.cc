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

  void DoSomething();

 private:
  int member_;
  DISALLOW_COPY_AND_ASSIGN(BasicClass);
};

// Test 2: Class where private section ONLY has the macro (should remove private:)
class OnlyMacroInPrivate {
 public:
  OnlyMacroInPrivate();
  ~OnlyMacroInPrivate();

  void Method();

 private:
  DISALLOW_COPY_AND_ASSIGN(OnlyMacroInPrivate);
};

// Test 3: Class with protected section before empty private
class WithProtected {
 public:
  WithProtected();

 protected:
  int protected_member_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WithProtected);
};

// Test 4: Class with multiple constructors (insert after last)
class MultipleConstructors {
 public:
  MultipleConstructors();
  MultipleConstructors(int x);
  MultipleConstructors(int x, int y);
  ~MultipleConstructors();

 private:
  DISALLOW_COPY_AND_ASSIGN(MultipleConstructors);
};

// Test 5: Class with only destructor in public (no constructor)
class OnlyDestructor {
 public:
  ~OnlyDestructor();

  void Method();

 private:
  DISALLOW_COPY_AND_ASSIGN(OnlyDestructor);
};

// Test 6: Class with inline constructor/destructor
class InlineCtorDtor {
 public:
  InlineCtorDtor() {}
  ~InlineCtorDtor() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(InlineCtorDtor);
};

// Test 7: Class with other members in private section (keep private:)
class OtherPrivateMembers {
 public:
  OtherPrivateMembers();

 private:
  int member_;
  DISALLOW_COPY_AND_ASSIGN(OtherPrivateMembers);
};

// Test 8: Nested class - both should be transformed independently
class OuterClass {
 public:
  OuterClass();

  class InnerClass {
   public:
    InnerClass();

   private:
    DISALLOW_COPY_AND_ASSIGN(InnerClass);
  };

 private:
  DISALLOW_COPY_AND_ASSIGN(OuterClass);
};

// Test 9: Template class
template <typename T>
class TemplateClass {
 public:
  TemplateClass();

 private:
  T value_;
  DISALLOW_COPY_AND_ASSIGN(TemplateClass);
};

// Test 10: Class with macro in protected section with other members (keep protected:)
class MacroInProtectedWithMembers {
 public:
  MacroInProtectedWithMembers();

 protected:
  int protected_member_;
  DISALLOW_COPY_AND_ASSIGN(MacroInProtectedWithMembers);
};

// Test 11: Class with macro ONLY in protected section (should remove protected:)
class OnlyMacroInProtected {
 public:
  OnlyMacroInProtected();

 protected:
  DISALLOW_COPY_AND_ASSIGN(OnlyMacroInProtected);
};

// Test 12: Class with no public section (should create one)
class NoPublicSection {
 private:
  NoPublicSection();
  int member_;
  DISALLOW_COPY_AND_ASSIGN(NoPublicSection);
};

// Test 13: Struct (default public access)
struct MyStruct {
  MyStruct();
  ~MyStruct();

  int value;

 private:
  DISALLOW_COPY_AND_ASSIGN(MyStruct);
};

// Test 14: Class in namespace
namespace myns {
namespace inner {

class NamespacedClass {
 public:
  NamespacedClass();
  ~NamespacedClass();

 private:
  DISALLOW_COPY_AND_ASSIGN(NamespacedClass);
};

}  // namespace inner
}  // namespace myns

// Test 15: Class with defaulted constructor
class DefaultedCtor {
 public:
  DefaultedCtor() = default;
  ~DefaultedCtor() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultedCtor);
};

// Test 16: Class with virtual destructor
class VirtualDtor {
 public:
  VirtualDtor();
  virtual ~VirtualDtor();

  virtual void Method() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(VirtualDtor);
};

// Test 17: Class with macro having trailing semicolon (common mistake)
class TrailingSemicolon {
 public:
  TrailingSemicolon();

 private:
  DISALLOW_COPY_AND_ASSIGN(TrailingSemicolon);
};

// Test 18: Class with comments around macro
class WithComments {
 public:
  WithComments();
  ~WithComments();

 private:
  // This class should not be copied
  DISALLOW_COPY_AND_ASSIGN(WithComments);
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

  enum Mode {
    ModeA,
    ModeB,
  };

  static Mode GetMode();
  void Method() override;

 private:
  void PrivateHelper();

  DISALLOW_COPY_AND_ASSIGN(DerivedClass);
};
