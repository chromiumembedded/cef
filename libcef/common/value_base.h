// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_COMMON_VALUE_BASE_H_
#define CEF_LIBCEF_COMMON_VALUE_BASE_H_
#pragma once

#include <map>
#include <set>
#include "include/cef_base.h"

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/notreached.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/threading/platform_thread.h"

// Controller implementation base class.
class CefValueController
    : public base::RefCountedThreadSafe<CefValueController> {
 public:
  // Implemented by a class controlled using the access controller.
  class Object {
   public:
    virtual ~Object() = default;

    // Called when the value has been removed.
    virtual void OnControlRemoved() = 0;
  };

  // Encapsulates context locking and verification logic.
  class AutoLock {
   public:
    explicit AutoLock(CefValueController* impl)
        : impl_(impl), verified_(impl && impl->VerifyThread()) {
      DCHECK(impl);
      if (verified_) {
        impl_->lock();
      }
    }

    AutoLock(const AutoLock&) = delete;
    AutoLock& operator=(const AutoLock&) = delete;

    ~AutoLock() {
      if (verified_) {
        impl_->unlock();
      }
    }

    inline bool verified() { return verified_; }

   private:
    scoped_refptr<CefValueController> impl_;
    bool verified_;
  };

  CefValueController();

  CefValueController(const CefValueController&) = delete;
  CefValueController& operator=(const CefValueController&) = delete;

  // Returns true if this controller is thread safe.
  virtual bool thread_safe() = 0;

  // Returns true if the current thread is allowed to access this controller.
  virtual bool on_correct_thread() = 0;

  // Lock the controller.
  virtual void lock() = 0;

  // Unlock the controller.
  virtual void unlock() = 0;

  // Returns true if the controller is locked on the current thread.
  virtual bool locked() = 0;

  // Assert that the lock has been acquired.
  virtual void AssertLockAcquired() = 0;

  // Verify that the current thread is correct for accessing the controller.
  inline bool VerifyThread() {
    if (!thread_safe() && !on_correct_thread()) {
      // This object should only be accessed from the thread that created it.
      DCHECK(false) << "object accessed from incorrect thread.";
      return false;
    }
    return true;
  }

  // The controller must already be locked before calling the below methods.

  // Set the owner for this controller.
  void SetOwner(void* value, Object* object);

  // Add a reference value and associated object.
  void AddReference(void* value, Object* object);

  // Remove the value. If |notify_object| is true the removed object will be
  // notified. If |value| is the owner all reference objects will be removed.
  // If |value| has dependencies those objects will also be removed.
  void Remove(void* value, bool notify_object);

  // Returns the object for the specified value.
  Object* Get(void* value);

  // Add a dependency between |parent| and |child|.
  void AddDependency(void* parent, void* child);

  // Recursively removes any dependent values.
  void RemoveDependencies(void* value);

  // Takes ownership of all references and dependencies currently controlled by
  // |other|. The |other| controller must already be locked.
  void TakeFrom(CefValueController* other);

  // Replace all instances of |old_value| with |new_value|. Used in cases where
  // move semantics may move the contents of an object without retaining the
  // object pointer itself.
  void Swap(void* old_value, void* new_value);

 protected:
  friend class base::RefCountedThreadSafe<CefValueController>;

  virtual ~CefValueController();

 private:
  // Owner object.
  void* owner_value_ = nullptr;
  Object* owner_object_ = nullptr;

  // Map of reference objects.
  using ReferenceMap = std::map<void*, Object*>;
  ReferenceMap reference_map_;

  // Map of dependency objects.
  using DependencySet = std::set<void*>;
  using DependencyMap = std::map<void*, DependencySet>;
  DependencyMap dependency_map_;
};

// Thread-safe access control implementation.
class CefValueControllerThreadSafe : public CefValueController {
 public:
  explicit CefValueControllerThreadSafe() = default;

  CefValueControllerThreadSafe(const CefValueControllerThreadSafe&) = delete;
  CefValueControllerThreadSafe& operator=(const CefValueControllerThreadSafe&) =
      delete;

