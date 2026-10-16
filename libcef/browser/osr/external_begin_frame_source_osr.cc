// Copyright 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cef/libcef/browser/osr/external_begin_frame_source_osr.h"

#include <algorithm>

#include "base/atomic_sequence_num.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"

// static
uint64_t CefExternalBeginFrameSourceOSR::AllocateSourceId() {
  static base::AtomicSequenceNumber counter;
  return counter.GetNext() + 1;
}

CefExternalBeginFrameSourceOSR::CefExternalBeginFrameSourceOSR(
    ui::Compositor* compositor)
    : compositor_(compositor) {
  CHECK(compositor_);
  // Acknowledge a tick as soon as the Display has drawn, not when every frame
  // sink has replied. This is what ui::ExternalBeginFrameAdapter does
  // ("prevents deadlocks when sinks join after frames are displayed"), and it
  // keeps the gate below open for most of the interval.
  compositor_->set_wait_for_all_frame_sinks(false);
  compositor_->SetExternalBeginFrameControllerClientFactory(this);
}

CefExternalBeginFrameSourceOSR::~CefExternalBeginFrameSourceOSR() {
  compositor_->SetExternalBeginFrameControllerClientFactory(nullptr);
}

base::TimeDelta CefExternalBeginFrameSourceOSR::Tick(
    base::TimeTicks frame_time,
    base::TimeDelta nominal_interval,
    bool client_dated) {
  DCHECK(nominal_interval.is_positive());
  if (!nominal_interval.is_positive()) {
    return base::TimeDelta();
  }

  // viz drops a BeginFrame whose frame time does not advance: keep it strictly
  // increasing even if two ticks are stamped with the same instant.
  if (!last_frame_time_.is_null() && frame_time <= last_frame_time_) {
    frame_time = last_frame_time_ + base::Microseconds(1);
  }

  // The interval. A frame dated at the time of the call carries the nominal
  // interval: its date has the jitter of the call, which must not enter the
  // interval. A client-dated tick carries the median of the last plausible
  // gaps between ticks (plausible: between half and one and a half of the
  // previous interval, so that a missed tick or a restart does not double
  // it); the median keeps the jitter of a client that dates its ticks on its
  // wake-up time out of the interval, and follows a rate change within a few
  // ticks.
  base::TimeDelta interval = nominal_interval;
  if (client_dated) {
    const base::TimeDelta reference =
        interval_.is_positive() ? interval_ : nominal_interval;
    if (!last_frame_time_.is_null()) {
      const base::TimeDelta measured = frame_time - last_frame_time_;
      if (measured > reference / 2 && measured < reference + reference / 2) {
        gaps_[gaps_i_] = measured;
        gaps_i_ = (gaps_i_ + 1) % kGapWindow;
        if (gaps_n_ < kGapWindow) {
          ++gaps_n_;
        }
      }
    }
    if (gaps_n_ > 0) {
      base::TimeDelta sorted[kGapWindow];
      std::copy_n(gaps_, gaps_n_, sorted);
      std::sort(sorted, sorted + gaps_n_);
      interval = sorted[gaps_n_ / 2];
    } else {
      interval = reference;
    }
  }
  interval_ = interval;

  // Keep the vsync parameters of the compositor in step with the external
  // clock (ui::ExternalBeginFrameAdapter::OnVSyncIntervalChanged), without a
  // message per tick for the jitter of a client clock: one percent of change.
  const base::TimeDelta vsync_delta =
      (interval - vsync_interval_set_).magnitude();
  if (vsync_interval_set_.is_zero() || vsync_delta > interval / 100) {
    vsync_interval_set_ = interval;
    compositor_->SetDisplayVSyncParameters(frame_time, interval);
  }

  const base::TimeTicks deadline = frame_time + interval;

  const PendingTick tick{frame_time, deadline, interval};
  if (in_flight_) {
    // One tick in flight at a time (viz reports a bad message otherwise).
    // Keep this one, with its own time stamps, for the acknowledgement.
    if (!stall_warned_ &&
        base::TimeTicks::Now() - issued_at_ > base::Seconds(1)) {
      // The gate only reopens on the acknowledgement or when the reply is
      // dropped; a reply that never comes keeps it closed for good. Say so.
      stall_warned_ = true;
      LOG(WARNING) << "External BeginFrame issued "
                   << (base::TimeTicks::Now() - issued_at_).InMilliseconds()
                   << " ms ago has not been acknowledged; ticks are queued "
                      "or dropped until it is";
    }
    if (queued_) {
      ++ticks_dropped_;
    }
    queued_ = tick;
    ++ticks_queued_;
    if ((ticks_queued_ % 600) == 1) {
      LOG(WARNING) << "External BeginFrame arrived while the previous frame "
                      "was pending; queued ("
                   << ticks_queued_ << " so far, " << ticks_dropped_
                   << " dropped)";
    }
    return interval;
  }
  Issue(tick);
  return interval;
}

