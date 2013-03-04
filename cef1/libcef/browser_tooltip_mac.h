// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_TOOLTIP_MAC_H_
#define CEF_LIBCEF_BROWSER_TOOLTIP_MAC_H_
#pragma once

@protocol BrowserTooltip
- (void)setToolTipAtMousePoint:(NSString *)string;
@end

#endif  // CEF_LIBCEF_BROWSER_TOOLTIP_MAC_H_
