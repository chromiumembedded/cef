// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _PRINTING_UNITS_H
#define _PRINTING_UNITS_H

namespace printing {

// Length of a thousanth of inches in 0.01mm unit.
const int kHundrethsMMPerInch = 2540;

// Converts from one unit system to another using integer arithmetics.
int ConvertUnit(int value, int old_unit, int new_unit);

// Converts from one unit system to another using doubles.
double ConvertUnitDouble(double value, double old_unit, double new_unit);

// Converts from 0.001 inch unit to 0.00001 meter.
int ConvertMilliInchToHundredThousanthMeter(int milli_inch);

// Converts from 0.00001 meter unit to 0.001 inch.
int ConvertHundredThousanthMeterToMilliInch(int cmm);

}  // namespace printing

#endif  // _PRINTING_UNITS_H