void CefExternalBeginFrameSourceOSR::Issue(const PendingTick& tick) {
  in_flight_ = true;
  issued_at_ = base::TimeTicks::Now();
  stall_warned_ = false;
  last_frame_time_ = tick.frame_time;
  ++ticks_issued_;

  // The generator estimates the sequence number from the time elapsed since
  // the previous tick, so a tick that was dropped leaves a hole, as with a
  // real vsync source; it also fills in |unthrottled_interval|.
  viz::BeginFrameArgs args = args_generator_.GenerateBeginFrameArgs(
      source_id_, tick.frame_time, tick.deadline, tick.interval, tick.interval);
  TRACE_EVENT2("viz", "CefExternalBeginFrameSourceOSR::Issue", "frame_time",
               tick.frame_time, "sequence_number",
               args.frame_id.sequence_number);
  DCHECK(args.IsValid());

  // The reply is the gate: it reopens when the reply runs, or when the reply
  // is destroyed without running (GPU process loss, or a tick kept pending by
  // the compositor until its frame sink exists and then replaced).
  compositor_->IssueExternalBeginFrame(
      args, mojo::WrapCallbackWithDropHandler(
                base::BindOnce(&CefExternalBeginFrameSourceOSR::OnBeginFrameAck,
                               weak_factory_.GetWeakPtr()),
                base::BindOnce(
                    &CefExternalBeginFrameSourceOSR::OnBeginFrameAckDropped,
                    weak_factory_.GetWeakPtr())));
}

void CefExternalBeginFrameSourceOSR::OnBeginFrameAck(
    const viz::BeginFrameAck& ack) {
  DCHECK(in_flight_);
  in_flight_ = false;
  IssueQueuedOrDrop();
}

void CefExternalBeginFrameSourceOSR::OnBeginFrameAckDropped() {
  if (!in_flight_) {
    return;
  }
  in_flight_ = false;
  ++ticks_dropped_;
  IssueQueuedOrDrop();
}

void CefExternalBeginFrameSourceOSR::IssueQueuedOrDrop() {
  if (!queued_) {
    return;
  }

  const PendingTick tick = *queued_;
  queued_.reset();

  // Issue the kept tick with its own time stamps while its interval is not
  // half over; past that point the next tick is due and the slot stays empty,
  // as it would with a missed vsync.
  if (base::TimeTicks::Now() - tick.frame_time < tick.interval / 2) {
    Issue(tick);
  } else {
    ++ticks_dropped_;
  }
}

mojo::PendingAssociatedRemote<viz::mojom::ExternalBeginFrameControllerClient>
CefExternalBeginFrameSourceOSR::CreateExternalBeginFrameControllerClient() {
  TRACE_EVENT0("ui", "CefExternalBeginFrameSourceOSR::CreateClient");
  // A new root frame sink (first creation, or re-creation after a GPU process
  // crash). The gate is left as it is: a reply still alive keeps it closed
  // (the compositor issues the tick it kept pending right after this call),
  // and a reply dropped with the GPU process has already reopened it. The
  // sequence numbers go on: the new Display has no history to match. Only
  // the timing state and the vsync parameters restart.
  receivers_.Clear();
  last_frame_time_ = base::TimeTicks();
  vsync_interval_set_ = base::TimeDelta();
  gaps_n_ = 0;
  gaps_i_ = 0;

  mojo::PendingAssociatedRemote<viz::mojom::ExternalBeginFrameControllerClient>
      remote;
  receivers_.Add(this, remote.InitWithNewEndpointAndPassReceiver());
  needs_begin_frames_ = true;
  return remote;
}

void CefExternalBeginFrameSourceOSR::SetNeedsBeginFrame(
    bool needs_begin_frames) {
  // Informational: the client clocks the ticks and keeps sending them, as it
  // did with the historical implementation.
  needs_begin_frames_ = needs_begin_frames;
}

void CefExternalBeginFrameSourceOSR::SetPreferredInterval(
    base::TimeDelta interval) {
  // The external clock is not negotiable.
}

void CefExternalBeginFrameSourceOSR::NeedsBeginFrameWithId(
    int64_t display_id,
    bool needs_begin_frames) {
  SetNeedsBeginFrame(needs_begin_frames);
}
