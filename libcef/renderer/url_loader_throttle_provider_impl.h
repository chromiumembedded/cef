// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_RENDERER_URL_LOADER_THROTTLE_PROVIDER_IMPL_H_
#define CEF_LIBCEF_RENDERER_URL_LOADER_THROTTLE_PROVIDER_IMPL_H_

#include <memory>
#include <vector>

#include "base/threading/thread_checker.h"
#include "content/public/renderer/url_loader_throttle_provider.h"

// Instances must be constructed on the render thread, and then used and
// destructed on a single thread, which can be different from the render thread.
class CefURLLoaderThrottleProviderImpl
    : public content::URLLoaderThrottleProvider {
 public:
  explicit CefURLLoaderThrottleProviderImpl(
      content::URLLoaderThrottleProviderType type);

  ~CefURLLoaderThrottleProviderImpl() override;

  // content::URLLoaderThrottleProvider implementation.
  std::unique_ptr<content::URLLoaderThrottleProvider> Clone() override;
  std::vector<std::unique_ptr<content::URLLoaderThrottle>> CreateThrottles(
      int render_frame_id,
      const blink::WebURLRequest& request,
      content::ResourceType resource_type) override;
  void SetOnline(bool is_online) override;

 private:
  // This copy constructor works in conjunction with Clone(), not intended for
  // general use.
  CefURLLoaderThrottleProviderImpl(
      const CefURLLoaderThrottleProviderImpl& other);

  content::URLLoaderThrottleProviderType type_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_ASSIGN(CefURLLoaderThrottleProviderImpl);
};

#endif  // CEF_LIBCEF_RENDERER_URL_LOADER_THROTTLE_PROVIDER_IMPL_H_
