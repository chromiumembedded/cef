// Copyright (c) 2025 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ---------------------------------------------------------------------------
//
// This file was ported from Chromium source file:
// base/memory/weak_ptr_unittest.cc
// Chromium commit: 70909488d8cc2
//
// ---------------------------------------------------------------------------

#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include "include/base/cef_bind.h"
#include "include/base/cef_callback.h"
#include "include/base/cef_weak_ptr.h"
#include "include/cef_task.h"
#include "include/cef_thread.h"
#include "include/cef_waitable_event.h"
#include "include/wrapper/cef_closure_task.h"
#include "tests/gtest/include/gtest/gtest.h"

namespace base {

namespace {

WeakPtr<int> PassThru(WeakPtr<int> ptr) {
  return ptr;
}

// Helper class to create objects on a different thread using CefThread.
template <class T>
class OffThreadObjectCreator {
 public:
  static T* NewObject() {
    T* result = nullptr;
    CefRefPtr<CefWaitableEvent> event =
        CefWaitableEvent::CreateWaitableEvent(false, false);
    CefRefPtr<CefThread> creator_thread =
        CefThread::CreateThread("creator_thread");
    EXPECT_TRUE(creator_thread.get());
    EXPECT_TRUE(creator_thread->GetTaskRunner().get());
    creator_thread->GetTaskRunner()->PostTask(CefCreateClosureTask(
        BindOnce(&OffThreadObjectCreator::CreateObject, &result, event)));
    event->Wait();
    creator_thread->Stop();
    EXPECT_TRUE(result);
    return result;
  }

 private:
  static void CreateObject(T** result, CefRefPtr<CefWaitableEvent> event) {
    *result = new T;
    event->Signal();
  }
};

struct Base {
  std::string member;
};
struct Derived : public Base {};

struct TargetBase {};

struct Target : public TargetBase {
  virtual ~Target() = default;
  WeakPtr<Target> AsWeakPtr() { return weak_ptr_factory_.GetWeakPtr(); }

 private:
  WeakPtrFactory<Target> weak_ptr_factory_{this};
};

struct DerivedTarget : public Target {};

// A class inheriting from Target and defining a nested type called 'Base'.
// To guard against strange compilation errors.
struct DerivedTargetWithNestedBase : public Target {
  using Base = void;
};

// A struct with a virtual destructor.
struct VirtualDestructor {
  virtual ~VirtualDestructor() = default;
};

// A class inheriting from Target where Target is not the first base, and where
// the first base has a virtual method table. This creates a structure where the
// Target base is not positioned at the beginning of
// DerivedTargetMultipleInheritance.
struct DerivedTargetMultipleInheritance : public VirtualDestructor,
                                          public Target {};

struct Arrow {
  WeakPtr<Target> target;
};
struct TargetWithFactory : public Target {
  TargetWithFactory() = default;
  WeakPtrFactory<Target> factory{this};
};

// Helper class to create and destroy weak pointer copies
// and delete objects on a background thread using CefThread.
class BackgroundThread {
 public:
  BackgroundThread() {
    thread_ = CefThread::CreateThread("owner_thread");
    EXPECT_TRUE(thread_.get());
    EXPECT_TRUE(thread_->GetTaskRunner().get());
  }

  ~BackgroundThread() { Stop(); }

  void Start() {
    // Thread is created in constructor, this is a no-op for compatibility.
  }

  void Stop() {
    if (thread_.get()) {
      thread_->Stop();
      thread_ = nullptr;
    }
  }

  void CreateArrowFromTarget(Arrow** arrow, Target* target) {
    CefRefPtr<CefWaitableEvent> completion =
        CefWaitableEvent::CreateWaitableEvent(false, false);
    thread_->GetTaskRunner()->PostTask(CefCreateClosureTask(
        BindOnce(&BackgroundThread::DoCreateArrowFromTarget, arrow, target,
                 completion)));
    completion->Wait();
  }

  void CreateArrowFromArrow(Arrow** arrow, const Arrow* other) {
    CefRefPtr<CefWaitableEvent> completion =
        CefWaitableEvent::CreateWaitableEvent(false, false);
    thread_->GetTaskRunner()->PostTask(CefCreateClosureTask(BindOnce(
        &BackgroundThread::DoCreateArrowFromArrow, arrow, other, completion)));
    completion->Wait();
  }

