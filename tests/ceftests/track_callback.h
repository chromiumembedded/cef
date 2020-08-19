// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_TESTS_CEFTESTS_TRACK_CALLBACK_H_
#define CEF_TESTS_CEFTESTS_TRACK_CALLBACK_H_
#pragma once

class TrackCallback {
 public:
  TrackCallback() : gotit_(false) {}
  void yes() { gotit_ = true; }
  bool isSet() { return gotit_; }
  void reset() { gotit_ = false; }
  operator bool() const { return gotit_; }

 protected:
  bool gotit_;
};

#endif  // CEF_TESTS_CEFTESTS_TRACK_CALLBACK_H_
