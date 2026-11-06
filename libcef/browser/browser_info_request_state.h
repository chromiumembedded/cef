// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_BROWSER_INFO_REQUEST_STATE_H_
#define CEF_LIBCEF_BROWSER_BROWSER_INFO_REQUEST_STATE_H_
#pragma once

#include <cstdint>
#include <map>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"

// Pending renderer lookups can precede frame registration or outlive frame
// deletion. Keep their completion and timeout handling independent of RFHs and
// Mojo so that these orderings can be tested directly. Callers must serialize
// access. Callbacks run on the supplied callback_runner, inline when already on
// that sequence, so callers must account for callback reentrancy.
// BrowserInfo must be default-constructible to represent a canceled lookup.
template <typename FrameToken, typename BrowserInfo>
class CefBrowserInfoRequestState {
 public:
  using Callback = base::OnceCallback<void(BrowserInfo, bool is_excluded)>;
  using TimeoutId = uint64_t;

  // Queues one request per token and returns its timeout generation. Scheduling
  // the timeout is the caller's responsibility.
  TimeoutId Queue(const FrameToken& token,
                  Callback callback,
                  scoped_refptr<base::SequencedTaskRunner> callback_runner) {
    DCHECK(callback);
    DCHECK(callback_runner);
    const auto timeout_id = ++next_timeout_id_;
    const bool inserted =
        pending_
            .try_emplace(token, timeout_id, std::move(callback),
                         std::move(callback_runner))
            .second;
    DCHECK(inserted);
    return timeout_id;
  }

  // Removes the request before delivering its response. Returns false if no
  // request was pending; true means consumed, even if delivery is posted.
  bool Complete(const FrameToken& token,
                BrowserInfo browser_info,
                bool is_excluded) {
    auto it = pending_.find(token);
    if (it == pending_.end()) {
      return false;
    }
    auto pending = std::move(it->second);
    pending_.erase(it);
    pending.Complete(std::move(browser_info), is_excluded);
    return true;
  }

  // Completes a pending request with default BrowserInfo and is_excluded=false.
  bool Cancel(const FrameToken& token) {
    return Complete(token, BrowserInfo(), /*is_excluded=*/false);
  }

  // Cancels only the matching generation, ignoring completed requests and stale
  // timeouts when a newer request uses the same token.
  bool Timeout(const FrameToken& token, TimeoutId timeout_id) {
    auto it = pending_.find(token);
    if (it == pending_.end() || it->second.timeout_id != timeout_id) {
      return false;
    }
    return Cancel(token);
  }

  // Used when the renderer process is destroyed. Remove all matching requests
  // before delivering callbacks, which may re-enter this object.
  template <typename Predicate>
  void CancelMatching(Predicate matches) {
    std::vector<Pending> canceled;
    for (auto it = pending_.begin(); it != pending_.end();) {
      if (matches(it->first)) {
        canceled.push_back(std::move(it->second));
        it = pending_.erase(it);
      } else {
        ++it;
      }
    }
    for (auto& pending : canceled) {
      pending.Complete(BrowserInfo(), /*is_excluded=*/false);
    }
  }

 private:
  struct Pending {
    Pending(TimeoutId timeout_id,
            Callback callback,
            scoped_refptr<base::SequencedTaskRunner> callback_runner)
        : timeout_id(timeout_id),
          callback(std::move(callback)),
          callback_runner(std::move(callback_runner)) {}

    void Complete(BrowserInfo browser_info, bool is_excluded) {
      if (callback_runner->RunsTasksInCurrentSequence()) {
        std::move(callback).Run(std::move(browser_info), is_excluded);
      } else {
        callback_runner->PostTask(
            FROM_HERE, base::BindOnce(std::move(callback),
                                      std::move(browser_info), is_excluded));
      }
    }

    TimeoutId timeout_id;
    Callback callback;
    scoped_refptr<base::SequencedTaskRunner> callback_runner;
  };

  std::map<FrameToken, Pending> pending_;
  TimeoutId next_timeout_id_ = 0;
};

#endif  // CEF_LIBCEF_BROWSER_BROWSER_INFO_REQUEST_STATE_H_
