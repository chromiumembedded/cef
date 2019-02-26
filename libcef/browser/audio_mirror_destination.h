// Copyright (c) 2018 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_AUDIO_MIRROR_DESTINATION_H_
#define CEF_LIBCEF_BROWSER_AUDIO_MIRROR_DESTINATION_H_
#pragma once

#include "include/cef_audio_handler.h"
#include "include/cef_base.h"
#include "libcef/browser/browser_host_impl.h"

#include "base/callback.h"
#include "base/threading/thread_checker.h"
#include "content/browser/media/capture/audio_mirroring_manager.h"
#include "media/audio/audio_io.h"
#include "media/base/audio_converter.h"
#include "media/base/audio_parameters.h"

class CefAudioPushSink;
class CefBrowserHostImpl;

class CefAudioMirrorDestination
    : public content::AudioMirroringManager::MirroringDestination {
 public:
  CefAudioMirrorDestination(CefRefPtr<CefBrowserHostImpl> browser,
                            CefRefPtr<CefAudioHandler> cef_audio_handler,
                            content::AudioMirroringManager* mirroring_manager);

  virtual ~CefAudioMirrorDestination() = default;

  // Start mirroring. This needs to be triggered on the IO thread.
  void Start();

  // Stop mirroring. This needs to be triggered on the IO thread.
  void Stop();

  // Asynchronously query whether this MirroringDestination wants to consume
  // audio sourced from each of the |candidates|.  |results_callback| is run
  // to indicate which of them (or none) should have audio routed to this
  // MirroringDestination.  The second parameter of |results_callback|
  // indicates whether the MirroringDestination wants either: 1) exclusive
  // access to a diverted audio flow versus 2) a duplicate copy of the audio
  // flow. |results_callback| must be run on the same thread as the one that
  // called QueryForMatches().
  void QueryForMatches(
      const std::set<content::GlobalFrameRoutingId>& candidates,
      MatchesCallback results_callback) override;

  // Create a consumer of audio data in the format specified by |params|, and
  // connect it as an input to mirroring.  This is used to provide
  // MirroringDestination with exclusive access to pull the audio flow from
  // the source. When Close() is called on the returned AudioOutputStream, the
  // input is disconnected and the object becomes invalid.
  media::AudioOutputStream* AddInput(
      const media::AudioParameters& params) override;

  // Create a consumer of audio data in the format specified by |params|, and
  // connect it as an input to mirroring.  This is used to provide
  // MirroringDestination with duplicate audio data, which is pushed from the
  // main audio flow. When Close() is called on the returned AudioPushSink,
  // the input is disconnected and the object becomes invalid.
  media::AudioPushSink* AddPushInput(
      const media::AudioParameters& params) override;

 private:
  void QueryForMatchesOnUIThread(
      const std::set<content::GlobalFrameRoutingId>& candidates,
      MatchesCallback results_callback);
  void ReleasePushInput(CefAudioPushSink* sink);

  CefRefPtr<CefBrowserHostImpl> browser_;
  CefRefPtr<CefAudioHandler> cef_audio_handler_;
  content::AudioMirroringManager* mirroring_manager_;

  base::ThreadChecker thread_checker_;
};

#endif  // CEF_LIBCEF_BROWSER_AUDIO_MIRROR_DESTINATION_H_
