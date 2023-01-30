// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_AUDIO_LOOPBACK_STREAM_CREATOR_H_
#define CEF_LIBCEF_BROWSER_AUDIO_LOOPBACK_STREAM_CREATOR_H_

#include "base/functional/callback.h"
#include "content/browser/media/forwarding_audio_stream_factory.h"
#include "content/common/content_export.h"
#include "media/mojo/mojom/audio_data_pipe.mojom.h"
#include "media/mojo/mojom/audio_input_stream.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace media {
class AudioParameters;
}

// This class handles creating a loopback stream that either captures audio from
// a WebContents or the system-wide loopback through the Audio Service.
// This class is operated on the UI thread.
// Based on InProcessAudioLoopbackStreamCreator which was deleted in
// https://crrev.com/a5af2468cf.
class CefAudioLoopbackStreamCreator final {
 public:
  CefAudioLoopbackStreamCreator();

  CefAudioLoopbackStreamCreator(const CefAudioLoopbackStreamCreator&) = delete;
  CefAudioLoopbackStreamCreator& operator=(
      const CefAudioLoopbackStreamCreator&) = delete;

  ~CefAudioLoopbackStreamCreator();

  // The callback that is called when the requested stream is created.
  using StreamCreatedCallback = base::RepeatingCallback<void(
      mojo::PendingRemote<media::mojom::AudioInputStream> stream,
      mojo::PendingReceiver<media::mojom::AudioInputStreamClient>
          client_receiver,
      media::mojom::ReadOnlyAudioDataPipePtr data_pipe)>;

  // Creates a loopback stream that captures the audio from |loopback_source|,
  // or the default system playback if |loopback_source| is null. Local output
  // of the source/system audio is muted during capturing.
  void CreateLoopbackStream(content::WebContents* loopback_source,
                            const media::AudioParameters& params,
                            uint32_t total_segments,
                            const StreamCreatedCallback& callback);

 private:
  content::ForwardingAudioStreamFactory factory_;
};

#endif  // CEF_LIBCEF_BROWSER_AUDIO_LOOPBACK_STREAM_CREATOR_H_