  void DeleteTarget(Target* object) {
    CefRefPtr<CefWaitableEvent> completion =
        CefWaitableEvent::CreateWaitableEvent(false, false);
    thread_->GetTaskRunner()->PostTask(CefCreateClosureTask(
        BindOnce(&BackgroundThread::DoDeleteTarget, object, completion)));
    completion->Wait();
  }

  void CopyAndAssignArrow(Arrow* object) {
    CefRefPtr<CefWaitableEvent> completion =
        CefWaitableEvent::CreateWaitableEvent(false, false);
    thread_->GetTaskRunner()->PostTask(CefCreateClosureTask(
        BindOnce(&BackgroundThread::DoCopyAndAssignArrow, object, completion)));
    completion->Wait();
  }

  void CopyAndAssignArrowBase(Arrow* object) {
    CefRefPtr<CefWaitableEvent> completion =
        CefWaitableEvent::CreateWaitableEvent(false, false);
    thread_->GetTaskRunner()->PostTask(CefCreateClosureTask(BindOnce(
        &BackgroundThread::DoCopyAndAssignArrowBase, object, completion)));
    completion->Wait();
  }

  void DeleteArrow(Arrow* object) {
    CefRefPtr<CefWaitableEvent> completion =
        CefWaitableEvent::CreateWaitableEvent(false, false);
    thread_->GetTaskRunner()->PostTask(CefCreateClosureTask(
        BindOnce(&BackgroundThread::DoDeleteArrow, object, completion)));
    completion->Wait();
  }

  Target* DeRef(const Arrow* arrow) {
    CefRefPtr<CefWaitableEvent> completion =
        CefWaitableEvent::CreateWaitableEvent(false, false);
    Target* result = nullptr;
    thread_->GetTaskRunner()->PostTask(CefCreateClosureTask(
        BindOnce(&BackgroundThread::DoDeRef, arrow, &result, completion)));
    completion->Wait();
    return result;
  }

 private:
  static void DoCreateArrowFromArrow(Arrow** arrow,
                                     const Arrow* other,
                                     CefRefPtr<CefWaitableEvent> completion) {
    *arrow = new Arrow;
    **arrow = *other;
    completion->Signal();
  }

  static void DoCreateArrowFromTarget(Arrow** arrow,
                                      Target* target,
                                      CefRefPtr<CefWaitableEvent> completion) {
    *arrow = new Arrow;
    (*arrow)->target = target->AsWeakPtr();
    completion->Signal();
  }

  static void DoDeRef(const Arrow* arrow,
                      Target** result,
                      CefRefPtr<CefWaitableEvent> completion) {
    *result = arrow->target.get();
    completion->Signal();
  }

  static void DoDeleteTarget(Target* object,
                             CefRefPtr<CefWaitableEvent> completion) {
    delete object;
    completion->Signal();
  }

  static void DoCopyAndAssignArrow(Arrow* object,
                                   CefRefPtr<CefWaitableEvent> completion) {
    // Copy constructor.
    Arrow a = *object;
    // Assignment operator.
    *object = a;
    completion->Signal();
  }

  static void DoCopyAndAssignArrowBase(Arrow* object,
                                       CefRefPtr<CefWaitableEvent> completion) {
    // Copy constructor.
    WeakPtr<TargetBase> b = object->target;
    // Assignment operator.
    WeakPtr<TargetBase> c;
    c = object->target;
    completion->Signal();
  }

  static void DoDeleteArrow(Arrow* object,
                            CefRefPtr<CefWaitableEvent> completion) {
    delete object;
    completion->Signal();
  }

