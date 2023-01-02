// Copyright 2022 The Chromium Embedded Framework Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/media_stream_registrar.h"

#include "libcef/browser/browser_host_base.h"
#include "libcef/browser/thread_util.h"

class CefMediaStreamUI : public content::MediaStreamUI {
 public:
  CefMediaStreamUI(base::WeakPtr<CefMediaStreamRegistrar> registrar,
                   bool has_video,
                   bool has_audio)
      : registrar_(registrar), has_video_(has_video), has_audio_(has_audio) {}

  ~CefMediaStreamUI() override {
    if (registrar_) {
      registrar_->UnregisterMediaStream(label_);
    }
  }

  CefMediaStreamUI(const CefMediaStreamUI&) = delete;
  CefMediaStreamUI& operator=(const CefMediaStreamUI&) = delete;

  gfx::NativeViewId OnStarted(
      base::RepeatingClosure stop,
      SourceCallback source,
      const std::string& label,
      std::vector<content::DesktopMediaID> screen_capture_ids,
      StateChangeCallback state_change) override {
    if (registrar_) {
      label_ = label;
      registrar_->RegisterMediaStream(label, has_video_, has_audio_);
    }
    return 0;
  }

  void OnDeviceStoppedForSourceChange(
      const std::string& label,
      const content::DesktopMediaID& old_media_id,
      const content::DesktopMediaID& new_media_id) override {}

  void OnDeviceStopped(const std::string& label,
                       const content::DesktopMediaID& media_id) override {}

 private:
  base::WeakPtr<CefMediaStreamRegistrar> registrar_;
  const bool has_video_;
  const bool has_audio_;
  std::string label_;
};

CefMediaStreamRegistrar::CefMediaStreamRegistrar(CefBrowserHostBase* browser)
    : browser_(browser) {}

std::unique_ptr<content::MediaStreamUI>
CefMediaStreamRegistrar::MaybeCreateMediaStreamUI(bool has_video,
                                                  bool has_audio) {
  // Only create the object if the callback will be executed.
  if (auto client = browser_->GetClient()) {
    if (auto handler = client->GetDisplayHandler()) {
      return std::make_unique<CefMediaStreamUI>(weak_ptr_factory_.GetWeakPtr(),
                                                has_video, has_audio);
    }
  }
  return nullptr;
}

void CefMediaStreamRegistrar::RegisterMediaStream(const std::string& label,
                                                  bool video,
                                                  bool audio) {
  CEF_REQUIRE_UIT();
  MediaStreamInfo info = {video, audio};
  registered_streams_.insert(std::make_pair(label, info));
  NotifyMediaStreamChange();
}

void CefMediaStreamRegistrar::UnregisterMediaStream(const std::string& label) {
  CEF_REQUIRE_UIT();
  registered_streams_.erase(label);
  NotifyMediaStreamChange();
}

void CefMediaStreamRegistrar::NotifyMediaStreamChange() {
  bool video = false;
  bool audio = false;
  for (const auto& media_stream : registered_streams_) {
    const auto& info = media_stream.second;
    if (!video) {
      video = info.video;
    }
    if (!audio) {
      audio = info.audio;
    }
  }

  if (audio == last_notified_info_.audio &&
      video == last_notified_info_.video) {
    return;
  }

  last_notified_info_ = {video, audio};

  if (auto client = browser_->GetClient()) {
    if (auto handler = client->GetDisplayHandler()) {
      handler->OnMediaAccessChange(browser_, video, audio);
    }
  }
}
