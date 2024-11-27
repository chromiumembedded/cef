// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_FRAME_SERVICE_BASE_H_
#define CEF_LIBCEF_BROWSER_FRAME_SERVICE_BASE_H_

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "cef/libcef/common/frame_util.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "url/origin.h"

// Base class for mojo interface implementations tied to a RenderFrameHost
// lifetime. The service will be destroyed on mojo interface connection error
// or RFH deletion.
//
// Based on the old implementation of DocumentServiceBase that existed prior to
// https://crrev.com/2809effa24. CEF requires the old implementation to support
// bindings that outlive navigation.
template <typename Interface>
class CefFrameServiceBase : public Interface,
                            public content::WebContentsObserver {
 public:
  CefFrameServiceBase(content::RenderFrameHost* render_frame_host,
                      mojo::PendingReceiver<Interface> pending_receiver)
      : content::WebContentsObserver(
            content::WebContents::FromRenderFrameHost(render_frame_host)),
        render_frame_host_(render_frame_host),
        receiver_(this, std::move(pending_receiver)) {
    // |this| owns |receiver_|, so unretained is safe.
    receiver_.set_disconnect_handler(
        base::BindOnce(&CefFrameServiceBase::Close, base::Unretained(this)));
  }

 protected:
  // Make the destructor private since |this| can only be deleted by Close().
  ~CefFrameServiceBase() override = default;

  // Returns the RenderFrameHost held by this object.
  content::RenderFrameHost* render_frame_host() const {
    return render_frame_host_;
  }

  // Subclasses can use this to check thread safety.
  // For example: DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  THREAD_CHECKER(thread_checker_);

 private:
  // Disallow calling web_contents() directly from the subclasses to ensure that
  // tab-level state doesn't get queried or updated when the RenderFrameHost is
  // not active.
  // Use WebContents::From(render_frame_host()) instead, but please keep in mind
  // that the render_frame_host() might not be active. See
  // RenderFrameHost::IsActive() for details.
  using content::WebContentsObserver::web_contents;

  // WebContentsObserver implementation.
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) final {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    if (render_frame_host == render_frame_host_) {
      DVLOG(1) << __func__ << ": "
               << frame_util::GetFrameDebugString(
                      render_frame_host->GetGlobalFrameToken())
               << " destroyed";
      if (receiver_.is_bound()) {
        receiver_.ResetWithReason(
            static_cast<uint32_t>(frame_util::ResetReason::kDeleted),
            "Deleted");
      }
      Close();
    }
  }

  // Stops observing WebContents and delete |this|.
  void Close() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DVLOG(1) << __func__;
    delete this;
  }

  const raw_ptr<content::RenderFrameHost> render_frame_host_ = nullptr;
  mojo::Receiver<Interface> receiver_;
};

#endif  // CEF_LIBCEF_BROWSER_FRAME_SERVICE_BASE_H_
