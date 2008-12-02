// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "precompiled_libcef.h"
#include "units.h"

#include "base/logging.h"

namespace printing {

int ConvertUnit(int value, int old_unit, int new_unit) {
  DCHECK_GT(new_unit, 0);
  DCHECK_GT(old_unit, 0);
  // With integer arithmetic, to divide a value with correct rounding, you need
  // to add half of the divisor value to the dividend value. You need to do the
  // reverse with negative number.
  if (value >= 0) {
    return ((value * new_unit) + (old_unit / 2)) / old_unit;
  } else {
    return ((value * new_unit) - (old_unit / 2)) / old_unit;
  }
}

double ConvertUnitDouble(double value, double old_unit, double new_unit) {
  DCHECK_GT(new_unit, 0);
  DCHECK_GT(old_unit, 0);
  return value * new_unit / old_unit;
}

int ConvertMilliInchToHundredThousanthMeter(int milli_inch) {
  // 1" == 25.4 mm
  // 1" == 25400 um
  // 0.001" == 25.4 um
  // 0.001" == 2.54 cmm
  return ConvertUnit(milli_inch, 100, 254);
}

int ConvertHundredThousanthMeterToMilliInch(int cmm) {
  return ConvertUnit(cmm, 254, 100);
}

}  // namespace printing

