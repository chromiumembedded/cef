// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFSIMPLE_CAPI_REF_COUNTED_H_
#define CEF_TESTS_CEFSIMPLE_CAPI_REF_COUNTED_H_

#include <stdatomic.h>
#include <stdlib.h>

// Generic macros for implementing reference counting on CEF C API structures.
// These macros help reduce boilerplate when implementing add_ref, release,
// has_one_ref, and has_at_least_one_ref functions.

// Macro to implement add_ref for a structure type.
// Usage: IMPLEMENT_ADDREF(my_struct_t, my_struct, ref_count)
#define IMPLEMENT_ADDREF(struct_type, struct_name, ref_field)             \
  void CEF_CALLBACK struct_name##_add_ref(cef_base_ref_counted_t* self) { \
    struct_type* obj = (struct_type*)self;                                \
    atomic_fetch_add(&obj->ref_field, 1);                                 \
  }

// Macro to implement has_one_ref for a structure type.
#define IMPLEMENT_HAS_ONE_REF(struct_type, struct_name, ref_field)           \
  int CEF_CALLBACK struct_name##_has_one_ref(cef_base_ref_counted_t* self) { \
    struct_type* obj = (struct_type*)self;                                   \
    return atomic_load(&obj->ref_field) == 1;                                \
  }

// Macro to implement has_at_least_one_ref for a structure type.
#define IMPLEMENT_HAS_AT_LEAST_ONE_REF(struct_type, struct_name, ref_field) \
  int CEF_CALLBACK struct_name##_has_at_least_one_ref(                      \
      cef_base_ref_counted_t* self) {                                       \
    struct_type* obj = (struct_type*)self;                                  \
    return atomic_load(&obj->ref_field) >= 1;                               \
  }

// Macro to implement a simple release function that only frees the object.
// Use this for structures that don't own other ref-counted objects.
// For complex cleanup, implement release manually.
// Usage: IMPLEMENT_RELEASE_SIMPLE(my_struct_t, my_struct, ref_count)
#define IMPLEMENT_RELEASE_SIMPLE(struct_type, struct_name, ref_field)    \
  int CEF_CALLBACK struct_name##_release(cef_base_ref_counted_t* self) { \
    struct_type* obj = (struct_type*)self;                               \
    int count = atomic_fetch_sub(&obj->ref_field, 1) - 1;                \
    if (count == 0) {                                                    \
      free(obj);                                                         \
      return 1;                                                          \
    }                                                                    \
    return 0;                                                            \
  }

// Implement all ref-counting for structures with no cleanup needed.
// Use when structure only needs free(), no owned objects to release.
// Usage: IMPLEMENT_REFCOUNTING_SIMPLE(my_struct_t, my_struct, ref_count)
#define IMPLEMENT_REFCOUNTING_SIMPLE(struct_type, struct_name, ref_field) \
  IMPLEMENT_ADDREF(struct_type, struct_name, ref_field)                   \
  IMPLEMENT_RELEASE_SIMPLE(struct_type, struct_name, ref_field)           \
  IMPLEMENT_HAS_ONE_REF(struct_type, struct_name, ref_field)              \
  IMPLEMENT_HAS_AT_LEAST_ONE_REF(struct_type, struct_name, ref_field)

// Implement ref-counting WITHOUT release for structures with custom cleanup.
// Use when you need to manually implement release() with custom logic.
// You must manually implement struct_name##_release().
// Usage: IMPLEMENT_REFCOUNTING_MANUAL(my_struct_t, my_struct, ref_count)
#define IMPLEMENT_REFCOUNTING_MANUAL(struct_type, struct_name, ref_field) \
  IMPLEMENT_ADDREF(struct_type, struct_name, ref_field)                   \
  IMPLEMENT_HAS_ONE_REF(struct_type, struct_name, ref_field)              \
  IMPLEMENT_HAS_AT_LEAST_ONE_REF(struct_type, struct_name, ref_field)

// Macro to initialize CEF base ref-counted structure.
// Must be called after allocating the structure.
// Usage: INIT_CEF_BASE_REFCOUNTED(ptr, cef_type_t, struct_name)
// Example: INIT_CEF_BASE_REFCOUNTED(&handler->handler, cef_display_handler_t,
//                                    display_handler)
#define INIT_CEF_BASE_REFCOUNTED(ptr, cef_type, struct_name)          \
  do {                                                                \
    (ptr)->size = sizeof(cef_type);                                   \
    (ptr)->add_ref = struct_name##_add_ref;                           \
    (ptr)->release = struct_name##_release;                           \
    (ptr)->has_one_ref = struct_name##_has_one_ref;                   \
    (ptr)->has_at_least_one_ref = struct_name##_has_at_least_one_ref; \
  } while (0)

#endif  // CEF_TESTS_CEFSIMPLE_CAPI_REF_COUNTED_H_
