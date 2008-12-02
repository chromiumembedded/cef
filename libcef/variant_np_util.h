// Copyright (c) 2008 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef _VARIANT_NP_UTIL_H
#define _VARIANT_NP_UTIL_H

#include "third_party/npapi/bindings/npruntime.h"

#include <string>
#include <vector>

#ifdef __cplusplus
extern "C" {
#endif

namespace WebCore {
class DOMWindow;
}

// Convert a vector of values to an NPObject, attached to the specified
// DOM Window, that represents a JavaScript Array of the same values.
NPObject* _NPN_StringVectorToArrayObject(WebCore::DOMWindow* domwindow,
    const std::vector<std::string>& vec);
NPObject* _NPN_WStringVectorToArrayObject(WebCore::DOMWindow* domwindow,
    const std::vector<std::wstring>& vec);
NPObject* _NPN_IntVectorToArrayObject(WebCore::DOMWindow* domwindow,
    const std::vector<int>& vec);
NPObject* _NPN_DoubleVectorToArrayObject(WebCore::DOMWindow* domwindow,
    const std::vector<double>& vec);
NPObject* _NPN_BooleanVectorToArrayObject(WebCore::DOMWindow* domwindow,
    const std::vector<bool>& vec);

// Convert an NPObject that represents a JavaScript Array to a vector of
// values.
bool _NPN_ArrayObjectToStringVector(NPObject* npobject,
    std::vector<std::string>& vec);
bool _NPN_ArrayObjectToWStringVector(NPObject* npobject,
    std::vector<std::wstring>& vec);
bool _NPN_ArrayObjectToIntVector(NPObject* npobject,
    std::vector<int>& vec);
bool _NPN_ArrayObjectToDoubleVector(NPObject* npobject,
    std::vector<double>& vec);
bool _NPN_ArrayObjectToBooleanVector(NPObject* npobject,
    std::vector<bool>& vec);

// Evaluate the types of values contained in an NPObject representing a
// JavaScript Array and suggest the most restrictive type that can safely store
// all of the Array values.  For instance, if the Array contains all Int32
// values, the suggested type will be NPVariantType_Int32. If, on the other
// hand, the Array contains a mix of Int32 values and String values, then the
// suggested type will be NPVariantType_String.  The supported values, from
// most restrictive to least restrictive, are NPVariantType_Bool,
// NPVariantType_Int32, NPVariantType_Double and NPVariantType_String. Arrays
// that contain values of type NPVariantType_Void, NPVariantType_Null or
// NPVariantType_Object will always result in a suggestion of type
// NPVariantType_String.
bool _NPN_ArrayObjectToVectorTypeHint(NPObject* npobject,
    NPVariantType &typehint);

#ifdef __cplusplus
}  /* end extern "C" */
#endif

#endif // _VARIANT_NP_UTIL_H
