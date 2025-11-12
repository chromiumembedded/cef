// Copyright (c) 2025 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFSIMPLE_CAPI_REF_COUNTED_H_
#define CEF_TESTS_CEFSIMPLE_CAPI_REF_COUNTED_H_

#include <stdatomic.h>

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

// Note: release functions are typically NOT implemented with macros because
// they need custom cleanup logic for each structure type (releasing owned
// objects, freeing allocated memory, etc.).

#endif  // CEF_TESTS_CEFSIMPLE_CAPI_REF_COUNTED_H_
