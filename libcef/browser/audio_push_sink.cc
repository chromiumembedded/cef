// Copyright (c) 2018 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/audio_push_sink.h"
#include "libcef/browser/thread_util.h"

#include "base/bind.h"
#include "base/logging.h"
#include "media/base/audio_sample_types.h"
#include "media/base/data_buffer.h"

using namespace media;

namespace {
cef_channel_layout_t TranslateChannelLayout(ChannelLayout channel) {
  switch (channel) {
    case CHANNEL_LAYOUT_UNSUPPORTED:
      return CEF_CHANNEL_LAYOUT_UNSUPPORTED;
    case CHANNEL_LAYOUT_MONO:
      return CEF_CHANNEL_LAYOUT_MONO;
    case CHANNEL_LAYOUT_STEREO:
      return CEF_CHANNEL_LAYOUT_STEREO;
    case CHANNEL_LAYOUT_2_1:
      return CEF_CHANNEL_LAYOUT_2_1;
    case CHANNEL_LAYOUT_SURROUND:
      return CEF_CHANNEL_LAYOUT_SURROUND;
    case CHANNEL_LAYOUT_4_0:
      return CEF_CHANNEL_LAYOUT_4_0;
    case CHANNEL_LAYOUT_2_2:
      return CEF_CHANNEL_LAYOUT_2_2;
    case CHANNEL_LAYOUT_QUAD:
      return CEF_CHANNEL_LAYOUT_QUAD;
    case CHANNEL_LAYOUT_5_0:
      return CEF_CHANNEL_LAYOUT_5_0;
    case CHANNEL_LAYOUT_5_1:
      return CEF_CHANNEL_LAYOUT_5_1;
    case CHANNEL_LAYOUT_5_0_BACK:
      return CEF_CHANNEL_LAYOUT_5_0_BACK;
    case CHANNEL_LAYOUT_5_1_BACK:
      return CEF_CHANNEL_LAYOUT_5_1_BACK;
    case CHANNEL_LAYOUT_7_0:
      return CEF_CHANNEL_LAYOUT_7_0;
    case CHANNEL_LAYOUT_7_1:
      return CEF_CHANNEL_LAYOUT_7_1;
    case CHANNEL_LAYOUT_7_1_WIDE:
      return CEF_CHANNEL_LAYOUT_7_1_WIDE;
    case CHANNEL_LAYOUT_STEREO_DOWNMIX:
      return CEF_CHANNEL_LAYOUT_STEREO_DOWNMIX;
    case CHANNEL_LAYOUT_2POINT1:
      return CEF_CHANNEL_LAYOUT_2POINT1;
    case CHANNEL_LAYOUT_3_1:
      return CEF_CHANNEL_LAYOUT_3_1;
    case CHANNEL_LAYOUT_4_1:
      return CEF_CHANNEL_LAYOUT_4_1;
    case CHANNEL_LAYOUT_6_0:
      return CEF_CHANNEL_LAYOUT_6_0;
    case CHANNEL_LAYOUT_6_0_FRONT:
      return CEF_CHANNEL_LAYOUT_6_0_FRONT;
    case CHANNEL_LAYOUT_HEXAGONAL:
      return CEF_CHANNEL_LAYOUT_HEXAGONAL;
    case CHANNEL_LAYOUT_6_1:
      return CEF_CHANNEL_LAYOUT_6_1;
    case CHANNEL_LAYOUT_6_1_BACK:
      return CEF_CHANNEL_LAYOUT_6_1_BACK;
    case CHANNEL_LAYOUT_6_1_FRONT:
      return CEF_CHANNEL_LAYOUT_6_1_FRONT;
    case CHANNEL_LAYOUT_7_0_FRONT:
      return CEF_CHANNEL_LAYOUT_7_0_FRONT;
    case CHANNEL_LAYOUT_7_1_WIDE_BACK:
      return CEF_CHANNEL_LAYOUT_7_1_WIDE_BACK;
    case CHANNEL_LAYOUT_OCTAGONAL:
      return CEF_CHANNEL_LAYOUT_OCTAGONAL;
    case CHANNEL_LAYOUT_DISCRETE:
      return CEF_CHANNEL_LAYOUT_DISCRETE;
    case CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC:
      return CEF_CHANNEL_LAYOUT_STEREO_AND_KEYBOARD_MIC;
    case CHANNEL_LAYOUT_4_1_QUAD_SIDE:
      return CEF_CHANNEL_LAYOUT_4_1_QUAD_SIDE;
    case CHANNEL_LAYOUT_BITSTREAM:
      return CEF_CHANNEL_LAYOUT_BITSTREAM;
    case CHANNEL_LAYOUT_NONE:
      return CEF_CHANNEL_LAYOUT_NONE;
  }
  return CEF_CHANNEL_LAYOUT_NONE;
}
}  // namespace

