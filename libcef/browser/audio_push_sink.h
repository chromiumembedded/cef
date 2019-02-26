// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_AUDIO_PUSH_SINK_H_
#define CEF_LIBCEF_BROWSER_AUDIO_PUSH_SINK_H_
#pragma once

#include "include/cef_audio_handler.h"
#include "include/cef_base.h"
#include "libcef/browser/browser_host_impl.h"

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread_checker.h"
#include "media/audio/audio_io.h"
#include "media/base/audio_converter.h"
#include "media/base/audio_parameters.h"
#include "media/base/channel_layout.h"

class CefAudioPushSink : public media::AudioPushSink {
 public:
  typedef base::Callback<void(CefAudioPushSink* sink)> CloseCallback;

  CefAudioPushSink(const media::AudioParameters& params,
                   CefRefPtr<CefBrowserHostImpl> browser,
                   CefRefPtr<CefAudioHandler> cef_audio_handler,
                   const CloseCallback& callback);
  virtual ~CefAudioPushSink();

  void OnData(std::unique_ptr<media::AudioBus> source,
              base::TimeTicks reference_time) override;
  void Close() override;

 private:
  void InitOnUIThread();

  const media::AudioParameters params_;
  CefRefPtr<CefBrowserHostImpl> browser_;
  CefRefPtr<CefAudioHandler> cef_audio_handler_;
  CloseCallback close_callback_;

  base::ThreadChecker thread_checker_;

  bool stop_stream_;
  int audio_stream_id_;

  static int audio_stream_id;

  DISALLOW_COPY_AND_ASSIGN(CefAudioPushSink);
};

#endif  // CEF_LIBCEF_BROWSER_AUDIO_PUSH_SINK_H_
