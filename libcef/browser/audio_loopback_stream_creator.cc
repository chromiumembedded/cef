// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/audio_loopback_stream_creator.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "media/audio/audio_device_description.h"
#include "media/base/user_input_monitor.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/mojom/media/renderer_audio_input_stream_factory.mojom.h"

namespace {

// A blink::mojom::RendererAudioInputStreamFactoryClient that holds a
// CefAudioLoopbackStreamCreator::StreamCreatedCallback. The callback runs when
// the requested audio stream is created.
class StreamCreatedCallbackAdapter final
    : public blink::mojom::RendererAudioInputStreamFactoryClient {
 public:
  explicit StreamCreatedCallbackAdapter(
      const CefAudioLoopbackStreamCreator::StreamCreatedCallback& callback)
      : callback_(callback) {
    DCHECK(callback_);
  }

  StreamCreatedCallbackAdapter(const StreamCreatedCallbackAdapter&) = delete;
  StreamCreatedCallbackAdapter& operator=(const StreamCreatedCallbackAdapter&) =
      delete;

  ~StreamCreatedCallbackAdapter() override = default;

  // blink::mojom::RendererAudioInputStreamFactoryClient implementation.
  void StreamCreated(
      mojo::PendingRemote<media::mojom::AudioInputStream> stream,
      mojo::PendingReceiver<media::mojom::AudioInputStreamClient>
          client_receiver,
      media::mojom::ReadOnlyAudioDataPipePtr data_pipe,
      bool initially_muted,
      const absl::optional<base::UnguessableToken>& stream_id) override {
    DCHECK(!initially_muted);  // Loopback streams shouldn't be started muted.
    callback_.Run(std::move(stream), std::move(client_receiver),
                  std::move(data_pipe));
  }

 private:
  const CefAudioLoopbackStreamCreator::StreamCreatedCallback callback_;
};

void CreateLoopbackStreamHelper(
    content::ForwardingAudioStreamFactory::Core* factory,
    content::AudioStreamBroker::LoopbackSource* loopback_source,
    const media::AudioParameters& params,
    uint32_t total_segments,
    mojo::PendingRemote<blink::mojom::RendererAudioInputStreamFactoryClient>
        client_remote) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  const bool mute_source = true;
  factory->CreateLoopbackStream(-1, -1, loopback_source, params, total_segments,
                                mute_source, std::move(client_remote));
}

void CreateSystemWideLoopbackStreamHelper(
    content::ForwardingAudioStreamFactory::Core* factory,
    const media::AudioParameters& params,
    uint32_t total_segments,
    mojo::PendingRemote<blink::mojom::RendererAudioInputStreamFactoryClient>
        client_remote) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  const bool enable_agc = false;
  factory->CreateInputStream(
      -1, -1, media::AudioDeviceDescription::kLoopbackWithMuteDeviceId, params,
      total_segments, enable_agc, media::mojom::AudioProcessingConfigPtr(),
      std::move(client_remote));
}

}  // namespace

CefAudioLoopbackStreamCreator::CefAudioLoopbackStreamCreator()
    : factory_(nullptr,
               content::BrowserMainLoop::GetInstance()
                   ? static_cast<media::UserInputMonitorBase*>(
                         content::BrowserMainLoop::GetInstance()
                             ->user_input_monitor())
                   : nullptr,
               content::AudioStreamBrokerFactory::CreateImpl()) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

CefAudioLoopbackStreamCreator::~CefAudioLoopbackStreamCreator() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void CefAudioLoopbackStreamCreator::CreateLoopbackStream(
    content::WebContents* loopback_source,
    const media::AudioParameters& params,
    uint32_t total_segments,
    const StreamCreatedCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  mojo::PendingRemote<blink::mojom::RendererAudioInputStreamFactoryClient>
      client;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<StreamCreatedCallbackAdapter>(callback),
      client.InitWithNewPipeAndPassReceiver());
  // Deletion of factory_.core() is posted to the IO thread when |factory_| is
  // destroyed, so Unretained is safe below.
  if (loopback_source) {
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&CreateLoopbackStreamHelper, factory_.core(),
                       static_cast<content::WebContentsImpl*>(loopback_source)
                           ->GetAudioStreamFactory()
                           ->core(),
                       params, total_segments, std::move(client)));
    return;
  }
  // A null |frame_of_source_web_contents| requests system-wide loopback.
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&CreateSystemWideLoopbackStreamHelper, factory_.core(),
                     params, total_segments, std::move(client)));
}