  CefRefPtr<CefThread> thread_;
};

}  // namespace

TEST(WeakPtrFactoryTest, Basic) {
  int data;
  WeakPtrFactory<int> factory(&data);
  WeakPtr<int> ptr = factory.GetWeakPtr();
  EXPECT_EQ(&data, ptr.get());
}

TEST(WeakPtrFactoryTest, Comparison) {
  int data;
  WeakPtrFactory<int> factory(&data);
  WeakPtr<int> ptr = factory.GetWeakPtr();
  WeakPtr<int> ptr2 = ptr;
  EXPECT_EQ(ptr.get(), ptr2.get());
}

TEST(WeakPtrFactoryTest, Move) {
  int data;
  WeakPtrFactory<int> factory(&data);
  WeakPtr<int> ptr = factory.GetWeakPtr();
  WeakPtr<int> ptr2 = factory.GetWeakPtr();
  WeakPtr<int> ptr3 = std::move(ptr2);
  EXPECT_NE(ptr.get(), ptr2.get());  // NOLINT(bugprone-use-after-move)
  EXPECT_EQ(ptr.get(), ptr3.get());
}

TEST(WeakPtrFactoryTest, OutOfScope) {
  WeakPtr<int> ptr;
  EXPECT_EQ(nullptr, ptr.get());
  {
    int data;
    WeakPtrFactory<int> factory(&data);
    ptr = factory.GetWeakPtr();
  }
  EXPECT_EQ(nullptr, ptr.get());
}

TEST(WeakPtrFactoryTest, Multiple) {
  WeakPtr<int> a, b;
  {
    int data;
    WeakPtrFactory<int> factory(&data);
    a = factory.GetWeakPtr();
    b = factory.GetWeakPtr();
    EXPECT_EQ(&data, a.get());
    EXPECT_EQ(&data, b.get());
  }
  EXPECT_EQ(nullptr, a.get());
  EXPECT_EQ(nullptr, b.get());
}

TEST(WeakPtrFactoryTest, MultipleStaged) {
  WeakPtr<int> a;
  {
    int data;
    WeakPtrFactory<int> factory(&data);
    a = factory.GetWeakPtr();
    {
      WeakPtr<int> b = factory.GetWeakPtr();
    }
    EXPECT_NE(nullptr, a.get());
  }
  EXPECT_EQ(nullptr, a.get());
}

TEST(WeakPtrFactoryTest, Dereference) {
  Base data;
  data.member = "123456";
  WeakPtrFactory<Base> factory(&data);
  WeakPtr<Base> ptr = factory.GetWeakPtr();
  EXPECT_EQ(&data, ptr.get());
  EXPECT_EQ(data.member, (*ptr).member);
  EXPECT_EQ(data.member, ptr->member);
}

TEST(WeakPtrFactoryTest, UpCast) {
  Derived data;
  WeakPtrFactory<Derived> factory(&data);
  WeakPtr<Base> ptr = factory.GetWeakPtr();
  ptr = factory.GetWeakPtr();
  EXPECT_EQ(ptr.get(), &data);
}

TEST(WeakPtrTest, ConstructFromNullptr) {
  WeakPtr<int> ptr = PassThru(nullptr);
  EXPECT_EQ(nullptr, ptr.get());
}

TEST(WeakPtrFactoryTest, BooleanTesting) {
  int data;
  WeakPtrFactory<int> factory(&data);

  WeakPtr<int> ptr_to_an_instance = factory.GetWeakPtr();
  EXPECT_TRUE(ptr_to_an_instance);
  EXPECT_FALSE(!ptr_to_an_instance);

  if (ptr_to_an_instance) {
  } else {
    ADD_FAILURE() << "Pointer to an instance should result in true.";
  }

  if (!ptr_to_an_instance) {  // check for operator!().
    ADD_FAILURE() << "Pointer to an instance should result in !x being false.";
  }

  WeakPtr<int> null_ptr;
  EXPECT_FALSE(null_ptr);
  EXPECT_TRUE(!null_ptr);

  if (null_ptr) {
    ADD_FAILURE() << "Null pointer should result in false.";
  }

  if (!null_ptr) {  // check for operator!().
  } else {
    ADD_FAILURE() << "Null pointer should result in !x being true.";
  }
}

TEST(WeakPtrFactoryTest, ComparisonToNull) {
  int data;
  WeakPtrFactory<int> factory(&data);

  WeakPtr<int> ptr_to_an_instance = factory.GetWeakPtr();
  EXPECT_NE(nullptr, ptr_to_an_instance);
  EXPECT_NE(ptr_to_an_instance, nullptr);

  WeakPtr<int> null_ptr;
  EXPECT_EQ(null_ptr, nullptr);
  EXPECT_EQ(nullptr, null_ptr);
}

struct ReallyBaseClass {};
struct BaseClass : ReallyBaseClass {
  virtual ~BaseClass() = default;
  void VirtualMethod() {}
};
struct OtherBaseClass {
  virtual ~OtherBaseClass() = default;
  virtual void VirtualMethod() {}
};
struct WithWeak final : BaseClass, OtherBaseClass {
  WeakPtrFactory<WithWeak> factory{this};
};

TEST(WeakPtrTest, ConversionOffsetsPointer) {
  WithWeak with;
  WeakPtr<WithWeak> ptr(with.factory.GetWeakPtr());
  {
    // Copy construction.
    WeakPtr<OtherBaseClass> base_ptr(ptr);
    EXPECT_EQ(static_cast<WithWeak*>(&*base_ptr), &with);
  }
  {
    // Move construction.
    WeakPtr<OtherBaseClass> base_ptr(std::move(ptr));
    EXPECT_EQ(static_cast<WithWeak*>(&*base_ptr), &with);
  }

  // WeakPtr doesn't have conversion operators for assignment.
}

TEST(WeakPtrTest, InvalidateWeakPtrs) {
  int data;
  WeakPtrFactory<int> factory(&data);
  WeakPtr<int> ptr = factory.GetWeakPtr();
  EXPECT_EQ(&data, ptr.get());
  EXPECT_TRUE(factory.HasWeakPtrs());
  factory.InvalidateWeakPtrs();
  EXPECT_EQ(nullptr, ptr.get());
  EXPECT_FALSE(factory.HasWeakPtrs());

  // Test that the factory can create new weak pointers after a
  // InvalidateWeakPtrs call, and they remain valid until the next
  // InvalidateWeakPtrs call.
  WeakPtr<int> ptr2 = factory.GetWeakPtr();
  EXPECT_EQ(&data, ptr2.get());
  EXPECT_TRUE(factory.HasWeakPtrs());
  factory.InvalidateWeakPtrs();
  EXPECT_EQ(nullptr, ptr2.get());
  EXPECT_FALSE(factory.HasWeakPtrs());
}

// Note: The EXPECT_DCHECK_DEATH portion is skipped; only the non-death portion
// of this test is ported.
TEST(WeakPtrTest, InvalidateWeakPtrsAndDoom) {
  int data;
  WeakPtrFactory<int> factory(&data);
  WeakPtr<int> ptr = factory.GetWeakPtr();
  EXPECT_EQ(&data, ptr.get());
  EXPECT_TRUE(factory.HasWeakPtrs());
  factory.InvalidateWeakPtrsAndDoom();
  EXPECT_EQ(nullptr, ptr.get());
  EXPECT_FALSE(factory.HasWeakPtrs());

  // EXPECT_DCHECK_DEATH portion skipped - death tests not portable to CEF
}

// Tests that WasInvalidated() is true only for invalidated WeakPtrs (not
// nullptr) and doesn't DCHECK (e.g. because of a dereference attempt).
TEST(WeakPtrTest, WasInvalidatedByFactoryDestruction) {
  WeakPtr<int> ptr;
  EXPECT_FALSE(ptr.WasInvalidated());

  // Test |data| destroyed. That is, the typical pattern when |data| (and its
  // associated factory) go out of scope.
  {
    int data = 0;
    WeakPtrFactory<int> factory(&data);
    ptr = factory.GetWeakPtr();

    // Verify that a live WeakPtr is not reported as Invalidated.
    EXPECT_FALSE(ptr.WasInvalidated());
  }

  // Checking validity shouldn't read beyond the stack frame.
  EXPECT_TRUE(ptr.WasInvalidated());
  ptr = nullptr;
  EXPECT_FALSE(ptr.WasInvalidated());
}

// As above, but testing InvalidateWeakPtrs().
TEST(WeakPtrTest, WasInvalidatedByInvalidateWeakPtrs) {
  int data = 0;
  WeakPtrFactory<int> factory(&data);
  WeakPtr<int> ptr = factory.GetWeakPtr();
  EXPECT_FALSE(ptr.WasInvalidated());
  factory.InvalidateWeakPtrs();
  EXPECT_TRUE(ptr.WasInvalidated());
  ptr = nullptr;
  EXPECT_FALSE(ptr.WasInvalidated());
}

// A WeakPtr should not be reported as 'invalidated' if nullptr was assigned to
// it.
TEST(WeakPtrTest, WasInvalidatedWhilstNull) {
  int data = 0;
  WeakPtrFactory<int> factory(&data);
  WeakPtr<int> ptr = factory.GetWeakPtr();
  EXPECT_FALSE(ptr.WasInvalidated());
  ptr = nullptr;
  EXPECT_FALSE(ptr.WasInvalidated());
  factory.InvalidateWeakPtrs();
  EXPECT_FALSE(ptr.WasInvalidated());
}

TEST(WeakPtrTest, MaybeValidOnSameSequence) {
  int data;
  WeakPtrFactory<int> factory(&data);
  WeakPtr<int> ptr = factory.GetWeakPtr();
  EXPECT_TRUE(ptr.MaybeValid());
  factory.InvalidateWeakPtrs();
  // Since InvalidateWeakPtrs() ran on this sequence, MaybeValid() should be
  // false.
  EXPECT_FALSE(ptr.MaybeValid());
}

TEST(WeakPtrTest, MaybeValidOnOtherSequence) {
  int data;
  WeakPtrFactory<int> factory(&data);
  WeakPtr<int> ptr = factory.GetWeakPtr();
  EXPECT_TRUE(ptr.MaybeValid());

  CefRefPtr<CefThread> other_thread = CefThread::CreateThread("other_thread");
  ASSERT_TRUE(other_thread.get());
  ASSERT_TRUE(other_thread->GetTaskRunner().get());

  other_thread->GetTaskRunner()->PostTask(CefCreateClosureTask(BindOnce(
      [](WeakPtr<int> ptr) {
        // Check that MaybeValid() _eventually_ returns false.
        const auto timeout = std::chrono::seconds(1);
        const auto begin = std::chrono::steady_clock::now();
        while (ptr.MaybeValid() &&
               (std::chrono::steady_clock::now() - begin) < timeout) {
          std::this_thread::yield();
        }
        EXPECT_FALSE(ptr.MaybeValid());
      },
      ptr)));
  factory.InvalidateWeakPtrs();
  // Stop() will wait for the posted task to complete.
  other_thread->Stop();
}

TEST(WeakPtrTest, HasWeakPtrs) {
  int data;
  WeakPtrFactory<int> factory(&data);
  {
    WeakPtr<int> ptr = factory.GetWeakPtr();
    EXPECT_TRUE(factory.HasWeakPtrs());
  }
  EXPECT_FALSE(factory.HasWeakPtrs());
}

TEST(WeakPtrTest, ObjectAndWeakPtrOnDifferentThreads) {
  // Test that it is OK to create an object that supports WeakPtr on one thread,
  // but use it on another.  This tests that we do not trip runtime checks that
  // ensure that a WeakPtr is not used by multiple threads.
  std::unique_ptr<Target> target(OffThreadObjectCreator<Target>::NewObject());
  WeakPtr<Target> weak_ptr = target->AsWeakPtr();
  EXPECT_EQ(target.get(), weak_ptr.get());
}

TEST(WeakPtrTest, WeakPtrInitiateAndUseOnDifferentThreads) {
  // Test that it is OK to create an object that has a WeakPtr member on one
  // thread, but use it on another.  This tests that we do not trip runtime
  // checks that ensure that a WeakPtr is not used by multiple threads.
  std::unique_ptr<Arrow> arrow(OffThreadObjectCreator<Arrow>::NewObject());
  Target target;
  arrow->target = target.AsWeakPtr();
  EXPECT_EQ(&target, arrow->target.get());
}

TEST(WeakPtrTest, MoveOwnershipImplicitly) {
  // Move object ownership to another thread by releasing all weak pointers
  // on the original thread first, and then establish WeakPtr on a different
  // thread.
  BackgroundThread background;
  background.Start();

  Target* target = new Target();
  {
    WeakPtr<Target> weak_ptr = target->AsWeakPtr();
    // Main thread deletes the WeakPtr, then the thread ownership of the
    // object can be implicitly moved.
  }
  Arrow* arrow;

  // Background thread creates WeakPtr(and implicitly owns the object).
  background.CreateArrowFromTarget(&arrow, target);
  EXPECT_EQ(background.DeRef(arrow), target);

  {
    // Main thread creates another WeakPtr, but this does not trigger implicitly
    // thread ownership move.
    Arrow scoped_arrow;
    scoped_arrow.target = target->AsWeakPtr();

    // The new WeakPtr is owned by background thread.
    EXPECT_EQ(target, background.DeRef(&scoped_arrow));
  }

  // Target can only be deleted on background thread.
  background.DeleteTarget(target);
  background.DeleteArrow(arrow);
}

TEST(WeakPtrTest, MoveOwnershipOfUnreferencedObject) {
  BackgroundThread background;
  background.Start();

  Arrow* arrow;
  {
    Target target;
    // Background thread creates WeakPtr.
    background.CreateArrowFromTarget(&arrow, &target);

    // Bind to background thread.
    EXPECT_EQ(&target, background.DeRef(arrow));

    // Release the only WeakPtr.
    arrow->target.reset();

    // Now we should be able to create a new reference from this thread.
    arrow->target = target.AsWeakPtr();

    // Re-bind to main thread.
    EXPECT_EQ(&target, arrow->target.get());

    // And the main thread can now delete the target.
  }

  delete arrow;
}

TEST(WeakPtrTest, MoveOwnershipAfterInvalidate) {
  BackgroundThread background;
  background.Start();

  Arrow arrow;
  auto target = std::make_unique<TargetWithFactory>();

  // Bind to main thread.
  arrow.target = target->factory.GetWeakPtr();
  EXPECT_EQ(target.get(), arrow.target.get());

  target->factory.InvalidateWeakPtrs();
  EXPECT_EQ(nullptr, arrow.target.get());

  arrow.target = target->factory.GetWeakPtr();
  // Re-bind to background thread.
  EXPECT_EQ(target.get(), background.DeRef(&arrow));

  // And the background thread can now delete the target.
  background.DeleteTarget(target.release());
}

TEST(WeakPtrTest, MainThreadRefOutlivesBackgroundThreadRef) {
  // Originating thread has a WeakPtr that outlives others.
  // - Main thread creates a WeakPtr
  // - Background thread creates a WeakPtr copy from the one in main thread
  // - Destruct the WeakPtr on background thread
  // - Destruct the WeakPtr on main thread
  BackgroundThread background;
  background.Start();

  Target target;
  Arrow arrow;
  arrow.target = target.AsWeakPtr();

  Arrow* arrow_copy;
  background.CreateArrowFromArrow(&arrow_copy, &arrow);
  EXPECT_EQ(arrow_copy->target.get(), &target);
  background.DeleteArrow(arrow_copy);
}

TEST(WeakPtrTest, BackgroundThreadRefOutlivesMainThreadRef) {
  // Originating thread drops all references before another thread.
  // - Main thread creates a WeakPtr and passes copy to background thread
  // - Destruct the pointer on main thread
  // - Destruct the pointer on background thread
  BackgroundThread background;
  background.Start();

  Target target;
  Arrow* arrow_copy;
  {
    Arrow arrow;
    arrow.target = target.AsWeakPtr();
    background.CreateArrowFromArrow(&arrow_copy, &arrow);
  }
  EXPECT_EQ(arrow_copy->target.get(), &target);
  background.DeleteArrow(arrow_copy);
}

TEST(WeakPtrTest, OwnerThreadDeletesObject) {
  // Originating thread invalidates WeakPtrs while its held by other thread.
  // - Main thread creates WeakPtr and passes Copy to background thread
  // - Object gets destroyed on main thread
  //   (invalidates WeakPtr on background thread)
  // - WeakPtr gets destroyed on Thread B
  BackgroundThread background;
  background.Start();
  Arrow* arrow_copy;
  {
    Target target;
    Arrow arrow;
    arrow.target = target.AsWeakPtr();
    background.CreateArrowFromArrow(&arrow_copy, &arrow);
  }
  EXPECT_EQ(nullptr, arrow_copy->target.get());
  background.DeleteArrow(arrow_copy);
}

TEST(WeakPtrTest, NonOwnerThreadCanCopyAndAssignWeakPtr) {
  // Main thread creates a Target object.
  Target target;
  // Main thread creates an arrow referencing the Target.
  Arrow* arrow = new Arrow();
  arrow->target = target.AsWeakPtr();

  // Background can copy and assign arrow (as well as the WeakPtr inside).
  BackgroundThread background;
  background.Start();
  background.CopyAndAssignArrow(arrow);
  background.DeleteArrow(arrow);
}

TEST(WeakPtrTest, NonOwnerThreadCanCopyAndAssignWeakPtrBase) {
  // Main thread creates a Target object.
  Target target;
  // Main thread creates an arrow referencing the Target.
  Arrow* arrow = new Arrow();
  arrow->target = target.AsWeakPtr();

  // Background can copy and assign arrow's WeakPtr to a base class WeakPtr.
  BackgroundThread background;
  background.Start();
  background.CopyAndAssignArrowBase(arrow);
  background.DeleteArrow(arrow);
}

TEST(WeakPtrTest, NonOwnerThreadCanDeleteWeakPtr) {
  // Main thread creates a Target object.
  Target target;
  // Main thread creates an arrow referencing the Target.
  Arrow* arrow = new Arrow();
  arrow->target = target.AsWeakPtr();

  // Background can delete arrow (as well as the WeakPtr inside).
  BackgroundThread background;
  background.Start();
  background.DeleteArrow(arrow);
}

TEST(WeakPtrTest, ConstUpCast) {
  Target target;

  // WeakPtrs can upcast from non-const T to const T.
  WeakPtr<const Target> const_weak_ptr = target.AsWeakPtr();

  // WeakPtrs don't enable conversion from const T to nonconst T.
  static_assert(
      !std::is_constructible_v<WeakPtr<Target>, WeakPtr<const Target>>);
}

// Note: This test uses EXPECT_EQ to compare const char* pointers, which relies
// on string pooling to merge identical string literals to the same address.
// On Windows/MSVC, this requires the /GF compiler flag (enabled in CEF's
// cmake/cef_variables.cmake.in and bazel/win/variables.bzl). GCC and Clang
// enable string merging by default via -fmerge-constants.
TEST(WeakPtrTest, ConstGetWeakPtr) {
  struct TestTarget {
    const char* Method() const { return "const method"; }
    const char* Method() { return "non-const method"; }

    WeakPtrFactory<TestTarget> weak_ptr_factory{this};
  } non_const_test_target;

  const TestTarget& const_test_target = non_const_test_target;

  EXPECT_EQ(const_test_target.weak_ptr_factory.GetWeakPtr()->Method(),
            "const method");
  EXPECT_EQ(non_const_test_target.weak_ptr_factory.GetWeakPtr()->Method(),
            "non-const method");
  EXPECT_EQ(const_test_target.weak_ptr_factory.GetMutableWeakPtr()->Method(),
            "non-const method");
}

TEST(WeakPtrTest, GetMutableWeakPtr) {
  struct TestStruct {
    int member = 0;
    WeakPtrFactory<TestStruct> weak_ptr_factory{this};
  };
  TestStruct test_struct;
  EXPECT_EQ(test_struct.member, 0);

  // GetMutableWeakPtr() grants non-const access to T.
  const TestStruct& const_test_struct = test_struct;
  WeakPtr<TestStruct> weak_ptr =
      const_test_struct.weak_ptr_factory.GetMutableWeakPtr();
  weak_ptr->member = 1;
  EXPECT_EQ(test_struct.member, 1);
}

}  // namespace base

// =============================================================================
// SKIPPED TESTS from Chromium's base/memory/weak_ptr_unittest.cc
// =============================================================================
// WeakPtrDeathTest.BindToCurrentSequence - Death test
// WeakPtrDeathTest.WeakPtrCopyDoesNotChangeThreadBinding - Death test
// WeakPtrDeathTest.NonOwnerThreadDereferencesWeakPtrAfterReference - Death test
// WeakPtrDeathTest.NonOwnerThreadDeletesWeakPtrAfterReference - Death test
// WeakPtrDeathTest.NonOwnerThreadDeletesObjectAfterReference - Death test
// WeakPtrDeathTest.NonOwnerThreadReferencesObjectAfterDeletion - Death test
// WeakPtrDeathTest.ArrowOperatorChecksOnBadDereference - Death test
// WeakPtrDeathTest.StarOperatorChecksOnBadDereference - Death test
// WeakPtrTest.InvalidateWeakPtrsAndDoom - EXPECT_DCHECK_DEATH portion skipped
