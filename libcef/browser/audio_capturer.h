// Copyright (c) 2019 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_AUDIO_CAPTURER_H_
#define CEF_LIBCEF_BROWSER_AUDIO_CAPTURER_H_
#pragma once

#include "include/internal/cef_ptr.h"
#include "include/internal/cef_types_wrappers.h"

#include "media/base/audio_capturer_source.h"

namespace content {
class AudioLoopbackStreamCreator;
}  // namespace content

namespace media {
class AudioInputDevice;
}  // namespace media

class CefAudioHandler;
class CefBrowserHostImpl;

class CefAudioCapturer : public media::AudioCapturerSource::CaptureCallback {
 public:
  CefAudioCapturer(const CefAudioParameters& params,
                   CefRefPtr<CefBrowserHostImpl> browser,
                   CefRefPtr<CefAudioHandler> audio_handler);
  ~CefAudioCapturer() override;

 private:
  void OnCaptureStarted() override;
  void Capture(const media::AudioBus* audio_source,
               base::TimeTicks audio_capture_time,
               double volume,
               bool key_pressed) override;
  void OnCaptureError(const std::string& message) override;
  void OnCaptureMuted(bool is_muted) override {}

  void StopStream();

  CefAudioParameters params_;
  CefRefPtr<CefBrowserHostImpl> browser_;
  CefRefPtr<CefAudioHandler> audio_handler_;
  std::unique_ptr<content::AudioLoopbackStreamCreator> audio_stream_creator_;
  scoped_refptr<media::AudioInputDevice> audio_input_device_;
  bool capturing_ = false;
  int channels_ = 0;
};

#endif  // CEF_LIBCEF_BROWSER_AUDIO_CAPTURER_H_