  // CefValueController methods.
  bool thread_safe() override { return true; }
  bool on_correct_thread() override { return true; }
  void lock() override NO_THREAD_SAFETY_ANALYSIS {
    lock_.Acquire();
    locked_thread_id_ = base::PlatformThread::CurrentId();
  }
  void unlock() override NO_THREAD_SAFETY_ANALYSIS {
    locked_thread_id_ = 0;
    lock_.Release();
  }
  bool locked() override {
    return (locked_thread_id_ == base::PlatformThread::CurrentId());
  }
  void AssertLockAcquired() override { lock_.AssertAcquired(); }

 private:
  base::Lock lock_;
  base::PlatformThreadId locked_thread_id_ = 0;
};

// Non-thread-safe access control implementation.
class CefValueControllerNonThreadSafe : public CefValueController {
 public:
  explicit CefValueControllerNonThreadSafe()
      : thread_id_(base::PlatformThread::CurrentId()) {}

  CefValueControllerNonThreadSafe(const CefValueControllerNonThreadSafe&) =
      delete;
  CefValueControllerNonThreadSafe& operator=(
      const CefValueControllerNonThreadSafe&) = delete;

  // CefValueController methods.
  bool thread_safe() override { return false; }
  bool on_correct_thread() override {
    return (thread_id_ == base::PlatformThread::CurrentId());
  }
  void lock() override {}
  void unlock() override {}
  bool locked() override { return on_correct_thread(); }
  void AssertLockAcquired() override { DCHECK(locked()); }

 private:
  base::PlatformThreadId thread_id_;
};

// Helper macros for verifying context.

#define CEF_VALUE_VERIFY_RETURN_VOID_EX(object, modify) \
  if (!VerifyAttached())                                \
    return;                                             \
  AutoLock auto_lock(object, modify);                   \
  if (!auto_lock.verified())                            \
    return;

#define CEF_VALUE_VERIFY_RETURN_VOID(modify) \
  CEF_VALUE_VERIFY_RETURN_VOID_EX(this, modify)

#define CEF_VALUE_VERIFY_RETURN_EX(object, modify, error_val) \
  if (!VerifyAttached())                                      \
    return error_val;                                         \
  AutoLock auto_lock(object, modify);                         \
  if (!auto_lock.verified())                                  \
    return error_val;

#define CEF_VALUE_VERIFY_RETURN(modify, error_val) \
  CEF_VALUE_VERIFY_RETURN_EX(this, modify, error_val)

// Template class for implementing CEF wrappers of other types.
template <class CefType, class ValueType>
class CefValueBase : public CefType, public CefValueController::Object {
 public:
  // Specifies how the value will be used.
  enum ValueMode {
    // A reference to a value managed by an existing controller. These values
    // can be safely detached but ownership should not be transferred (make a
    // copy of the value instead).
    kReference,

    // The value has its own controller and will be deleted on destruction.
    // These values can only be detached to another controller otherwise any
    // references will not be properly managed.
    kOwnerWillDelete,

    // The value has its own controller and will not be deleted on destruction.
    // This should only be used for global values or scope-limited values that
    // will be explicitly detached.
    kOwnerNoDelete,
  };

  // Create a new object.
  // If |read_only| is true mutable access will not be allowed.
  // If |parent_value| is non-NULL and the value mode is kReference a dependency
  // will be added.
  CefValueBase(ValueType* value,
               void* parent_value,
               ValueMode value_mode,
               bool read_only,
               CefValueController* controller)
      : value_(value),
        value_mode_(value_mode),
        read_only_(read_only),
        controller_(controller) {
    DCHECK(value_);

    // Specifying a parent value for a non-reference doesn't make sense.
    DCHECK(!(!reference() && parent_value));

    if (!reference() && !controller_.get()) {
      // For owned values default to using a new multi-threaded controller.
      controller_ = new CefValueControllerThreadSafe();
      SetOwnsController();
    }

    // A controller is required.
    DCHECK(controller_.get());

    if (reference()) {
      // Register the reference with the controller.
      controller_->AddReference(value_, this);

      // Add a dependency on the parent value.
      if (parent_value) {
        controller_->AddDependency(parent_value, value_);
      }
    }
  }

  CefValueBase(const CefValueBase&) = delete;
  CefValueBase& operator=(const CefValueBase&) = delete;

  ~CefValueBase() override {
    if (controller_.get() && value_) {
      Delete();
    }
  }

  // True if the underlying value is referenced instead of owned.
  inline bool reference() const { return (value_mode_ == kReference); }