int CefAudioPushSink::audio_stream_id = 0;

CefAudioPushSink::CefAudioPushSink(const AudioParameters& params,
                                   CefRefPtr<CefBrowserHostImpl> browser,
                                   CefRefPtr<CefAudioHandler> cef_audio_handler,
                                   const CloseCallback& callback)
    : params_(params),
      browser_(browser),
      cef_audio_handler_(cef_audio_handler),
      close_callback_(callback),
      stop_stream_(false),
      audio_stream_id_(++audio_stream_id) {
  // Verify that our enum matches Chromium's values.
  static_assert(
      static_cast<int>(CEF_CHANNEL_LAYOUT_MAX) ==
          static_cast<int>(CHANNEL_LAYOUT_MAX),
      "cef_channel_layout_t must match the ChannelLayout enum in Chromium");

  DCHECK(params_.IsValid());
  DCHECK(browser);
  DCHECK(cef_audio_handler);

  // VAOS can be constructed on any thread, but will DCHECK that all
  // AudioOutputStream methods are called from the same thread.
  thread_checker_.DetachFromThread();

  base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::UI},
                           base::BindOnce(&CefAudioPushSink::InitOnUIThread,
                                          base::Unretained(this)));
}

CefAudioPushSink::~CefAudioPushSink() = default;

void CefAudioPushSink::InitOnUIThread() {
  DCHECK(thread_checker_.CalledOnValidThread());

  cef_audio_handler_->OnAudioStreamStarted(
      browser_.get(), audio_stream_id_, params_.channels(),
      TranslateChannelLayout(params_.channel_layout()), params_.sample_rate(),
      params_.frames_per_buffer());
}

void CefAudioPushSink::OnData(std::unique_ptr<media::AudioBus> source,
                              base::TimeTicks reference_time) {
  // early exit if stream already stopped
  if (stop_stream_)
    return;

  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefAudioPushSink::OnData,
                                          base::Unretained(this),
                                          std::move(source), reference_time));
    return;
  }

  const int channels = source->channels();
  std::vector<const float*> data(channels);
  for (int c = 0; c < channels; ++c) {
    data[c] = source->channel(c);
  }
  // Add the packet to the buffer.
  base::TimeDelta pts = reference_time - base::TimeTicks::UnixEpoch();
  cef_audio_handler_->OnAudioStreamPacket(browser_.get(), audio_stream_id_,
                                          data.data(), source->frames(),
                                          pts.InMilliseconds());
}

void CefAudioPushSink::Close() {
  if (!CEF_CURRENTLY_ON_UIT()) {
    CEF_POST_TASK(CEF_UIT, base::BindOnce(&CefAudioPushSink::Close,
                                          base::Unretained(this)));
    return;
  }

  if (!stop_stream_) {
    stop_stream_ = true;
    cef_audio_handler_->OnAudioStreamStopped(browser_.get(), audio_stream_id_);

    if (!close_callback_.is_null()) {
      std::move(close_callback_).Run(this);
    }
  }
}
