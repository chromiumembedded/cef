// Copyright 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_OSR_EXTERNAL_BEGIN_FRAME_SOURCE_OSR_H_
#define CEF_LIBCEF_BROWSER_OSR_EXTERNAL_BEGIN_FRAME_SOURCE_OSR_H_
#pragma once

#include <cstdint>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "services/viz/privileged/mojom/compositing/external_begin_frame_controller.mojom.h"
#include "ui/compositor/compositor.h"

// Drives the viz external BeginFrame controller of a windowless compositor
// from the ticks that the client supplies through
// CefBrowserHost::SendExternalBeginFrame (dated or not).
//
// Modeled on ui::ExternalBeginFrameAdapter, the platform BeginFrame source
// that Chromium uses with Wayland, which is only built on Linux. Compared with
// the historical implementation in CefRenderWidgetHostViewOSR:
//
// - the BeginFrameArgs carry the real frame time, deadline and interval of the
//   external clock. The historical path stamped the frame with the time of the
//   call, no deadline and a fixed 60 Hz interval; with no deadline the Display
//   drew as soon as the first frame sink had submitted, so a page and a
//   <video> that both animate were drawn one tick apart and the video dropped
//   frames (its submitter was still waiting for the ack of the previous frame
//   when the next BeginFrame arrived);
// - the sequence number comes from viz::BeginFrameSource::
//   BeginFrameArgsGenerator, so a missed tick leaves a hole in the sequence as
//   it would with a real vsync source;
// - the compositor is configured with wait_for_all_frame_sinks = false: a
//   tick is acknowledged as soon as the Display has drawn, not when every
//   frame sink has replied, which is also what the Wayland adapter does;
// - a tick that arrives while another one is in flight is kept (one deep)
//   with its own time stamps and issued when the acknowledgement comes back,
//   provided its interval is not half over; otherwise it is dropped like a
//   missed vsync. viz accepts a single IssueExternalBeginFrame in flight per
//   controller (mojo bad message otherwise), hence the gate.
//
// All methods are called on the UI thread.
class CefExternalBeginFrameSourceOSR
    : public ui::ExternalBeginFrameControllerClientFactory,
      public viz::mojom::ExternalBeginFrameControllerClient {
 public:
  explicit CefExternalBeginFrameSourceOSR(ui::Compositor* compositor);

  CefExternalBeginFrameSourceOSR(const CefExternalBeginFrameSourceOSR&) =
      delete;
  CefExternalBeginFrameSourceOSR& operator=(
      const CefExternalBeginFrameSourceOSR&) = delete;

  ~CefExternalBeginFrameSourceOSR() override;

  // Issue a BeginFrame for the tick that occurred at |frame_time| (at or
  // before now) and return the interval used. |nominal_interval| is the
  // interval of the windowless frame rate and must be positive. When
  // |client_dated| is true the tick time comes from the client's clock: the
  // interval is the median of the last few plausible gaps between ticks
  // (plausible: within half of the previous interval, so that a missed tick
  // does not double it); the jitter of the calls stays out of the interval
  // and a variable refresh rate is followed within a few ticks. When false,
  // the frame is dated at the time of the call and the interval is the
  // nominal one. The deadline is always one interval after the tick, as with
  // Chromium's own BeginFrame sources.
  base::TimeDelta Tick(base::TimeTicks frame_time,
                       base::TimeDelta nominal_interval,
                       bool client_dated);

  // The interval of the last tick (zero before the first tick).
  base::TimeDelta interval() const { return interval_; }

  // Whether viz currently asks for BeginFrames.
  bool needs_begin_frames() const { return needs_begin_frames_; }

  // Counters for diagnostics.
  uint64_t ticks_issued() const { return ticks_issued_; }
  uint64_t ticks_queued() const { return ticks_queued_; }
  uint64_t ticks_dropped() const { return ticks_dropped_; }

  // ui::ExternalBeginFrameControllerClientFactory methods:
  mojo::PendingAssociatedRemote<viz::mojom::ExternalBeginFrameControllerClient>
  CreateExternalBeginFrameControllerClient() override;

  // viz::mojom::ExternalBeginFrameControllerClient methods:
  void SetNeedsBeginFrame(bool needs_begin_frames) override;
  void SetPreferredInterval(base::TimeDelta interval) override;
  void NeedsBeginFrameWithId(int64_t display_id,
                             bool needs_begin_frames) override;

 private:
  struct PendingTick {
    base::TimeTicks frame_time;
    base::TimeTicks deadline;
    base::TimeDelta interval;
  };

  void Issue(const PendingTick& tick);
  void OnBeginFrameAck(const viz::BeginFrameAck& ack);

  static uint64_t AllocateSourceId();

  const raw_ptr<ui::Compositor> compositor_;
  const uint64_t source_id_ = AllocateSourceId();
  viz::BeginFrameSource::BeginFrameArgsGenerator args_generator_;
  mojo::AssociatedReceiverSet<viz::mojom::ExternalBeginFrameControllerClient>
      receivers_;

  base::TimeDelta interval_;  // of the last tick (the vsync parameter)
  base::TimeDelta vsync_interval_set_;  // last value handed to the compositor
  // The last plausible gaps between client-dated ticks; the interval is their
  // median, so that the jitter of a client that dates its ticks on the wake-up
  // time of its thread does not enter the interval, while a rate change (a
  // variable refresh rate display) is followed within a few ticks.
  static constexpr int kGapWindow = 5;
  base::TimeDelta gaps_[kGapWindow];
  int gaps_n_ = 0;
  int gaps_i_ = 0;
  base::TimeTicks last_frame_time_;
  bool in_flight_ = false;
  std::optional<PendingTick> queued_;
  bool needs_begin_frames_ = false;

  uint64_t ticks_issued_ = 0;
  uint64_t ticks_queued_ = 0;
  uint64_t ticks_dropped_ = 0;

  base::WeakPtrFactory<CefExternalBeginFrameSourceOSR> weak_factory_{this};
};

#endif  // CEF_LIBCEF_BROWSER_OSR_EXTERNAL_BEGIN_FRAME_SOURCE_OSR_H_
