// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webwidget_host.h"

#include "base/message_loop.h"

void WebWidgetHost::ScheduleAnimation() {
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      factory_.NewRunnableMethod(&WebWidgetHost::ScheduleComposite), 10);
}
