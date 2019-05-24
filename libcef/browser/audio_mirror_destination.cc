// Copyright (c) 2018 The Chromium Embedded Framework Authors.
// Portions copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/audio_mirror_destination.h"
#include "libcef/browser/audio_push_sink.h"

#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "media/base/bind_to_current_loop.h"

CefAudioMirrorDestination::CefAudioMirrorDestination(
    CefRefPtr<CefBrowserHostImpl> browser,
    CefRefPtr<CefAudioHandler> cef_audio_handler,
    content::AudioMirroringManager* mirroring_manager)
    : browser_(browser),
      cef_audio_handler_(cef_audio_handler),
      mirroring_manager_(mirroring_manager) {
  DCHECK(mirroring_manager_);

  thread_checker_.DetachFromThread();
}

void CefAudioMirrorDestination::Start() {
  DCHECK(thread_checker_.CalledOnValidThread());

  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&content::AudioMirroringManager::StartMirroring,
                     base::Unretained(mirroring_manager_),
                     base::Unretained(this)));
}

void CefAudioMirrorDestination::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());

  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&content::AudioMirroringManager::StopMirroring,
                     base::Unretained(mirroring_manager_),
                     base::Unretained(this)));
}

// Asynchronously query whether this MirroringDestination wants to consume
// audio sourced from each of the |candidates|.  |results_callback| is run
// to indicate which of them (or none) should have audio routed to this
// MirroringDestination.  The second parameter of |results_callback|
// indicates whether the MirroringDestination wants either: 1) exclusive
// access to a diverted audio flow versus 2) a duplicate copy of the audio
// flow. |results_callback| must be run on the same thread as the one that
// called QueryForMatches().
void CefAudioMirrorDestination::QueryForMatches(
    const std::set<content::GlobalFrameRoutingId>& candidates,
    MatchesCallback results_callback) {
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&CefAudioMirrorDestination::QueryForMatchesOnUIThread,
                     base::Unretained(this), candidates,
                     media::BindToCurrentLoop(std::move(results_callback))));
}

void CefAudioMirrorDestination::QueryForMatchesOnUIThread(
    const std::set<content::GlobalFrameRoutingId>& candidates,
    MatchesCallback results_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::set<content::GlobalFrameRoutingId> matches;
  for (auto& candidate : candidates) {
    CefRefPtr<CefBrowserHostImpl> browser =
        CefBrowserHostImpl::GetBrowserForFrameRoute(candidate.child_id,
                                                    candidate.frame_routing_id);
    if (browser == browser_) {
      matches.insert(candidate);
    }
  }

  std::move(results_callback).Run(matches, true);
}

// Create a consumer of audio data in the format specified by |params|, and
// connect it as an input to mirroring.  This is used to provide
// MirroringDestination with exclusive access to pull the audio flow from
// the source. When Close() is called on the returned AudioOutputStream, the
// input is disconnected and the object becomes invalid.
media::AudioOutputStream* CefAudioMirrorDestination::AddInput(
    const media::AudioParameters& params) {
  // TODO Check and add usage on CEF
  return nullptr;
}

// Create a consumer of audio data in the format specified by |params|, and
// connect it as an input to mirroring.  This is used to provide
// MirroringDestination with duplicate audio data, which is pushed from the
// main audio flow. When Close() is called on the returned AudioPushSink,
// the input is disconnected and the object becomes invalid.
media::AudioPushSink* CefAudioMirrorDestination::AddPushInput(
    const media::AudioParameters& params) {
  return new CefAudioPushSink(
      params, browser_, cef_audio_handler_,
      base::Bind(&CefAudioMirrorDestination::ReleasePushInput,
                 base::Unretained(this)));
}

void CefAudioMirrorDestination::ReleasePushInput(CefAudioPushSink* sink) {
  delete sink;
}