  // True if the underlying value will be deleted.
  inline bool will_delete() const { return (value_mode_ == kOwnerWillDelete); }

  // True if access to the underlying value is read-only.
  inline bool read_only() const { return read_only_; }

  // Convert a writable value to read-only. The reverse could be surprising and
  // is therefore not supported.
  void MarkReadOnly() {
    DCHECK(!read_only_);
    read_only_ = true;
  }

  // True if the underlying value has been detached.
  inline bool detached() const { return !controller_.get(); }

  // Returns the controller.
  inline CefValueController* controller() const { return controller_.get(); }

  // Deletes the underlying value.
  void Delete() {
    CEF_VALUE_VERIFY_RETURN_VOID(false);

    // Remove the object from the controller. If this is the owner object any
    // references will be detached.
    controller()->Remove(value_, false);

    if (will_delete()) {
      // Remove any dependencies.
      controller()->RemoveDependencies(value_);

      // Delete the value.
      DeleteValue(value_);
    }

    controller_ = nullptr;
    value_ = nullptr;
  }

  // Detaches the underlying value and returns a pointer to it. If this is an
  // owner and a |new_controller| value is specified any existing references
  // will be passed to the new controller.
  [[nodiscard]] ValueType* Detach(CefValueController* new_controller) {
    CEF_VALUE_VERIFY_RETURN(false, nullptr);

    if (new_controller && !reference()) {
      // Pass any existing references and dependencies to the new controller.
      // They will be removed from this controller.
      new_controller->TakeFrom(controller());
    }

    // Remove the object from the controller. If this is the owner object any
    // references will be detached.
    controller()->Remove(value_, false);
    controller_ = nullptr;

    // Return the old value.
    ValueType* old_val = value_;
    value_ = nullptr;
    return old_val;
  }

  // Verify that the value is attached.
  inline bool VerifyAttached() const {
    if (detached()) {
      // This object should not be accessed after being detached.
      DCHECK(false) << "object accessed after being detached.";
      return false;
    }
    return true;
  }

 protected:
  // CefValueController::Object methods.
  void OnControlRemoved() override {
    DCHECK(controller()->locked());

    // Only references should be removed in this manner.
    DCHECK(reference());

    controller_ = nullptr;
    value_ = nullptr;
  }

  // Override to customize value deletion.
  virtual void DeleteValue(ValueType* value) { delete value; }

  // Returns a mutable reference to the value.
  inline ValueType* mutable_value() const {
    DCHECK(value_);
    DCHECK(!read_only_);
    DCHECK(controller()->locked());
    return value_;
  }

  // Returns a const reference to the value.
  inline const ValueType& const_value() const {
    DCHECK(value_);
    DCHECK(controller()->locked());
    return *value_;
  }

  // Returns an mutable reference to the value without checking read-only state.
  inline ValueType* mutable_value_unchecked() const {
    return const_cast<ValueType*>(&const_value());
  }

  // Verify that the value can be accessed.
  inline bool VerifyAccess(bool modify) const {
    // The controller must already be locked.
    DCHECK(controller()->locked());

    if (read_only() && modify) {
      // This object cannot be modified.
      DCHECK(false) << "mutation attempted on read-only object.";
      return false;
    }

    return true;
  }

  // Used to indicate that this object owns the controller.
  inline void SetOwnsController() {
    CefValueController::AutoLock lock_scope(controller_.get());
    if (lock_scope.verified()) {
      controller_->SetOwner(value_, this);
    }
  }

  // Encapsulates value locking and verification logic.
  class AutoLock {
   public:
    explicit AutoLock(CefValueBase* impl, bool modify)
        : auto_lock_(impl->controller()) {
      verified_ = (auto_lock_.verified() && impl->VerifyAccess(modify));
    }

    AutoLock(const AutoLock&) = delete;
    AutoLock& operator=(const AutoLock&) = delete;

    inline bool verified() { return verified_; }

   private:
    CefValueController::AutoLock auto_lock_;
    bool verified_;
  };

 private:
  ValueType* value_;
  ValueMode value_mode_;
  bool read_only_;
  scoped_refptr<CefValueController> controller_;

  IMPLEMENT_REFCOUNTING(CefValueBase);
};

#endif  // CEF_LIBCEF_COMMON_VALUE_BASE_H_
